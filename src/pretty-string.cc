#include "font.hh"
#include "utfit.hh"

SN::Context sn;

extern "C"
int teg_main(int argc, char* argv[]) {
  Font::Load();
  for(int n = 1; n < argc; ++n) {
    auto str = std::string(argv[n]);
    auto cit = str.cbegin();
    while(cit != str.cend()) {
      uint32_t c = getNextCodePoint(cit, str.cend());
      auto& glyph = Font::GetGlyph(c);
      int count = glyph.wide ? 2 : 1;
      for(int n = 0; n < count; ++n) {
        const uint8_t* p = glyph.tiles + n * 16;
        for(int n = 0; n < 8; ++n) putchar(p[n]);
        for(int n = 0; n < 8; ++n) putchar(p[n]);
        for(int n = 8; n < 16; ++n) putchar(p[n]);
        for(int n = 8; n < 16; ++n) putchar(p[n]);
      }
    }
  }
  return 0;
}
