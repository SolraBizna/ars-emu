#include "ars-emu.hh"
#include "et209.hh"
#include "config.hh"
#include "io.hh"

#include <iostream>
#include <iomanip>
#include <list>
#include <chrono>
#include <thread>

using namespace ARS;

ARS::MessageImp ARS::ui;
std::unique_ptr<ARS::CPU> ARS::cpu;
typedef std::chrono::duration<int64_t, std::ratio<1,60> > frame_duration;
uint8_t ARS::dram[0x8000];

namespace {
#include "gen/messagefont.hh"
  uint8_t bankMap[8];
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
  const char* EMULATOR_CONFIG_FILE = "ARS-emu.conf";
  const Config::Element EMULATOR_CONFIG_ELEMENTS[] = {
    Config::Element("window_width", window_width),
    Config::Element("window_height", window_height),
  };
  bool window_visible = true, window_minimized = false, quit = false,
    allow_debug_port = false;
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
  struct ResetException {};
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
  void draw_glyph(uint8_t* _pixels, const MessageGlyph& glyph, int skip_rows,
                  int pitch) {
    const uint16_t* glyphp = glyph.data + skip_rows * (glyph.w + 2);
    for(int y = skip_rows; y < message_font_height + 2; ++y) {
      uint16_t* pixels = reinterpret_cast<uint16_t*>(_pixels);
      for(int w = 0; w < glyph.w + 2; ++w) {
        if(*glyphp)
          *pixels = *glyphp;
        ++glyphp;
        ++pixels;
      }
      _pixels += pitch;
    }
  }
  void render_messages(SDL_Texture* texture) {
    messages_local_rect.x = 0;
    messages_local_rect.y = 0;
    messages_local_rect.w = 0;
    messages_local_rect.h = 1;
    for(LoggedMessage& msg : logged_messages) {
      messages_local_rect.h += message_font_height + 1;
      int width = 1;
      for(char c : msg.string) {
        if(c == 32) width += 2;
        else {
          if(c < 32 || c > 126) c = '?';
          else if(c >= 'a' && c <= 'z') c -= 32;
          c -= 32;
          auto&glyph = message_font_glyphs[message_font_glyph_indices[(int)c]];
          width += glyph.w + 1;
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
      while(it != logged_messages.crend() && y > -message_font_height) {
        y -= message_font_height + 1;
        pixels -= (message_font_height + 1) * pitch;
        auto& msg = *it;
        int x = 0;
        for(char c : msg.string) {
          if(c == 32) x += 2;
          else {
            if(c < 32 || c > 126) c = '?';
            else if(c >= 'a' && c <= 'z') c -= 32;
            c -= 32;
            auto&glyph=message_font_glyphs[message_font_glyph_indices[(int)c]];
            if(x + glyph.w + 2 > messages_local_rect.w) break;
            int skip_rows = y >= 0 ? 0 : -y;
            draw_glyph(pixels + skip_rows * pitch + x * 2,
                       glyph, skip_rows, pitch);
            x += glyph.w + 1;
          }
          if(x >= messages_local_rect.w) break;
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
    ui << "Incorrectly emulating bad memory read: $"
       << std::hex << std::setw(4) << std::setfill('0') << addr
       << std::dec << ui;
  }
  void badwrite(uint16_t addr) {
    ui << "Incorrectly emulating bad memory write: $"
       << std::hex << std::setw(4) << std::setfill('0') << addr
       << std::dec << ui;
  }
  void busconflict(uint16_t addr) {
    ui << "Incorrectly emulating bus conflict: $"
       << std::hex << std::setw(4) << std::setfill('0') << addr
       << std::dec << ui;
  }
  void mainLoop() {
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
      ui << "Unexpected temporal anomaly. Hic!" << ui;
      logic_frame = target_frame;
    }
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
    cycle_messages();
    if(target_frame < logic_frame)
      std::this_thread::sleep_for((epoch + std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(frame_duration(logic_frame))) - now);
    SDL_Event evt;
    while(SDL_PollEvent(&evt)) {
      if(Controller::filterEvent(evt)) continue;
      switch(evt.type) {
      case SDL_QUIT: quit = true; triggerReset();
      case SDL_KEYDOWN:
        switch(evt.key.keysym.sym) {
        case SDLK_F1:
          PPU::show_overlay = !PPU::show_overlay;
          ui << "Overlay " << (PPU::show_overlay?"shown":"hidden") << ui;
          break;
        case SDLK_F2:
          PPU::show_sprites = !PPU::show_sprites;
          ui << "Sprites " << (PPU::show_sprites?"shown":"hidden") << ui;
          break;
        case SDLK_F3:
          PPU::show_background = !PPU::show_background;
          ui << "Background " << (PPU::show_background?"shown":"hidden")
             << ui;
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
    cpu->runCycles(CYCLES_PER_VBLANK);
    cartridge->oncePerFrame();
  }
  void printUsage() {
    std::cout <<
      "Usage: ars-emu [options] cartridge.etars\n"
      "Known options:\n"
      "-?: This usage string\n"
      "-c: Specify core type\n"
      "    Known cores:\n"
      "      fast: Scanline-based renderer. Fast, but not entirely accurate.\n"
      "      fast_debug: Scanline-based renderer, built-in debugger.\n"
      "-d: Allow debug port\n"
      ;
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
          case 'c':
            if(n >= argc) {
              std::cout << "-c needs an argument\n";
              valid = false;
            }
            else {
              std::string nextarg = argv[n++];
              if(nextarg == "fast") makeCPU = makeScanlineCPU;
              else if(nextarg == "fast_debug") makeCPU = makeScanlineDebugCPU;
              else {
                std::cout << "Unknown core: " << nextarg << "\n";
                valid = false;
              }
            }
            break;
          case 'd':
            allow_debug_port = true;
            break;
          default:
            std::cout << "Unknown option: " << arg[-1] << "\n";
            valid = false;
          }
        }
      }
      else if(rom_path_specified) {
        std::cout << "Error: More than one ROM path specified\n";
        printUsage();
        return false;
      }
      else {
        rom_path = arg;
        rom_path_specified = true;
      }
    }
    if(!rom_path_specified) {
      std::cout << "Error: No ROM path specified\n";
      printUsage();
      return false;
    }
    if(!valid) {
      printUsage();
      return false;
    }
    auto f = IO::OpenRawPathForRead(rom_path);
    if(!f || !*f) return false;
    Cartridge::loadRom(rom_path, *f);
    if(allow_debug_port) {
      if(cartridge->hasHardware(ARS::Cartridge::EXPANSION_DEBUG_PORT))
        ui << "Debug port is active" << ui;
      else
        ui << "Debug port enabled, but not used by this cartridge" << ui;
    }
    return true;
  }
}

void MessageImp::outputLine() {
  std::string msg = stream.str();
  int lifespan = MESSAGE_LIFESPAN;
  for(auto& msg : logged_messages)
    lifespan -= msg.lifespan;
  std::cerr << msg << std::endl;
  logged_messages.emplace_back(std::move(msg), lifespan);
  stream.clear();
  stream.str("");
  messages_dirty = true;
}

void ARS::triggerReset() {
  throw ResetException();
}

void ARS::triggerQuit() {
  quit = true;
  throw ResetException();
}

void ARS::temporalAnomaly() {
  experiencing_temporal_anomaly = true;
}

uint8_t ARS::read(uint16_t addr, bool OL, bool VPB, bool SYNC) {
  // TODO: detect bus conflicts
  (void)busconflict;
  if(addr < 0x8000) {
    if((addr & 0xFFF9) == 0x0211)
      return PPU::complexRead(addr);
    else if((addr & 0xFFF8) == 0x0240) {
      switch(addr&7) {
      case 0: return controller1->input();
      case 1: return controller2->input();
      case 6: break; // TODO: HAM
      case 7:
        if(cartridge->hasHardware(ARS::Cartridge::EXPANSION_DEBUG_PORT)) {
          cpu->setSO(true);
          cpu->setSO(false);
          return 0xFF;
        }
        break;
      }
    }
    else return dram[addr];
  }
  else return cartridge->read(bankMap[addr>>12], addr, OL, VPB, SYNC);
  badread(addr);
  return 0xBB;
}

void ARS::write(uint16_t addr, uint8_t value) {
  // TODO: detect bus conflicts
  (void)busconflict;
  if(addr < 0x8000) {
    dram[addr] = value;
    if(addr >= 0x0200 && addr < 0x0250) {
      if((addr ^ 0x0210) < 16) PPU::complexWrite(addr, value);
      else if((addr & 0xFFE0) == 0x0220) apu.write(addr&0x1F, value);
      else if((addr & 0xFFF0) == 0x0240) {
        if(addr >= 0x0248) {
          auto bs = cartridge->getBS();
          auto startBank = (addr&7)&~(7>>bs);
          auto stopBank = ((addr&7)|(7>>bs))+1;
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
          case 6:
            if(cartridge->hasHardware(ARS::Cartridge::EXPANSION_HAM)) {
              // TODO: HAM
            }
            break;
          case 7:
            if(cartridge->hasHardware(ARS::Cartridge::EXPANSION_DEBUG_PORT)) {
              std::cerr << value;
            }
            break;
          }
        }
      }
    }
    return;
  }
  else {
    cartridge->write(bankMap[addr>>12], addr, value);
    return;
  }
  badwrite(addr);
}

extern "C" int teg_main(int argc, char** argv) {
  if(!parseCommandLine(argc, const_cast<const char**>(argv))) return 1;
  srandom(time(NULL)); // rand() is only used for trashing memory on reset
  if(SDL_Init(SDL_INIT_VIDEO))
    die("Failed to initialize SDL!");
  atexit(cleanup);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  Config::Read(EMULATOR_CONFIG_FILE, EMULATOR_CONFIG_ELEMENTS,
               elementcount(EMULATOR_CONFIG_ELEMENTS));
  std::string windowtitle = "ARS-emu: "+rom_path;
  window = SDL_CreateWindow(windowtitle.c_str(),
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            window_width, window_height,
                            SDL_WINDOW_RESIZABLE);
  if(window == NULL) die("Couldn't create emulator window. Try deleting ARS-emu.conf.");
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
  if(renderer == NULL) die("Couldn't create emulator renderer. Try deleting ARS-emu.conf.");
  SDL_RenderSetLogicalSize(renderer, PPU::VISIBLE_SCREEN_WIDTH,
                           PPU::VISIBLE_SCREEN_HEIGHT);
  frametexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   PPU::INTERNAL_SCREEN_WIDTH,
                                   PPU::INTERNAL_SCREEN_HEIGHT);
  if(frametexture == NULL) die("Couldn't create emulator rendering surface.");
  messagetexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB4444,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     PPU::VISIBLE_SCREEN_WIDTH
                                     -MESSAGES_MARGIN_X*2,
                                     PPU::VISIBLE_SCREEN_HEIGHT
                                     -MESSAGES_MARGIN_Y*2);
  if(messagetexture == NULL) die("Couldn't create emulator message surface.");
  SDL_SetTextureBlendMode(messagetexture, SDL_BLENDMODE_BLEND);
  Controller::initControllers();
  quit = false;
  cpu = makeCPU(rom_path);
  fillDramWithGarbage(dram, sizeof(dram));
  PPU::fillWithGarbage();
  while(!quit) {
    cartridge->handleReset();
    PPU::handleReset();
    cpu->handleReset();
    for(size_t n = 0; n < sizeof(bankMap); ++n)
      bankMap[n] = cartridge->getPowerOnBank();
    try {
      while(!quit) mainLoop();
    }
    catch(const ResetException& e) {
    }
    if(!quit) {
      ui << "Reset!" << ui;
    }
  }
  return 0;
}
