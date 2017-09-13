#include "display.hh"

#include "prefs.hh"
#include "config.hh"

#include <assert.h>
#include "upscale.hh"
#include "menu.hh"

namespace {
  template<class T> constexpr T clamp(T value, T min, T max) {
    if(value < min) return min;
    else if(value > max) return max;
    else return value;
  }
  bool we_are_active = false;
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
    // dstrect: the main output rectangle
    // left/rightrect: repeat the left/right column of the screen
    // top/botrect: black out
    SDL_Rect dstrect, leftrect, rightrect, toprect, botrect;
  public:
    SDLDisplay() {
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
      unsigned int display_width, display_height;
      SDL_DisplayMode mode;
      if(SDL_GetDesktopDisplayMode(0, &mode) != 0) {
        mode.w = 640;
        mode.h = 480;
        mode.refresh_rate = 60;
      }
#ifdef EMSCRIPTEN
      display_width = mode.w;
      display_height = mode.h;
#else
      unsigned int chosen_upsize_factor = 1;
      for(unsigned int upsize_factor = 2; upsize_factor <= 10;++upsize_factor){
        if(visible_width * upsize_factor <= static_cast<unsigned int>(mode.w)
           && visible_height*upsize_factor<=static_cast<unsigned int>(mode.h)){
          chosen_upsize_factor = upsize_factor;
        }
      }
      display_width = visible_width * chosen_upsize_factor;
      display_height = visible_height * chosen_upsize_factor;
#endif
      window = SDL_CreateWindow(ARS::window_title.c_str(),
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                display_width, display_height,
                                SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
      if(window == NULL) throw sn.Get("WINDOW_FAIL"_Key,
                                      {SDL_GetError()});
      renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
      if(renderer == NULL) throw sn.Get("RENDERER_FAIL"_Key,
                                        {SDL_GetError()});
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      frametexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       upscaled_width, upscaled_height);
      if(frametexture == NULL) throw sn.Get("FRAMETEXTURE_FAIL"_Key,
                                            {SDL_GetError()});
      resize();
      we_are_active = true;
    }
    ~SDLDisplay() {
      we_are_active = false;
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
      SDL_RenderCopy(renderer, frametexture, &srcrect, &dstrect);
      if(leftrect.w != 0) {
        // redundant assignments are commented out
        //srcrect.x = output_left;
        //srcrect.y = output_top;
        srcrect.w = 1;
        //srcrect.h = output_bottom - output.top;
        SDL_RenderCopy(renderer, frametexture, &srcrect, &leftrect);
      }
      if(rightrect.w != 0) {
        srcrect.x = output_right-1;
        //srcrect.y = output_top;
        srcrect.w = 1;
        //srcrect.h = output_bottom - output.top;
        SDL_RenderCopy(renderer, frametexture, &srcrect, &rightrect);
      }
      if(toprect.h != 0) {
        SDL_RenderFillRect(renderer, &toprect);
      }
      if(botrect.h != 0) {
        SDL_RenderFillRect(renderer, &botrect);
      }
      SDL_RenderPresent(renderer);
    }
    void resize() {
      int snew_w, snew_h;
      if(SDL_GetRendererOutputSize(renderer, &snew_w, &snew_h) != 0) {
        die("INTERNAL ERROR! SDL_GetRendererOutputSize failure!");
      }
      unsigned int new_w = snew_w <= 1 ? 1 : snew_w;
      unsigned int new_h = snew_h <= 1 ? 1 : snew_h;
      SDL_RenderSetLogicalSize(renderer, new_w, new_h);
      if(new_h * visible_width / visible_height > new_w) {
        /* size to fit the width */
        dstrect.x = 0;
        dstrect.w = new_w;
        dstrect.h = new_w * visible_height / visible_width;
        assert(static_cast<unsigned int>(dstrect.h) < new_h);
        dstrect.y = (new_h - dstrect.h) / 2;
        leftrect.w = 0;
        rightrect.h = 0;
        toprect.x = 0;
        toprect.y = 0;
        toprect.w = new_w;
        toprect.h = dstrect.y;
        botrect.x = 0;
        botrect.y = new_h + dstrect.h;
        botrect.w = new_w;
        botrect.h = new_h - toprect.y;
      }
      else {
        /* size to fit the height */
        dstrect.y = 0;
        dstrect.h = new_h;
        dstrect.w = new_h * visible_width / visible_height;
        dstrect.x = (new_w - dstrect.w) / 2;
        leftrect.x = 0;
        leftrect.y = 0;
        leftrect.w = dstrect.x;
        leftrect.h = new_h;
        rightrect.x = dstrect.x + dstrect.w;
        rightrect.y = 0;
        rightrect.w = new_w - rightrect.x;
        rightrect.h = new_h;
        toprect.h = 0;
        botrect.h = 0;
      }
    }
    bool filterEvent(SDL_Event& evt) override {
      switch(evt.type) {
      case SDL_WINDOWEVENT:
        switch(evt.window.event) {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
          resize();
          return true;
        }
        break;
      }
      return false;
    }
  };
  ARS::DisplayDescriptor
  _("sdl", "DISPLAY_NAME_SDL"_Key, 100,
    []() {
      return std::make_unique<SDLDisplay>();
    },
    []() {
      std::vector<std::shared_ptr<Menu::Item> > items;
      std::vector<std::string> signal_types;
      signal_types.reserve(MAX_SIGNAL_TYPE+1);
      for(auto& key : Upscaler::SIGNAL_TYPE_KEYS) {
        signal_types.emplace_back(sn.Get(key));
      }
      items.emplace_back(new Menu::Selector(sn.Get(Upscaler::SIGNAL_TYPE_SELECTOR),
                                            signal_types,
                                            signal_type,
                                            [](size_t nu) {
                                              signal_type = nu;
                                              if(we_are_active) {
                                                ARS::display.reset();
                                                ARS::display = _.constructor();
                                              }
                                            }));
      std::vector<std::string> upscale_types;
      upscale_types.reserve(MAX_UPSCALE_TYPE+1);
      for(auto& key : Upscaler::UPSCALE_TYPE_KEYS) {
        upscale_types.emplace_back(sn.Get(key));
      }
      items.emplace_back(new Menu::Selector(sn.Get(Upscaler::UPSCALE_TYPE_SELECTOR),
                                            upscale_types,
                                            upscale_type,
                                            [](size_t nu) {
                                              upscale_type = nu;
                                              if(we_are_active) {
                                                ARS::display.reset();
                                                ARS::display = _.constructor();
                                              }
                                            }));
      items.emplace_back(new Menu::Selector(sn.Get("OVERSCAN_TYPE"_Key),
                                            {sn.Get("OVERSCAN_DISABLED"_Key),
                                              sn.Get("OVERSCAN_ENABLED"_Key)},
                                            enable_overscan,
                                            [](size_t nu) {
                                              enable_overscan = !!nu;
                                              if(we_are_active) {
                                                ARS::display.reset();
                                                ARS::display = _.constructor();
                                              }
                                            }));
      items.emplace_back(new Menu::Divider());
      items.emplace_back(new Menu::BackButton(sn.Get("GENERIC_MENU_FINISHED_LABEL"_Key)));
      return std::make_shared<Menu>(sn.Get("SDL_VIDEO_MENU_TITLE"_Key), items);
    });
}
