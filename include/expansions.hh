#ifndef EXPANSIONSHH
#define EXPANSIONSHH

#include "ars-emu.hh"

namespace ARS {
  class Expansion {
  public:
    virtual ~Expansion() {}
    virtual void output(uint8_t) = 0;
    virtual uint8_t input() = 0;
  };
  // first two entries read from controller ports 1 and 2
  extern std::unique_ptr<Expansion> expansions[8];
  void map_expansion(uint16_t addr, const std::string& type);
  void map_expansion(uint16_t addr, std::unique_ptr<Expansion>);
  extern bool always_allow_config_port, allow_secure_config_port,
    secure_config_port_checked, allow_debug_port;
}

#endif
