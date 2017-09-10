#include "display.hh"

#include "prefs.hh"
#include "config.hh"

#include <assert.h>
#include "fx.hh"

namespace {
  enum { UPSCALE_NONE=0, UPSCALE_SMOOTH=1, UPSCALE_SCANLINES_CRISP=2,
         UPSCALE_SCANLINES_BRIGHT=3, NUM_UPSCALE_MODES };
  // don't forget to copy these into the instance so we don't completely go
  // crazy when configuration is a thing
  bool television_mode, enable_overscan;
  int upscale_mode;
  const Config::Element elements[] = {
    {"television_mode", television_mode},
    {"upscale_mode", upscale_mode},
    {"enable_overscan", enable_overscan},
  };
  class DisplaySystemPrefsLogic : public PrefsLogic {
  protected:
    void Load() override {
      Config::Read("SDL Display.utxt",
                   elements, elementcount(elements));
    }
    void Save() override {
      Config::Write("SDL Display.utxt",
                    elements, elementcount(elements));
    }
    void Defaults() override {
      enable_overscan = true;
      television_mode = true;
      upscale_mode = UPSCALE_SCANLINES_BRIGHT;
    }
  } displaySystemPrefsLogic;
  class SDLDisplay : public ARS::Display {
    bool television_mode = ::television_mode;
    bool enable_overscan = ::enable_overscan;
    int upscale_mode = ::upscale_mode;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* frametexture = nullptr;
    uint8_t* pixbuf = nullptr; // TODO: alignment
    unsigned int window_width, window_height;
    // active region: size of the region we're working with, filter-wise
    unsigned int active_width, active_height;
    unsigned int active_left, active_top, active_right, active_bottom;
    // upscaled region: size of the texture we get after applying the upscale
    // filter of our choice
    unsigned int upscaled_width, upscaled_height;
    // visible region: size of the region we can SEE
    // these are relative to the upscaled region
    unsigned int visible_width, visible_height;
    unsigned int visible_left, visible_top, visible_right, visible_bottom;
    // output region: size of the region in display pixels (sort of)
    unsigned int output_width, output_height;
    bool need_pixbuf() {
      return television_mode;
    }
  public:
    SDLDisplay(const std::string& window_title) {
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                  upscale_mode == UPSCALE_NONE ? "nearest" : "linear");
      if(enable_overscan) {
        visible_width = ARS::PPU::CONVENIENT_OVERSCAN_WIDTH;
        visible_height = ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT;
      }
      else {
        visible_width = ARS::PPU::CONVENIENT_OVERSCAN_WIDTH * ARS::PPU::LIVE_SCREEN_HEIGHT / ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT;
        visible_height = ARS::PPU::LIVE_SCREEN_HEIGHT;
      }
      visible_left = (ARS::PPU::TOTAL_SCREEN_WIDTH-visible_width)/2;
      visible_right = visible_left + visible_width;
      visible_top = (ARS::PPU::TOTAL_SCREEN_HEIGHT-visible_height)/2;
      visible_bottom = visible_top + visible_height;
      active_left = visible_left&~7;
      active_right = (visible_right&7) ? (visible_right&~7)+8 : visible_right;
      active_top = visible_top;
      active_bottom = visible_bottom;
      if(television_mode) {
        // we need to include about 8 more pixels on either side, to avoid
        // artifacts when a background color is in effect
        active_left -= 8;
        active_right += 8;
      }
      active_width = active_right - active_left;
      active_height = active_bottom - active_top;
      visible_left -= active_left;
      visible_right -= active_left;
      visible_top -= active_top;
      visible_bottom -= active_top;
      window_width = visible_width * 2;
      window_height = visible_height * 2;
      output_width = visible_width;
      output_height = visible_height;
      if(television_mode) {
        upscaled_width = active_width * 2;
        visible_left *= 2;
        visible_right *= 2;
        visible_width *= 2;
      }
      else
        upscaled_width = active_width;
      switch(upscale_mode) {
      case UPSCALE_NONE:
      case UPSCALE_SMOOTH:
        upscaled_height = active_height;
        break;
      case UPSCALE_SCANLINES_CRISP:
      case UPSCALE_SCANLINES_BRIGHT:
        upscaled_height = active_height * 2;
        visible_top *= 2;
        visible_bottom *= 2;
        visible_height *= 2;
        break;
      }
      if(need_pixbuf()) {
        pixbuf = new uint8_t[active_width * active_height * 4];
      }
      window = SDL_CreateWindow(window_title.c_str(),
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                window_width, window_height,
                                SDL_WINDOW_RESIZABLE);
      if(window == NULL) throw sn.Get("WINDOW_FAIL"_Key,
                                      {SDL_GetError()});
      renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
      if(renderer == NULL) throw sn.Get("RENDERER_FAIL"_Key,
                                        {SDL_GetError()});
      // TODO: do aspect ratio handling ourselves so we can overscan properly
      SDL_RenderSetLogicalSize(renderer, output_width, output_height);
      frametexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       upscaled_width, upscaled_height);
      if(frametexture == NULL) throw sn.Get("FRAMETEXTURE_FAIL"_Key,
                                            {SDL_GetError()});
    }
    ~SDLDisplay() {
      if(pixbuf != nullptr) {
        delete[] pixbuf;
        pixbuf = nullptr;
      }
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
      bool do_scanlines = upscale_mode >= UPSCALE_SCANLINES_CRISP;
      uint8_t* pixels;
      int pitch;
      SDL_LockTexture(frametexture, nullptr,
                      reinterpret_cast<void**>(&pixels), &pitch);
      assert(static_cast<unsigned int>(pitch) == upscaled_width * 4);
      FX::raw_screen_to_bgra(src, pixbuf == nullptr ? pixels : pixbuf,
                             active_left, active_top,
                             active_right, active_bottom,
                             do_scanlines && !television_mode);
      if(television_mode) {
        FX::televise_bgra(pixbuf, pixels,
                          active_width, active_height,
                          do_scanlines);
      }
      switch(upscale_mode) {
      case UPSCALE_NONE:
      case UPSCALE_SMOOTH:
        break;
      case UPSCALE_SCANLINES_CRISP:
        FX::scanline_crisp(pixels, upscaled_width, active_height);
        break;
      case UPSCALE_SCANLINES_BRIGHT:
        FX::scanline_bright(pixels, upscaled_width, active_height);
        break;
      }
      SDL_UnlockTexture(frametexture);
      SDL_RenderClear(renderer);
      SDL_Rect srcrect;
      srcrect.x = visible_left;
      srcrect.y = visible_top;
      srcrect.w = visible_width;
      srcrect.h = visible_height;
      SDL_Rect dstrect;
      dstrect.x = 0;
      dstrect.y = 0;
      dstrect.w = output_width;
      dstrect.h = output_height;
      SDL_RenderCopy(renderer, frametexture, &srcrect, &dstrect);
      SDL_RenderPresent(renderer);
    }
  };
  ARS::DisplayDescriptor
  _("sdl", "DISPLAY_NAME_SDL"_Key, 100,
    [](const std::string& window_title) {
      return std::make_unique<SDLDisplay>(window_title);
    }, nullptr);
}
