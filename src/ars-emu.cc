#include "ars-emu.hh"
#include "et209.hh"
#include "config.hh"
#include "io.hh"
#include "font.hh"
#include "utfit.hh"
#include "prefs.hh"
#include "windower.hh"

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

ARS::MessageImp ARS::ui;
std::unique_ptr<ARS::CPU> ARS::cpu;
typedef std::chrono::duration<int64_t, std::ratio<1,60> > frame_duration;
bool ARS::safe_mode = false, ARS::debugging_audio = false,
  ARS::debugging_video = false;
uint8_t ARS::dram[0x8000];
SN::Context sn;

namespace {
  uint8_t bankMap[8];
  uint16_t last_known_pc;
  std::string rom_path;
  bool rom_path_specified = false;
  std::unique_ptr<CPU>(*makeCPU)(const std::string&) = makeScanlineCPU;
  bool experiencing_temporal_anomaly = true;
  int64_t logic_frame;
  decltype(std::chrono::high_resolution_clock::now()) epoch;
  int window_width = PPU::VISIBLE_SCREEN_WIDTH*3,
    window_height = PPU::VISIBLE_SCREEN_HEIGHT*3;
  constexpr int MESSAGES_MARGIN_X = 12;
  constexpr int MESSAGES_MARGIN_Y = 8;
  constexpr int MESSAGE_LIFESPAN = 300;
  bool window_visible = true, window_minimized = false, quit = false,
    allow_debug_port = false, always_allow_config_port = false,
    allow_secure_config_port = true, quit_on_stop = false,
    stop_has_been_detected = false, need_reset = true,
    secure_config_port_checked = false, secure_config_port_available;
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* frametexture = nullptr, *messagetexture = nullptr;
  void cleanup() {
    if(cartridge != nullptr) cartridge->flushSRAM();
    if(messagetexture != nullptr) SDL_DestroyTexture(messagetexture);
    if(frametexture != nullptr) SDL_DestroyTexture(frametexture);
    if(renderer != nullptr) SDL_DestroyRenderer(renderer);
    if(window != nullptr) SDL_DestroyWindow(window);
    SDL_Quit();
  }
  struct EscapeException {};
  struct LoggedMessage {
    std::string string;
    int lifespan;
    LoggedMessage(std::string&& string, int lifespan)
      : string(string), lifespan(lifespan) {}
  };
  std::list<LoggedMessage> logged_messages;
  bool messages_dirty = false;
  SDL_Rect messages_local_rect = {0,0,0,0}, messages_global_rect = {0,0,0,0};
  void cycle_messages() {
    while(!logged_messages.empty() && logged_messages.begin()->lifespan-- <=0){
      logged_messages.pop_front();
      messages_dirty = true;
    }
  }
  void draw_glyph(uint8_t* _pixels, const Font::Glyph& glyph, int skip_rows,
                  int pitch) {
    for(int y = skip_rows; y < Font::HEIGHT + 2; ++y) {
      uint16_t* pixels = reinterpret_cast<uint16_t*>(_pixels);
      int w = glyph.wide ? 18 : 10;
      // uint32_t avoids negative bitshift
      uint32_t bitmap = y > 0 ? glyph.tiles[y-1]<<16 : 0,
        bitmap2 = y < Font::HEIGHT ? glyph.tiles[y]<<16 : 0,
        bitmap3 = y < Font::HEIGHT-1 ? glyph.tiles[y+1]<<16 : 0;
      if(glyph.wide) {
        bitmap |= y > 0 ? glyph.tiles[y+15]<<8 : 0;
        bitmap2 |= y < Font::HEIGHT ? glyph.tiles[y+16]<<8 : 0;
        bitmap3 |= y < Font::HEIGHT-1 ? glyph.tiles[y+17]<<8 : 0;
      }
      uint32_t shadowmap = bitmap|bitmap2|bitmap3;
      shadowmap |= shadowmap<<1; shadowmap |= shadowmap>>1;
      for(int x = 0; x < w; ++x) {
        if(bitmap2 & (0x1000000>>x))
          *pixels = 0xFFFF;
        else if(shadowmap & (0x1000000>>x))
          *pixels = 0xF000;
        ++pixels;
      }
      _pixels += pitch;
    }
  }
  void render_messages(SDL_Texture* texture) {
    messages_local_rect.x = 0;
    messages_local_rect.y = 0;
    messages_local_rect.w = 0;
    messages_local_rect.h = 2;
    for(LoggedMessage& msg : logged_messages) {
      messages_local_rect.h += Font::HEIGHT;
      int width = 2;
      auto cit = msg.string.cbegin();
      while(cit != msg.string.cend()) {
        uint32_t c = getNextCodePoint(cit, msg.string.cend());
        if(c == 0x20) width += 8;
        else if(c == 0x3000) width += 16;
        else {
          auto& glyph = Font::GetGlyph(c);
          width += glyph.wide ? 16 : 8;
        }
      }
      if(width > PPU::VISIBLE_SCREEN_WIDTH-MESSAGES_MARGIN_X*2)
        width = PPU::VISIBLE_SCREEN_WIDTH-MESSAGES_MARGIN_X*2;
      if(width > messages_local_rect.w)
        messages_local_rect.w = width;
    }
    if(messages_local_rect.w != 0) {
      if(messages_local_rect.h > PPU::VISIBLE_SCREEN_HEIGHT-MESSAGES_MARGIN_Y*2)
        messages_local_rect.h = PPU::VISIBLE_SCREEN_HEIGHT-MESSAGES_MARGIN_Y*2;
      messages_local_rect.y = PPU::VISIBLE_SCREEN_HEIGHT-MESSAGES_MARGIN_Y*2-messages_local_rect.h;
      uint8_t* pixels;
      int pitch;
      SDL_LockTexture(texture, &messages_local_rect,
                      reinterpret_cast<void**>(&pixels), &pitch);
      for(int y = 0; y < messages_local_rect.h; ++y) {
        memset(pixels, 0, messages_local_rect.w * 2);
        pixels += pitch;
      }
      pixels -= pitch;
      int y = messages_local_rect.h-1;
      auto it = logged_messages.crbegin();
      while(it != logged_messages.crend() && y > -Font::HEIGHT) {
        y -= Font::HEIGHT;
        pixels -= Font::HEIGHT * pitch;
        auto& msg = *it;
        int x = 0;
        auto cit = msg.string.cbegin();
        while(cit != msg.string.cend()) {
          uint32_t c = getNextCodePoint(cit, msg.string.cend());
          if(c == 0x20) x += 8;
          else if(c == 0x3000) x += 16;
          else {
            auto& glyph = Font::GetGlyph(c);
            if(x + (glyph.wide ? 16 : 8) > messages_local_rect.w) break;
            int skip_rows = y >= 0 ? 0 : -y;
            draw_glyph(pixels + skip_rows * pitch + x * 2,
                       glyph, skip_rows, pitch);
            x += glyph.wide ? 16 : 8;
          }
          if(x > messages_local_rect.w-2) break;
        }
        ++it;
      }
      SDL_UnlockTexture(texture);
    }
    messages_global_rect.x = messages_local_rect.x + MESSAGES_MARGIN_X;
    messages_global_rect.y = messages_local_rect.y + MESSAGES_MARGIN_Y;
    messages_global_rect.w = messages_local_rect.w;
    messages_global_rect.h = messages_local_rect.h;
  }
  void badread(uint16_t addr) {
    ui << sn.Get("BAD_READ"_Key, {TEG::format("%04X",addr)}) << ui;
  }
  void badwrite(uint16_t addr) {
    ui << sn.Get("BAD_WRITE"_Key, {TEG::format("%04X",addr)}) << ui;
  }
  void busconflict(uint16_t addr) {
    ui << sn.Get("BUS_CONFLICT"_Key, {TEG::format("%04X",addr)}) << ui;
  }
  bool config_port_access_is_secure(uint16_t pc) {
    return pc >= 0xF000 || pc <= 0xF7FF;
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
      cpu->runCycles(CYCLES_PER_VBLANK);
      PPU::dummyRender();
      cpu->frameBoundary();
      cycle_messages();
    }
#endif
    cartridge->oncePerFrame();
    cpu->runCycles(CYCLES_PER_VBLANK);
    if(logic_frame >= target_frame && window_visible && !window_minimized) {
      PPU::renderToTexture(frametexture);
      if(messages_dirty) {
        render_messages(messagetexture);
        messages_dirty = false;
      }
      SDL_RenderClear(renderer);
      SDL_Rect srcrect = {PPU::INVISIBLE_SCANLINE_COUNT,
                          PPU::INVISIBLE_COLUMN_COUNT,
                          PPU::VISIBLE_SCREEN_WIDTH,
                          PPU::VISIBLE_SCREEN_HEIGHT};
      SDL_RenderCopy(renderer, frametexture, &srcrect, NULL);
      if(messages_local_rect.w != 0)
        SDL_RenderCopy(renderer, messagetexture,
                       &messages_local_rect, &messages_global_rect);
      SDL_RenderPresent(renderer);
    }
    else {
      PPU::dummyRender();
    }
    Windower::Update();
    cpu->frameBoundary();
    cycle_messages();
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
    auto f = IO::OpenRawPathForRead(rom_path);
    if(!f || !*f) return false;
    try {
      Cartridge::loadRom(rom_path, *f);
    }
    catch(std::string& reason) {
      std::string death = sn.Get("CARTRIDGE_LOADING_FAIL"_Key, {reason});
      die("%s", death.c_str());
    }
    if(allow_debug_port
       && !cartridge->hasHardware(ARS::Cartridge::EXPANSION_DEBUG_PORT))
      ui << sn.Get("UNUSED_DEBUG"_Key) << ui;
    return true;
  }
}

