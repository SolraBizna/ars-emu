#ifndef NO_DEBUG_CORES
/* TODO: Debugger is entirely in English */

#include "ars-emu.hh"
#include "w65c02.hh"
#include "w65c02_dis.hh"
#include "io.hh"
#include "eval.hh"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <regex>

/* TODO: stack trace */

/* WTF, Microsoft */
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

using ARS::getBankForAddr;

namespace std {
  static string to_string(W65C02::ReadType rt) {
    switch(rt) {
      case W65C02::ReadType::OPCODE: return "OPCODE";
      case W65C02::ReadType::PREEMPTED: return "PREEMPTED";
      case W65C02::ReadType::UNUSED: return "UNUSED";
      case W65C02::ReadType::OPERAND: return "OPERAND";
      case W65C02::ReadType::POINTER: return "POINTER";
      case W65C02::ReadType::DATA: return "DATA";
      case W65C02::ReadType::POP: return "POP";
      case W65C02::ReadType::IOP: return "IOP";
      case W65C02::ReadType::WAI: return "WAI";
      case W65C02::ReadType::STP: return "STP";
    }
    /* NOTREACHED */
    return "UNKNOWN";
  }
  static string to_string(W65C02::WriteType rt) {
    switch(rt) {
      case W65C02::WriteType::DATA: return "DATA";
      case W65C02::WriteType::PUSH: return "PUSH";
    }
    /* NOTREACHED */
    return "UNKNOWN";
  }
}

