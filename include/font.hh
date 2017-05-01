#ifndef FONT_HH
#define FONT_HH

#include "ars-emu.hh"

namespace Font {
  constexpr int HEIGHT = 16;
  struct Glyph {
    bool wide;
    const uint8_t* tiles;
  };
  void Load(); // call this first!
  const Glyph& GetGlyph(uint32_t codepoint);
}

#endif
