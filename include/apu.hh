#ifndef APU_HH
#define APU_HH

#include "ars-emu.hh"

class ET209;

namespace ARS {
  extern ET209 apu;
  void init_apu(); // may be called more than once
  void output_apu_sample();
}

#endif
