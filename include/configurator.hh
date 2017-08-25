#ifndef CONFIGURATORHH
#define CONFIGURATORHH

#include "ars-emu.hh"

namespace ARS {
  namespace Configurator {
    bool is_active();
    bool is_secure_configurator_present();
    inline bool is_secure_configuration_address(uint16_t addr) {
      return addr >= 0xF000 && addr <= 0xF7FF;
    }
    inline bool is_protected_memory_address(uint16_t addr) {
      return (addr >= 0xEE && addr <= 0xFF)
        || (addr >= 0x4000 && addr <= 0x7FEF)
        || addr >= 0x8000;
    }
    void write(uint8_t d);
    uint8_t read();
  }
}

#endif
