#include "ars-emu.hh"
#include <iostream>
#include <iomanip>
#include "windower.hh"
#include "cpu.hh"
#include "ppu.hh"

using namespace ARS::PPU;

namespace {
  uint8_t spriteFetch[NUM_SPRITES*3];
  const uint8_t horizFlip[256] = {
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
}

namespace {
  template<class BGEngine> void renderBits(raw_screen& out) {
    uint16_t overlay_ptr = 0, overlay_attr_ptr = 0;
    uint8_t active_sprites[NUM_SPRITES];
    uint8_t num_active_sprites;
    for(int scanline = 0; scanline < LIVE_SCREEN_HEIGHT; ++scanline) {
      updateScanline(scanline);
      ARS::cpu->runCycles(ARS::SAFE_BLANK_CYCLES_PER_SCANLINE);
      auto& out_row = out[scanline];
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
      memset(out_row.data(), static_cast<uint8_t>(ARS::Regs.colorMod + 0xFF),
             LIVE_SCREEN_LEFT);
      /* Draw! */
      for(int column = 0; column < LIVE_SCREEN_WIDTH; ++column) {
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
        out_row[column+LIVE_SCREEN_LEFT] = out_color;
        bg_engine.advance();
      }
      ARS::cpu->runCycles(ARS::LIVE_CYCLES_PER_SCANLINE);
      memset(out_row.data() + LIVE_SCREEN_RIGHT,
             static_cast<uint8_t>(ARS::Regs.colorMod + 0xFF),
             TOTAL_SCREEN_WIDTH - LIVE_SCREEN_RIGHT);
      if((scanline&7) != 7 || (scanline < 8)
         || (scanline > LIVE_SCREEN_HEIGHT-8)) {
        overlay_ptr -= OVERLAY_TILES_WIDE;
        overlay_attr_ptr -= OVERLAY_TILES_WIDE/8;
      }
    }
  }
}

void ARS::PPU::renderFrame(raw_screen& out) {
  cpu->frameBoundary();
  cpu->runCycles(CYCLES_PER_VBLANK);
  ARS::cpu->setNMI(false);
  if(!(ARS::Regs.multi1&ARS::Regs::M1_VIDEO_ENABLE_MASK)) {
    ARS::cpu->runCycles((ARS::BLANK_CYCLES_PER_SCANLINE
                         + ARS::LIVE_CYCLES_PER_SCANLINE)
                        * LIVE_SCREEN_HEIGHT);
    memset(out.data(), static_cast<uint8_t>(ARS::Regs.colorMod + 0xFF),
           sizeof(out));
  }
  else {
    if(ARS::Regs.multi1 & ARS::Regs::M1_BACKGROUND_MODE_MASK)
      renderBits<mode2_bg_engine>(out);
    else
      renderBits<mode1_bg_engine>(out);
    updateScanline(LIVE_SCREEN_HEIGHT);
  }
  ARS::cpu->setNMI(true);
  renderMessages(out);
}

void ARS::PPU::renderInvisible() {
  cpu->frameBoundary();
  cpu->runCycles(CYCLES_PER_VBLANK);
  ARS::cpu->setNMI(false);
  if(ARS::Regs.multi1&ARS::Regs::M1_VIDEO_ENABLE_MASK) {
    for(int scanline = 0; scanline < LIVE_SCREEN_HEIGHT; ++scanline) {
      updateScanline(scanline);
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
                        * LIVE_SCREEN_HEIGHT);
  }
  updateScanline(LIVE_SCREEN_HEIGHT);
  ARS::cpu->setNMI(true);
  cycleMessages();
}
