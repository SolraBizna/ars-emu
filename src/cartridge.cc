/* TODO: This entire file uses die() for error handling. */

#include "ars-emu.hh"
#include "teg.hh"
#include <iomanip>
#include <algorithm>
#include "io.hh"
#include <zlib.h>

#if ZLIB_VERNUM < 0x1210
#error zlib version 1.2.1 or later is required
#endif

using namespace ARS;

namespace {
  // constexpr auto MAXIMUM_POSSIBLE_ROM_SIZE = (1<<23)*4+16;
  bool unsafe_char(char c) {
    if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
       || (c >= '0' && c <= '9')) return true;
    switch(c) {
    case '\'': case '"': case '-': case ',': case '+':
      return true;
    default:
      return false;
    }
  }
  class zbuf : public std::streambuf {
    std::istream& src;
    z_stream z;
    uint8_t outbuf[512], inbuf[512];
  public:
    zbuf(std::istream& src, uint8_t* red, size_t red_len)
      : src(src) {
      assert(red_len <= sizeof(inbuf));
      memcpy(inbuf, red, red_len);
      z.next_in = inbuf;
      z.avail_in = red_len;
      z.next_out = outbuf;
      z.avail_out = sizeof(outbuf);
      z.zalloc = nullptr;
      z.zfree = nullptr;
      z.opaque = nullptr;
      // |16 = decode gzip instead of zlib
      if(inflateInit2(&z, MAX_WBITS|16) != Z_OK)
        die("inflateInit2 failed");
    }
    zbuf(std::istream& src, uint8_t* red, size_t red_len, std::string& romname)
      : zbuf(src, red, red_len) {
      gz_header header;
      char filenamebuf[256];
      filenamebuf[0] = 0;
      filenamebuf[sizeof(filenamebuf)-1] = 0;
      header.extra = nullptr;
      header.name = reinterpret_cast<Bytef*>(filenamebuf);
      header.name_max = sizeof(filenamebuf)-1;
      if(inflateGetHeader(&z, &header) != Z_OK)
        die("logic error");
      do {
        if(z.avail_in == 0)
          get_more_input();
        if(z.avail_in == 0)
          die("EOF while reading gzip header");
        int res = inflate(&z, 0);
        if(res == Z_OK)
          ; // do nothing
        else if(res == Z_STREAM_END)
          die("EOS while reading gzip header");
        else
          die("error while reading gzip header");
      } while(header.done == 0 && z.next_out == outbuf);
      if(header.done == 1 && filenamebuf[0]) romname = filenamebuf;
    }
    ~zbuf() {
      inflateEnd(&z);
    }
    void get_more_input() {
      if(src) {
        src.read(reinterpret_cast<char*>(inbuf), sizeof(inbuf));
        z.next_in = inbuf;
        z.avail_in = src.gcount();
      }
      else
        z.avail_in = 0;
    }
    int underflow() {
      if(egptr() == reinterpret_cast<char*>(z.next_out)) {
        z.next_out = outbuf;
        z.avail_out = sizeof(outbuf);
      }
      while(z.next_out == outbuf) {
        if(z.avail_in == 0)
          get_more_input();
        if(z.avail_in == 0)
          break;
        int res = inflate(&z, 0);
        if(res == Z_OK || res == Z_STREAM_END)
          ; // do nothing
        else
          die("zlib error while reading compressed ROM");
      }
      if(z.next_out != outbuf) {
        setg(reinterpret_cast<char*>(outbuf), reinterpret_cast<char*>(outbuf),
             reinterpret_cast<char*>(z.next_out));
        return *outbuf;
      }
      return std::streambuf::underflow();
    }
  };
  void parse_size(uint8_t in, uint32_t& size, bool& has) {
    if(in == 0x80)
      die("ETARS image claims to contain a 0-byte initialization image");
    has = !!(in&0x80);
    if((in&0x7F) > 0x18)
      die("ETARS image contains excessively large initialization data");
    else if(in&0x7F)
      size = 1<<((in&0x7F)-1);
    else
      size = 0;
  }
}

