#include "ppu.hh"
#include "cpu.hh"
#include "windower.hh"

#include <iomanip>

using namespace ARS::PPU;

namespace {
#if !NO_DEBUG_CORES
  constexpr int DEBUG_WINDOW_PIXEL_SIZE = 1,
    DEBUG_TILES_WIDE = 128, DEBUG_TILES_HIGH = 64,
    DEBUG_DIVIDER_COUNT=15, DEBUG_TILES_PER_DIVIDER = 8;
  SDL_Window* debugwindow = nullptr;
  SDL_Renderer* debugrenderer;
  SDL_Texture* debugtexture;
  void debug_event(SDL_Event& evt);
  void maybe_make_debug_window() {
    if(debugwindow != nullptr) return;
    debugwindow = SDL_CreateWindow("ARS-emu VRAM",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   (8*DEBUG_TILES_WIDE+DEBUG_DIVIDER_COUNT)
                                   *DEBUG_WINDOW_PIXEL_SIZE,
                                   DEBUG_WINDOW_PIXEL_SIZE*8*DEBUG_TILES_HIGH,
                                   0);
    if(debugwindow == nullptr)
      die("Couldn't create video debugging window: %s", SDL_GetError());
    Windower::Register(SDL_GetWindowID(debugwindow), debug_event);
    debugrenderer = SDL_CreateRenderer(debugwindow, -1, 0);
    if(debugrenderer == nullptr)
      die("Couldn't create video debugging renderer: %s", SDL_GetError());
    SDL_RenderSetLogicalSize(debugrenderer, 8*DEBUG_TILES_WIDE+DEBUG_DIVIDER_COUNT, 8*DEBUG_TILES_HIGH);
    debugtexture = SDL_CreateTexture(debugrenderer,
                                     SDL_PIXELFORMAT_RGB332,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     8*DEBUG_TILES_WIDE+DEBUG_DIVIDER_COUNT,
                                     8*DEBUG_TILES_HIGH);
  }
  void debug_event(SDL_Event& evt) {
    switch(evt.type) {
    default:
      if(evt.type == Windower::UPDATE_EVENT) {
        maybe_make_debug_window();
        uint8_t* pixels;
        int pitch;
        SDL_LockTexture(debugtexture, nullptr,
                        reinterpret_cast<void**>(&pixels), &pitch);
        const uint8_t* vramp = vram;
        for(int x = 0; x < DEBUG_TILES_WIDE; ++x) {
          if(x % DEBUG_TILES_PER_DIVIDER == 0 && x != 0) {
            for(int y = 0; y < DEBUG_TILES_HIGH * 8; ++y) {
              pixels[pitch*y] = 0x80;
            }
            ++pixels;
          }
          for(int y = 0; y < DEBUG_TILES_HIGH; ++y) {
            for(int r = 0; r < 8; ++r) {
              uint8_t inv = (vramp-vram) == vramAccessPtr ? 0xFF : 0x00;
              uint8_t plane = *vramp++;
              for(int bit = 0; bit < 8; ++bit) {
                pixels[bit] = ((plane&(128>>bit))?0x1C:0x00)^inv;
              }
              pixels += pitch;
            }
          }
          pixels -= pitch * DEBUG_TILES_HIGH * 8;
          pixels += 8;
        }
        SDL_UnlockTexture(debugtexture);
        SDL_RenderClear(debugrenderer);
        SDL_RenderCopy(debugrenderer, debugtexture, NULL, NULL);
        SDL_RenderPresent(debugrenderer);
      }
    }
  }
#endif
  int cur_scanline = LIVE_SCREEN_HEIGHT;
}

namespace ARS {
  namespace PPU {
    bool show_overlay = true, show_sprites = true, show_background = true;
    SpriteState ssm[NUM_SPRITES];
    SpriteAttr sam[NUM_SPRITES];
    uint8_t vram[0x10000];
    uint8_t cram[0x100];
    uint16_t vramAccessPtr;
    uint8_t cramAccessPtr, ssmAccessPtr, samAccessPtr;
  }
}

void ARS::PPU::updateScanline(int new_scanline) {
#if !NO_DEBUG_CORES
  if(ARS::debugging_video)
    maybe_make_debug_window();
#endif
  cur_scanline = new_scanline;
  ARS::cpu->setIRQ(cur_scanline >= ARS::Regs.irqScanline
                   && (cur_scanline & 0x80)==(ARS::Regs.irqScanline&0x80));
}

