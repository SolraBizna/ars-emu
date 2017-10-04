#ifndef MEMORYHH
#define MEMORYHH

#include "ars-emu.hh"

#include "teg.hh"

namespace ARS {
  class Memory {
  protected:
    uint8_t* memory_buffer;
    const uint32_t size, mask;
    Memory(uint8_t* memory_buffer, uint32_t size)
      : memory_buffer(memory_buffer), size(size), mask(size-1) {
      if(size > MAXIMUM_POSSIBLE_MEMORY_SIZE || size == 0
         || (size & mask) != 0)
        die("INTERNAL ERROR: Invalid memory size!");
    }
  public:
    static constexpr uint32_t MAXIMUM_POSSIBLE_MEMORY_SIZE = 1 << 30;
    virtual ~Memory() = 0; // you must do the right thing vis memory_buffer!
    uint8_t read(uint32_t address) const { return memory_buffer[address&mask];}
    virtual void write(uint32_t address, uint8_t value) {
      ui << sn.Get("ROM_CHIP_WRITE"_Key, {TEG::format("%08X",address),
            TEG::format("%02X",value)}) << ui;
    }
    virtual void oncePerFrame() {}
  };
  inline Memory::~Memory() {} // comply! COMPLY!
  class WritableMemory : public Memory {
    static constexpr int DIRTY_WRITE_DELAY = 15;
    int dirty;
  protected:
    WritableMemory(uint8_t* memory_buffer, uint32_t size)
      : Memory(memory_buffer, size) {}
    // will be called whenever the memory needs to be updated on disk,
    // including on destruction
    virtual void flush() {}
  public:
    ~WritableMemory() { if(dirty) flush(); }
    void write(uint32_t address, uint8_t value) override {
      dirty = DIRTY_WRITE_DELAY;
      memory_buffer[address&mask] = value;
    }
    void oncePerFrame() override {
      if(dirty > 0) {
        if(--dirty == 0) flush();
      }
    }
  };
}

#endif