class Cartridge* ARS::cartridge = nullptr;

void ARS::Cartridge::loadRom(const std::string& rom_path, std::istream& f) {
  std::string rom_name = rom_path;
  if(cartridge != nullptr) {
    delete cartridge;
    cartridge = nullptr;
  }
  uint8_t header[IMAGE_HEADER_SIZE];
  f.read(reinterpret_cast<char*>(header), IMAGE_HEADER_SIZE);
  if(!f) die("Error while reading ROM header");
  zbuf* z = nullptr;
  std::istream* rom_image_stream;
  if(header[0] == 0x1F && header[1] == 0x8B) {
    z = new zbuf(f, header, IMAGE_HEADER_SIZE, rom_name);
    rom_image_stream = new std::istream(z);
    rom_image_stream->read(reinterpret_cast<char*>(header), IMAGE_HEADER_SIZE);
    if(!rom_image_stream || rom_image_stream->gcount() != IMAGE_HEADER_SIZE)
      die("Error while reading compressed ROM header");
  }
  else rom_image_stream = &f;
  if(header[0] != 'A' || header[1] != 'R' || header[2] != 'S'
     || header[3] != 26) die("Invalid ETARS image");
  if(header[15] != 0) die("Unsupported ETARS image header extension");
  if(header[4] != 0) die("Unsupported ETARS image bus mapping");
  uint32_t rom1_size, rom2_size, sram_size, dram_size;
  bool has_rom1, has_rom2, has_sram, has_dram;
  parse_size(header[6], rom1_size, has_rom1);
  parse_size(header[7], rom2_size, has_rom2);
  parse_size(header[8], sram_size, has_sram);
  parse_size(header[9], dram_size, has_dram);
  if(rom1_size && !has_rom1) die("ETARS image has uninitialized ROM1");
  if(rom2_size && !has_rom2) die("ETARS image has uninitialized ROM2");
  if(!has_rom1 && !has_rom2 && !has_sram && !has_dram)
    die("ETARS image is empty");
  if(has_dram) ui << "Warning: ROM image contains initialized DRAM" << ui;
  uint8_t* rom1_data, *rom2_data, *dram_data, *sram_data;
  if(has_rom1) {
    assert(rom1_size != 0);
    rom1_data = new uint8_t[rom1_size];
    if(!rom_image_stream->read(reinterpret_cast<char*>(rom1_data),
                               rom1_size))
      die("Error while reading ROM1 data");
  }
  else rom1_data = nullptr;
  if(has_rom2) {
    assert(rom2_size != 0);
    rom2_data = new uint8_t[rom2_size];
    if(!rom_image_stream->read(reinterpret_cast<char*>(rom2_data),
                               rom2_size))
      die("Error while reading ROM2 data");
  }
  else rom2_data = nullptr;
  if(has_dram) {
    assert(dram_size != 0);
    dram_data = new uint8_t[dram_size];
    if(!rom_image_stream->read(reinterpret_cast<char*>(dram_data),
                               dram_size))
      die("Error while reading DRAM data");
  }
  else dram_data = nullptr;
  if(has_sram) {
    assert(sram_size != 0);
    sram_data = new uint8_t[sram_size];
    if(!rom_image_stream->read(reinterpret_cast<char*>(sram_data),
                               sram_size))
      die("Error while reading SRAM data");
  }
  else sram_data = nullptr;
  if(header[5] & 0x7E) die("Unsupported expansion hardware");
  if(header[10] & 0x30) die("Unsupported pin mapping");
  cartridge = new Cartridge(rom_name,
                            rom1_size, rom1_data, rom2_size, rom2_data,
                            dram_size, dram_data, sram_size, sram_data,
                            header[10]&0xF, header[10]>>6, header[11],
                            header[12], header[5], header[13]);
  assert(cartridge != nullptr);
  if(rom_image_stream != &f) delete rom_image_stream;
  if(z != nullptr) delete z;
}

