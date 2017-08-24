#include "ars-emu.hh"
#include <iostream>
#include <iomanip>
#include "windower.hh"

using namespace ARS::PPU;

bool ARS::PPU::show_overlay = true,
  ARS::PPU::show_sprites = true,
  ARS::PPU::show_background = true;

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
#endif
  constexpr int NUM_SPRITES = 64;
  struct SpriteState {
    // $0, $1; Y>240 = effectively disabled
    uint8_t X, Y;
    // $2: TTTTTFVH
    // $3: TTTTTTTT
    // H = horizontal flip
    // V = vertical flip
    // F = foreground bit
    // TTTT TTTT TTTT T000 = starting address
    static constexpr uint8_t TILE_ADDR_MASK = 0xF8;
    static constexpr uint8_t HFLIP_MASK = 1;
    static constexpr uint8_t VFLIP_MASK = 2;
    static constexpr uint8_t FOREGROUND_MASK = 4;
    uint8_t TileAddr, TilePage;
  } ssm[NUM_SPRITES];
#define ssmBytes reinterpret_cast<uint8_t*>(ssm)
  static_assert(sizeof(SpriteState) == 4, "Sprite size has slipped");
  // HHHHHPPP
  // H = tile height-1, range 1..32 (8..128)
  typedef uint8_t SpriteAttr;
  SpriteAttr sam[NUM_SPRITES];
  constexpr uint8_t* samBytes = sam;
  static constexpr uint8_t SA_HEIGHT_SHIFT = 3;
  static constexpr uint8_t SA_HEIGHT_MASK = 0x1F;
  static constexpr uint8_t SA_PALETTE_SHIFT = 0;
  static constexpr uint8_t SA_PALETTE_MASK = 0x07;
  uint8_t vram[0x10000];
  uint8_t cram[0x100];
  uint16_t vramAccessPtr;
  uint8_t cramAccessPtr, ssmAccessPtr, samAccessPtr;
  int curScanline = INTERNAL_SCREEN_HEIGHT;
  struct Background_Mode1 {
    uint8_t Tiles[MODE1_BACKGROUND_TILES_WIDE*MODE1_BACKGROUND_TILES_HIGH];
    uint8_t Attributes[MODE1_BACKGROUND_TILES_WIDE*MODE1_BACKGROUND_TILES_HIGH
                       /16];
    uint8_t padding[4];
  }* backgrounds_mode1 = reinterpret_cast<struct Background_Mode1*>(vram);
  struct Background_Mode2 {
    uint8_t Tiles[MODE2_BACKGROUND_TILES_WIDE*MODE2_BACKGROUND_TILES_HIGH];
  }* backgrounds_mode2 = reinterpret_cast<struct Background_Mode2*>(vram);
  static_assert(sizeof(Background_Mode1) == 0x400, "Background size slipped");
  static_assert(sizeof(Background_Mode2) == 0x400, "Background size slipped");
  struct Overlay {
    uint8_t Tiles[OVERLAY_TILES_WIDE*OVERLAY_TILES_HIGH];
    uint8_t Attributes[OVERLAY_TILES_WIDE*OVERLAY_TILES_HIGH/8];
    uint8_t padding[16];
  }& overlay = *reinterpret_cast<struct Overlay*>(ARS::dram+sizeof(ARS::dram)
                                                  -sizeof(struct Overlay));
  static_assert(sizeof(Overlay) == 0x400, "Overlay size has slipped");
  uint8_t spriteFetch[NUM_SPRITES*3];
  bool lastRenderWasBlank = false;
  uint8_t horizFlip[256] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0,
    0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
    0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
    0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC,
    0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
    0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA,
    0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6,
    0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1,
    0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9,
    0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
    0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED,
    0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
    0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
    0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7,
    0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF,
    0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
  };
  uint32_t hardwarePalette[128] = {
    0xFF2B2B2B, 0xFF552B2B, 0xFF55402B, 0xFF554A2B,
    0xFF55552B, 0xFF4A552B, 0xFF2B552B, 0xFF2B5555,
    0xFF2B4A55, 0xFF2B4055, 0xFF2B2B55, 0xFF402B55,
    0xFF4A2B55, 0xFF552B55, 0xFF000000, 0xFF000000,
    0xFF555555, 0xFF550000, 0xFF552B00, 0xFF554000,
    0xFF555500, 0xFF405500, 0xFF005500, 0xFF005555,
    0xFF004055, 0xFF002B55, 0xFF000055, 0xFF2B0055,
    0xFF400055, 0xFF550055, 0xFF000000, 0xFF000000,
    0xFF808080, 0xFFAA5555, 0xFFAA8055, 0xFFAA9555,
    0xFFAAAA55, 0xFF95AA55, 0xFF55AA55, 0xFF55AAAA,
    0xFF5595AA, 0xFF5580AA, 0xFF5555AA, 0xFF8055AA,
    0xFF9555AA, 0xFFAA55AA, 0xFF000000, 0xFF000000,
    0xFFAAAAAA, 0xFFAA0000, 0xFFAA5500, 0xFFAA8000,
    0xFFAAAA00, 0xFF80AA00, 0xFF00AA00, 0xFF00AAAA,
    0xFF0080AA, 0xFF0055AA, 0xFF0000AA, 0xFF5500AA,
    0xFF8000AA, 0xFFAA00AA, 0xFF000000, 0xFF000000,
    0xFFD5D5D5, 0xFFFF8080, 0xFFFFBF80, 0xFFFFDF80,
    0xFFFFFF80, 0xFFDFFF80, 0xFF80FF80, 0xFF80FFFF,
    0xFF80DFFF, 0xFF80BFFF, 0xFF8080FF, 0xFFBF80FF,
    0xFFDF80FF, 0xFFFF80FF, 0xFF000000, 0xFF000000,
    0xFFFFFFFF, 0xFFFF0000, 0xFFFF8000, 0xFFFFBF00,
    0xFFFFFF00, 0xFFBFFF00, 0xFF00FF00, 0xFF00FFFF,
    0xFF00BFFF, 0xFF0080FF, 0xFF0000FF, 0xFF8000FF,
    0xFFBF00FF, 0xFFFF00FF, 0xFF000000, 0xFF000000,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  };
  struct mode1_bg_engine {
    int bg_x_tile, bg_x_col;
    int bg_y_tile, bg_y_row;
    int bga_y_tile, bga_x_tile;
    int cur_screen;
    int bg_rowptr, bga_rowptr;
    int bg_ptr, bga_ptr;
    uint8_t bg_low_plane, bg_high_plane, bga_block;
    mode1_bg_engine(int scanline) {
      bg_y_tile = (ARS::Regs.bgScrollY + scanline) >> 3;
      bg_y_row = (ARS::Regs.bgScrollY + scanline) & 7;
      bg_x_tile = ARS::Regs.bgScrollX >> 3;
      bg_x_col = ARS::Regs.bgScrollX & 7;
      bga_y_tile = (ARS::Regs.bgScrollY + scanline) >> 4;
      bga_x_tile = ARS::Regs.bgScrollX >> 4;
      cur_screen = ARS::Regs.multi1 & ARS::Regs::M1_FLIP_MAGIC_MASK;
      while(bg_y_tile >= MODE1_BACKGROUND_TILES_HIGH) {
        cur_screen ^= 2;
        bg_y_tile -= MODE1_BACKGROUND_TILES_HIGH;
      }
      bg_rowptr = bg_y_tile * MODE1_BACKGROUND_TILES_WIDE;
      bga_rowptr = bga_y_tile * MODE1_BACKGROUND_TILES_WIDE / 8;
      bg_ptr = bg_rowptr + bg_x_tile;
      bga_ptr = bga_rowptr + bga_x_tile / 4;
      uint8_t bg_tile = backgrounds_mode1[cur_screen].Tiles[bg_ptr++];
      uint8_t bgBase;
      switch(cur_screen) {
      default:
      case 0: bgBase = ARS::Regs.bgTileBaseTop & 15; break;
      case 1: bgBase = ARS::Regs.bgTileBaseTop >> 4; break;
      case 2: bgBase = ARS::Regs.bgTileBaseBot & 15; break;
      case 3: bgBase = ARS::Regs.bgTileBaseBot >> 4; break;
      }
      bg_low_plane = vram[((bgBase<<12)|(bg_tile<<4))+bg_y_row];
      bg_high_plane = vram[((bgBase<<12)|(bg_tile<<4))+bg_y_row+8];
      bga_block = backgrounds_mode1[cur_screen].Attributes[bga_ptr++];
    }
    void getState(uint8_t& bg_color, bool& bg_priority) {
      uint8_t raw_color = ((bg_low_plane>>(~bg_x_col&7))&1)
        | (((bg_high_plane>>(~bg_x_col&7))&1)<<1);
      uint8_t palette = getPalette();
      bg_color = (ARS::Regs.bgBasePalette[cur_screen]
                  <<4)|(palette<<2)|raw_color;
      auto bg_num_background_colors =
        4 - ((ARS::Regs.bgForegroundInfo[cur_screen]>>(palette<<1))&3);
      if(bg_num_background_colors != 4) --bg_num_background_colors;
      bg_priority = raw_color >= bg_num_background_colors;
    }
    uint8_t getPalette() {
      return (bga_block >> (((bg_x_tile>>1)&3)<<1))&3;
    }
    void advance() {
      ++bg_x_col;
      if(bg_x_col == 8) {
        bg_x_col = 0;
        ++bg_x_tile;
        if(bg_x_tile == MODE1_BACKGROUND_TILES_WIDE) {
          cur_screen ^= 1;
          bg_x_tile = 0;
          bg_ptr = bg_rowptr;
          bga_x_tile = 0;
          bga_ptr = bga_rowptr;
          bga_block = backgrounds_mode1[cur_screen].Attributes[bga_ptr++];
        }
        else if((bg_x_tile&7)==0) {
          bga_block = backgrounds_mode1[cur_screen].Attributes[bga_ptr++];
        }
        uint8_t bg_tile = backgrounds_mode1[cur_screen].Tiles[bg_ptr++];
        uint8_t bgBase;
        switch(cur_screen) {
        default:
        case 0: bgBase = ARS::Regs.bgTileBaseTop & 15; break;
        case 1: bgBase = ARS::Regs.bgTileBaseTop >> 4; break;
        case 2: bgBase = ARS::Regs.bgTileBaseBot & 15; break;
        case 3: bgBase = ARS::Regs.bgTileBaseBot >> 4; break;
        }
        bg_low_plane = vram[((bgBase<<12)|(bg_tile<<4))+bg_y_row];
        bg_high_plane = vram[((bgBase<<12)|(bg_tile<<4))+bg_y_row+8];
      }
    }
  };
  struct mode2_bg_engine {
    int bg_x_tile, bg_x_col;
    int bg_y_tile, bg_y_row;
    int cur_screen;
    int bg_rowptr, bg_ptr;
    uint8_t bg_pal;
    uint8_t bg_low_plane, bg_high_plane;
    mode2_bg_engine(int scanline) {
      bg_y_tile = (ARS::Regs.bgScrollY + scanline) >> 3;
      bg_y_row = (ARS::Regs.bgScrollY + scanline) & 7;
      bg_x_tile = ARS::Regs.bgScrollX >> 3;
      bg_x_col = ARS::Regs.bgScrollX & 7;
      cur_screen = ARS::Regs.multi1 & ARS::Regs::M1_FLIP_MAGIC_MASK;
      if(bg_y_tile >= MODE2_BACKGROUND_TILES_HIGH) {
        cur_screen ^= 2;
        bg_y_tile -= MODE2_BACKGROUND_TILES_HIGH;
      }
      bg_rowptr = bg_y_tile * MODE2_BACKGROUND_TILES_WIDE;
      bg_ptr = bg_rowptr + bg_x_tile;
      uint8_t bg_byte = backgrounds_mode2[cur_screen].Tiles[bg_ptr++];
      bg_pal = bg_byte&3;
      uint8_t bg_tile = bg_byte&0xFC;
      if(bg_x_tile&1) bg_tile |= 1;
      if(bg_y_tile&1) bg_tile |= 2;
      uint8_t bgBase;
      switch(cur_screen) {
      default:
      case 0: bgBase = ARS::Regs.bgTileBaseTop & 15; break;
      case 1: bgBase = ARS::Regs.bgTileBaseTop >> 4; break;
      case 2: bgBase = ARS::Regs.bgTileBaseBot & 15; break;
      case 3: bgBase = ARS::Regs.bgTileBaseBot >> 4; break;
      }
      bg_low_plane = vram[((bgBase<<12)|(bg_tile<<4))+bg_y_row];
      bg_high_plane = vram[((bgBase<<12)|(bg_tile<<4))+bg_y_row+8];
    }
    void getState(uint8_t& bg_color, bool& bg_priority) {
      uint8_t raw_color = ((bg_low_plane>>(~bg_x_col&7))&1)
        | (((bg_high_plane>>(~bg_x_col&7))&1)<<1);
      uint8_t palette = getPalette();
      bg_color = (ARS::Regs.bgBasePalette[cur_screen]
                  <<4)|(palette<<2)|raw_color;
      auto bg_num_background_colors =
        4 - ((ARS::Regs.bgForegroundInfo[cur_screen] >>(palette<<1))&3);
      if(bg_num_background_colors != 4) --bg_num_background_colors;
      bg_priority = raw_color >= bg_num_background_colors;
    }
    uint8_t getPalette() {
      return bg_pal;
    }
    void advance() {
      ++bg_x_col;
      if(bg_x_col == 8) {
        bg_x_col = 0;
        ++bg_x_tile;
        if(bg_x_tile == MODE2_BACKGROUND_TILES_WIDE) {
          cur_screen ^= 1;
          bg_x_tile = 0;
          bg_ptr = bg_rowptr;
        }
        uint8_t bg_byte = backgrounds_mode2[cur_screen].Tiles[bg_ptr++];
        bg_pal = bg_byte&3;
        uint8_t bg_tile = bg_byte&0xFC;
        if(bg_x_tile&1) bg_tile |= 1;
        if(bg_y_tile&1) bg_tile |= 2;
        uint8_t bgBase;
        switch(cur_screen) {
        default:
        case 0: bgBase = ARS::Regs.bgTileBaseTop & 15; break;
        case 1: bgBase = ARS::Regs.bgTileBaseTop >> 4; break;
        case 2: bgBase = ARS::Regs.bgTileBaseBot & 15; break;
        case 3: bgBase = ARS::Regs.bgTileBaseBot >> 4; break;
        }
        bg_low_plane = vram[((bgBase<<12)|(bg_tile<<4))+bg_y_row];
        bg_high_plane = vram[((bgBase<<12)|(bg_tile<<4))+bg_y_row+8];
      }
    }
  };
  void updateIRQ() {
    ARS::cpu->setIRQ(curScanline >= ARS::Regs.irqScanline
                     && (curScanline & 0x80)==(ARS::Regs.irqScanline & 0x80));
  }
