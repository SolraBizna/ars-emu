#include "ars-emu.hh"
#include "cpu.hh"
#include "ppu.hh"
#include "apu.hh"
#include "w65c02.hh"

#if INTPROF
#include <array>
#endif

namespace {
#if INTPROF
#define CPU_Scanline CPU_ScanlineIntProf
#define makeScanlineCPU makeScanlineIntProfCPU
#endif
  class CPU_Scanline : public ARS::CPU {
    int cycle_budget = 0;
    uint32_t audio_cycle_counter = 0;
    W65C02::Core<CPU_Scanline> core;
#if INTPROF
    enum {
      // Executing from the Reset entry point (includes the next two)
      NORMAL = 0,
      // Executing NORMAL AND interrupts are disabled
      CLI,
      // Executing NORMAL AND interrupts are disabled AND IRQB# is asserted
      CLI_BLOCK,
      // Fetching or executing the IRQBRK handler
      IRQBRK,
      // Fetching or executing the NMI handler
      NMI,
      // Cycles eaten by APU stall / DMA / OL fetch / etc.
      EAT,
      // Calmly waiting for interrupts
      WAI,
      // Number of possible states
      STATECOUNT
    } cur_state = NORMAL;
    uint32_t counted_cycles = 0;
    std::array<uint32_t, STATECOUNT> state_cycles;
    bool irq_is_asserted;
#endif
  public:
    CPU_Scanline() : core(*this) {
#if INTPROF
      state_cycles.fill(0);
      irq_is_asserted = false;
#endif
    }
    ~CPU_Scanline() {}
    void handleReset() override {
      cycle_budget = 0;
      audio_cycle_counter = 0;
#if INTPROF
      counted_cycles = 0;
      state_cycles.fill(0);
#endif
      core.reset();
    }
    void eatCycles(int count) override {
      cycle_budget -= count;
#if INTPROF
      state_cycles[EAT] += count;
      counted_cycles += count;
#endif
    }
    void runCycles(int count) override {
      cycle_budget += count;
      audio_cycle_counter += count;
      while(core.in_productive_state() && cycle_budget > 0) {
        core.step();
      }
      if(cycle_budget > 0) {
#if INTPROF
        state_cycles[WAI] += cycle_budget;
        counted_cycles += cycle_budget;
#endif
        cycle_budget = 0;
      }
      while(audio_cycle_counter >= 256) {
        audio_cycle_counter -= 256;
        ARS::output_apu_sample();
      }
    }
    void setIRQ(bool irq) override {
#if INTPROF
      irq_is_asserted = irq;
#endif
      core.set_irq(irq);
    }
    void setSO(bool so) override { core.set_so(so); }
    void setNMI(bool nmi) override { core.set_nmi(nmi); }
#if INTPROF
    void intprof_cycle() {
      ++counted_cycles;
      ++state_cycles[cur_state];
      if(cur_state == NORMAL) {
        if(core.read_p() & core.P_I) {
          ++state_cycles[CLI];
          if(irq_is_asserted) {
            ++state_cycles[CLI_BLOCK];
          }
        }
      }
    }
#endif
    uint8_t read_byte(uint16_t addr, W65C02::ReadType) {
#if INTPROF
      intprof_cycle();
#endif
      --cycle_budget;
      return ARS::read(addr);
    }
    uint8_t read_opcode(uint16_t addr, W65C02::ReadType) {
#if INTPROF
      intprof_cycle();
#endif
      --cycle_budget;
      auto ret = ARS::read(addr, false, false, true);
#if INTPROF
      // wrong if we return from RTI to an IRQ handler, but that should NOT
      // happen in a well-functioning game
      if(ret == 0x40) cur_state = NORMAL;
#endif
      return ret;
    }
    uint8_t fetch_vector_byte(uint16_t addr) {
#if INTPROF
      switch(addr) {
      case 0xfffe: cur_state = IRQBRK; break;
      case 0xfffc: cur_state = NORMAL; break;
      case 0xfffa: cur_state = NMI; break;
      }
      intprof_cycle();
#endif
      --cycle_budget;
      return ARS::read(addr, false, true);
    }
    void write_byte(uint16_t addr, uint8_t byte, W65C02::WriteType) {
#if INTPROF
      intprof_cycle();
#endif
      --cycle_budget;
      ARS::write(addr, byte);
    }
    bool isStopped() override {
      return core.is_stopped();
    }
#if INTPROF
    void frameBoundary() override {
      if(counted_cycles > 0 && counted_cycles != state_cycles[WAI]) {
        std::cout << counted_cycles;
        for(auto i : state_cycles) {
          std::cout << '\t' << i;
        }
        std::cout << '\n';
      }
      counted_cycles = 0;
      state_cycles.fill(0);
    }
#endif
  };
}

std::unique_ptr<ARS::CPU> ARS::makeScanlineCPU(const std::string&) {
  return std::make_unique<CPU_Scanline>();
}
