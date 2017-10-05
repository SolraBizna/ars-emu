#include "ars-emu.hh"
#include "et209.hh"
#include "config.hh"
#include "io.hh"
#include "font.hh"
#include "utfit.hh"
#include "prefs.hh"
#include "windower.hh"
#include "cpu.hh"
#include "cartridge.hh"
#include "controller.hh"
#include "configurator.hh"
#include "apu.hh"
#include "ppu.hh"
#include "fx.hh"
#include "display.hh"
#include "expansions.hh"

#include <iostream>
#include <iomanip>
#include <list>
#include <chrono>
#include <thread>
#include <algorithm>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

using namespace ARS;

std::unique_ptr<ARS::CPU> ARS::cpu;
typedef std::chrono::duration<int64_t, std::ratio<1,60> > frame_duration;
bool ARS::safe_mode = false, ARS::debugging_audio = false,
  ARS::debugging_video = false;
std::string ARS::window_title;
uint8_t ARS::dram[0x8000];
SN::Context sn;
std::unique_ptr<Display> ARS::display;
uint16_t ARS::last_known_pc;

namespace {
  uint8_t bankMap[8];
  std::string rom_path;
  bool rom_path_specified = false;
  std::unique_ptr<CPU>(*makeCPU)(const std::string&) = makeScanlineCPU;
  bool experiencing_temporal_anomaly = true;
  int64_t logic_frame;
  decltype(std::chrono::high_resolution_clock::now()) epoch;
  bool window_visible = true, window_minimized = false, quit = false,
    quit_on_stop = false, stop_has_been_detected = false, need_reset = true;
  PPU::raw_screen screenbuf;
  void cleanup() {
    cartridge.reset();
    display.reset();
    SDL_Quit();
  }
  struct EscapeException {};
  void badread(uint16_t addr) {
    ui << sn.Get("BAD_READ"_Key, {TEG::format("%04X",addr)}) << ui;
  }
  void badwrite(uint16_t addr) {
    ui << sn.Get("BAD_WRITE"_Key, {TEG::format("%04X",addr)}) << ui;
  }
  void busconflict(uint16_t addr, std::string bus) {
    ui << sn.Get("BUS_CONFLICT"_Key, {TEG::format("%04X",addr), std::move(bus)}) << ui;
  }
  void mainLoop() {
    if(need_reset) {
      need_reset = false;
      cartridge->handleReset();
      PPU::handleReset();
      cpu->handleReset();
      secure_config_port_checked = false;
      for(size_t n = 0; n < sizeof(bankMap); ++n)
        bankMap[n] = cartridge->getPowerOnBank();
      epoch = std::chrono::high_resolution_clock::now();
    }
    ++logic_frame;
    auto now = std::chrono::high_resolution_clock::now();
    auto target_frame = std::chrono::duration_cast<frame_duration>(now - epoch)
      .count();
    if(experiencing_temporal_anomaly) {
      logic_frame = target_frame;
      experiencing_temporal_anomaly = false;
    }
    auto framediff = target_frame - logic_frame;
    if(framediff > 10) {
      // computer is VERY slow
      logic_frame = target_frame - 10;
    }
    else if(framediff < -30) {
      ui << sn.Get("TEMPORAL_ANOMALY"_Key) << ui;
      logic_frame = target_frame;
    }
#ifdef EMSCRIPTEN
    while(logic_frame < target_frame) {
      ++logic_frame;
      cartridge->oncePerFrame();
      PPU::renderInvisible();
    }
#endif
    cartridge->oncePerFrame();
    if(logic_frame >= target_frame && window_visible && !window_minimized) {
      PPU::renderFrame(screenbuf);
      display->update(screenbuf);
    }
    else {
      PPU::renderInvisible();
    }
    Windower::Update();
    if(target_frame < logic_frame) {
#ifdef __WIN32__
      SDL_Delay(std::chrono::duration_cast<std::chrono::milliseconds>((epoch + std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(frame_duration(logic_frame))) - now).count());
#else
      std::this_thread::sleep_for((epoch + std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(frame_duration(logic_frame))) - now);
#endif
    }
    SDL_Event evt;
    while(SDL_PollEvent(&evt)) {
      if(Windower::HandleEvent(evt)) continue;
      if(Controller::filterEvent(evt)) continue;
      if(display->filterEvent(evt)) continue;
      switch(evt.type) {
      case SDL_DROPFILE: SDL_free(evt.drop.file); break;
      case SDL_QUIT: quit = true; break;
      case SDL_KEYDOWN:
        switch(evt.key.keysym.sym) {
        case SDLK_F3:
          PPU::show_overlay = !PPU::show_overlay;
          ui << sn.Get(PPU::show_overlay?"OVERLAY_SHOWN"_Key
                       :"OVERLAY_HIDDEN"_Key) << ui;
          break;
        case SDLK_F2:
          PPU::show_sprites = !PPU::show_sprites;
          ui << sn.Get(PPU::show_sprites?"SPRITES_SHOWN"_Key
                       :"SPRITES_HIDDEN"_Key) << ui;
          break;
        case SDLK_F1:
          PPU::show_background = !PPU::show_background;
          ui << sn.Get(PPU::show_background?"BACKGROUND_SHOWN"_Key
                       :"BACKGROUND_HIDDEN"_Key) << ui;
          break;
        }
        break;
      case SDL_WINDOWEVENT:
        switch(evt.window.event) {
        case SDL_WINDOWEVENT_SHOWN: window_visible = true; break;
        case SDL_WINDOWEVENT_HIDDEN: window_visible = false; break;
        case SDL_WINDOWEVENT_MINIMIZED: window_minimized = true; break;
        case SDL_WINDOWEVENT_RESTORED: window_minimized = false; break;
        }
        break;
      }
    }
    if(!stop_has_been_detected && cpu->isStopped()) {
      ui << sn.Get("CPU_STOPPED"_Key) << ui;
      stop_has_been_detected = true;
      if(quit_on_stop)
        quit = true;
    }
  }
  void printUsage() {
    sn.Out(std::cout, "USAGE"_Key);
  }
  bool parseCommandLine(int argc, const char** argv) {
    int n = 1;
    bool noMoreOptions = false;
    bool valid = true;
    while(n < argc) {
      const char* arg = argv[n++];
      if(!strcmp(arg, "--")) noMoreOptions = true;
      else if(!noMoreOptions && arg[0] == '-') {
        ++arg;
        while(*arg) {
          switch(*arg++) {
          case '?': printUsage(); return false;
          case 't':
            if(n >= argc) {
              sn.Out(std::cout, "MISSING_COMMAND_LINE_ARGUMENT"_Key, {"-t"});
              valid = false;
            }
            else {
              std::string nextarg = argv[n++];
              if(nextarg == "fast") makeCPU = makeScanlineCPU;
#ifndef NO_DEBUG_CORES
              else if(nextarg == "fast_intprof") makeCPU = makeScanlineIntProfCPU;
              else if(nextarg == "fast_debug") makeCPU = makeScanlineDebugCPU;
#endif
              else {
                sn.Out(std::cout, "UNKNOWN_CORE"_Key, {nextarg});
                valid = false;
              }
            }
            break;
          case 'd':
            allow_debug_port = true;
            break;
          case 'A':
            debugging_audio = true;
            break;
          case 'V':
            debugging_video = true;
            break;
          case 'z':
            always_allow_config_port = false;
            allow_secure_config_port = false;
            break;
          case 'C':
            always_allow_config_port = true;
            allow_secure_config_port = false;
            /* fall through */
          case 'q':
            quit_on_stop = true;
            break;
          case 'S':
            safe_mode = true;
            break;
          default:
            sn.Out(std::cout, "UNKNOWN_OPTION"_Key, {std::string(arg-1,1)});
            valid = false;
          }
        }
      }
      else if(rom_path_specified) {
        sn.Out(std::cout, "MULTIPLE_ROM_PATHS"_Key);
        printUsage();
        return false;
      }
      else {
        rom_path = arg;
        rom_path_specified = true;
      }
    }
    if(!rom_path_specified && !SDL_Init(SDL_INIT_VIDEO)) {
      // Did we get a drag-and-drop?
      SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
      SDL_Event evt;
      while(SDL_PollEvent(&evt)) {
        switch(evt.type) {
        case SDL_DROPFILE:
          rom_path = evt.drop.file;
          SDL_free(evt.drop.file);
          rom_path_specified = true;
          break;
        }
      }
    }
    if(!rom_path_specified) {
#ifdef DROPPY_OS
      die("%s", sn.Get("DRAG_AND_DROP_A_ROM"_Key).c_str());
#else
      sn.Out(std::cout, "NO_ROM_PATHS"_Key);
      printUsage();
#endif
      return false;
    }
    if(!valid) {
      printUsage();
      return false;
    }
    bool mapped_debug_port = false;
    try {
      std::unique_ptr<GameFolder> gamefolder = GameFolder::open(rom_path);
      if(!gamefolder)
        die("%s", sn.Get("CARTRIDGE_COULD_NOT_BE_LOADED"_Key).c_str());
      ARS::cartridge = ARS::Cartridge::load(*gamefolder);
    }
    catch(std::string& reason) {
      std::string death = sn.Get("CARTRIDGE_LOADING_FAIL"_Key, {reason});
      die("%s", death.c_str());
    }
    if(allow_debug_port && !mapped_debug_port)
      ui << sn.Get("UNUSED_DEBUG"_Key) << ui;
    return true;
  }
}