void MessageImp::outputLine(std::string line, int lifespan_value) {
  std::cerr << sn.Get("UI_MESSAGE_STDERR"_Key, {line}) << std::endl;
  logged_messages.emplace_back(std::move(line), lifespan_value);
}

void MessageImp::outputBuffer() {
  std::string msg = stream.str();
  int lifespan = MESSAGE_LIFESPAN;
  for(auto& lmsg : logged_messages)
    lifespan -= lmsg.lifespan;
  auto begin = msg.begin();
  do {
    auto it = std::find(begin, msg.end(), '\n');
    outputLine(std::string(begin, it), lifespan);
    if(it != msg.end()) ++it;
    begin = it;
    lifespan = 0;
  } while(begin != msg.end());
  stream.clear();
  stream.str("");
  messages_dirty = true;
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
      switch(addr&7) {
      case 0: return controller1->input();
      case 1: return controller2->input();
      case 5: return 0; break; // TODO: HAM
      case 6:
        if(cartridge->hasHardware(ARS::Cartridge::EXPANSION_CONFIG)) {
          if(always_allow_config_port)
            return Configurator::read();
          else if(allow_secure_config_port) {
            if(!secure_config_port_checked) {
              secure_config_port_checked = true;
              secure_config_port_available
                = Configurator::is_secure_configurator_present();
            }
            if(secure_config_port_available) {
              if(config_port_access_is_secure(last_known_pc))
                return Configurator::read();
              else
                return 0xEC;
            }
          }
        }
        return 0xBB;
      case 7:
        if(allow_debug_port
           && cartridge->hasHardware(ARS::Cartridge::EXPANSION_DEBUG_PORT)) {
          cpu->setSO(true);
          cpu->setSO(false);
          return 0xFF;
        }
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
          switch(addr&7) {
          case 0:
            controller1->output(value);
            break;
          case 1:
            controller2->output(value);
            break;
          case 5:
            if(cartridge->hasHardware(ARS::Cartridge::EXPANSION_HAM)) {
              // TODO: HAM
            }
            break;
          case 6:
            if(cartridge->hasHardware(ARS::Cartridge::EXPANSION_CONFIG)) {
              if(always_allow_config_port)
                return Configurator::write(value);
              else if(allow_secure_config_port) {
                if(!secure_config_port_checked) {
                  secure_config_port_checked = true;
                  secure_config_port_available
                    = Configurator::is_secure_configurator_present();
                }
                if(secure_config_port_available) {
                  if(config_port_access_is_secure(last_known_pc))
                    return Configurator::write(value);
                }
              }
            }
            return;
          case 7:
            if(allow_debug_port
               &&cartridge->hasHardware(ARS::Cartridge::EXPANSION_DEBUG_PORT)){
              std::cerr << value;
            }
            return;
          }
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
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  std::string windowtitle = sn.Get("WINDOW_TITLE"_Key, {rom_path});
  window = SDL_CreateWindow(windowtitle.c_str(),
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            window_width, window_height,
                            SDL_WINDOW_RESIZABLE);
  if(window == NULL) die("%s",sn.Get("WINDOW_FAIL"_Key,
                                     {SDL_GetError()}).c_str());
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
  if(renderer == NULL) die("%s",sn.Get("RENDERER_FAIL"_Key,
                                       {SDL_GetError()}).c_str());
  SDL_RenderSetLogicalSize(renderer, PPU::VISIBLE_SCREEN_WIDTH,
                           PPU::VISIBLE_SCREEN_HEIGHT);
  frametexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   PPU::INTERNAL_SCREEN_WIDTH,
                                   PPU::INTERNAL_SCREEN_HEIGHT);
  if(frametexture == NULL) die("%s",sn.Get("FRAMETEXTURE_FAIL"_Key,
                                           {SDL_GetError()}).c_str());
  messagetexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB4444,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     PPU::VISIBLE_SCREEN_WIDTH
                                     -MESSAGES_MARGIN_X*2,
                                     PPU::VISIBLE_SCREEN_HEIGHT
                                     -MESSAGES_MARGIN_Y*2);
  if(messagetexture == NULL) die("%s",
                                 sn.Get("MESSAGETEXTURE_FAIL"_Key,
                                        {SDL_GetError()}).c_str());
  SDL_SetTextureBlendMode(messagetexture, SDL_BLENDMODE_BLEND);
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
  return 0;
}
