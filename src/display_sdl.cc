#include "display.hh"

#include "prefs.hh"
#include "config.hh"

#include <assert.h>
#include "upscale.hh"

namespace {
  template<class T> constexpr T clamp(T value, T min, T max) {
    if(value < min) return min;
    else if(value > max) return max;
    else return value;
  }
  // don't forget to copy these into the instance so we don't completely go
  // crazy when configuration is a thing
  bool enable_overscan;
  int signal_type;
  int upscale_type;
  const Config::Element elements[] = {
    {"signal_type", *reinterpret_cast<int*>(&signal_type)},
    {"upscale_type", *reinterpret_cast<int*>(&upscale_type)},
    {"enable_overscan", enable_overscan},
  };
  class DisplaySystemPrefsLogic : public PrefsLogic {
  protected:
    void Load() override {
      Config::Read("SDL Display.utxt",
                   elements, elementcount(elements));
      signal_type = clamp(signal_type, 0, MAX_SIGNAL_TYPE);
      upscale_type = clamp(upscale_type, 0, MAX_UPSCALE_TYPE);
    }
    void Save() override {
      Config::Write("SDL Display.utxt",
                    elements, elementcount(elements));
    }
    void Defaults() override {
      enable_overscan = true;
      signal_type = static_cast<int>(SignalType::RGB);
      upscale_type = static_cast<int>(UpscaleType::SCANLINES_BRIGHT);
    }
  } displaySystemPrefsLogic;
  class SDLDisplay : public ARS::Display {
    Upscaler upscaler;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* frametexture = nullptr;
    // size of the region we care about
    unsigned int visible_width, visible_height;
    // upscaled region: size of the texture we get after applying the upscaler
    unsigned int upscaled_width, upscaled_height;
    // output region: area of the upscaled texture we want to see
    unsigned int output_left, output_top, output_right, output_bottom;
  public:
    SDLDisplay(const std::string& window_title) {
      if(enable_overscan) {
        visible_width = ARS::PPU::CONVENIENT_OVERSCAN_WIDTH;
        visible_height = ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT;
      }
      else {
        visible_width = ARS::PPU::CONVENIENT_OVERSCAN_WIDTH * ARS::PPU::LIVE_SCREEN_HEIGHT / ARS::PPU::CONVENIENT_OVERSCAN_HEIGHT;
        visible_height = ARS::PPU::LIVE_SCREEN_HEIGHT;
      }
      unsigned int visible_left, visible_top, visible_right, visible_bottom;
      visible_left = (ARS::PPU::TOTAL_SCREEN_WIDTH-visible_width)/2;
      visible_right = visible_left + visible_width;
      visible_top = (ARS::PPU::TOTAL_SCREEN_HEIGHT-visible_height)/2;
      visible_bottom = visible_top + visible_height;
      upscaler = Upscaler(static_cast<SignalType>(signal_type),
                          static_cast<UpscaleType>(upscale_type),
                          visible_left, visible_top,
                          visible_right, visible_bottom,
                          upscaled_width, upscaled_height,
                          output_left, output_top,
                          output_right, output_bottom);
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                  upscaler.shouldSmoothResult() ? "linear" : "nearest");
      window = SDL_CreateWindow(window_title.c_str(),
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                visible_width, visible_height,
                                SDL_WINDOW_RESIZABLE);
      if(window == NULL) throw sn.Get("WINDOW_FAIL"_Key,
                                      {SDL_GetError()});
      renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
      if(renderer == NULL) throw sn.Get("RENDERER_FAIL"_Key,
                                        {SDL_GetError()});
      // TODO: do aspect ratio handling ourselves so we can overscan properly
      SDL_RenderSetLogicalSize(renderer, visible_width, visible_height);
      frametexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       upscaled_width, upscaled_height);
      if(frametexture == NULL) throw sn.Get("FRAMETEXTURE_FAIL"_Key,
                                            {SDL_GetError()});
    }
    ~SDLDisplay() {
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
      assert(static_cast<unsigned int>(pitch) == upscaled_width * 4);
      upscaler.apply(src, pixels);
      SDL_UnlockTexture(frametexture);
      SDL_RenderClear(renderer);
      SDL_Rect srcrect;
      srcrect.x = output_left;
      srcrect.y = output_top;
      srcrect.w = output_right - output_left;
      srcrect.h = output_bottom - output_top;
      SDL_Rect dstrect;
      dstrect.x = 0;
      dstrect.y = 0;
      dstrect.w = visible_width;
      dstrect.h = visible_height;
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
