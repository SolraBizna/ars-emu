#include "font.hh"
#include "io.hh"
#include <zlib.h>

namespace {
  constexpr int PAGE_COUNT = 0x1100;
  std::unique_ptr<std::istream> file;
  const Font::Glyph nullglyph = {false,
                                 (uint8_t[]){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
  struct Buffer {
    uint8_t* ptr;
    Buffer(size_t size) : ptr(reinterpret_cast<uint8_t*>(safe_malloc(size))){}
    ~Buffer() { safe_free(ptr); }
  };
  struct Page {
    uint32_t offset, size;
    uint8_t* loaded_data;
    const Font::Glyph* glyphs[256];
  } pages[PAGE_COUNT];
  uint8_t getb(std::istream& s) {
    char c;
    s >> c;
    return uint8_t(c);
  }
  uint32_t getcb(std::istream& s) {
    uint8_t ret = getb(s);
    if(ret >= 0xC0 || ret < 0x80)
      throw std::iostream::failure("missing continuation byte");
    return ret & 0x3F;
  }
  uint32_t inUTF8(std::istream& s) {
    uint8_t first_c = getb(s);
    if(first_c >= 0xFC)
      return ((first_c & 3) << 30)
        | (getcb(s) << 24)
        | (getcb(s) << 18)
        | (getcb(s) << 12)
        | (getcb(s) << 6)
        | getcb(s);
    else if(first_c >= 0xF8)
      return ((first_c & 3) << 24)
        | (getcb(s) << 18)
        | (getcb(s) << 12)
        | (getcb(s) << 6)
        | getcb(s);
    else if(first_c >= 0xF0)
      return ((first_c & 7) << 18)
        | (getcb(s) << 12)
        | (getcb(s) << 6)
        | getcb(s);
    else if(first_c >= 0xE0)
      return ((first_c & 15) << 12)
        | (getcb(s) << 6)
        | getcb(s);
    else if(first_c >= 0xC0)
      return ((first_c & 31) << 6)
        | getcb(s);
    else if(first_c >= 0x80)
      throw std::iostream::failure("Unexpected continuation byte");
    else
      return first_c;
  }
}

void Font::Load() {
  SDL_assert(!file);
  file = IO::OpenDataFileForRead("Font");
  if(!file) die("%s",sn.Get("FONT_FAIL"_Key).c_str());
  file->exceptions(std::iostream::failbit|std::iostream::badbit
                  |std::iostream::eofbit);
  try {
    uint32_t offset = inUTF8(*file);
    for(int page = 0; page < PAGE_COUNT; ++page) {
      pages[page].offset = offset;
      offset += (pages[page].size = inUTF8(*file));
    }
  }
  catch(std::iostream::failure&) {
    die("%s",sn.Get("FONT_CORRUPT"_Key).c_str());
  }
}

const Font::Glyph& Font::GetGlyph(uint32_t codepoint) {
  if(codepoint > 0x10FFFF) return GetGlyph(0xFFFD);
  try {
    auto& page = pages[codepoint >> 8];
    if(page.size == 0) return GetGlyph(0xFFFD);
    if(page.loaded_data == nullptr) {
      Buffer loadbuf(page.size);
      uint8_t widths[256];
      file->seekg(page.offset);
      file->read(reinterpret_cast<char*>(loadbuf.ptr), page.size);
      if(file->gcount() != static_cast<std::streamsize>(page.size))
        throw std::iostream::failure("early EOF");
      z_stream z;
      z.zalloc = nullptr;
      z.zfree = nullptr;
      z.opaque = nullptr;
      if(inflateInit(&z) != Z_OK) {
        std::cerr << "zlib error: " << z.msg << "\n";
        throw std::iostream::failure("zlib error");
      }
      try {
        z.next_in = loadbuf.ptr;
        z.avail_in = page.size;
        z.next_out = widths;
        z.avail_out = 256;
        if(inflate(&z, Z_SYNC_FLUSH) != Z_OK) {
          std::cerr << "zlib error: " << z.msg << "\n";
          throw std::iostream::failure("zlib error");
        }
        if(z.avail_out != 0)
          throw std::iostream::failure("short stream");
        uint32_t bufsize = 0;
        int glyphcount = 0;
        for(uint8_t width : widths) {
          if(width > 2)
            throw std::iostream::failure("invalid char width");
          bufsize += 16 * width;
          if(width != 0) ++glyphcount;
        }
        page.loaded_data = new uint8_t[bufsize];
        Font::Glyph* glyphp = new Font::Glyph[glyphcount];
        z.next_out = page.loaded_data;
        z.avail_out = bufsize;
        if(inflate(&z, Z_SYNC_FLUSH) != Z_OK) {
          std::cerr << "zlib error: " << z.msg << "\n";
          throw std::iostream::failure("zlib error");
        }
        if(z.avail_out != 0 || z.avail_in != 0)
          throw std::iostream::failure("overlong zlib stream");
        uint8_t* bufp = page.loaded_data;
        for(int c = 0; c < 256; ++c) {
          if(widths[c] == 0)
            page.glyphs[c] = &nullglyph;
          else {
            glyphp->wide = widths[c] > 1;
            glyphp->tiles = bufp;
            bufp += glyphp->wide ? 32 : 16;
            page.glyphs[c] = glyphp++;
          }
        }
        inflateEnd(&z);
      }
      catch(...) {
        inflateEnd(&z);
      }
    }
    return *page.glyphs[codepoint & 255];
  }
  catch(std::iostream::failure&) {
    die("%s",sn.Get("FONT_CORRUPT"_Key).c_str());
  }
}
