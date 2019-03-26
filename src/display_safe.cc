#include "display.hh"

#include <assert.h>
#include "fx.hh"

namespace {
  class SafeModeDisplay : public ARS::Display {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* frametexture = nullptr;
    static constexpr int VISIBLE_WIDTH = ARS::PPU::LIVE_SCREEN_WIDTH;
    static constexpr int VISIBLE_HEIGHT = ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT;
    static constexpr int WINDOW_WIDTH = VISIBLE_WIDTH*2;
    static constexpr int WINDOW_HEIGHT = VISIBLE_HEIGHT*2;
    static constexpr int VISIBLE_LEFT = (ARS::PPU::TOTAL_SCREEN_WIDTH
                                  - VISIBLE_WIDTH) / 2;
    static constexpr int VISIBLE_TOP = (ARS::PPU::TOTAL_SCREEN_HEIGHT
                                 - VISIBLE_HEIGHT) / 2;
    static constexpr int VISIBLE_RIGHT = VISIBLE_LEFT + VISIBLE_WIDTH;
    static constexpr int VISIBLE_BOTTOM = VISIBLE_TOP + VISIBLE_HEIGHT;
  public:
    SafeModeDisplay() {
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
      window = SDL_CreateWindow(ARS::window_title.c_str(),
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                WINDOW_WIDTH, WINDOW_HEIGHT,
                                SDL_WINDOW_RESIZABLE);
      if(window == NULL) throw sn.Get("WINDOW_FAIL"_Key,
                                      {SDL_GetError()});
      renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
      if(renderer == NULL) throw sn.Get("RENDERER_FAIL"_Key,
                                        {SDL_GetError()});
      SDL_RenderSetLogicalSize(renderer, VISIBLE_WIDTH, VISIBLE_HEIGHT);
      frametexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       VISIBLE_WIDTH, VISIBLE_HEIGHT);
      if(frametexture == NULL) throw sn.Get("FRAMETEXTURE_FAIL"_Key,
                                            {SDL_GetError()});
    }
    ~SafeModeDisplay() {
      if(frametexture != nullptr) {
        SDL_DestroyTexture(frametexture);
        frametexture = nullptr;
      }
      if(renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
      }
      if(window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
      }
    }
    SDL_Window* getWindow() const override {
      return window;
    }
    void update(const ARS::PPU::raw_screen& src) override {
      uint8_t* pixels;
      int pitch;
      SDL_LockTexture(frametexture, nullptr,
                      reinterpret_cast<void**>(&pixels), &pitch);
      assert(pitch == VISIBLE_WIDTH * 4);
      FX::raw_screen_to_bgra(src, pixels,
                             VISIBLE_LEFT, VISIBLE_TOP,
                             VISIBLE_RIGHT, VISIBLE_BOTTOM);
      SDL_UnlockTexture(frametexture);
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, frametexture, nullptr, nullptr);
      SDL_RenderPresent(renderer);
    }
    bool windowSpaceToVirtualScreenSpace(int& x, int& y) override {
      bool ret = x >= 0 && x < VISIBLE_WIDTH && y >= 0 && y < VISIBLE_HEIGHT;
      x += VISIBLE_LEFT - ARS::PPU::LIVE_SCREEN_LEFT;
      y += VISIBLE_TOP - ARS::PPU::LIVE_SCREEN_TOP;
      return ret;
    }
  };
  ARS::DisplayDescriptor
  _("safe", "DISPLAY_NAME_SAFE"_Key, 0,
    []() {
      return std::make_unique<SafeModeDisplay>();
    }, nullptr);
}

std::unique_ptr<ARS::Display> ARS::Display::makeSafeModeDisplay() {
  try {
    return std::make_unique<SafeModeDisplay>();
  }
  catch(std::string e) {
    std::cerr << e << "\n";
    die("%s",sn.Get("TOTAL_DISPLAY_FAILURE"_Key).c_str());
  }
}
