#include "mappers.hh"

#include <array>

namespace {
  class DevCart : public ARS::Cartridge {
    static constexpr unsigned int UNSHIFT_SHIFT = 2;
    static constexpr unsigned int NUM_DIP_GROUPS = 4;
    static constexpr unsigned int NUM_DIP_CONFIGURATIONS = 4;
    static_assert(NUM_DIP_CONFIGURATIONS <= 1 << UNSHIFT_SHIFT,
                  "UNSHIFT_SHIFT needs to be increased");
    static_assert((NUM_DIP_CONFIGURATIONS&(NUM_DIP_CONFIGURATIONS - 1)) == 0,
                  "These must be powers of two");
    static_assert(NUM_DIP_CONFIGURATIONS == NUM_DIP_GROUPS,
                  "These can't actually be different");
    std::array<std::unique_ptr<ARS::Memory>, NUM_DIP_CONFIGURATIONS>
    memory_storage;
    std::array<uint8_t, NUM_DIP_GROUPS> dips_and_shifts;
    uint8_t power_on_bank, overlay_bank, bs;
    bool overlay_override;
    uint32_t map_addr(ARS::Memory*& p, uint8_t bank, uint16_t addr,
                      bool OL, bool, bool) {
      if(OL && overlay_override) bank = overlay_bank;
      auto das = dips_and_shifts[bank >> 6];
      p = memory_storage[das & (NUM_DIP_CONFIGURATIONS-1)].get();
      auto shift = das >> UNSHIFT_SHIFT;
      return (bank << (15-shift)) | (addr & (0x7FFF >> shift));
    }
  public:
    DevCart(byuuML::cursor tag,
            std::unordered_map<std::string, std::unique_ptr<ARS::Memory> >
            memories) {
      bs = tag.query("bs").value<unsigned long>(0)&3;
      power_on_bank = tag.query("power-on-bank").value<unsigned long>(0);
      auto c = tag.query("overlay-bank");
      if(c) {
        overlay_override = true;
        overlay_bank = c.value<unsigned long>();
      }
      else overlay_override = false;
      std::array<std::string, NUM_DIP_CONFIGURATIONS> chips;
      for(auto& c : chips) c = "open";
      for(unsigned int i = 0; i < NUM_DIP_GROUPS; ++i) {
        std::string q;
        q.push_back('0' + i);
        auto c = tag.query(q);
        if(!c)
          throw sn.Get("BOARD_DEVCART_MEMORY_ID_MISSING"_Key, {q});
        uint8_t shift = bs - c.query("unshift").value<unsigned long>(0);
        if(shift & 0x80)
          throw sn.Get("BOARD_DEVCART_MEMORY_UNSHIFT_GREATER_THAN_BS"_Key, {q});
        const std::string& id = c->data();
        if(id == "open") {
          // This will lead to a null pointer in every configuration that
          // includes at least one "open" set.
          dips_and_shifts[i] = NUM_DIP_CONFIGURATIONS
            | (shift << UNSHIFT_SHIFT);
        }
        else {
          for(unsigned int j = 0; j < NUM_DIP_CONFIGURATIONS; ++j) {
            if(chips[j] == id) {
              dips_and_shifts[i] = j | (shift << UNSHIFT_SHIFT);
              break;
            }
            else if(chips[j] == "open") {
              auto p = memories.find(id);
              if(p == memories.end())
                throw sn.Get("BOARD_DEVCART_MEMORY_ID_UNKNOWN"_Key, {q});
              assert(p->second);
              chips[j] = id;
              dips_and_shifts[i] = j | (shift << UNSHIFT_SHIFT);
              memory_storage[j] = std::move(p->second);
              break;
            }
          }
        }
        /* the loop cannot end without finding a slot */
      }
      /* and therefore, by this point, 0-3 are fully handled */
    }
    uint8_t read(uint8_t bank, uint16_t addr, bool OL, bool VPB,
                 bool SYNC) override {
      ARS::Memory* mem = nullptr;
      uint32_t real_addr = map_addr(mem, bank, addr, OL, VPB, SYNC);
      if(!mem) {
        ARS::ui
          << sn.Get("MISSING_CHIP_READ"_Key, {TEG::format("%04X",real_addr)})
          << ARS::ui;
        return 0xBB;
      }
      else
        return mem->read(real_addr);
    }
    void write(uint8_t bank, uint16_t addr, uint8_t value) override {
      ARS::Memory* mem = nullptr;
      uint32_t real_addr = map_addr(mem, bank, addr, false, false, false);
      if(!mem) {
        ARS::ui
          << sn.Get("MISSING_CHIP_WRITE"_Key,{TEG::format("%04X",real_addr)})
          << ARS::ui;
      }
      else
        mem->write(real_addr, value);
    }
    void handleReset() override {}
    uint8_t getPowerOnBank() override {
      return power_on_bank;
    }
    uint8_t getBS() override {
      return bs;
    }
    void oncePerFrame() override {
      for(auto& p : memory_storage) {
        if(p) p->oncePerFrame();
      }
    }
  };
}

std::unique_ptr<ARS::Cartridge> ARS::Mappers::MakeDevCart(byuuML::cursor tag, std::unordered_map<std::string, std::unique_ptr<ARS::Memory> > memories) {
  return std::make_unique<DevCart>(tag, std::move(memories));
}
