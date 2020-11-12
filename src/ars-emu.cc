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
#include "floppy.hh"

#include <iostream>
#include <iomanip>
#include <list>
#include <chrono>
#include <thread>
#include <algorithm>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif
#ifndef DISALLOW_SCREENSHOTS
#include <png.h>
#include "fxinternal.hh"
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
  unsigned int thread_count = 0;
  uint8_t bankMap[8];
  std::string rom_path;
  bool rom_path_specified = false;
  std::unique_ptr<CPU>(*makeCPU)(const std::string&) = makeScanlineCPU;
  bool experiencing_temporal_anomaly = true;
  Controller::Type port1type = Controller::Type::AUTO;
  Controller::Type port2type = Controller::Type::AUTO;
  int64_t logic_frame = 0;
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
      cpu->setNMI(false);
      cpu->setIRQ(false);
      cpu->handleReset();
      secure_config_port_checked = false;
      for(size_t n = 0; n < sizeof(bankMap) / sizeof(*bankMap); ++n)
        bankMap[n] = cartridge->getPowerOnBank();
      epoch = std::chrono::high_resolution_clock::now();
      logic_frame = -1;
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
#ifndef DISALLOW_FLOPPY
    sn.Out(std::cout, "FLOPPY_USAGE"_Key);
#endif
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
#ifndef DISALLOW_FLOPPY
          case 'f':
            if(n >= argc) {
              sn.Out(std::cout, "MISSING_COMMAND_LINE_ARGUMENT"_Key, {"-1"});
              valid = false;
            }
            else {
              std::string nextarg = argv[n++];
              if(!mount_floppy(nextarg)) valid = false;
            }
            break;
          case 'F':
            fast_floppy_mode = true;
            break;
#endif
          case '1':
            if(n >= argc) {
              sn.Out(std::cout, "MISSING_COMMAND_LINE_ARGUMENT"_Key, {"-1"});
              valid = false;
            }
            else {
              std::string nextarg = argv[n++];
              port1type = Controller::typeFromString(nextarg);
              if(port1type == Controller::Type::INVALID) {
                sn.Out(std::cout, "UNKNOWN_CONTROLLER_TYPE"_Key, {nextarg});
                valid = false;
              }
            }
            break;
          case '2':
            if(n >= argc) {
              sn.Out(std::cout, "MISSING_COMMAND_LINE_ARGUMENT"_Key, {"-2"});
              valid = false;
            }
            else {
              std::string nextarg = argv[n++];
              port2type = Controller::typeFromString(nextarg);
              if(port2type == Controller::Type::INVALID) {
                sn.Out(std::cout, "UNKNOWN_CONTROLLER_TYPE"_Key, {nextarg});
                valid = false;
              }
            }
            break;
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
          case 'j':
            if(n >= argc) {
              sn.Out(std::cout, "MISSING_COMMAND_LINE_ARGUMENT"_Key, {"-j"});
              valid = false;
            }
            else {
              std::string nextarg = argv[n++];
              unsigned long l = std::stoul(nextarg);
              if(l > 128) l = 128; // we can't make much use of more cores
              thread_count = l;
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
    try {
      std::unique_ptr<GameFolder> gamefolder;
      std::string true_path = rom_path;
      std::string::size_type found_dirsep;
      gamefolder = GameFolder::open(true_path);
      /*
        as long as:
         - We haven't succeeded in opening the game folder yet
         AND
         - There is a directory separator in the path
      */
      while(!gamefolder
            && (found_dirsep = true_path.rfind(DIR_SEP)) != std::string::npos){
        // Delete everything after the last separator, but leave it intact
        true_path.resize(found_dirsep + sizeof(DIR_SEP) - 1);
        // Try to open this new path as a game folder
        // (It contains a trailing directory separator, so it will only be
        // opened as a Game Folder)
        gamefolder = GameFolder::open(true_path);
        // If that open didn't succeed, delete the trailing directory separator
        // (so that it won't get in the way next time)
        if(!gamefolder)
          true_path.resize(found_dirsep);
      }
      if(!gamefolder)
        die("%s", sn.Get("CARTRIDGE_COULD_NOT_BE_LOADED"_Key).c_str());
      /*
        We don't want to try a synthesized path with no directory separator,
        because that would lead to some funky (though, usually harmless)
        behavior.
       */
      if(rom_path != true_path) {
        sn.Out(std::cerr, "GAME_FOLDER_WAS_PARENT_OF_REQUESTED_FILE"_Key,
               {rom_path, true_path});
        rom_path = true_path;
      }
      ARS::cartridge = ARS::Cartridge::load(*gamefolder, port1type, port2type);
    }
    catch(std::string& reason) {
      std::string death = sn.Get("CARTRIDGE_LOADING_FAIL"_Key, {reason});
      die("%s", death.c_str());
    }
    if(allow_debug_port && !mapped_debug_port)
      ui << sn.Get("UNUSED_DEBUG"_Key) << ui;
    return true;
  }