void ARS::PPU::complexWrite(uint16_t addr, uint8_t value) {
  switch(addr) {
  case 0x0210: vramAccessPtr = value<<8; break;
  case 0x0211: vram[vramAccessPtr++] = value; break;
  case 0x0212: cramAccessPtr = value; break;
  case 0x0213: cram[cramAccessPtr++] = value; break;
  case 0x0214: ssmAccessPtr = value; break;
  case 0x0215: ssmBytes[ssmAccessPtr++] = value; break;
  case 0x0216: samAccessPtr = value; break;
  case 0x0217: samBytes[(samAccessPtr++)&63] = value; break;
  case 0x0218: vramAccessPtr = (vramAccessPtr&0xFF00)|value; break;
  case 0x0219: updateScanline(cur_scanline); break;
  case 0x021A: {
    uint16_t addr = value<<8;
    for(int n = 0; n < 256; ++n) {
      vram[vramAccessPtr++] = ARS::read(addr++);
    }
    ARS::cpu->eatCycles(257);
  } break;
  case 0x021B: {
    uint16_t addr = value<<8;
    for(int y = 0; y < 16; ++y) {
      for(int x = 0; x < 16; ++x) {
        vram[vramAccessPtr++] = ARS::read(addr+x);
        vram[vramAccessPtr++] = ARS::read(addr+x);
      }
      for(int x = 0; x < 16; ++x) {
        vram[vramAccessPtr++] = ARS::read(addr+x);
        vram[vramAccessPtr++] = ARS::read(addr+x);
      }
      addr += 16;
    }
    ARS::cpu->eatCycles(1025);
  } break;
  case 0x021C: {
    uint16_t addr = value<<8;
    for(int n = 0; n < 256; ++n) {
      cram[cramAccessPtr++] = ARS::read(addr++);
    }
    ARS::cpu->eatCycles(257);
  } break;
  case 0x021D: {
    uint16_t addr = value<<8;
    for(int n = 0; n < 256; ++n) {
      ssmBytes[ssmAccessPtr++] = ARS::read(addr++);
    }
    ARS::cpu->eatCycles(257);
  } break;
  case 0x021E: {
    uint16_t addr = value<<8;
    for(int n = 0; n < 64; ++n) {
      samBytes[(samAccessPtr++)&63] = ARS::read(addr++);
    }
    ARS::cpu->eatCycles(65);
  } break;
  case 0x021F: {
    uint16_t addr = value<<8;
    for(int n = 0; n < 64; ++n) {
      ssmBytes[ssmAccessPtr++] = ARS::read(addr);
      ssmBytes[ssmAccessPtr++] = ARS::read(addr+0x40);
      ssmBytes[ssmAccessPtr++] = ARS::read(addr+0x80);
      ssmBytes[ssmAccessPtr++] = ARS::read(addr+0xC0);
      ++addr;
    }
    ARS::cpu->eatCycles(257);
  } break;
  default:
    // harmless
    break;
  }
}

uint8_t ARS::PPU::complexRead(uint16_t addr) {
  switch(addr) {
  case 0x0211: return vram[vramAccessPtr];
  case 0x0213: return cram[cramAccessPtr];
  case 0x0215: return ssmBytes[ssmAccessPtr];
  case 0x0217: return samBytes[samAccessPtr];
  default:
    SDL_assert("ARS::Regs::complexRead called with an inappropriate address"
               && false); // "interesting" idiom there...
    return 0xCC;
  }
}


void ARS::PPU::fillWithGarbage() {
  fillDramWithGarbage(vram, sizeof(vram));
  fillDramWithGarbage(cram, sizeof(cram));
  fillDramWithGarbage(ssmBytes, sizeof(ssm));
  fillDramWithGarbage(samBytes, sizeof(sam));
}

void ARS::PPU::handleReset() {
  ARS::Regs.multi1 = 0;
  ARS::Regs.irqScanline = 255;
  cur_scanline = 255;
  cpu->setIRQ(false);
}

void ARS::PPU::dumpSpriteMemory() {
  std::cerr << "Sprite memory:\n## ...SM... AM\n";
  std::cerr << std::hex;
  for(int n = 0; n < NUM_SPRITES; ++n) {
    std::cerr << std::setw(2) << std::setfill('0') << n << " " <<
      std::setw(2) << std::setfill('0') << (int)ssm[n].X <<
      std::setw(2) << std::setfill('0') << (int)ssm[n].Y <<
      std::setw(2) << std::setfill('0') << (int)ssm[n].TileAddr <<
      std::setw(2) << std::setfill('0') << (int)ssm[n].TilePage << " " <<
      std::setw(2) << std::setfill('0') << (int)sam[n] << "\n";
  }
  std::cerr << std::dec;
}
