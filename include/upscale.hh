#ifndef UPSCALE_HH
#define UPSCALE_HH

#include "ppu.hh"
#include "teg.hh"

enum class SignalType : int { RGB=0, SVIDEO, COMPOSITE };
enum class UpscaleType : int { NONE=0, SMOOTH,
    SCANLINES_CRISP, SCANLINES_BRIGHT };
constexpr int MAX_SIGNAL_TYPE = 2;
constexpr int MAX_UPSCALE_TYPE = 3;
class Upscaler {
  SignalType signal_type;
  UpscaleType upscale_type;
  uint8_t* interbuf;
  unsigned int active_left, active_top, active_right, active_bottom;
#ifndef SIMD_NOT_REAL
  uint8_t* interstart;
  uint8_t* getInterstart() const { return interstart; }
  uint8_t* align_to(uint8_t* in, size_t align) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(in);
    if(addr&(align-1)) addr += align-(addr&(align-1));
    return reinterpret_cast<uint8_t*>(addr);
  }
#else
  uint8_t* getInterstart() const { return interbuf; }
#endif
  Upscaler(const Upscaler&) = delete;
  Upscaler& operator=(const Upscaler&) = delete;
public:
  Upscaler() : interbuf(nullptr) {} // uninitialized!
  /*

    VISIBLE region: The region of the input raw_screen that you want to
    display. It does not have to be 8-aligned, we will take care of all of
    that.

    UPSCALED size: The size of the buffer that will be written into. This will
    have been altered to ensure 8-alignment, in addition to being expanded on
    one or more axes.

    OUTPUT region: The region within the UPSCALED buffer that corresponds to
    the region you want to display.

   */
  Upscaler(SignalType signal_type, UpscaleType upscale_type,
           unsigned int visible_left, unsigned int visible_top,
           unsigned int visible_right, unsigned int visible_bottom,
           unsigned int& upscaled_width, unsigned int& upscaled_height,
           unsigned int& output_left, unsigned int& output_top,
           unsigned int& output_right, unsigned int& output_bottom);
  void apply(const ARS::PPU::raw_screen& in, void* out);
  bool shouldSmoothResult() const { return upscale_type != UpscaleType::NONE; }
  ~Upscaler();
  Upscaler(Upscaler&& other) : interbuf(nullptr) {
    *this = std::move(other);
  }
  Upscaler& operator=(Upscaler&& other) {
    if(interbuf != nullptr) safe_free(interbuf);
    interbuf = other.interbuf;
#ifndef SIMD_NOT_REAL
    interstart = align_to(interbuf, 16);
#endif
    other.interbuf = nullptr;
    signal_type = other.signal_type;
    upscale_type = other.upscale_type;
    active_left = other.active_left;
    active_top = other.active_top;
    active_right = other.active_right;
    active_bottom = other.active_bottom;
    return *this;
  }
  static const SN::ConstKey SIGNAL_TYPE_SELECTOR;
  static const std::array<SN::ConstKey, MAX_SIGNAL_TYPE+1> SIGNAL_TYPE_KEYS;
  static const SN::ConstKey UPSCALE_TYPE_SELECTOR;
  static const std::array<SN::ConstKey, MAX_UPSCALE_TYPE+1> UPSCALE_TYPE_KEYS;
};

#endif
