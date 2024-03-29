#include "optimize-this-file.hh"
#include "fxinternal.hh"

#include <assert.h>

using namespace FX;

namespace {
  void raw_screen_to_bgra(const ARS::PPU::raw_screen& in,
                          void* _out,
                          unsigned int left, unsigned int top,
                          unsigned int right, unsigned int bot,
                          bool output_skips_rows) {
    assert(left%8 == 0);
    assert(right%8 == 0);
    uint32_t* out = reinterpret_cast<uint32_t*>(_out);
    for(unsigned int y = top; y < bot; ++y) {
      auto& in_row = in[y];
      for(unsigned int x = left; x < right; x += 8) {
        for(unsigned int i = 0; i < 8; ++i)
          out[i] = hardwarePalette[in_row[x+i]];
        out += 8;
      }
      if(output_skips_rows) out += (right-left);
    }
  }
  IMPLEMENT(raw_screen_to_bgra);
  void raw_screen_to_bgra_2x(const ARS::PPU::raw_screen& in,
                             void* _out,
                             unsigned int left, unsigned int top,
                             unsigned int right, unsigned int bot,
                             bool output_skips_rows) {
    assert(left%8 == 0);
    assert(right%8 == 0);
    uint32_t* out = reinterpret_cast<uint32_t*>(_out);
    for(unsigned int y = top; y < bot; ++y) {
      auto& in_row = in[y];
      for(unsigned int x = left; x < right; x += 8) {
        for(unsigned int i = 0; i < 8; ++i)
          out[i*2+1] = out[i*2] = hardwarePalette[in_row[x+i]];
        out += 16;
      }
      if(output_skips_rows) out += (right-left)*2;
    }
  }
  IMPLEMENT(raw_screen_to_bgra_2x);
  void composite_bgra(const void* restrict _in, void* restrict _out,
                      unsigned int width, unsigned int height,
                      bool output_skips_rows) {
    assert(width%8 == 0);
    const uint32_t* restrict in_row = reinterpret_cast<const uint32_t*>(_in);
    uint32_t* restrict out = reinterpret_cast<uint32_t*>(_out);
    for(unsigned int y = 0; y < height; ++y) {
      unsigned int start_phase = (ARS::HARD_BLANK_CYCLES_PER_SCANLINE / 2);
      if(y & 1) start_phase += NUM_CHROMA_PHASES/2;
      start_phase %= NUM_CHROMA_PHASES;
      int start_x = -COMPOSITE_FILTER_RADIUS;
      int end_x = COMPOSITE_FILTER_RADIUS;
      for(int center_x = 0; center_x < static_cast<int>(width); ++center_x) {
        for(unsigned int subpixel = 0; subpixel < 2; ++subpixel) {
          unsigned int phase = start_phase;
          const int32_t* restrict filter_ptr = COMPOSITE_FIR[phase][subpixel];
          if(start_x < 0)
            filter_ptr += start_x * -9;
          int32_t summed_red = 0, summed_green = 0, summed_blue = 0;
          for(int sub_x = start_x < 0 ? 0 : start_x; sub_x <= end_x; ++sub_x) {
            uint32_t pix = in_row[sub_x];
            auto in_r = (pix >> 16) & 255;
            auto in_g = (pix >> 8) & 255;
            auto in_b = pix & 255;
            summed_red += in_r * filter_ptr[0];
            summed_green += in_r * filter_ptr[1];
            summed_blue += in_r * filter_ptr[2];
            summed_red += in_g * filter_ptr[3];
            summed_green += in_g * filter_ptr[4];
            summed_blue += in_g * filter_ptr[5];
            summed_red += in_b * filter_ptr[6];
            summed_green += in_b * filter_ptr[7];
            summed_blue += in_b * filter_ptr[8];
            filter_ptr += 9;
          }
          *out++ = pack_pixel(summed_red, summed_green, summed_blue);
        }
        start_phase = (start_phase + 1) % NUM_CHROMA_PHASES;
        ++start_x;
        ++end_x;
      }
      if(output_skips_rows) out += width * 2;
      in_row += width;
    }
  }
  IMPLEMENT(composite_bgra);
  void svideo_bgra(const void* restrict _in, void* restrict _out,
                   unsigned int width, unsigned int height,
                   bool output_skips_rows) {
    assert(width%8 == 0);
    const uint32_t* restrict in_row = reinterpret_cast<const uint32_t*>(_in);
    uint32_t* restrict out = reinterpret_cast<uint32_t*>(_out);
    for(unsigned int y = 0; y < height; ++y) {
      unsigned int start_phase = (ARS::HARD_BLANK_CYCLES_PER_SCANLINE / 2);
      if(y & 1) start_phase += NUM_CHROMA_PHASES/2;
      start_phase %= NUM_CHROMA_PHASES;
      int start_x = -SVIDEO_FILTER_RADIUS;
      int end_x = SVIDEO_FILTER_RADIUS;
      for(int center_x = 0; center_x < static_cast<int>(width); ++center_x) {
        for(unsigned int subpixel = 0; subpixel < 2; ++subpixel) {
          unsigned int phase = start_phase;
          const int32_t* restrict filter_ptr = SVIDEO_FIR[phase][subpixel];
          if(start_x < 0)
            filter_ptr += start_x * -9;
          int32_t summed_red = 0, summed_green = 0, summed_blue = 0;
          for(int sub_x = start_x < 0 ? 0 : start_x; sub_x <= end_x; ++sub_x) {
            uint32_t pix = in_row[sub_x];
            auto in_r = (pix >> 16) & 255;
            auto in_g = (pix >> 8) & 255;
            auto in_b = pix & 255;
            summed_red += in_r * filter_ptr[0];
            summed_green += in_r * filter_ptr[1];
            summed_blue += in_r * filter_ptr[2];
            summed_red += in_g * filter_ptr[3];
            summed_green += in_g * filter_ptr[4];
            summed_blue += in_g * filter_ptr[5];
            summed_red += in_b * filter_ptr[6];
            summed_green += in_b * filter_ptr[7];
            summed_blue += in_b * filter_ptr[8];
            filter_ptr += 9;
          }
          *out++ = pack_pixel(summed_red, summed_green, summed_blue);
        }
        start_phase = (start_phase + 1) % NUM_CHROMA_PHASES;
        ++start_x;
        ++end_x;
      }
      if(output_skips_rows) out += width * 2;
      in_row += width;
    }
  }
  IMPLEMENT(svideo_bgra);
  void scanline_crisp_bgra(void* restrict _buf,
                           unsigned int width, unsigned int height) {
    auto& linearizer = FX::linearizer();
    auto& delinearizer = FX::delinearizer();
    assert(width%8 == 0);
    uint32_t* restrict in_row1 = reinterpret_cast<uint32_t*>(_buf);
    uint32_t* restrict out = in_row1 + width;
    uint32_t* restrict in_row2 = out + width;
    for(unsigned int y = 0; y < height; ++y) {
      for(unsigned int x = 0; x < width; ++x) {
        uint32_t pix_above = *in_row1++;
        uint32_t pix_below = *in_row2++;
        uint32_t red = linearizer[pix_above >> 16] + linearizer[pix_below >> 16];
        uint32_t green = linearizer[pix_above >> 8] + linearizer[pix_below >> 8];
        uint32_t blue = linearizer[pix_above] + linearizer[pix_below];
        *out++ = (delinearizer[red / 8] << 16) | (delinearizer[green / 8] << 8)
          | delinearizer[blue / 8];
      }
      in_row1 += width;
      out += width;
      in_row2 += width;
    }
  }
  IMPLEMENT(scanline_crisp_bgra);
  void scanline_bright_bgra(void* restrict _buf,
                            unsigned int width, unsigned int height) {
    auto& linearizer = FX::linearizer();
    auto& delinearizer = FX::delinearizer();
    assert(width%8 == 0);
    uint32_t* restrict in_row1 = reinterpret_cast<uint32_t*>(_buf);
    uint32_t* restrict out = in_row1 + width;
    uint32_t* restrict in_row2 = out + width;
    for(unsigned int y = 0; y < height; ++y) {
      for(unsigned int x = 0; x < width; ++x) {
        uint32_t pix_above = *in_row1++;
        uint32_t pix_below = *in_row2++;
        uint32_t red = linearizer[pix_above >> 16] + linearizer[pix_below >> 16];
        uint32_t green = linearizer[pix_above >> 8] + linearizer[pix_below >> 8];
        uint32_t blue = linearizer[pix_above] + linearizer[pix_below];
        *out++ = (delinearizer[red / 4] << 16) | (delinearizer[green / 4] << 8)
          | delinearizer[blue / 4];
      }
      in_row1 += width;
      out += width;
      in_row2 += width;
    }
  }
  IMPLEMENT(scanline_bright_bgra);
}
