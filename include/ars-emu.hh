#ifndef ARSEMU_HH
#define ARSEMU_HH

#include <stdint.h>
#include <sstream>
#include <memory>
#include "SDL.h"
#include "sn.hh"

extern "C" void die(const char* format, ...) __attribute__((noreturn,
                                                          format(printf,1,2)));

class ET209;

extern SN::Context sn;

namespace ARS {
  static constexpr int SAFE_BLANK_CYCLES_PER_SCANLINE = 74;
  static constexpr int UNSAFE_BLANK_CYCLES_PER_SCANLINE = 194;
  static constexpr int BLANK_CYCLES_PER_SCANLINE = 268;
  static_assert(SAFE_BLANK_CYCLES_PER_SCANLINE+UNSAFE_BLANK_CYCLES_PER_SCANLINE
                ==BLANK_CYCLES_PER_SCANLINE, "wrong H-scan cycle counts");
  static constexpr int LIVE_CYCLES_PER_SCANLINE = 512;
  static constexpr int CYCLES_PER_SCANLINE = BLANK_CYCLES_PER_SCANLINE
    + LIVE_CYCLES_PER_SCANLINE;
  static constexpr int CYCLES_PER_VBLANK = 17550;
  static constexpr int CYCLES_PER_FRAME = 204750;
  static_assert(CYCLES_PER_VBLANK == CYCLES_PER_SCANLINE * 45 / 2,
                "Incorrect CYCLES_PER_VBLANK");
  static_assert(CYCLES_PER_FRAME == CYCLES_PER_SCANLINE * 525 / 2,
                "Incorrect CYCLES_PER_FRAME");
  static constexpr int CYCLES_PER_SCANOUT = CYCLES_PER_FRAME-CYCLES_PER_VBLANK;
  static constexpr int SAMPLES_PER_LOAD = 400;
  extern bool safe_mode;
  // $0000-7FFF
  extern uint8_t dram[0x8000];
  static struct Regs {
    // ars-emu.cc makes some assumptions about the layout, check there if you
    // change anything
    uint8_t bgScrollX, bgScrollY; // $0200,$0201
    static constexpr uint8_t M1_FLIP_X_MASK = 1;
    static constexpr uint8_t M1_FLIP_Y_MASK = 2;
    static constexpr uint8_t M1_FLIP_MAGIC_MASK = 3;
    static constexpr uint8_t M1_BACKGROUND_MODE_MASK = 4;
    static constexpr uint8_t M1_VIDEO_ENABLE_MASK = 8;
    static constexpr uint8_t M1_OLBASE_MASK = 0xF;
    static constexpr uint8_t M1_OLBASE_SHIFT = 4;
    uint8_t multi1; // $0202
    uint8_t bgTileBaseTop, bgTileBaseBot; // $0203,$0204
    uint8_t spBasePalette; // $0205
    uint8_t bgBasePalette[4]; // $0206-$0209
    uint8_t bgForegroundInfo[4]; // $020A-$020D
    uint8_t olBasePalette; // $020E
    uint8_t colorMod; // $020F
    // $0210-$021F are "complex"
    // $0211,$0213,$0215,$0217 inhibit DRAM read and do an IO
    // other "complex" reads are as normal register-space reads
    uint8_t vramAccessPage, vramAccessPort; // $0210,$0211
    uint8_t cramAccessByte, cramAccessPort; // $0212,$0213
    uint8_t ssmAccessByte, ssmAccessPort; // $0214,$0215
    uint8_t samAccessByte, samAccessPort; // $0216,$0217
    uint8_t vramAccessByte, irqScanline; // $0218,$0219
    uint8_t vramDMA, vramDMASplat; // $021A,$021B
    uint8_t cramDMA, ssmDMA, samDMA; // $021C,$021D,$021E
    uint8_t ssmPlanarDMA;
    // $0220-$0227 require special read/write logic
    uint8_t ioPorts[8];
    // $0228-$022F are complex on write
    uint8_t bankMap[8];
  }& Regs = *reinterpret_cast<struct Regs*>(dram+0x0200);
  namespace PPU {
    constexpr int INTERNAL_SCREEN_WIDTH = 256, INTERNAL_SCREEN_HEIGHT = 240;
    constexpr int INTERNAL_SCREEN_TILES_WIDE = 32,
      INTERNAL_SCREEN_TILES_HIGH = 30;
    static_assert(INTERNAL_SCREEN_WIDTH/8 == INTERNAL_SCREEN_TILES_WIDE,
                  "faulty WIDTH constant");
    static_assert(INTERNAL_SCREEN_HEIGHT/8 == INTERNAL_SCREEN_TILES_HIGH,
                  "faulty HEIGHT constant");
    constexpr int INVISIBLE_SCANLINE_COUNT = 8;
    constexpr int INVISIBLE_COLUMN_COUNT = 8;
    constexpr int VISIBLE_SCREEN_WIDTH = 240, VISIBLE_SCREEN_HEIGHT = 224;
    constexpr int VISIBLE_SCREEN_TILES_WIDE = 30, VISIBLE_SCREEN_TILES_HIGH=28;
    static_assert(VISIBLE_SCREEN_WIDTH/8 == VISIBLE_SCREEN_TILES_WIDE,
                  "faulty WIDTH constant");
    static_assert(VISIBLE_SCREEN_HEIGHT/8 == VISIBLE_SCREEN_TILES_HIGH,
                  "faulty HEIGHT constant");
    constexpr int MODE1_BACKGROUND_TILES_WIDE = 32;
    constexpr int MODE1_BACKGROUND_TILES_HIGH = 30;
    constexpr int MODE2_BACKGROUND_TILES_WIDE = 32;
    constexpr int MODE2_BACKGROUND_TILES_HIGH = 32;
    constexpr int OVERLAY_TILES_WIDE = 32;
    constexpr int OVERLAY_TILES_HIGH = 28;
    void renderToTexture(SDL_Texture*);
    void dummyRender();
    void fillWithGarbage();
    void handleReset();
    void dumpSpriteMemory();
    extern bool show_overlay, show_sprites, show_background;
    // $0211, $0213, $0215, $0217
    uint8_t complexRead(uint16_t addr);
    // $0210-$021F
    void complexWrite(uint16_t addr, uint8_t value);
  }
  static_assert(CYCLES_PER_SCANOUT == CYCLES_PER_SCANLINE
                * PPU::INTERNAL_SCREEN_HEIGHT,
                "CYCLES_PER_SCAN needs to be updated");
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
    /* used by the debugger, maps from a 2x-bit ARS address to a 32-bit symbol
       address */
    /*virtual*/ uint32_t map_addr(uint8_t bank, uint16_t addr, bool OL = false,
                                  bool VPB = false, bool SYNC = false,
                                  bool write = false);
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
  class CPU {
  protected:
    CPU() {}
  public:
    virtual ~CPU() {}
    virtual void handleReset() = 0;
    virtual void eatCycles(int count) = 0;
    virtual void runCycles(int count) = 0;
    virtual void setIRQ(bool irq) = 0;
    virtual void setSO(bool so) = 0;
    virtual void setNMI(bool nmi) = 0;
    virtual bool isStopped() = 0;
    virtual void frameBoundary() {}
    // don't forget, NMI active = masked IRQ
  };
  extern std::unique_ptr<CPU> cpu;
  std::unique_ptr<CPU> makeScanlineCPU(const std::string& rom_path);
  std::unique_ptr<CPU> makeScanlineIntProfCPU(const std::string& rom_path);
  std::unique_ptr<CPU> makeScanlineDebugCPU(const std::string& rom_path);
  class Controller {
    uint8_t dOut, dIn;
    bool dataIsFresh, strobeIsHigh;
  protected:
    Controller() : strobeIsHigh(true) {}
    virtual uint8_t onStrobeFall(uint8_t curData) = 0;
  public:
    virtual ~Controller() {}
    void output(uint8_t d);
    uint8_t input();
    static void initControllers();
    /* returns true if the event was fully handled, false if the event needs
       further handling (it may have been modified in the mean time) */
    static bool filterEvent(SDL_Event&);
    static std::string getNameOfHardKey(int player, int button);
    static std::string getNamesOfBoundKeys(int player, int button);
    static void startBindingKey(int player, int button);
    static bool keyIsBeingBound();
    static std::string getKeyBindingInstructions(int player, int button);
  };
  namespace Configurator {
    void write(uint8_t d);
    uint8_t read();
  }
  extern ET209 apu;
  void init_apu(); // may be called more than once
  void output_apu_sample();
  extern std::unique_ptr<Controller> controller1, controller2;
  extern class MessageImp {
    std::ostringstream stream;
    void outputLine(std::string line, int lifespan_value);
    void outputBuffer();
  public:
    template<class T> MessageImp& operator<<(const T& i) {
      stream << i;
      return *this;
    }
    MessageImp& operator<<(MessageImp&) {
      outputBuffer();
      return *this;
    }
  } ui;
  void triggerReset() __attribute__((noreturn));
  void triggerQuit() __attribute__((noreturn));
  /* Whenever an outside event causes a time discontinuity, it should call
     this function to avoid bad interactions with the frameskipping logic. */
  void temporalAnomaly();
  uint8_t read(uint16_t addr, bool OL = false, bool VPB = false,
               bool SYNC = false);
  inline uint16_t read16(uint16_t addr) { return read(addr)|(read(addr+1)<<8);}
  void write(uint16_t addr, uint8_t value);
  inline void write16(uint16_t addr, uint16_t value) {
    write(addr, value);
    write(addr+1, value>>8);
  }
  uint8_t getBankForAddr(uint16_t addr);
  static_assert(RAND_MAX >= 4095, "not enough rand() bits for garbage");
  static inline uint8_t garbage() {
    return rand()>>4;
  }
  static inline void fillWithGarbage(void* _target, size_t count) {
    uint8_t* p = reinterpret_cast<uint8_t*>(_target);
    while(count-- > 0) {
      *p++ = garbage();
    }
  }
  static inline void fillDramWithGarbage(void* _target, size_t count) {
    uint8_t* p = reinterpret_cast<uint8_t*>(_target);
    while(count-- > 0) {
      *p++ = garbage()&garbage();
    }
  }
}

#endif
