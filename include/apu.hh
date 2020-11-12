#ifndef APU_HH
#define APU_HH

#include "ars-emu.hh"

class ET209;

namespace ARS {
  extern ET209 apu;
  enum class FloppySoundType {
    SHORT_SEEK=0,
    LONG_SEEK_1,
    LONG_SEEK_2,
    NUM_FLOPPY_SOUNDS,
  };
  void init_apu(); // may be called more than once
  void output_apu_sample();
  // once set up, will remain set up across init_apu calls
  void setup_floppy_sounds();
  // drive is 0 or 1, delay_till_next is in frames and must not be zero.
  void queue_floppy_sound(unsigned int drive, FloppySoundType snd,
                          unsigned int min_delay_till_next);
}

#endif
