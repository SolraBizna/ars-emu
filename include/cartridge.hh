#ifndef CARTRIDGEHH
#define CARTRIDGEHH

#include "ars-emu.hh"

namespace ARS {
  /* more of Cartridge's methods can be made virtual if more than one mapper
     ever needs to be implemented */
  extern class Cartridge {
    std::string sram_path;
    int sram_dirty = 0;
    static constexpr int SRAM_DIRTY_FRAMES = 60;
  protected:
    const uint8_t* rom1, *rom2;
    uint8_t* dram, *sram;
    size_t sram_size;
    size_t rom1_mask, rom2_mask, dram_mask, sram_mask;
    uint8_t bank_shift, bank_size, dip_switches, power_on_bank,
      expansion_hardware, overlay_bank;
    Cartridge(const std::string& rom_path,
              size_t rom1_size, const uint8_t* rom1_data,
              size_t rom2_size, const uint8_t* rom2_data,
              size_t dram_size, const uint8_t* dram_data,
              size_t sram_size, const uint8_t* sram_data,
              uint8_t bank_shift, uint8_t bank_size,
              uint8_t dip_switches, uint8_t power_on_bank,
              uint8_t expansion_hardware, uint8_t overlay_bank);
    /*virtual*/ ~Cartridge();
    void markSramAsDirty() { sram_dirty = SRAM_DIRTY_FRAMES; }
    uint32_t map_addr(uint8_t bank, uint16_t addr, bool OL = false,
                      bool VPB = false, bool SYNC = false,
                      bool write = false);
  public:
    static constexpr uint8_t EXPANSION_DEBUG_PORT = 0x01;
    static constexpr uint8_t EXPANSION_HAM = 0x40;
    static constexpr uint8_t EXPANSION_CONFIG = 0x80;
    static constexpr uint8_t SUPPORTED_EXPANSION_HARDWARE = 0xC1;
    static constexpr int IMAGE_HEADER_SIZE = 16;
    /*virtual*/ uint8_t read(uint8_t bank, uint16_t addr,
                             bool OL = false, bool VPB = false,
                             bool SYNC = false);
    /*virtual*/ void write(uint8_t bank, uint16_t addr, uint8_t value);
    /*virtual*/ void handleReset();
    uint8_t getPowerOnBank() { return power_on_bank; }
    uint8_t getBS() { return bank_size; }
    void flushSRAM();
    /* Throws a std::string if loading the cartridge failed */
    static void loadRom(const std::string& rom_path_name, std::istream&);
    void oncePerFrame() {
      if(sram_dirty > 1) --sram_dirty;
      else if(sram_dirty == 1) flushSRAM();
    }
    bool hasHardware(uint8_t h) {
      return !!(expansion_hardware & h);
    }
  }* cartridge;
}

#endif