ARS::Cartridge::Cartridge(const std::string& rom_path,
                          size_t rom1_size, const uint8_t* rom1_data,
                          size_t rom2_size, const uint8_t* rom2_data,
                          size_t dram_size, const uint8_t* dram_data,
                          size_t sram_size, const uint8_t* sram_data,
                          uint8_t bank_shift, uint8_t bank_size,
                          uint8_t dip_switches, uint8_t power_on_bank,
                          uint8_t expansion_hardware, uint8_t overlay_bank)
  : sram_size(sram_size), bank_shift(bank_shift), bank_size(bank_size),
    dip_switches(dip_switches), power_on_bank(power_on_bank),
    expansion_hardware(expansion_hardware), overlay_bank(overlay_bank) {
  if(rom1_size != 0) {
    uint8_t* copy = new uint8_t[rom1_size];
    assert(rom1_data != nullptr);
    memcpy(copy, rom1_data, rom1_size);
    rom1 = copy;
    rom1_mask = rom1_size - 1;
  }
  else {
    rom1 = nullptr;
    rom1_mask = 0;
  }
  if(rom2_size != 0) {
    uint8_t* copy = new uint8_t[rom2_size];
    assert(rom2_data != nullptr);
    memcpy(copy, rom2_data, rom2_size);
    rom2 = copy;
    rom2_mask = rom2_size - 1;
  }
  else {
    rom2 = nullptr;
    rom2_mask = 0;
  }
  if(dram_size != 0) {
    dram = new uint8_t[dram_size];
    if(dram_data != nullptr)
      memcpy(dram, dram_data, dram_size);
    else
      fillDramWithGarbage(dram, dram_size);
    dram_mask = dram_size - 1;
  }
  else {
    dram = nullptr;
    dram_mask = 0;
  }
  if(sram_size != 0) {
    sram = new uint8_t[sram_size];
    if(sram_data != nullptr)
      memcpy(sram, sram_data, sram_size);
    else
      memset(sram, 0, sram_size);
    sram_dirty = 0;
    sram_mask = sram_size - 1;
  }
  else {
    sram = nullptr;
    sram_mask = 0;
  }
  // calculate the name of the .srm file for this ROM
  if(sram_size != 0) {
    // Strip off an optional .gz suffix, then strip off any file extension
    auto rit = rom_path.rend();
    if(rom_path.length() > 3
       && (rit[0] == 'z' || rit[0] == 'Z')
       && (rit[1] == 'g' || rit[1] == 'G')
       && rit[2] == '.') {
      rit -= 3;
    }
    rit = std::find_if(rom_path.rbegin(), rom_path.rend(),
                       [](char c) { return c == '/' || c == '\\'; });
    // rend() is a fine answer
    sram_path.append(rom_path.rbegin(), rit);
    std::reverse(sram_path.begin(), sram_path.end());
    auto it = std::find(sram_path.begin(), sram_path.end(), '.');
    if(it != sram_path.end()) sram_path.erase(it);
    std::replace_if(sram_path.begin(), sram_path.end(), unsafe_char, '_');
    sram_path.append(".sram");
    std::unique_ptr<std::istream> f = IO::OpenConfigFileForRead(sram_path);
    if(f && *f) f->read(reinterpret_cast<char*>(sram), sram_size);
  }
  if(rom1_size != 0 || rom2_size != 0) {
    if(rom1_size != 0)
      ui << (rom1_size>>10);
    if(rom1_size != 0 && rom2_size != 0)
      ui << "+";
    if(rom2_size != 0)
      ui << (rom2_size>>10);
    ui << "KiB ROM" << ui;
  }
  if(sram_size != 0) ui << (sram_size>>10) << "KiB SRAM" << ui;
  if(dram_size != 0) ui << (dram_size>>10) << "KiB DRAM" << ui;
}

ARS::Cartridge::~Cartridge() {
  flushSRAM();
  if(rom1 != nullptr) {
    delete rom1;
    rom1 = nullptr;
  }
  if(rom2 != nullptr) {
    delete rom2;
    rom2 = nullptr;
  }
  if(dram != nullptr) {
    delete dram;
    dram = nullptr;
  }
  if(sram != nullptr) {
    delete sram;
    sram = nullptr;
  }
}

