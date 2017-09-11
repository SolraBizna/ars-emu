#ifndef FXHH
#define FXHH

#include "ppu.hh"

namespace FX {
  void init();
  // lefts, rights, and widths must be multiples of 8
  // output_skips_rows should be true if you plan to scanline-filter the result
  void raw_screen_to_bgra(const ARS::PPU::raw_screen& in,
                          void* out,
                          unsigned int left = 0, unsigned int top = 0,
                          unsigned int right = ARS::PPU::TOTAL_SCREEN_WIDTH,
                          unsigned int bottom = ARS::PPU::TOTAL_SCREEN_HEIGHT,
                          bool output_skips_rows = false);
  void raw_screen_to_bgra_2x(const ARS::PPU::raw_screen& in,
                             void* out,
                             unsigned int left = 0, unsigned int top = 0,
                             unsigned int right = ARS::PPU::TOTAL_SCREEN_WIDTH,
                           unsigned int bottom = ARS::PPU::TOTAL_SCREEN_HEIGHT,
                             bool output_skips_rows = false);
  // doubles the width of the input
  // CANNOT be in-place
  void composite_bgra(const void* in, void* out,
                      unsigned int width, unsigned int height,
                      bool output_skips_rows);
  void svideo_bgra(const void* in, void* out,
                   unsigned int width, unsigned int height,
                   bool output_skips_rows);
  // assumes the input skipped rows
  void scanline_crisp(void* buf,
                      unsigned int width, unsigned int height);
  void scanline_bright(void* buf,
                       unsigned int width, unsigned int height);
}

#endif
