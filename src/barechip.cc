#include "mappers.hh"

namespace {
  class BareChip : public ARS::Cartridge {
    std::unique_ptr<ARS::Memory> mem;
  public:
    BareChip(std::unique_ptr<ARS::Memory> mem) : mem(std::move(mem)) {}
    uint8_t read(uint8_t bank, uint16_t addr, bool, bool, bool) override {
      return mem->read((addr & 0x7FFF) | (bank << 15));
    }
    void write(uint8_t bank, uint16_t addr, uint8_t value) override {
      mem->write((addr & 0x7FFF) | (bank << 15), value);
    }
    void oncePerFrame() override {
      mem->oncePerFrame();
    }
  };
}

std::unique_ptr<ARS::Cartridge> ARS::Mappers::MakeBareChip(byuuML::cursor, std::unordered_map<std::string, std::unique_ptr<ARS::Memory> > memory) {
  if(memory.size() != 1)
    throw sn.Get("MAPPER_BARE_WRONG_MEMORY_COUNT"_Key);
  return std::make_unique<BareChip>(std::move(memory.begin()->second));
}
