#ifndef UTIL_HH
#define UTIL_HH

#include <stdint.h>

namespace Util {
  void block_copy(uint16_t to, uint16_t from, uint16_t count);
  void block_output(uint16_t to, uint16_t from, uint16_t count);
  void block_zero(uint16_t to, uint16_t count);
  void block_output_zero(uint16_t to, uint16_t count);
  // scanline callback block, 256 bytes
  extern uint8_t scanline_callback_table[240];
  extern void(*scanline_callbacks[8])();
  void null_callback();
}

#endif