#if !NO_DEBUG_CORES
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
  case 0x0219: updateIRQ(); break;
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

namespace {
  template<class BGEngine> void renderBits(uint8_t* basep, int pitch) {
    uint16_t overlay_ptr = 0, overlay_attr_ptr = 0;
    uint8_t active_sprites[NUM_SPRITES];
    uint8_t num_active_sprites;
    for(int scanline = 0; scanline < INTERNAL_SCREEN_HEIGHT; ++scanline) {
      curScanline = scanline;
      updateIRQ();
      ARS::cpu->runCycles(ARS::SAFE_BLANK_CYCLES_PER_SCANLINE);
      uint32_t* outp = reinterpret_cast<uint32_t*>(basep);
      basep += pitch;
      /* "prefetch" all sprite tiles */
      num_active_sprites = 0;
      int spriteFetchIndex = 0;
      for(int n = 0; n < NUM_SPRITES; ++n) {
        const SpriteState& sprite = ssm[n];
        SpriteAttr attr = sam[n];
        int height = (((attr>>SA_HEIGHT_SHIFT)&SA_HEIGHT_MASK)+1)*8;
        if(sprite.Y > scanline || sprite.Y+height <= scanline) {
          // sprite not active
        }
        else {
          active_sprites[num_active_sprites++] = n;
          int effective_y = scanline - sprite.Y;
          if(sprite.TileAddr&SpriteState::VFLIP_MASK)
            effective_y = height - effective_y - 1;
          effective_y = (effective_y & 7) + (effective_y >> 3) * 24;
          uint16_t tile_address =
            (sprite.TileAddr & SpriteState::TILE_ADDR_MASK)
            | (sprite.TilePage<<8);
          if(sprite.TileAddr & SpriteState::HFLIP_MASK) {
            spriteFetch[spriteFetchIndex++] = vram[tile_address+effective_y];
            spriteFetch[spriteFetchIndex++] = vram[tile_address+effective_y+8];
            spriteFetch[spriteFetchIndex++] =vram[tile_address+effective_y+16];
          }
          else {
            spriteFetch[spriteFetchIndex++] =
              horizFlip[vram[tile_address+effective_y]];
            spriteFetch[spriteFetchIndex++] =
              horizFlip[vram[tile_address+effective_y+8]];
            spriteFetch[spriteFetchIndex++] =
              horizFlip[vram[tile_address+effective_y+16]];
          }
        }
      }
      /* Initialize the background state machine */
      BGEngine bg_engine(scanline);
      /* Overlay state machine */
      uint8_t overlay_tile, overlay_attr = 0;
      uint8_t overlay_low_plane = 0, overlay_high_plane = 0;
      ARS::cpu->runCycles(ARS::UNSAFE_BLANK_CYCLES_PER_SCANLINE);
      /* Draw! */
      for(int column = 0; column < INTERNAL_SCREEN_WIDTH; ++column) {
        uint8_t out_color;
        if((ARS::Regs.multi1>>ARS::Regs::M1_OLBASE_SHIFT)
           &ARS::Regs::M1_OLBASE_MASK) {
          /* Find overlay color. */
          if((column & 7) == 0) {
            overlay_tile = overlay.Tiles[overlay_ptr++];
            overlay_low_plane = ARS::read((((ARS::Regs.multi1
                                             >>ARS::Regs::M1_OLBASE_SHIFT)
                                            &ARS::Regs::M1_OLBASE_MASK)<<12)
                                          +(overlay_tile << 4)
                                          +(scanline&7), true);
            overlay_high_plane = ARS::read((((ARS::Regs.multi1
                                              >>ARS::Regs::M1_OLBASE_SHIFT)
                                             &ARS::Regs::M1_OLBASE_MASK)<<12)
                                           +(overlay_tile << 4)
                                           +(scanline&7)+8, true);
            ARS::cpu->eatCycles(3);
          }
          if((column & 63) == 0) {
            overlay_attr = overlay.Attributes[overlay_attr_ptr++];
          }
          out_color = ((overlay_low_plane>>(~column&7))&1)
            | (((overlay_high_plane>>(~column&7))&1)<<1);
        }
        else {
          out_color = 0;
          if((column & 7) == 0)
            ++overlay_ptr;
          if((column & 63) == 0)
            ++overlay_attr_ptr;
        }
        if(out_color != 0 && show_overlay) {
          /* Non-zero overlay pixels always take priority */
          out_color = static_cast<uint8_t>
            (cram[((ARS::Regs.olBasePalette << 3)
                   | (((overlay_attr>>((~column>>3)&7))&1)<<2)
                   | out_color)] + ARS::Regs.colorMod);
        }
        else {
          uint8_t bg_color, sprite_color = 0;
          bool bg_priority, sprite_priority = false, sprite_exists = false;
          bg_engine.getState(bg_color, bg_priority);
          int sfi = 0;
          for(uint8_t i = 0; i < num_active_sprites; ++i, sfi += 3) {
            auto& sprite = ssm[active_sprites[i]];
            if(column < sprite.X || column >= sprite.X + 8) continue;
            uint8_t me_color = ((spriteFetch[sfi]>>(column-sprite.X))&1)
              | (((spriteFetch[sfi+1]>>(column-sprite.X))&1)<<1)
              | (((spriteFetch[sfi+2]>>(column-sprite.X))&1)<<2);
            if(me_color != 0) {
              sprite_color = static_cast<uint8_t>
                (cram[((((ARS::Regs
                          .spBasePalette>>(bg_engine.cur_screen<<1))&3)<<6)
                       |(((sam[active_sprites[i]]
                           >>SA_PALETTE_SHIFT)
                          &SA_PALETTE_MASK)<<3)
                       |me_color)]+ARS::Regs.colorMod);
              sprite_exists = true;
              sprite_priority = !!(sprite.TileAddr
                                   & SpriteState::FOREGROUND_MASK);
              break;
            }
          }
          if(sprite_exists && (sprite_priority || !bg_priority)
             && show_sprites) {
            out_color = sprite_color;
          }
          else if(show_background) {
            out_color = static_cast<uint8_t>
              (cram[bg_color]+ARS::Regs.colorMod);
          }
          else out_color = cram[ARS::Regs.colorMod];
        }
        *outp++ = hardwarePalette[out_color<128?out_color:127];
        bg_engine.advance();
      }
      ARS::cpu->runCycles(ARS::LIVE_CYCLES_PER_SCANLINE);
      if((scanline&7) != 7 || (scanline < 8)
         || (scanline > INTERNAL_SCREEN_HEIGHT-8)) {
        overlay_ptr -= OVERLAY_TILES_WIDE;
        overlay_attr_ptr -= OVERLAY_TILES_WIDE/8;
      }
    }
  }
}

