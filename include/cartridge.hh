#ifndef CARTRIDGEHH
#define CARTRIDGEHH

#include "ars-emu.hh"
#include "gamefolder.hh"
#include "memory.hh"

#include <unordered_map>

namespace ARS {
  class Cartridge {
    Cartridge(const Cartridge&) = delete;
    Cartridge(Cartridge&&) = delete;
    Cartridge& operator=(const Cartridge&) = delete;
    Cartridge& operator=(Cartridge&&) = delete;
  protected:
    Cartridge() {}
  public:
    virtual ~Cartridge() {}
    virtual uint8_t read(uint8_t bank, uint16_t addr,
                         bool OL = false, bool VPB = false,
                         bool SYNC = false) = 0;
    virtual void write(uint8_t bank, uint16_t addr, uint8_t value) = 0;
    virtual void oncePerFrame() = 0;
    virtual void handleReset() { return; }
    virtual uint8_t getPowerOnBank() { return 0; }
    virtual uint8_t getBS() { return 0; }
    static std::unique_ptr<Cartridge> load(GameFolder& gamefolder,
                                           std::string language_override = "");
  };
  extern std::unique_ptr<Cartridge> cartridge;
  extern const std::unordered_map<std::string, std::function<std::unique_ptr<Cartridge>(byuuML::cursor mapper_tag, std::unordered_map<std::string, std::unique_ptr<Memory> >)> > mapper_types;
}

#endif
