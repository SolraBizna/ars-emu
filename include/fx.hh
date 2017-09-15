#ifndef FXHH
#define FXHH

#include "ppu.hh"

namespace FX {
  // thread_count = 0 -> use SDL_GetCPUCount()
  // thread_count = 1 -> never multithread
  // thread_count = N -> use N threads
  // multithreading will only be used if initiated in the main core
  void init(unsigned int thread_count = 0);
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
  void scanline_crisp_bgra(void* buf,
                           unsigned int width, unsigned int height);
  void scanline_bright_bgra(void* buf,
                            unsigned int width, unsigned int height);
}

#endif