#ifndef DISALLOW_SCREENSHOTS
  void write_to_ostream(png_structp libpng, png_bytep data, png_size_t len) {
    std::ostream* out= reinterpret_cast<std::ostream*>(png_get_io_ptr(libpng));
    out->write(reinterpret_cast<char*>(data), len);
    if(!out) {
      png_longjmp(libpng, 1);
    }
  }
  void flush_ostream(png_structp libpng) {
    std::ostream* out= reinterpret_cast<std::ostream*>(png_get_io_ptr(libpng));
    out->flush();
  }
#endif
  void takeScreenshot() {
#ifndef DISALLOW_SCREENSHOTS
    // TODO: make these work in Emscripten
    png_structp libpng = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                 nullptr, nullptr, nullptr);
    if(libpng == nullptr) return;
    png_infop info = png_create_info_struct(libpng);
    if(info == nullptr) {
      png_destroy_write_struct(&libpng, nullptr);
      return;
    }
    std::string filename;
    std::unique_ptr<std::ostream> out;
    for(int n = 1; !out && n < 10000; ++n) {
      std::ostringstream str;
      str << std::setw(4) << std::setfill('0') << n;
      filename = sn.Get("SCREENSHOT_FILENAME"_Key, {str.str()});
      out = IO::OpenDesktopFileForWrite(filename);
    }
    if(!out) {
      png_destroy_write_struct(&libpng, &info);
      return;
    }
    const int MAX_COLORS_ON_SCREEN = 85;
    uint8_t colormap[0x60];
    memset(colormap, 0xFF, sizeof(colormap));
    png_color colors[MAX_COLORS_ON_SCREEN] = {};
    int num_colors = 0;
    PPU::raw_screen remapped;
    const int width = 256;
    const int height = 224;
    const int left = (ARS::PPU::TOTAL_SCREEN_WIDTH-width)/2;
    const int right = left + width;
    const int top = (ARS::PPU::TOTAL_SCREEN_HEIGHT-height)/2;
    const int bottom = top + height;
    for(int y = top; y < bottom; ++y) {
      auto& inrow = screenbuf[y];
      auto& outrow = remapped[y];
      for(int x = left; x < right; ++x) {
        auto pixel = inrow[x];
        if(pixel >= 0x60 || (pixel & 0xF) >= 0xE) pixel = 0xE;
        if(colormap[pixel] == 0xFF) {
          colormap[pixel] = num_colors;
          colors[num_colors].red = (FX::hardwarePalette[pixel] >> 16) & 255;
          colors[num_colors].green = (FX::hardwarePalette[pixel] >> 8) & 255;
          colors[num_colors].blue = FX::hardwarePalette[pixel] & 255;
          ++num_colors;
        }
        outrow[x] = colormap[pixel];
      }
    }
    if(setjmp(png_jmpbuf(libpng))) {
      ui << sn.Get("SCREENSHOT_SAVE_ERROR"_Key) << ui;
      png_destroy_write_struct(&libpng, &info);
      return;
    }
    png_set_write_fn(libpng, out.get(), write_to_ostream, flush_ostream);
    png_set_IHDR(libpng, info, width, height, 8, PNG_COLOR_TYPE_PALETTE,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_set_filter(libpng, 0, PNG_FILTER_NONE);
    png_set_PLTE(libpng, info, colors, num_colors);
    png_bytep rows[height];
    for(int y = 0; y < height; ++y) {
      rows[y] = remapped[y+top].data() + left;
    }
    png_set_rows(libpng, info, rows);
    png_write_png(libpng, info, 0, nullptr);
    png_destroy_write_struct(&libpng, &info);
    ui << sn.Get("SCREENSHOT_SAVE_SUCCESS"_Key, {filename}) << ui;
#endif
  }
}

void ARS::handleEmulatorButtonPress(EmulatorButton button) {
  switch(button) {
  case EMUBUTTON_RESET:
    triggerReset();
    break;
  case EMUBUTTON_TOGGLE_BG:
    PPU::show_background = !PPU::show_background;
    ui << sn.Get(PPU::show_background?"BACKGROUND_SHOWN"_Key
                 :"BACKGROUND_HIDDEN"_Key) << ui;
    break;
  case EMUBUTTON_TOGGLE_SP:
    PPU::show_sprites = !PPU::show_sprites;
    ui << sn.Get(PPU::show_sprites?"SPRITES_SHOWN"_Key
                 :"SPRITES_HIDDEN"_Key) << ui;
    break;
  case EMUBUTTON_TOGGLE_OL:
    PPU::show_overlay = !PPU::show_overlay;
    ui << sn.Get(PPU::show_overlay?"OVERLAY_SHOWN"_Key
                 :"OVERLAY_HIDDEN"_Key) << ui;
    break;
  case EMUBUTTON_SCREENSHOT:
    takeScreenshot();
    break;
  default:
    break;
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
        // let spurious debug port reads/writes slide
        if(addr != 0x247) badread(addr);
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
          // let spurious debug port reads/writes slide
          else if(addr != 0x247) badwrite(addr);
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
#if __WIN32__
  IO::DoRedirectOutput();
#endif
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
    FX::init(thread_count);
    window_title = sn.Get("WINDOW_TITLE"_Key, {rom_path});
    display = safe_mode ? Display::makeSafeModeDisplay()
      : Display::makeConfiguredDisplay();
    Controller::initControllers(port1type, port2type);
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
