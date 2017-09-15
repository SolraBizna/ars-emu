#include "upscale.hh"
#include "fx.hh"

Upscaler::Upscaler(SignalType signal_type, UpscaleType upscale_type,
                   unsigned int visible_left, unsigned int visible_top,
                   unsigned int visible_right, unsigned int visible_bottom,
                   unsigned int& upscaled_width, unsigned int& upscaled_height,
                   unsigned int& output_left, unsigned int& output_top,
                   unsigned int& output_right, unsigned int& output_bottom)
  : signal_type(signal_type), upscale_type(upscale_type), interbuf(nullptr) {
  active_left = visible_left;
  active_top = visible_top;
  active_right = visible_right;
  active_bottom = visible_bottom;
  if(active_left&7) active_left = active_left&~7;
  if(active_right&7) active_right += 8-(active_right&7);
  if(signal_type != SignalType::RGB) {
    // add 8 pixels on either side, in case a background color is in effect
    if(active_left >= 8) active_left -= 8;
    if(active_right <= ARS::PPU::TOTAL_SCREEN_WIDTH-8) active_right += 8;
  }
  unsigned int active_width = active_right - active_left;
  unsigned int active_height = active_bottom - active_top;
  unsigned int upscale_factor_x = 1, upscale_factor_y = 1;
  switch(signal_type) {
  default:
  case SignalType::RGB:
    // nothing to do
    if(upscale_type != UpscaleType::NONE)
      upscale_factor_x = std::max(upscale_factor_x, 2U);
    break;
  case SignalType::SVIDEO:
  case SignalType::COMPOSITE:
    upscale_factor_x = std::max(upscale_factor_x, 2U);
    break;
  }
  switch(upscale_type) {
  default:
  case UpscaleType::NONE:
    // nothing to do
    break;
  case UpscaleType::SCANLINES_CRISP:
  case UpscaleType::SCANLINES_BRIGHT:
    upscale_factor_y = std::max(upscale_factor_y, 2U);
  case UpscaleType::SMOOTH:
    upscale_factor_x = std::max(upscale_factor_x, 2U);
    break;
  }
  upscaled_width = active_width * upscale_factor_x;
  upscaled_height = active_width * upscale_factor_y;
  output_left = (visible_left - active_left) * upscale_factor_x;
  output_top = (visible_top - active_top) * upscale_factor_y;
  output_right = (visible_right - active_left) * upscale_factor_x;
  output_bottom = (visible_bottom - active_top) * upscale_factor_y;
  if(signal_type != SignalType::RGB) {
    interbuf = reinterpret_cast<uint8_t*>
      (safe_calloc(active_width,(active_height+(upscale_type>=UpscaleType::SCANLINES_CRISP?1:0))*sizeof(uint32_t)
#ifndef SIMD_NOT_REAL
                   +15
#endif
                   ));
#ifndef SIMD_NOT_REAL
    interstart = align_to(interbuf, 16);
#endif
  }
}
void Upscaler::apply(const ARS::PPU::raw_screen& in, void* out) {
  if(signal_type == SignalType::RGB && upscale_type >= UpscaleType::SMOOTH)
    FX::raw_screen_to_bgra_2x(in, interbuf == nullptr ? out : interbuf,
                              active_left, active_top,
                              active_right, active_bottom,
                              upscale_type >= UpscaleType::SCANLINES_CRISP
                              && signal_type == SignalType::RGB);
  else
    FX::raw_screen_to_bgra(in, interbuf == nullptr ? out : interbuf,
                           active_left, active_top,
                           active_right, active_bottom,
                           upscale_type >= UpscaleType::SCANLINES_CRISP
                           && signal_type == SignalType::RGB);
  switch(signal_type) {
  case SignalType::RGB:
    break;
  case SignalType::SVIDEO:
    FX::svideo_bgra(interbuf, out,
                    active_right-active_left, active_bottom-active_top,
                    upscale_type >= UpscaleType::SCANLINES_CRISP);
    break;
  case SignalType::COMPOSITE:
    FX::composite_bgra(interbuf, out,
                       active_right-active_left, active_bottom-active_top,
                       upscale_type >= UpscaleType::SCANLINES_CRISP);
    break;
  }
  switch(upscale_type) {
  case UpscaleType::NONE:
  case UpscaleType::SMOOTH:
    break;
  case UpscaleType::SCANLINES_CRISP:
    FX::scanline_crisp_bgra(out, (active_right-active_left)*2,
                            active_bottom-active_top);
    break;
  case UpscaleType::SCANLINES_BRIGHT:
    FX::scanline_bright_bgra(out, (active_right-active_left)*2,
                             active_bottom-active_top);
    break;
  }
}
Upscaler::~Upscaler() {
  if(interbuf != nullptr) {
    safe_free(interbuf);
    interbuf = nullptr;
  }
}

const SN::ConstKey Upscaler::SIGNAL_TYPE_SELECTOR = "VIDEO_SIGNAL_TYPE"_Key;
const std::array<SN::ConstKey, MAX_SIGNAL_TYPE+1> Upscaler::SIGNAL_TYPE_KEYS{
  "VIDEO_SIGNAL_TYPE_COMPONENT"_Key,
  "VIDEO_SIGNAL_TYPE_SVIDEO"_Key,
  "VIDEO_SIGNAL_TYPE_COMPOSITE"_Key,
};

const SN::ConstKey Upscaler::UPSCALE_TYPE_SELECTOR = "VIDEO_UPSCALE_TYPE"_Key;
const std::array<SN::ConstKey, MAX_UPSCALE_TYPE+1> Upscaler::UPSCALE_TYPE_KEYS{
  "VIDEO_UPSCALE_TYPE_NONE"_Key,
  "VIDEO_UPSCALE_TYPE_SMOOTH"_Key,
  "VIDEO_UPSCALE_TYPE_SCANLINES_CRISP"_Key,
  "VIDEO_UPSCALE_TYPE_SCANLINES_BRIGHT"_Key,
};