void ARS::triggerReset() {
  need_reset = true;
  throw EscapeException();
}

void ARS::triggerQuit() {
  quit = true;
  throw EscapeException();
}

void ARS::temporalAnomaly() {
  experiencing_temporal_anomaly = true;
}

uint8_t ARS::read(uint16_t addr, bool OL, bool VPB, bool SYNC) {
  // TODO: detect bus conflicts
  if(SYNC) last_known_pc = addr;
  (void)busconflict;
  if(addr < 0x8000) {
    if((addr & 0xFFF9) == 0x0211)
      return PPU::complexRead(addr);
    else if((addr & 0xFFF8) == 0x0240) {
      if(expansions[addr&7]) return expansions[addr&7]->input();
      else {
        badread(addr);
        return 0xBB;
      }
    }
    else return dram[addr];
  }
  else return cartridge->read(bankMap[(addr>>12)-8], addr, OL, VPB, SYNC);
  badread(addr);
  return 0xBB;
}

void ARS::write(uint16_t addr, uint8_t value) {
  // TODO: detect bus conflicts
  (void)busconflict;
  if(addr < 0x8000) {
    // prevent writes to SimpleConfig's working or display memory as long as
    // the configurator is in use
    if(allow_secure_config_port
       && Configurator::is_protected_memory_address(addr)
       && !Configurator::is_secure_configuration_address(last_known_pc)
       && Configurator::is_active())
      return;
    dram[addr] = value;
    if(addr >= 0x0200 && addr < 0x0250) {
      if((addr ^ 0x0210) < 16) PPU::complexWrite(addr, value);
      else if((addr & 0xFFE0) == 0x0220) apu.write(addr&0x1F, value);
      else if((addr & 0xFFF0) == 0x0240) {
        if(addr >= 0x0248) {
          auto bs = cartridge->getBS();
          auto startBank = (addr&7)&~(7>>bs);
          auto stopBank = ((addr&7)|(7>>bs))+1;
          secure_config_port_checked = false;
          for(auto n = startBank; n < stopBank; ++n) {
            bankMap[n] = value;
          }
        }
        else {
          if(expansions[addr&7]) expansions[addr&7]->output(value);
          else badwrite(addr);
        }
      }
    }
    return;
  }
  else {
    cartridge->write(bankMap[(addr>>12)-8], addr, value);
    return;
  }
  badwrite(addr);
}

