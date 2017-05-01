#include "utfit.hh"

namespace {
  uint32_t ckcbs(uint32_t staato, int count,
                 std::string::const_iterator& it,
                 std::string::const_iterator end) {
    if(end-it < count) {
      it = end;
      return 0xFFFD;
    }
    int shift = count * 6;
    for(int n = 0; n < count; ++n) {
      uint8_t cb = *it++;
      if(cb < 0x80 || cb >= 0xC0) return 0xFFFD;
      shift -= 6;
      staato |= (cb&63)<<shift;
    }
    return staato;
  }
}

uint32_t getNextCodePoint(std::string::const_iterator& it,
                          std::string::const_iterator end) {
  if(it == end) return 0xFFFD;
  uint8_t first_c = *it++;
  if(first_c >= 0xFC)
    return ckcbs((first_c & 3) << 30, 5, it, end);
  else if(first_c >= 0xF8)
    return ckcbs((first_c & 3) << 24, 4, it, end);
  else if(first_c >= 0xF0)
    return ckcbs((first_c & 7) << 18, 3, it, end);
  else if(first_c >= 0xE0)
    return ckcbs((first_c & 15) << 12, 2, it, end);
  else if(first_c >= 0xC0)
    return ckcbs((first_c & 31) << 6, 1, it, end);
  else if(first_c >= 0x80)
    return 0xFFFD;
  else
    return first_c;
}
