#ifndef PPUHH
#define PPUHH

#include "ars-emu.hh"

namespace ARS {
  namespace PPU {
    constexpr int INTERNAL_SCREEN_WIDTH = 256, INTERNAL_SCREEN_HEIGHT = 240;
    constexpr int INTERNAL_SCREEN_TILES_WIDE = 32,
      INTERNAL_SCREEN_TILES_HIGH = 30;
    static_assert(INTERNAL_SCREEN_WIDTH/8 == INTERNAL_SCREEN_TILES_WIDE,
                  "faulty WIDTH constant");
    static_assert(INTERNAL_SCREEN_HEIGHT/8 == INTERNAL_SCREEN_TILES_HIGH,
                  "faulty HEIGHT constant");
    constexpr int INVISIBLE_SCANLINE_COUNT = 8;
    constexpr int INVISIBLE_COLUMN_COUNT = 8;
    constexpr int VISIBLE_SCREEN_WIDTH = 240, VISIBLE_SCREEN_HEIGHT = 224;
    constexpr int VISIBLE_SCREEN_TILES_WIDE = 30, VISIBLE_SCREEN_TILES_HIGH=28;
    static_assert(VISIBLE_SCREEN_WIDTH/8 == VISIBLE_SCREEN_TILES_WIDE,
                  "faulty WIDTH constant");
    static_assert(VISIBLE_SCREEN_HEIGHT/8 == VISIBLE_SCREEN_TILES_HIGH,
                  "faulty HEIGHT constant");
    constexpr int MODE1_BACKGROUND_TILES_WIDE = 32;
    constexpr int MODE1_BACKGROUND_TILES_HIGH = 30;
    constexpr int MODE2_BACKGROUND_TILES_WIDE = 32;
    constexpr int MODE2_BACKGROUND_TILES_HIGH = 32;
    constexpr int OVERLAY_TILES_WIDE = 32;
    constexpr int OVERLAY_TILES_HIGH = 28;
    void renderToTexture(SDL_Texture*);
    void dummyRender();
    void fillWithGarbage();
    void handleReset();
    void dumpSpriteMemory();
    extern bool show_overlay, show_sprites, show_background;
    // $0211, $0213, $0215, $0217
    uint8_t complexRead(uint16_t addr);
    // $0210-$021F
    void complexWrite(uint16_t addr, uint8_t value);
  }
}

#endif