namespace {
  class CPU_ScanlineDebug : public ARS::CPU {
    int cycle_budget = 0;
    uint64_t cycle_count = 0;
    uint32_t audio_cycle_counter = 0;
    W65C02::Core<CPU_ScanlineDebug> core;
    W65C02::Disassembler<CPU_ScanlineDebug> disassembler;
    std::multimap<uint32_t, std::string> addr_to_label_map;
    std::multimap<uint32_t, std::string> addr_to_definition_map;
    std::map<std::string, uint32_t> symdef_to_addr_map;
    std::vector<uint32_t> breakpoints, watchpoints, rwatchpoints;
    enum class TimeDisplayMode {
      ABSOLUTE, FRAME, DETAILED
    } time_display_mode = TimeDisplayMode::FRAME;
    bool trace_pins = false, trace_reads = false, trace_writes = false,
      trace_execs = false, trace_other_accesses = false, irq = false,
      so = false, nmi = false, brk_brk = true, brk_loop = true,
      brk_wai = false;
    bool stopped = true;
    bool read_break_occurred = false, write_break_occurred = false;
    uint32_t read_break_addr, write_break_addr;
    uint8_t read_break_value, write_break_value;
    int debug_steps = 0;
    uint32_t last_exec_address = ~uint32_t(0);
    uint16_t executing_pc = 0;
    std::string repeat_command;
    bool get_symbol(const std::string& string, uint32_t& out) {
      if(string.length() > 0 && string[0] == '_') {
        if(string == "_A") { out = core.read_a(); return true; }
        else if(string == "_X") { out = core.read_x(); return true; }
        else if(string == "_Y") { out = core.read_y(); return true; }
        else if(string == "_P") { out = core.read_p(); return true; }
        else if(string == "_S") { out = core.read_s(); return true; }
        else if(string == "_PC") { out = core.read_pc(); return true; }
      }
      auto it = symdef_to_addr_map.find(string);
      if(it == symdef_to_addr_map.end()) return false;
      else {
        out = it->second;
        return true;
      }
    }
    uint8_t read_address(uint32_t addr) {
      return peek_byte(addr);
    }
    void do_commands() {
      stopped = true;
      ARS::temporalAnomaly();
      do {
        std::cout << "> ";
        std::string input_line;
        std::getline(std::cin, input_line);
        if(!std::cin) {
          std::cout << "(no more commands)\n";
          ARS::triggerQuit();
        }
        std::vector<std::string> args;
        auto it = input_line.cbegin();
        while(it != input_line.cend()) {
          while(it != input_line.cend() && isspace(*it)) ++it;
          if(it == input_line.cend()) break;
          std::ostringstream sstream;
          bool quoted = false, bork = false;
          while(it != input_line.cend() && !bork) {
            switch(*it) {
            case '"':
              quoted = !quoted;
              ++it;
              break;
            case '\\':
              ++it;
              if(it == input_line.cend()) sstream << '\\';
              else sstream << *it++;
              break;
            default:
              if(!quoted && isspace(*it)) bork = true;
              else sstream << *it++;
              break;
            }
          }
          args.emplace_back(sstream.str());
        }
        if(args.size() == 0 && repeat_command.size() > 0)
          args.emplace_back(repeat_command);
        if(args.size() == 0) continue;
        std::string chosen_command = std::move(args[0]);
        args.erase(args.begin());
        repeat_command = "";
        bool found_command = false;
        for(auto& cmd : commands) {
          if(chosen_command == cmd.name) {
            (this->*cmd.func)(args);
            found_command = true;
            break;
          }
        }
        if(!found_command) {
          std::cout << "Unknown command\n";
        }
      } while(stopped);
    }
    bool is_normal_read(W65C02::ReadType rt) {
      switch(rt) {
      case W65C02::ReadType::POINTER:
      case W65C02::ReadType::DATA:
      case W65C02::ReadType::POP:
        return true;
      default:
        return false;
      }
    }
    void output_time(std::ostream& out) {
      switch(cycle_count < ARS::CYCLES_PER_FRAME ? TimeDisplayMode::ABSOLUTE
             : time_display_mode) {
      case TimeDisplayMode::ABSOLUTE:
        out <<"@cycle "<<cycle_count;
        break;
      case TimeDisplayMode::FRAME:
        out <<"@frame "<<cycle_count/ARS::CYCLES_PER_FRAME<<" cycle "<<std::setw(6)
            <<cycle_count%ARS::CYCLES_PER_FRAME;
        break;
      case TimeDisplayMode::DETAILED:
        out <<"@frame "<<cycle_count/ARS::CYCLES_PER_FRAME;
        int cycle_within_frame = (int)(cycle_count%ARS::CYCLES_PER_FRAME);
        if(cycle_within_frame < ARS::CYCLES_PER_SCANOUT) {
          // the -64 is to fixup the scanline renderer's fake OL accesses
          int scanline_number = cycle_within_frame / (ARS::CYCLES_PER_SCANLINE-64);
          if(scanline_number < ARS::BLANK_CYCLES_PER_SCANLINE)
            out<<" H-blank "<<scanline_number<<" cycle "<<cycle_within_frame;
          else
            out<<" scan "<<scanline_number<<" cycle "
               <<(cycle_within_frame - ARS::BLANK_CYCLES_PER_SCANLINE);
        }
        else
          out<<" V-blank cycle "<<(cycle_within_frame-ARS::CYCLES_PER_SCANOUT);
        out<<"(ish)";
        break;
      }
    }
    void output_addr(std::ostream& out, uint16_t addr,
                     bool OL = false, bool VPB = false, bool SYNC = false) {
      output_mapped_addr(out,
                         ARS::cartridge->map_addr(getBankForAddr(addr),
                                                  addr, OL, VPB, SYNC));
    }
    void output_mapped_addr(std::ostream& out, uint32_t addr) {
      bool have_outputted_alternate = false;
      if(addr < 0x10000)
        out << "$" << std::hex << std::setw(4) << std::setfill('0') << addr
            << std::dec;
      else
        out << "$" <<std::hex << std::setw(4) << std::setfill('0')
            << (addr>>16) << ":" << std::setw(4) << std::setfill('0')
            << (addr&65535) << std::dec;
      auto it = addr_to_label_map.upper_bound(addr);
      if(it != addr_to_label_map.begin()) {
        --it;
        uint32_t found = it->first;
        while(it != addr_to_label_map.begin()) {
          --it;
          if(it->first != found) { ++it; break; }
        }
        do {
          out << (have_outputted_alternate?" (":", ")
              << it->second << "+" << (addr-found);
          ++it;
        } while(it != addr_to_label_map.end() && it->first == found);
      }
      it = addr_to_definition_map.find(addr);
      if(it != addr_to_definition_map.end()) {
        do {
          out << (have_outputted_alternate?" (":", ") << it->first;
          have_outputted_alternate = true;
          ++it;
        } while(it != addr_to_label_map.end() && it->first == addr);
      }
      if(have_outputted_alternate) out << ")";
    }
    uint16_t parse_address(const std::string& res) {
      size_t pos = 0;
      unsigned long addr = std::stoul(res, &pos, 16);
      if(addr > 65535 || pos != res.length())
        throw std::out_of_range("out of range 0000-ffff");
      return addr;
    }
    void tryDelocalizeName(std::string& name, uint32_t addr) {
      SDL_assert(name.length() > 0 && name[0] == '_');
      auto it = addr_to_label_map.upper_bound(addr);
      if(it == addr_to_label_map.begin()) return;
      while(it-- != addr_to_label_map.begin()) {
        if((addr>>16) != (it->first>>16)) {
          // bank number is different, we're out of parental candidates
          break;
        }
        else if(it->second[0] == '_'
                || std::find(it->second.begin(), it->second.end(), ':')
                != it->second.end()) {
          // another local label, can't be our parent
          continue;
        }
        else {
          /* found it! */
          name = it->second + ':' + name;
          return;
        }
      }
    }
  public:
    CPU_ScanlineDebug() : core(*this), disassembler(*this) {}
    ~CPU_ScanlineDebug() {}
    void handleReset() override {
      if(trace_pins) {
        output_time(std::cout);
        std::cout << ": RESET!\n";
      }
      cycle_budget = 0;
      cycle_count = 0;
      last_exec_address = ~uint32_t(0);
      core.reset();
    }
    void eatCycles(int count) override {
      cycle_budget -= count;
      cycle_count += count;
    }
    void runCycles(int count) override {
      cycle_budget += count;
      audio_cycle_counter += count;
      while(core.in_productive_state() && cycle_budget > 0) {
        uint32_t mapped_pc = ARS::cartridge->map_addr(getBankForAddr
                                                      (core.read_pc()),
                                                      core.read_pc(),
                                                      false, false, true);
        if(!stopped) {
          if(mapped_pc == last_exec_address && brk_loop) {
            std::cout << "Breaking an obvious infinite loop!\n";
            stopped = true;
          }
          else if(std::find(breakpoints.begin(), breakpoints.end(), mapped_pc)
                  != breakpoints.end()) {
            std::cout << "Breakpoint hit!\n";
            stopped = true;
          }
          else if(brk_brk || brk_wai) {
            uint8_t opcode = peek_opcode(core.read_pc());
            switch(opcode) {
            case 0x00: // BRK
              if(brk_brk) {
                std::cout << "Breaking on BRK instruction!"
                  " (Welcome to the weeds.)\n";
                stopped = true;
              }
              break;
            case 0xCB: // WAI
              if(brk_wai) {
                std::cout << "Breaking on WAI instruction!\n";
                stopped = true;
              }
              break;
            }
          }
        }
        if(read_break_occurred || write_break_occurred) {
          if(read_break_occurred) {
            std::cout << "Breaking on read from ";
            output_mapped_addr(std::cout, read_break_addr);
            std::cout << "!\n";
          }
          if(write_break_occurred) {
            std::cout << "Breaking on write to ";
            output_mapped_addr(std::cout, write_break_addr);
            std::cout << "!\n";
          }
          std::cout << "The instruction that did it was:\n";
          uint32_t last_try_again = ARS::cartridge->map_addr(getBankForAddr
                                                             (executing_pc),
                                                             executing_pc,
                                                             false, false,
                                                             true);
          if(last_try_again != last_exec_address) {
            output_mapped_addr(std::cout, last_exec_address);
            std::cout << "\n(currently inaccessible as the mapper state has changed)\n";
          }
          else {
            uint16_t cur_pc = core.read_pc();
            core.write_pc(executing_pc);
            std::vector<std::string> empty;
            cmd_here(empty);
            core.write_pc(cur_pc);
          }
          stopped = true;
          read_break_occurred = false;
          write_break_occurred = false;
          std::cout << "and the instruction that's about to execute is:\n";
        }
        last_exec_address = mapped_pc;
        if(stopped) {
          std::vector<std::string> empty;
          cmd_here(empty);
          do_commands();
        }
        executing_pc = core.read_pc();
        core.step();
        if(debug_steps > 0) {
          --debug_steps;
          if(debug_steps == 0)
            stopped = true;
        }
        if(!core.in_productive_state() || stopped)
          last_exec_address = ~0;
      }
      if(cycle_budget > 0) {
        cycle_count += cycle_budget;
        cycle_budget = 0;
      }
      while(audio_cycle_counter >= 256) {
        audio_cycle_counter -= 256;
        ARS::output_apu_sample();
      }
    }
    void setIRQ(bool irq) override {
      if(trace_pins && this->irq != irq) {
        output_time(std::cout);
        if(irq)
          std::cout << ": IRQB# becomes active" << std::endl;
        else
          std::cout << ": IRQB# becomes inactive" << std::endl;
      }
      core.set_irq(this->irq = irq);
    }
    void setSO(bool so) override {
      if(trace_pins && this->so != so) {
        output_time(std::cout);
        if(so)
          std::cout << ": SOB# becomes active" << std::endl;
        else
          std::cout << ": SOB# becomes inactive" << std::endl;
      }
      core.set_so(this->so = so);
    }
    void setNMI(bool nmi) override {
      if(trace_pins && this->nmi != nmi) {
        output_time(std::cout);
        if(nmi)
          std::cout << ": NMIB# becomes active" << std::endl;
        else
          std::cout << ": NMIB# becomes inactive" << std::endl;
      }
      core.set_nmi(this->nmi = nmi);
    }
    uint8_t read_byte(uint16_t addr, W65C02::ReadType rt) {
      uint32_t mapped_addr = ARS::cartridge->map_addr(getBankForAddr(addr),
                                                      addr, false, false,
                                                      false, false);
      uint8_t ret;
      ret = ARS::read(addr);
      if(std::find(rwatchpoints.begin(), rwatchpoints.end(), mapped_addr)
         != rwatchpoints.end()) {
        read_break_occurred = true;
        read_break_addr = mapped_addr;
        read_break_value = ret;
      }
      if(trace_reads || trace_other_accesses) {
        if(rt != W65C02::ReadType::WAI && rt != W65C02::ReadType::STP
           && (is_normal_read(rt) ? trace_reads : trace_other_accesses)) {
          output_time(std::cout);
          std::cout << ": read ";
          output_addr(std::cout, addr);
          std::cout << " ("
                    << std::to_string(rt) << ") returns $"
                    << std::hex << std::setw(2) << std::setfill('0')
                    << (int)ret << std::dec << "\n";
        }
      }
      --cycle_budget;
      ++cycle_count;
      return ret;
    }
    uint8_t read_opcode(uint16_t addr, W65C02::ReadType rt) {
      if(rt == W65C02::ReadType::PREEMPTED) {
        last_exec_address = ~uint32_t(0);
      }
      uint32_t mapped_addr = ARS::cartridge->map_addr(getBankForAddr(addr),
                                                      addr, false, false,
                                                      true, false);
      uint8_t ret;
      ret = ARS::read(addr, false, false, true);
      if(std::find(rwatchpoints.begin(), rwatchpoints.end(), mapped_addr)
         != rwatchpoints.end()) {
        read_break_occurred = true;
        read_break_addr = mapped_addr;
        read_break_value = ret;
      }
      if(trace_execs) {
        output_time(std::cout);
        std::cout << ":\n  P=";
        uint8_t p = core.read_p();
        std::cout
          << (char)((p&0x80)?'N':'n')
          << (char)((p&0x40)?'V':'v')
          << (char)((p&0x20)?'1':'0')
          << (char)((p&0x10)?'B':'b')
          << (char)((p&0x08)?'D':'d')
          << (char)((p&0x04)?'I':'i')
          << (char)((p&0x02)?'Z':'z')
          << (char)((p&0x01)?'C':'c')
          << std::hex
          << ", A=$" << std::setfill('0') << std::setw(2) << (int)core.read_a()
          << ", X=$" << std::setfill('0') << std::setw(2) << (int)core.read_x()
          << ", Y=$" << std::setfill('0') << std::setw(2) << (int)core.read_y()
          << ", S=$" << std::setfill('0') << std::setw(2) << (int)core.read_s()
          << std::dec << "\n  ";
        if(rt == W65C02::ReadType::PREEMPTED)
          std::cout << "would have executed ";
        else
          std::cout << "execute ";
        output_addr(std::cout, addr, false, false, true);
        std::cout << ": ";
        disassembler.disassemble(addr, std::cout);
        std::cout << "\n";
      }
      --cycle_budget;
      ++cycle_count;
      return ret;
    }
    uint8_t fetch_vector_byte(uint16_t addr) {
      last_exec_address = ~uint32_t(0);
      uint32_t mapped_addr = ARS::cartridge->map_addr(getBankForAddr(addr),
                                                      addr, false, true,
                                                      false, false);
      uint8_t ret = ARS::read(addr, false, true);
      if(std::find(rwatchpoints.begin(), rwatchpoints.end(), mapped_addr)
         != rwatchpoints.end()) {
        read_break_occurred = true;
        read_break_addr = mapped_addr;
        read_break_value = ret;
      }
      if(trace_other_accesses) {
        output_time(std::cout);
        std::cout << ": read ";
        output_addr(std::cout, addr, false, true);
        std::cout << " (vector pull, ";
        switch(addr) {
        case core.NMI_VECTOR: std::cout << "NMI low"; break;
        case core.NMI_VECTOR+1: std::cout << "NMI high"; break;
        case core.RESET_VECTOR: std::cout << "RESET low"; break;
        case core.RESET_VECTOR+1: std::cout << "RESET high"; break;
        case core.IRQ_VECTOR: std::cout << "IRQ low"; break;
        case core.IRQ_VECTOR+1: std::cout << "IRQ high"; break;
        default: std::cout << "INVALID"; break;
        }
        std::cout << ") returns $"
                  << std::hex << std::setw(2) << std::setfill('0')
                  << (int)ret << std::dec << "\n";
      }
      --cycle_budget;
      ++cycle_count;
      return ret;
    }
    void write_byte(uint16_t addr, uint8_t byte, W65C02::WriteType wt) {
      uint32_t mapped_addr = ARS::cartridge->map_addr(getBankForAddr(addr),
                                                      addr, false, false,
                                                      false, true);
      if(std::find(watchpoints.begin(), watchpoints.end(), mapped_addr)
         != watchpoints.end()) {
        write_break_occurred = true;
        write_break_addr = mapped_addr;
        write_break_value = byte;
      }
      if(trace_writes) {
        output_time(std::cout);
        std::cout << ": write $" << std::hex << std::setw(2)
                  << std::setfill('0') << (int)byte << std::dec;
        std::cout << " to ";
        output_addr(std::cout, addr);
        std::cout << " (" << std::to_string(wt) << ")\n";
      }
      ARS::write(addr, byte);
      --cycle_budget;
      ++cycle_count;
    }
    bool isStopped() override {
      return core.is_stopped();
    }
    uint8_t peek_byte(uint16_t addr) {
      return ARS::read(addr);
    }
    uint8_t peek_opcode(uint16_t addr) {
      return ARS::read(addr, false, false, true);
    }
    void silently_write(uint16_t addr, uint8_t value) {
      return ARS::write(addr, value);
    }
    void loadSymbols(std::istream& f) {
      char line[128];
      enum class sec {
        LABELS, DEFINITIONS, OTHER
      } curSec = sec::OTHER;
      std::regex label_regex("^([0-9a-f][0-9a-f][0-9a-f][0-9a-f]):([0-9a-f][0-9a-f][0-9a-f][0-9a-f]) (.*)$");
      std::regex definition_regex("^([0-9a-f][0-9a-f][0-9a-f][0-9a-f])([0-9a-f][0-9a-f][0-9a-f][0-9a-f]) (.*)$");
      std::cmatch matchResult;
      while(f) {
        f.getline(line, sizeof(line));
        if(!f) break;
        if(!line[0]) continue;
        if(line[strlen(line)-1] == '\r') line[strlen(line)-1] = 0;
        if(!line[0]) continue;
        if(line[0] == '[') {
          if(!strcmp(line, "[definitions]")) curSec = sec::DEFINITIONS;
          else if(!strcmp(line, "[labels]")) curSec = sec::LABELS;
          else curSec = sec::OTHER;
        }
        else if(curSec == sec::LABELS) {
          if(std::regex_match(line, matchResult, label_regex,
                              std::regex_constants::match_continuous)) {
            std::string name(matchResult[3]);
            if(name.length() == 0) {
              std::cout << "Ignoring unnamed symbol\n";
              continue;
            }
            try {
              uint16_t bank = parse_address(matchResult[1]);
              uint16_t addr = parse_address(matchResult[2]);
              uint32_t mapped = (static_cast<uint32_t>(bank)<<16)|addr;
              if(name[0] == '_') tryDelocalizeName(name, mapped);
              if(symdef_to_addr_map.find(name) != symdef_to_addr_map.end()) {
                std::cout << "Ignoring duplicate symbol "<<name<<"\n";
                continue;
              }
              addr_to_label_map.insert(std::make_pair(mapped, name));
              symdef_to_addr_map.insert(std::make_pair(name, mapped));
            }
            catch(std::exception& e) {
              std::cout << "Ignoring invalid label "<<matchResult[3]<<"\n";
            }
          }
        }
        else if(curSec == sec::DEFINITIONS) {
          if(std::regex_match(line, matchResult, definition_regex,
                              std::regex_constants::match_continuous)) {
            std::string name(matchResult[3]);
            if(name.length() == 0) {
              std::cout << "Ignoring unnamed symbol\n";
              continue;
            }
            else if(symdef_to_addr_map.find(name) != symdef_to_addr_map.end()){
              std::cout << "Ignoring duplicate symbol "<<name<<"\n";
              continue;
            }
            try {
              uint16_t high = parse_address(matchResult[1]);
              uint16_t low = parse_address(matchResult[2]);
              uint32_t value = (static_cast<uint32_t>(high)<<16)|low;
              addr_to_definition_map.insert(std::make_pair(value, name));
              symdef_to_addr_map.insert(std::make_pair(name, value));
            }
            catch(std::exception& e) {
              std::cout << "Ignoring invalid label "<<matchResult[3]<<"\n";
            }
          }
        }
      }
    }
  private:
        static const struct command {
      std::string name, help;
      void(CPU_ScanlineDebug::*func)(std::vector<std::string>& args);
    } commands[25];
    void cmd_help(std::vector<std::string>&) {
      std::cout << "Known commands:\n";
      for(auto& cmd : commands) {
        std::cout << "\n" << cmd.name << ":\n" << cmd.help << "\n";
      }
      std::cout << "\n";
    }
    bool get_boolean_arg(std::vector<std::string>& args, bool& out) {
      if(args.size() == 0) return false;
      auto& arg = args[0];
      if(arg.length() == 0) return false;
      else if(arg == "0" || arg[0] == 'n' || arg[0] == 'N' || arg[0] == 'f'
         || arg[0] == 'F') {
        out = false;
        return true;
      }
      else if(arg == "1" || arg[0] == 'y' || arg[0] == 'Y' || arg[0] == 't'
         || arg[0] == 'T') {
        out = true;
        return true;
      }
      else return false;
    }
    void cmd_load_symbols(std::vector<std::string>& args) {
      if(args.empty()) {
        std::cout << "usage: load-symbols path/to/symbols.sym"
          " [path/to/more/symbols.sym ...]\n";
      }
      else {
        for(auto& path : args) {
          auto symfile = IO::OpenRawPathForRead(path);
          if(symfile && *symfile)
            loadSymbols(*symfile);
        }
      }
    }
    void cmd_dump_sprite_memory(std::vector<std::string>&) {
      ARS::PPU::dumpSpriteMemory();
    }
    void cmd_trace_pins(std::vector<std::string>& args) {
      get_boolean_arg(args, trace_pins);
      std::cout << "trace-pins is " << (trace_pins ? "ON" : "OFF") << "\n";
    }
    void cmd_trace_reads(std::vector<std::string>& args) {
      get_boolean_arg(args, trace_reads);
      std::cout << "trace-reads is " << (trace_reads ? "ON" : "OFF") << "\n";
    }
    void cmd_trace_writes(std::vector<std::string>& args) {
      get_boolean_arg(args, trace_writes);
      std::cout << "trace-writes is " << (trace_writes ? "ON" : "OFF") << "\n";
    }
    void cmd_trace_other_accesses(std::vector<std::string>& args) {
      get_boolean_arg(args, trace_other_accesses);
      std::cout << "trace-accesses is " << (trace_other_accesses ? "ON" : "OFF") << "\n";
    }
    void cmd_trace_execs(std::vector<std::string>& args) {
      get_boolean_arg(args, trace_execs);
      std::cout << "trace_execs is " << (trace_execs ? "ON" : "OFF") << "\n";
    }
    void cmd_break(std::vector<std::string>& args) {
      if(args.size() == 0) {
        std::cout << "Breakpoints:\n";
        for(uint32_t addr : breakpoints) {
          std::cout << "\t";
          output_mapped_addr(std::cout, addr);
          std::cout << "\n";
        }
        if(brk_brk) std::cout << "\t(will break on BRK instructions)\n";
        if(brk_wai) std::cout << "\t(will break on WAI instructions)\n";
        if(brk_loop) std::cout << "\t(will break on infinite loops)\n";
      }
      else {
        for(const std::string& arg : args) {
          if(arg == "brk" || arg == "BRK") {
            if(brk_brk)
              std::cout << "Already breaking on BRK instructions\n";
            else {
              brk_brk = true;
              std::cout << "Will now break on BRK instructions\n";
            }
          }
          else if(arg == "wai" || arg == "WAI") {
            if(brk_wai)
              std::cout << "Already breaking on WAI instructions\n";
            else {
              brk_wai = true;
              std::cout << "Will now break on WAI instructions\n";
            }
          }
          else if(arg == "loop" || arg == "loops") {
            if(brk_loop)
              std::cout << "Already breaking on infinite loops\n";
            else {
              brk_loop = true;
              std::cout << "Will now break on infinite loops\n";
            }
          }
          else {
            uint32_t breakpoint;
            if(eval(arg, std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                   std::placeholders::_1,
                                   std::placeholders::_2),
                    std::bind(&CPU_ScanlineDebug::read_address, this,
                              std::placeholders::_1), breakpoint)) {
              if(std::find(breakpoints.begin(), breakpoints.end(),
                           breakpoint) != breakpoints.end())
                std::cout << "Already breaking on ";
              else {
                std::cout << "Will break on ";
                breakpoints.push_back(breakpoint);
              }
              output_mapped_addr(std::cout, breakpoint);
              std::cout << "\n";
            }
          }
        }
      }
    }
    void cmd_unbreak(std::vector<std::string>& args) {
      if(args.size() == 0) {
        cmd_break(args); // show breakpoints
      }
      else {
        for(const std::string& arg : args) {
          if(arg == "brk" || arg == "BRK") {
            if(!brk_brk)
              std::cout << "Not currently breaking on BRK instructions\n";
            else {
              brk_brk = false;
              std::cout << "Will no longer break on BRK instructions\n";
            }
          }
          else if(arg == "wai" || arg == "WAI") {
            if(!brk_wai)
              std::cout << "Not currently breaking on WAI instructions\n";
            else {
              brk_wai = false;
              std::cout << "Will no longer break on WAI instructions\n";
            }
          }
          else if(arg == "loop" || arg == "loops") {
            if(!brk_loop)
              std::cout << "Already not breaking on infinite loops\n";
            else {
              brk_loop = false;
              std::cout << "Will no longer break on infinite loops\n";
            }
          }
          else {
            uint32_t breakpoint;
            if(eval(arg, std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                   std::placeholders::_1,
                                   std::placeholders::_2),
                    std::bind(&CPU_ScanlineDebug::read_address, this,
                              std::placeholders::_1), breakpoint)) {
              auto it = std::remove(breakpoints.begin(), breakpoints.end(),
                                    breakpoint);
              if(it == breakpoints.end())
                std::cout << "No existing breakpoints for ";
              else {
                breakpoints.erase(it);
                std::cout << "Removed breakpoints for ";
              }
              output_mapped_addr(std::cout, breakpoint);
              std::cout << "\n";
            }
          }
        }
      }
    }
    void cmd_watch(std::vector<std::string>& args) {
      if(args.size() == 0) {
        std::cout << "Watchpoints:\n";
        for(uint32_t addr : watchpoints) {
          std::cout << "\t";
          output_mapped_addr(std::cout, addr);
          std::cout << "\n";
        }
      }
      else {
        for(const std::string& arg : args) {
          uint32_t watchpoint;
          if(eval(arg, std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
                  std::bind(&CPU_ScanlineDebug::read_address, this,
                            std::placeholders::_1), watchpoint)) {
            if(std::find(watchpoints.begin(), watchpoints.end(),
                         watchpoint) != watchpoints.end())
              std::cout << "Already breaking on writes to ";
            else {
              std::cout << "Will break on writes to ";
              watchpoints.push_back(watchpoint);
            }
            output_mapped_addr(std::cout, watchpoint);
            std::cout << "\n";
          }
        }
      }
    }
    void cmd_unwatch(std::vector<std::string>& args) {
      if(args.size() == 0) {
        cmd_watch(args); // show watchpoints
      }
      else {
        for(const std::string& arg : args) {
          uint32_t watchpoint;
          if(eval(arg, std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
                  std::bind(&CPU_ScanlineDebug::read_address, this,
                            std::placeholders::_1), watchpoint)) {
            auto it = std::remove(watchpoints.begin(), watchpoints.end(),
                                  watchpoint);
            if(it == watchpoints.end())
              std::cout << "Not currently breaking on writes to ";
            else {
              watchpoints.erase(it);
              std::cout << "No longer breaking on writes to ";
            }
            output_mapped_addr(std::cout, watchpoint);
            std::cout << "\n";
          }
        }
      }
    }
    void cmd_rwatch(std::vector<std::string>& args) {
      if(args.size() == 0) {
        std::cout << "Read watchpoints:\n";
        for(uint32_t addr : rwatchpoints) {
          std::cout << "\t";
          output_mapped_addr(std::cout, addr);
          std::cout << "\n";
        }
      }
      else {
        for(const std::string& arg : args) {
          uint32_t watchpoint;
          if(eval(arg, std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
                  std::bind(&CPU_ScanlineDebug::read_address, this,
                            std::placeholders::_1), watchpoint)) {
            if(std::find(rwatchpoints.begin(), rwatchpoints.end(),
                         watchpoint) != rwatchpoints.end())
              std::cout << "Already breaking on reads from ";
            else {
              std::cout << "Will break on reads from ";
              rwatchpoints.push_back(watchpoint);
            }
            output_mapped_addr(std::cout, watchpoint);
            std::cout << "\n";
          }
        }
      }
    }
    void cmd_unrwatch(std::vector<std::string>& args) {
      if(args.size() == 0) {
        cmd_rwatch(args); // show watchpoints
      }
      else {
        for(const std::string& arg : args) {
          uint32_t watchpoint;
          if(eval(arg, std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
                  std::bind(&CPU_ScanlineDebug::read_address, this,
                            std::placeholders::_1), watchpoint)) {
            auto it = std::remove(rwatchpoints.begin(), rwatchpoints.end(),
                                  watchpoint);
            if(it == rwatchpoints.end())
              std::cout << "Not currently breaking on reads to ";
            else {
              rwatchpoints.erase(it);
              std::cout << "No longer breaking on reads to ";
            }
            output_mapped_addr(std::cout, watchpoint);
            std::cout << "\n";
          }
        }
      }
    }
    void cmd_eval(std::vector<std::string>& args) {
      for(size_t n = 0; n < args.size(); ++n) {
        std::cout << "Expression #"<<(n+1)<<":\n";
        uint32_t value;
        if(eval(args[n], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                   std::placeholders::_1,
                                   std::placeholders::_2),
                std::bind(&CPU_ScanlineDebug::read_address, this,
                                   std::placeholders::_1), value))
          std::cout << "\t$"<<std::hex<<value<<std::dec<<" ("<<value<<")\n";
      }
    }
    void cmd_poke_reg(std::vector<std::string>& args) {
      if(args.size() != 2) {
        std::cout << "Two arguments are required, see help\n";
        return;
      }
      uint32_t value;
      if(!eval(args[1], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
              std::bind(&CPU_ScanlineDebug::read_address, this,
                        std::placeholders::_1), value))
        return;
      if(args[0] == "a" || args[0] == "A") core.write_a(value);
      else if(args[0] == "x" || args[0] == "X") core.write_x(value);
      else if(args[0] == "y" || args[0] == "Y") core.write_y(value);
      else if(args[0] == "p" || args[0] == "P") core.write_p(value);
      else if(args[0] == "s" || args[0] == "S") core.write_s(value);
      else if(args[0] == "pc" || args[0] == "PC") core.write_pc(value);
      else std::cout << "Valid register names: A, X, Y, P, S, PC\n";
    }
    void cmd_poke8(std::vector<std::string>& args) {
      if(args.size() != 2) {
        std::cout << "Two arguments are required, see help\n";
        return;
      }
      uint32_t addr, value;
      if(!eval(args[0], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
              std::bind(&CPU_ScanlineDebug::read_address, this,
                        std::placeholders::_1), addr))
        return;
      if(!eval(args[1], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
              std::bind(&CPU_ScanlineDebug::read_address, this,
                        std::placeholders::_1), value))
        return;
      if(addr > 0x10000) {
        std::cout << "You cannot poke to an \"unmapped\" address. You can only poke into the CPU's\nmemory space directly.\n";
        return;
      }
      silently_write(addr, value);
    }
    void cmd_poke16(std::vector<std::string>& args) {
      if(args.size() != 2) {
        std::cout << "Two arguments are required, see help\n";
        return;
      }
      uint32_t addr, value;
      if(!eval(args[0], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
              std::bind(&CPU_ScanlineDebug::read_address, this,
                        std::placeholders::_1), addr))
        return;
      if(!eval(args[1], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
              std::bind(&CPU_ScanlineDebug::read_address, this,
                        std::placeholders::_1), value))
        return;
      if(addr > 0x10000) {
        std::cout << "You cannot poke to an \"unmapped\" address. You can only poke into the CPU's\nmemory space directly.\n";
        return;
      }
      silently_write(addr, value);
      silently_write(addr+1, value>>8);
    }
    void cmd_poke24(std::vector<std::string>& args) {
      if(args.size() != 2) {
        std::cout << "Two arguments are required, see help\n";
        return;
      }
      uint32_t addr, value;
      if(!eval(args[0], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
              std::bind(&CPU_ScanlineDebug::read_address, this,
                        std::placeholders::_1), addr))
        return;
      if(!eval(args[1], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
              std::bind(&CPU_ScanlineDebug::read_address, this,
                        std::placeholders::_1), value))
        return;
      if(addr > 0x10000) {
        std::cout << "You cannot poke to an \"unmapped\" address. You can only poke into the CPU's\nmemory space directly.\n";
        return;
      }
      silently_write(addr, value);
      silently_write(addr+1, value>>8);
      silently_write(addr+2, value>>16);
    }
    void cmd_poke32(std::vector<std::string>& args) {
      if(args.size() != 2) {
        std::cout << "Two arguments are required, see help\n";
        return;
      }
      uint32_t addr, value;
      if(!eval(args[0], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
              std::bind(&CPU_ScanlineDebug::read_address, this,
                        std::placeholders::_1), addr))
        return;
      if(!eval(args[1], std::bind(&CPU_ScanlineDebug::get_symbol, this,
                                 std::placeholders::_1,
                                 std::placeholders::_2),
              std::bind(&CPU_ScanlineDebug::read_address, this,
                        std::placeholders::_1), value))
        return;
      if(addr > 0x10000) {
        std::cout << "You cannot poke to an \"unmapped\" address. You can only poke into the CPU's\nmemory space directly.\n";
        return;
      }
      silently_write(addr, value);
      silently_write(addr+1, value>>8);
      silently_write(addr+2, value>>16);
      silently_write(addr+3, value>>24);
    }
    void cmd_continue(std::vector<std::string>&) {
      stopped = false;
    }
    void cmd_here(std::vector<std::string>&) {
      uint16_t pc = core.read_pc();
      output_addr(std::cout, pc, false, false, true);
      std::cout << ":\n\t";
      disassembler.disassemble(pc, std::cout);
      std::cout << "\n";
    }
    void cmd_step(std::vector<std::string>&) {
      repeat_command = "step";
      debug_steps = 1; // TODO: step count
      stopped = false;
    }
    void cmd_reset(std::vector<std::string>&) {
      ARS::triggerReset();
    }
    void cmd_quit(std::vector<std::string>&) {
      ARS::triggerQuit();
    }
  };
  const CPU_ScanlineDebug::command CPU_ScanlineDebug::commands[] = {
    {"help",
     "Displays this help message.",
     &CPU_ScanlineDebug::cmd_help},
    {"load-symbols",
     "Load one or more symbol files.",
     &CPU_ScanlineDebug::cmd_load_symbols},
    {"dump-sprite-memory",
     "Dump the current state of sprite memory.",
     &CPU_ScanlineDebug::cmd_dump_sprite_memory},
    {"trace-pins",
     "Whether to trace the IRQB#, NMIB#, SOB#, and RESB# pins.",
     &CPU_ScanlineDebug::cmd_trace_pins},
    {"trace-reads",
     "Whether to trace memory reads.",
     &CPU_ScanlineDebug::cmd_trace_reads},
    {"trace-writes",
     "Whether to trace memory writes.",
     &CPU_ScanlineDebug::cmd_trace_writes},
    {"trace-execs",
     "Whether to trace instruction execution.",
     &CPU_ScanlineDebug::cmd_trace_execs},
    {"trace-accesses",
     "Whether to trace the stranger bus cycles not covered by the above.",
     &CPU_ScanlineDebug::cmd_trace_other_accesses},
    {"eval",
     "Evaluates expressions. Symbol names and registers (_A, _X, _Y, ...) may\n"
     "participate in expressions. Many of the normal C integer operators are\n"
     "available. The 8@, 16@, and 24@ operators read the indicated number of bits.\n"
     "Example: eval 8@fruitcake+4 (retrieves a byte at the `fruitcake` pointer, and\n"
     "adds 4 to that byte)",
     &CPU_ScanlineDebug::cmd_eval},
    {"poke-reg",
     "Must have exactly two arguments. The first is a register name (WITHOUT any\n"
     "leading _). The second is an expression giving the value to write.",
     &CPU_ScanlineDebug::cmd_poke_reg},
    {"poke",
     "Must have exactly two arguments. The first is an expression giving the address\n"
     "to write. The second is an expression giving the value to write. The high 24\n"
     "bits of this value are discarded.",
     &CPU_ScanlineDebug::cmd_poke8},
    {"poke-word",
     "Must have exactly two arguments. The first is an expression giving the address\n"
     "to write. The second is an expression giving the value to write. The high 16\n"
     "bits of this value are discarded.",
     &CPU_ScanlineDebug::cmd_poke16},
    {"poke-24",
     "Must have exactly two arguments. The first is an expression giving the address\n"
     "to write. The second is an expression giving the value to write. The high 8\n"
     "bits of this value are discarded.",
     &CPU_ScanlineDebug::cmd_poke24},
    {"poke-word",
     "Must have exactly two arguments. The first is an expression giving the address\n"
     "to write. The second is an expression giving the value to write. All 32 bits\n"
     "of this value are written.",
     &CPU_ScanlineDebug::cmd_poke32},
    {"break",
     "Add execution breakpoints for a given set of mapped addresses. If no addresses\n"
     "are given, shows all current breakpoints. Addresses may be specified as\n"
     "expressions; see \"eval\".\n"
     "Special parameters:\n"
     "brk (break when BRK instructions is about to be executed; on by default)\n"
     "loops (break when a one-instruction infinite loop is detected; on by default)\n"
     "wai (break when a WAI instruction is about to be executed)",
     &CPU_ScanlineDebug::cmd_break},
    {"unbreak",
     "Remove execution breakpoints for a given set of mapped addresses.",
     &CPU_ScanlineDebug::cmd_unbreak},
    {"watch",
     "Add write watchpoints for a given set of mapped addresses. If no addresses are\n"
     "given, shows all current write watchpoints. Addresses may be specified as\n"
     "expressions; see \"eval\".",
     &CPU_ScanlineDebug::cmd_watch},
    {"unwatch",
     "Remove write watchpoints for a given set of mapped addresses.",
     &CPU_ScanlineDebug::cmd_unwatch},
    {"rwatch",
     "Add read watchpoints for a given set of mapped addresses. If no addresses are\n"
     "given, shows all current read watchpoints. Addresses may be specified as\n"
     "expressions; see \"eval\".",
     &CPU_ScanlineDebug::cmd_rwatch},
    {"unrwatch",
     "Remove read watchpoints for a given set of mapped addresses.",
     &CPU_ScanlineDebug::cmd_unrwatch},
    {"continue",
     "Proceed with execution.",
     &CPU_ScanlineDebug::cmd_continue},
    {"step",
     "Perform a single step.",
     &CPU_ScanlineDebug::cmd_step},
    {"here",
     "Describe the next instruction that will execute.",
     &CPU_ScanlineDebug::cmd_here},
    {"reset",
     "Reset the system.",
     &CPU_ScanlineDebug::cmd_reset},
    {"quit",
     "Quit the emulator.",
     &CPU_ScanlineDebug::cmd_quit},
  };
}

std::unique_ptr<ARS::CPU>
ARS::makeScanlineDebugCPU(const std::string& rom_path) {
  auto ret = std::make_unique<CPU_ScanlineDebug>();
  auto it = std::find(rom_path.rbegin(), rom_path.rend(), '.');
  if(it != rom_path.rend()) {
    std::string sympath(rom_path.begin(),
                        rom_path.begin()+rom_path.length()
                        -(it-rom_path.rbegin())-1);
    sympath += ".sym";
    auto symfile = IO::OpenRawPathForRead(sympath);
    if(!symfile || !*symfile)
      std::cout << "Warning: Couldn't load symbol file. You must use the load-symbols command if\n"
                << "you want symbols to be available.\n";
    else
      ret->loadSymbols(*symfile);
  }
  std::cout << "Debugger started.\n";
  return ret;
}
#endif
