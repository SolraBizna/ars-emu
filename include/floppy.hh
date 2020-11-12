#ifndef FLOPPYHH
#define FLOPPYHH

#ifndef DISALLOW_FLOPPY

#include "ars-emu.hh"
#include "expansions.hh"

namespace ARS {
  bool mount_floppy(const std::string& arg);
  std::unique_ptr<Expansion> make_floppy_expansion();
  extern bool fast_floppy_mode;
}

#endif

#endif
