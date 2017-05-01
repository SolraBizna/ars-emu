/* This could be a lot faster, but this design is simple. Ach, if the me from
   2001 saw this code, he would snap my neck... */
/* TODO: Is it okay that this is English only? */

#include "io.hh"
#include "sn.hh"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <assert.h>
#include <zlib.h>

// Dummy to get TEG to link
SN::Context sn;

namespace {
  /*const uint8_t dictionary[] = {
    0xAA,0xAA,
    0x00,0x01,0x80,0x00,0x00,0x01,0x80,0x00,
    0x55,0x55,
    };*/
  int hexdigit2value(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    else if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    else if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    else return -1;
  }
  int countUTF8(uint32_t v) {
    if(v < 0x80) return 1;
    else if(v < 0x800) return 2;
    else if(v < 0x10000) return 3;
    else if(v < 0x200000) return 4;
    else if(v < 0x4000000) return 5;
    else return 6;
  }
  void outUTF8(std::ostream& s, uint32_t v) {
    if(v < 0x80) s << uint8_t(v);
    else if(v < 0x800) s << uint8_t(0xC0|(v>>6)) << uint8_t(0x80|(v&63));
    else if(v < 0x10000) s << uint8_t(0xE0|(v>>12))
                           << uint8_t(0x80|((v>>6)&63))
                           << uint8_t(0x80|(v&63));
    else if(v < 0x200000) s << uint8_t(0xF0|(v>>18))
                            << uint8_t(0x80|((v>>12)&63))
                            << uint8_t(0x80|((v>>6)&63))
                            << uint8_t(0x80|(v&63));
    else if(v < 0x4000000) s << uint8_t(0xF8|(v>>24))
                             << uint8_t(0x80|((v>>18)&63))
                             << uint8_t(0x80|((v>>12)&63))
                             << uint8_t(0x80|((v>>6)&63))
                             << uint8_t(0x80|(v&63));
    else s << uint8_t(0xFC|(v>>30))
           << uint8_t(0x80|((v>>24)&63))
           << uint8_t(0x80|((v>>18)&63))
           << uint8_t(0x80|((v>>12)&63))
           << uint8_t(0x80|((v>>6)&63))
           << uint8_t(0x80|(v&63));
  }
  struct Char {
    bool wide;
    std::vector<uint8_t> tiles;
    Char(std::string::iterator begin, std::string::iterator end) {
      assert(end-begin == 64 || end-begin == 32);
      wide = end-begin == 64;
      tiles.reserve(wide ? 32 : 16);
      int rowstride = wide ? 4 : 2;
      int columns = wide ? 2 : 1;
      std::string::iterator p;
      for(int col = 0; col < columns; ++col) {
        p = begin + col * 2;
        for(int y = 0; y < 16; ++y) {
          tiles.push_back((hexdigit2value(*p)<<4)|(hexdigit2value(*(p+1))));
          p += rowstride;
        }
      }
    }
  };
  struct Page {
    std::map<uint8_t, std::shared_ptr<Char> > chars;
    std::vector<uint8_t> compressed_bytes;
    void compress() {
      compressed_bytes.clear();
      std::vector<uint8_t> uncompressed_bytes;
      uncompressed_bytes.insert(uncompressed_bytes.begin(), 256, 0);
      for(int i = 0; i < 256; ++i) {
        auto c = chars[i];
        if(c != nullptr) {
          uncompressed_bytes[i] = c->wide ? 2 : 1;
          uncompressed_bytes.insert(uncompressed_bytes.end(),
                                    c->tiles.begin(), c->tiles.end());
        }
      }
      if(uncompressed_bytes.size() == 256) return;
      z_stream z;
      z.zalloc = nullptr;
      z.zfree = nullptr;
      z.opaque = nullptr;
      if(deflateInit(&z, 9) != Z_OK) assert(false);
      // This makes it larger?
      // deflateSetDictionary(&z, dictionary, sizeof(dictionary));
      compressed_bytes.insert(compressed_bytes.end(),
                              deflateBound(&z, uncompressed_bytes.size()), 0);
      z.next_in = uncompressed_bytes.data();
      z.avail_in = uncompressed_bytes.size();
      z.next_out = compressed_bytes.data();
      z.avail_out = compressed_bytes.size();
      if(deflate(&z, Z_STREAM_END) != Z_OK) assert(false);
      deflateEnd(&z);
      compressed_bytes.resize(z.next_out - compressed_bytes.data());
    }
  };
  std::map<uint32_t, Page> pages;
}

int teg_main(int argc, char* argv[]) {
  if(argc <= 1) {
    std::cerr << "Please provide one or more Unifont-style .hex files as arguments.\n";
    return 1;
  }
  std::string line;
  for(int n = 1; n < argc; ++n) {
    std::unique_ptr<std::istream> f = IO::OpenRawPathForRead(argv[n]);
    if(!f || !*f) return 1;
    while(std::getline(*f, line)) {
      auto colon_it = std::find(line.begin(), line.end(), ':');
      if(colon_it == line.end()) {
        std::cerr << argv[n] << " contains a line without a colon\n";
        return 1;
      }
      auto codepoint_len = colon_it - line.begin();
      if(codepoint_len > 6 || codepoint_len < 1) {
        std::cerr << argv[n] << " contains an invalid codepoint\n";
        return 1;
      }
      uint32_t shift = codepoint_len*4;
      uint32_t codepoint = 0;
      auto it = line.begin();
      while(it != colon_it) {
        int digit = hexdigit2value(*it++);
        if(digit < 0) {
          std::cerr << argv[n] << " contains an invalid codepoint\n";
          return 1;
        }
        shift -= 4;
        codepoint |= static_cast<uint32_t>(digit) << shift;
      }
      if(line.end() == colon_it + 65 || line.end() == colon_it + 33) {
        it = colon_it + 1;
        while(it != line.end()) {
          if(hexdigit2value(*it++) < 0) {
            std::cerr << argv[n] << ":" << std::hex << std::setw(4)
                      << std::setfill('0') << codepoint << std::dec
                      << ": invalid bitmap\n";
            return 1;
          }
        }
        pages[codepoint>>8].chars[codepoint&255]
          = std::make_shared<Char>(colon_it + 1, line.end());
      }
      else {
        std::cerr << argv[n] << ":" << std::hex << std::setw(4)
                  << std::setfill('0') << codepoint << std::dec
                  << ": glyph is neither 8 nor 16 pixels wide\n";
        return 1;
      }
    }
  }
  if(pages.empty()) {
    std::cerr << "No characters were read\n";
    return 1;
  }
  uint32_t start = 3; // magic
  for(uint32_t page = 0; page <= 0x10FF; ++page) {
    pages[page].compress();
    start += countUTF8(pages[page].compressed_bytes.size());
  }
  outUTF8(std::cout, start);
  for(uint32_t page = 0; page <= 0x10FF; ++page) {
    outUTF8(std::cout, pages[page].compressed_bytes.size());
  }
  for(uint32_t page = 0; page <= 0x10FF; ++page) {
    std::cout.write(reinterpret_cast<char*>
                    (pages[page].compressed_bytes.data()),
                    pages[page].compressed_bytes.size());
  }
  return 0;
}