void ARS::PPU::renderToTexture(SDL_Texture* texture) {
#if !NO_DEBUG_CORES
  if(ARS::debugging_video)
    maybe_make_debug_window();
#endif
  ARS::cpu->setNMI(false);
  if(!(ARS::Regs.multi1&ARS::Regs::M1_VIDEO_ENABLE_MASK)) {
    ARS::cpu->runCycles((ARS::BLANK_CYCLES_PER_SCANLINE
                         + ARS::LIVE_CYCLES_PER_SCANLINE)
                        * INTERNAL_SCREEN_HEIGHT);
    if(!lastRenderWasBlank) {
      uint8_t* pixels;
      int pitch;
      SDL_LockTexture(texture, nullptr,
                      reinterpret_cast<void**>(&pixels), &pitch);
      memset(pixels, 0, pitch * INTERNAL_SCREEN_HEIGHT);
      SDL_UnlockTexture(texture);
      lastRenderWasBlank = true;
    }
  }
  else {
    lastRenderWasBlank = false;
    uint8_t* basep;
    int pitch;
    SDL_LockTexture(texture, nullptr,
                    reinterpret_cast<void**>(&basep), &pitch);
    if(ARS::Regs.multi1 & ARS::Regs::M1_BACKGROUND_MODE_MASK)
      renderBits<mode2_bg_engine>(basep, pitch);
    else
      renderBits<mode1_bg_engine>(basep, pitch);
    SDL_UnlockTexture(texture);
    curScanline = INTERNAL_SCREEN_HEIGHT;
    updateIRQ();
  }
  ARS::cpu->setNMI(true);
}

void ARS::PPU::dummyRender() {
  ARS::cpu->setNMI(false);
  if(ARS::Regs.multi1&ARS::Regs::M1_VIDEO_ENABLE_MASK) {
    for(int scanline = 0; scanline < INTERNAL_SCREEN_HEIGHT; ++scanline) {
      curScanline = scanline;
      updateIRQ();
      if((ARS::Regs.multi1>>ARS::Regs::M1_OLBASE_SHIFT)
         &ARS::Regs::M1_OLBASE_MASK)
        ARS::cpu->eatCycles(3*OVERLAY_TILES_WIDE);
      ARS::cpu->runCycles(ARS::BLANK_CYCLES_PER_SCANLINE
                          + ARS::LIVE_CYCLES_PER_SCANLINE);
    }
  }
  else {
    ARS::cpu->runCycles((ARS::BLANK_CYCLES_PER_SCANLINE
                         + ARS::LIVE_CYCLES_PER_SCANLINE)
                        * INTERNAL_SCREEN_HEIGHT);
  }
  curScanline = INTERNAL_SCREEN_HEIGHT;
  updateIRQ();
  ARS::cpu->setNMI(true);
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