uint8_t ARS::getBankForAddr(uint16_t addr) {
  if(addr < 0x8000) return 0; // no bank
  else return bankMap[(addr>>12)-8];
}

extern "C" int teg_main(int argc, char** argv) {
  try {
    sn.AddCatSource(IO::GetSNCatSource());
    if(!sn.SetLanguage(sn.GetSystemLanguage()))
      die("Unable to load language files. Please ensure a Lang directory exists in the Data directory next to the emulator.");
    if(!parseCommandLine(argc, const_cast<const char**>(argv))) return 1;
    Font::Load();
    srand(time(NULL)); // rand() is only used for trashing memory on reset
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER))
      die("%s", sn.Get("SDL_FAIL"_Key).c_str());
    SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
    atexit(cleanup);
    PrefsLogic::DefaultsAll();
    PrefsLogic::LoadAll();
    ARS::init_apu();
    FX::init();
    window_title = sn.Get("WINDOW_TITLE"_Key, {rom_path});
    display = safe_mode ? Display::makeSafeModeDisplay()
      : Display::makeConfiguredDisplay();
    Controller::initControllers();
    quit = false;
    cpu = makeCPU(rom_path);
    fillDramWithGarbage(dram, sizeof(dram));
    PPU::fillWithGarbage();
#ifdef EMSCRIPTEN
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while(!quit) {
      try {
        while(!quit) mainLoop();
      }
      catch(const EscapeException& e) {
      }
      if(need_reset) {
        ui << sn.Get("RESET"_Key) << ui;
      }
    }
#endif
  }
  catch(std::string str) {
    std::string why = sn.Get("UNCAUGHT_FATAL_ERROR"_Key, {str});
    die("%s", why.c_str());
  }
  catch(const char* str) {
    std::string why = sn.Get("UNCAUGHT_FATAL_ERROR"_Key, {str});
    die("%s", why.c_str());
  }
  return 0;
}
