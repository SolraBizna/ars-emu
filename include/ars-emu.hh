#ifndef ARSEMU_HH
#define ARSEMU_HH

#include <stdint.h>
#include <sstream>
#include <memory>
#include "SDL.h"
#include "sn.hh"

extern "C" void die(const char* format, ...) __attribute__((noreturn,
                                                          format(printf,1,2)));

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
  static constexpr int CYCLES_PER_VBLANK = 17160;
  static constexpr int CYCLES_PER_FRAME = 204360;
  static_assert(CYCLES_PER_VBLANK == CYCLES_PER_SCANLINE * 22,
                "Incorrect CYCLES_PER_VBLANK");
  static_assert(CYCLES_PER_FRAME == CYCLES_PER_SCANLINE * 262,
                "Incorrect CYCLES_PER_FRAME");
  static constexpr int CYCLES_PER_SCANOUT = CYCLES_PER_FRAME-CYCLES_PER_VBLANK;
  static constexpr int LIVE_PIXELS_PER_SCANLINE = 256;
  static_assert(LIVE_PIXELS_PER_SCANLINE == LIVE_CYCLES_PER_SCANLINE / 2,
                "wrong implied dot clock");
  static constexpr int LIVE_SCANLINES_PER_FRAME = 240;
  static_assert((LIVE_SCANLINES_PER_FRAME * CYCLES_PER_SCANLINE
                 + CYCLES_PER_VBLANK) * 60 == 12261600,
                "wrong implied core clock");
  extern bool safe_mode, debugging_audio, debugging_video;
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
