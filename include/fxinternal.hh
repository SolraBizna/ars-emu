#ifndef FXINTERNALHH
#define FXINTERNALHH

#include "fx.hh"

#include <ctime>
#include <cmath>

#define restrict __restrict__

namespace FX {
  // TODO: support big-endian better
  extern const uint32_t hardwarePalette[256];
  class Linearize {
    uint16_t linearized[256];
    Linearize(const Linearize&) = delete;
  public:
    Linearize() {
      for(int n = 0; n < 256; ++n) {
        linearized[n] = static_cast<uint16_t>(std::floor(std::pow(n / 255.0, 2.2) * 65535 + 0.5));
      }
    }
    uint16_t operator[](uint8_t in) const { return linearized[in]; }
  };
  const Linearize& linearizer();
  class Delinearize {
    uint8_t delinearized[65536];
    Delinearize(const Delinearize&) = delete;
  public:
    Delinearize() {
      for(int n = 0; n < 65536; ++n) {
        delinearized[n] = static_cast<uint16_t>(std::floor(std::pow(n / 65535.0, 1 / 2.2) * 255 + 0.5));
      }
    }
    uint8_t operator[](uint16_t in) const { return delinearized[in]; }
  };
  const Delinearize& delinearizer();
  // TODO: generate filters at runtime, allowing configurable HSB, and maybe
  // even alternative upscaling factors!
  constexpr int NUM_CHROMA_PHASES = 12;
  constexpr int NUM_TV_SUBPIXELS = 2;
  constexpr int COMPOSITE_FILTER_RADIUS = 6;
  constexpr int SVIDEO_FILTER_RADIUS = 4;
  extern const int32_t COMPOSITE_FIR[NUM_CHROMA_PHASES][NUM_TV_SUBPIXELS][(COMPOSITE_FILTER_RADIUS*2+1)*9];
  extern const int32_t SVIDEO_FIR[NUM_CHROMA_PHASES][NUM_TV_SUBPIXELS][(SVIDEO_FILTER_RADIUS*2+1)*9];
  constexpr uint32_t pack_pixel(int32_t red, int32_t green, int32_t blue) {
    if(red > 16777215) red = 255;
    else if(red < 0) red = 0;
    else red = uint32_t(red)>>16;
    if(green > 16777215) green = 255;
    else if(green < 0) green = 0;
    else green = uint32_t(green)>>16;
    if(blue > 16777215) blue = 255;
    else if(blue < 0) blue = 0;
    else blue = uint32_t(blue)>>16;
    return (red << 16) | (green << 8) | blue;
  }
  // type names for implementations of the FX go here
  namespace Proto {
    typedef void(*raw_screen_to_bgra)(const ARS::PPU::raw_screen& in,
                                      void* _out,
                                      unsigned int left, unsigned int top,
                                      unsigned int right, unsigned int bot,
                                      bool output_skips_rows);
    typedef raw_screen_to_bgra raw_screen_to_bgra_2x;
    typedef void(*composite_bgra)(const void* in, void* out,
                                  unsigned int width, unsigned int height,
                                  bool output_skips_rows);
    typedef composite_bgra svideo_bgra;
    typedef void(*scanline_crisp_bgra)(void* _buf,
                                       unsigned int width,
                                       unsigned int height);
    typedef scanline_crisp_bgra scanline_bright_bgra;
  }
  namespace Imp {
    template <class T> class imps {
      std::vector<T> candidates;
      std::clock_t fastest_time;
      imps(const imps<T>&) = delete;
      imps(imps<T>&&) = delete;
      imps& operator=(const imps<T>&) = delete;
      imps& operator=(imps<T>&&) = delete;
    public:
      imps() {}
      void add(T candidate) { candidates.emplace_back(candidate); }
      T best(std::function<void(T)> tester) {
        T best = nullptr;
        std::clock_t start, stop, fastest_time = 0;
        for(auto candidate : candidates) {
          tester(candidate);
          start = std::clock();
          for(unsigned int n = 0; n < 3; ++n) tester(candidate);
          stop = std::clock();
          std::clock_t current_time = stop - start;
          if(fastest_time == 0 || current_time < fastest_time) {
            best = candidate;
            fastest_time = current_time;
          }
        }
        this->fastest_time = fastest_time;
        return best;
      }
    };
    template<class T> struct lementation {
    public:
      lementation(T candidate, imps<T>& imps) { imps.add(candidate); }
    };
#define DEEPER_LEMENTATION_NAME(y) _lementation_##y
#define LEMENTATION_NAME(y) DEEPER_LEMENTATION_NAME(y)
#define IMPLEMENT(target) namespace { FX::Imp::lementation<FX::Proto::target> LEMENTATION_NAME(__LINE__)(target, FX::Imp::target()); }
#define MAKE_IMPS(x) imps<Proto::x>& x()
    MAKE_IMPS(raw_screen_to_bgra);
    MAKE_IMPS(raw_screen_to_bgra_2x);
    MAKE_IMPS(composite_bgra);
    MAKE_IMPS(svideo_bgra);
    MAKE_IMPS(scanline_crisp_bgra);
    MAKE_IMPS(scanline_bright_bgra);
#undef MAKE_IMPS
  }
}

#endif
