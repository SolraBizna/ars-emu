#ifndef PPUHH
#define PPUHH

#include "ars-emu.hh"

#include <array>

namespace ARS {
  namespace PPU {
    constexpr int LIVE_SCREEN_WIDTH = 256, LIVE_SCREEN_HEIGHT = 240;
    // we leave off three always-overscanned pixels for performance
    // this way rows are not only 16-byte aligned, but also cache line aligned
    constexpr int TOTAL_SCREEN_WIDTH = 320;
    constexpr int TOTAL_SCREEN_HEIGHT = LIVE_SCREEN_HEIGHT;
    constexpr int LIVE_SCREEN_LEFT = 32;
    constexpr int LIVE_SCREEN_RIGHT = LIVE_SCREEN_LEFT+LIVE_SCREEN_WIDTH;
    constexpr int LIVE_SCREEN_TOP = 0;
    constexpr int LIVE_SCREEN_BOTTOM = TOTAL_SCREEN_HEIGHT;
    constexpr int CONVENIENT_OVERSCAN_WIDTH = 272;
    constexpr int CONVENIENT_OVERSCAN_HEIGHT = 224;
    constexpr int CONVENIENT_OVERSCAN_LEFT = (TOTAL_SCREEN_WIDTH-CONVENIENT_OVERSCAN_WIDTH)/2;
    constexpr int CONVENIENT_OVERSCAN_RIGHT = CONVENIENT_OVERSCAN_LEFT + CONVENIENT_OVERSCAN_WIDTH;
    constexpr int CONVENIENT_OVERSCAN_TOP = (TOTAL_SCREEN_HEIGHT-CONVENIENT_OVERSCAN_HEIGHT)/2;
    constexpr int CONVENIENT_OVERSCAN_BOTTOM = CONVENIENT_OVERSCAN_TOP + CONVENIENT_OVERSCAN_HEIGHT;
#ifndef CACHE_NOT_REAL
    static_assert(CONVENIENT_OVERSCAN_WIDTH % 16 == 0,
                  "Convenient overscan width should be a multiple of 16, for"
                  " best performance");
#endif
    constexpr int SCREEN_TILES_WIDE = 32;
    constexpr int SCREEN_TILES_HIGH = 30;
    constexpr int TILE_SIZE = 8;
    static_assert(LIVE_SCREEN_WIDTH/TILE_SIZE == SCREEN_TILES_WIDE,
                  "faulty WIDTH constant");
    static_assert(LIVE_SCREEN_HEIGHT/TILE_SIZE == SCREEN_TILES_HIGH,
                  "faulty HEIGHT constant");
    class raw_scanline : public std::array<uint8_t, TOTAL_SCREEN_WIDTH>
    {}
#ifndef CACHE_NOT_REAL
      __attribute__((aligned(64)))
#endif
      ;
    typedef std::array<raw_scanline, LIVE_SCREEN_HEIGHT> raw_screen;
#ifndef CACHE_NOT_REAL
    static_assert(alignof(raw_screen) == 64,
                  "alignment for caching and SIMD is crucial here!");
#endif
    constexpr int MODE1_BACKGROUND_TILES_WIDE = 32;
    constexpr int MODE1_BACKGROUND_TILES_HIGH = 30;
    constexpr int MODE2_BACKGROUND_TILES_WIDE = 32;
    constexpr int MODE2_BACKGROUND_TILES_HIGH = 32;
    constexpr int OVERLAY_TILES_WIDE = 32;
    constexpr int OVERLAY_TILES_HIGH = 28;
    constexpr int NUM_SPRITES = 64;
    extern struct SpriteState {
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
    constexpr uint8_t* ssmBytes() {return reinterpret_cast<uint8_t*>(ssm);}
    static_assert(sizeof(SpriteState) == 4, "Sprite size has slipped");
    // HHHHHPPP
    // H = tile height-1, range 1..32 (8..128)
    typedef uint8_t SpriteAttr;
    extern SpriteAttr sam[NUM_SPRITES];
    constexpr uint8_t* samBytes() {return sam;}
    constexpr uint8_t SA_HEIGHT_SHIFT = 3;
    constexpr uint8_t SA_HEIGHT_MASK = 0x1F;
    constexpr uint8_t SA_PALETTE_SHIFT = 0;
    constexpr uint8_t SA_PALETTE_MASK = 0x07;
    extern uint8_t vram[0x10000];
    extern uint8_t cram[0x100];
    extern uint16_t vramAccessPtr;
    extern uint8_t cramAccessPtr, ssmAccessPtr, samAccessPtr;
    struct Background_Mode1 {
      uint8_t Tiles[MODE1_BACKGROUND_TILES_WIDE * MODE1_BACKGROUND_TILES_HIGH];
      uint8_t Attributes[MODE1_BACKGROUND_TILES_WIDE
                         * MODE1_BACKGROUND_TILES_HIGH / 16];
      uint8_t padding[4];
    };
    constexpr struct Background_Mode1* backgrounds_mode1() {
      return reinterpret_cast<struct Background_Mode1*>(vram);
    }
    struct Background_Mode2 {
      uint8_t Tiles[MODE2_BACKGROUND_TILES_WIDE * MODE2_BACKGROUND_TILES_HIGH];
    };
    constexpr struct Background_Mode2* backgrounds_mode2() {
      return reinterpret_cast<struct Background_Mode2*>(vram);
    }
    static_assert(sizeof(Background_Mode1)==0x400, "Background size slipped");
    static_assert(sizeof(Background_Mode2)==0x400, "Background size slipped");
    struct Overlay {
      uint8_t Tiles[OVERLAY_TILES_WIDE*OVERLAY_TILES_HIGH];
      uint8_t Attributes[OVERLAY_TILES_WIDE*OVERLAY_TILES_HIGH/8];
      uint8_t padding[16];
    };
    constexpr struct Overlay& overlay() {
      return *reinterpret_cast<struct Overlay*>(ARS::dram+sizeof(ARS::dram)
                                                -sizeof(struct Overlay));
    }
    static_assert(sizeof(Overlay) == 0x400, "Overlay size has slipped");
    void updateScanline(int new_scanline);
    void renderFrame(raw_screen& screenbuf); // also calls renderMessages
    void renderMessages(raw_screen& screenbuf); // don't explicitly call this
    void renderInvisible(); // also calls cycleMessages
    void cycleMessages(); // don't explicitly call this
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
