#include "expansions.hh"
#include "configurator.hh"
#include "cpu.hh"

using namespace ARS;

bool ARS::always_allow_config_port = false,
  ARS::allow_secure_config_port = true,
  ARS::secure_config_port_checked = false,
  ARS::allow_debug_port = false,
  ARS::mapped_debug_port = false;
std::unique_ptr<ARS::Expansion> ARS::expansions[8];

namespace {
  bool config_port_access_is_secure(uint16_t pc) {
    return pc >= 0xF000 || pc <= 0xF7FF;
  }
  bool secure_config_port_available;
  class ConfigPort : public ARS::Expansion {
  public:
    void output(uint8_t value) override {
      if(always_allow_config_port)
        return Configurator::write(value);
      else if(allow_secure_config_port) {
        if(!secure_config_port_checked) {
          secure_config_port_checked = true;
          secure_config_port_available
            = Configurator::is_secure_configurator_present();
        }
        if(secure_config_port_available) {
          if(config_port_access_is_secure(last_known_pc))
            return Configurator::write(value);
        }
      }
    }
    uint8_t input() override {
      if(always_allow_config_port)
        return Configurator::read();
      else if(allow_secure_config_port) {
        if(!secure_config_port_checked) {
          secure_config_port_checked = true;
          secure_config_port_available
            = Configurator::is_secure_configurator_present();
        }
        if(secure_config_port_available) {
          if(config_port_access_is_secure(last_known_pc))
            return Configurator::read();
          else
            return 0xEC;
        }
      }
      return 0xBB;
    }
  };
  class DebugPort : public ARS::Expansion {
    void output(uint8_t value) override {
      std::cerr << value;
    }
    uint8_t input() override {
      cpu->setSO(true);
      cpu->setSO(false);
      return 0xFF;
    }
  public:
    DebugPort() {
      mapped_debug_port = true;
    }
  };
  class NullPort : public ARS::Expansion {
    void output(uint8_t) override {}
    uint8_t input() override { return 0xBB; }
  };
}

void ARS::map_expansion(uint16_t addr, const std::string& type) {
  if(addr != 0 && (addr < 0x240 || addr > 0x247))
    die("INTERNAL ERROR: Expansion IO address out of range");
  if(type == "debug") {
    if(addr == 0) addr = 0x247;
    if(allow_debug_port) map_expansion(addr, std::make_unique<DebugPort>());
    else map_expansion(addr, std::make_unique<NullPort>());
  }
  else if(type == "config") {
    if(addr == 0) addr = 0x246;
    if(always_allow_config_port || allow_secure_config_port)
      map_expansion(addr, std::make_unique<ConfigPort>());
    else map_expansion(addr, std::make_unique<NullPort>());
  }
}

void ARS::map_expansion(uint16_t addr, std::unique_ptr<Expansion> p) {
  if(addr < 0x240 || addr > 0x247)
    die("INTERNAL ERROR: Expansion IO address out of range");
  if(expansions[addr&7])
    throw sn.Get("BOARD_EXPANSION_ADDRESS_CONFLICT"_Key);
  expansions[addr&7] = std::move(p);
}