void ARS::Cartridge::flushSRAM() {
  if(sram == nullptr || sram_dirty == 0) return;
  std::unique_ptr<std::ostream> f = IO::OpenConfigFileForWrite(sram_path);
  if(f && *f && f->write(reinterpret_cast<char*>(sram), sram_size)) {
    sram_dirty = 0;
    f.reset();
    IO::UpdateConfigFile(sram_path);
  }
  else
    ui << "Unable to write SRAM: " << strerror(errno) << ui;
}

uint32_t ARS::Cartridge::map_addr(uint8_t bank, uint16_t addr,
                                  bool OL, bool, bool, bool) {
  if(OL && overlay_bank != 0xFF) bank = overlay_bank;
  return (addr & (0xFFFF >> bank_shift)) | (bank << 16 >> bank_shift);
}

uint8_t ARS::Cartridge::read(uint8_t bank, uint16_t addr, bool OL, bool VPB,
                             bool SYNC) {
  uint32_t raw_addr = map_addr(bank, addr, OL, VPB, SYNC, false);
  switch((dip_switches >> (bank >> 6 << 1)) & 3) {
  case 0:
    if(rom1 != nullptr) return rom1[raw_addr & rom1_mask];
    else {
      ui << "Invalid read from $" << std::hex << std::setfill('0')
         << std::setw(6) << raw_addr << std::dec << " on nonexistent ROM1 chip"
         << ui;
      break;
    }
  case 1:
    if(rom2 != nullptr) return rom2[raw_addr & rom2_mask];
    else {
      ui << "Invalid read from $" << std::hex << std::setfill('0')
         << std::setw(6) << raw_addr << std::dec << " on nonexistent ROM2 chip"
         << ui;
      break;
    }
  case 2:
    if(dram != nullptr) return dram[raw_addr & dram_mask];
    else {
      ui << "Invalid read from $" << std::hex << std::setfill('0')
         << std::setw(6) << raw_addr << std::dec << " on nonexistent DRAM chip"
         << ui;
      break;
    }
  case 3:
    if(sram != nullptr) return sram[raw_addr & sram_mask];
    else {
      ui << "Invalid read from $" << std::hex << std::setfill('0')
         << std::setw(6) << raw_addr << std::dec << " on nonexistent SRAM chip"
         << ui;
      break;
    }
  }
  return 0xFF;
}

void ARS::Cartridge::write(uint8_t bank, uint16_t addr, uint8_t value) {
  uint32_t raw_addr = map_addr(bank, addr, false, false, false, true);
  switch((dip_switches >> (bank >> 6 << 1)) & 3) {
  case 0:
    ui << "Ignoring write of $" << std::hex << std::setw(2)
       << std::setfill('0') << value << " to $" << std::setfill('0')
       << std::setw(6) << raw_addr << " on ROM1 chip" << ui;
    return;
  case 1:
    ui << "Ignoring write of $" << std::hex << std::setw(2)
       << std::setfill('0') << value << " to $" << std::setfill('0')
       << std::setw(6) << raw_addr << " on ROM2 chip" << ui;
    return;
  case 2:
    if(dram != nullptr) {
      dram[raw_addr & dram_mask] = value;
    }
    else {
      ui << "Ignoring write of $" << std::hex << std::setw(2)
         << std::setfill('0') << value << " to $" << std::setfill('0')
         << std::setw(6) << raw_addr << " on nonexistent DRAM chip" << ui;
    }
    return;
  case 3:
    if(sram != nullptr) {
      sram_dirty = 1;
      sram[raw_addr & sram_mask] = value;
    }
    else {
      ui << "Ignoring write of $" << std::hex << std::setw(2)
         << std::setfill('0') << value << " to $" << std::setfill('0')
         << std::setw(6) << raw_addr << " on nonexistent SRAM chip" << ui;
    }
    return;
  }
}

void ARS::Cartridge::handleReset() {}
