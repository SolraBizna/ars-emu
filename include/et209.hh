/*

  Copyright (C) 2017 Solra Bizna

  This software is provided "as-is", without any express or implied warranty.
  In no event will the authors be held liable for any damages arising from the
  use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

 */

/*

  This class simulates the entire digital logic of the ET209 synthesizer chip,
  which serves as the Audio Processing Unit of the Eiling Technologies ARS. It
  is 100% accurate, except that it assumes no writes to the register memory can
  occur mid-sample. This should have no noticeable effect on the output.

  In the ARS, the ET209 is clocked at ~6.136MHz. It generates one output sample
  per 128 clocks. Therefore, the correct sample rate to use is 47940.341Hz.
  It ends up generating 799.8046875 each frame. For most emulation purposes,
  48000Hz and 800 samples per frame is "accurate enough".

  Generated samples range from -256 to 248, and will typically be much smaller.
  A real ET209 internally generates samples in the range 0 to 504, but an
  emulator would just have to recenter it anyway. Note that this IS the correct
  range, even though it appears to be centered closer to -4 than to 0!

  (The above applies both to each individual channel, and to the sum of each
  channel.)

  This class does NOT simulate the analog component of the ARS audio circuit,
  which also includes a low-pass RC filter with τ≈0.000024.

 */

#ifndef ET209_HH
#define ET209_HH

#include <stdint.h>

class ET209 {
public:
  static constexpr int REGISTER_MEMORY_SIZE = 32;
  static constexpr int NUM_VOICES = 7;
  static constexpr int ADDR_RATE_LO = 0;
  static constexpr int ADDR_RATE_HI = 8;
  static constexpr int ADDR_WAVEFORM = 16;
  static constexpr int ADDR_VOLUME = 24;
  static constexpr int ADDR_NOISE_PERIOD = 15;
  static constexpr int ADDR_NOISE_WAVEFORM = 23;
  static constexpr int ADDR_NOISE_VOLUME = 31;
  static constexpr uint16_t RATE_INSTANT_CHANGE = 0x0000;
  static constexpr uint16_t RATE_FAST_SLIDE = 0x4000;
  static constexpr uint16_t RATE_MEDIUM_SLIDE = 0x8000;
  static constexpr uint16_t RATE_SLOW_SLIDE = 0xC000;
  static constexpr uint8_t WAVEFORM_INVERT_EIGHTH_FLAG = 1;
  static constexpr uint8_t WAVEFORM_INVERT_QUARTER_FLAG = 2;
  static constexpr uint8_t WAVEFORM_INVERT_HALF_FLAG = 4;
  static constexpr uint8_t WAVEFORM_INVERT_ALL_FLAG = 8;
  static constexpr uint8_t WAVEFORM_TOGGLE_INVERT_ON_CARRY_FLAG = 16;
  static constexpr uint8_t WAVEFORM_OUTPUT_ACCUMULATOR_FLAG = 32;
  static constexpr uint8_t WAVEFORM_SIGNED_RESET_MASK = 48;
  static constexpr uint8_t WAVEFORM_PAN_MASK = 192;
  static constexpr uint8_t WAVEFORM_PAN_CENTER = 0;
  static constexpr uint8_t WAVEFORM_PAN_LEFT = 128;
  static constexpr uint8_t WAVEFORM_PAN_RIGHT = 64;
  static constexpr uint8_t WAVEFORM_PAN_FULL = 192;
  static constexpr uint8_t VOLUME_MAX = 64;
  static constexpr uint8_t VOLUME_RESET_FLAG = 128;
private:
  union {
    uint8_t ram[REGISTER_MEMORY_SIZE];
    struct {
      uint8_t rate_lo[NUM_VOICES];
      uint8_t pad;
      uint8_t rate_hi[NUM_VOICES];
      uint8_t noise_period;
      uint8_t waveform[NUM_VOICES];
      uint8_t noise_waveform;
      uint8_t volume[NUM_VOICES];
      uint8_t noise_volume;
    } user;
    static_assert(sizeof(user) == 32,
                  "Inappropriate padding is being applied");
  };
  uint16_t real_rate[7], voice_accumulator[7], lfsr;
  uint8_t noise_accumulator, sample_number;
  uint8_t eval_waveform(uint16_t accum, uint8_t waveform) {
    uint8_t ret;
    if(waveform & WAVEFORM_OUTPUT_ACCUMULATOR_FLAG) ret = accum >> 10;
    else ret = 0;
    if(((waveform & WAVEFORM_INVERT_EIGHTH_FLAG) != 0 && (accum & 0xE000) == 0)
       ^((waveform & WAVEFORM_INVERT_QUARTER_FLAG) != 0 && (accum&0xC000) ==0)
       ^((waveform & WAVEFORM_INVERT_HALF_FLAG) != 0 && (accum & 0x8000) == 0)
       ^((waveform & WAVEFORM_INVERT_ALL_FLAG) != 0))
      ret ^= 63;
    return ret;
  }
  int16_t q6_multiply(uint8_t a, uint8_t b) {
    int8_t signed_a = int8_t(a) - 32;
    if(b & 64) return signed_a; // ET209's multiplier saturates B at 64
    else return (int16_t)((signed_a * b) >> 6);
  }
public:
  ET209() : lfsr(1), noise_accumulator(0), sample_number(0) {}
  /* rand is a function (or function-like object) which returns 8 bits of
     randomish data. This is only used for simulating a randomized power-on
     state. */
  template<class T> ET209(T& rand) { resetstate(rand); }
  template<class T> void resetstate(T& rand) {
    lfsr = 1;
    for(int i = 0; i < REGISTER_MEMORY_SIZE; ++i)
      ram[i] = rand();
    for(int i = 0; i < NUM_VOICES; ++i) {
      real_rate[i] = rand() | (rand()<<8);
      voice_accumulator[i] = rand() | (rand()<<8);
    }
    noise_accumulator = rand();
    sample_number = rand();
  }
  /*
    out_frame indices map directly to hardware pan values, and the samples are
    therefore in the order: Center, Right, Left, Boosted. (noise is mapped to
    Boosted.)

    My "replica" will have four DACs and output these signals separately,
    allowing various effects including surround sound. This emulation does the
    equivalent, giving you the freedom to either add wacky filters of your own
    or just implement the "original" logic at very little extra cost.

    The "original" ET209 had only two DACs, which outputted the equivalent of:

     left DAC = L+B+(C>>1)    (the low bit of C is *discarded*)
    right DAC = R+B+(C>>1)

    If L and R are 0dB, B is +6dB (because it's present on both channels, and
    correlated) and C is also 0dB (attenuated by 6dB and then placed into both
    channels). Any mixing you do should try to preserve these volume levels.
  */
  void output_frame(int16_t out_samples[4]) {
    out_samples[3] = out_samples[2] = out_samples[1] = out_samples[0] = 0;
    for(int voice = 0; voice < NUM_VOICES; ++voice) {
      uint16_t target_rate = user.rate_lo[voice] | (user.rate_hi[voice]<<8);
      auto shift_rate = target_rate>>14;
      target_rate &= 0x3FFF;
      switch(shift_rate) {
      case 0: real_rate[voice] = target_rate; break;
      default:
        if(sample_number & ((1<<shift_rate<<2)-1)) break;
        if(real_rate[voice] < target_rate) ++real_rate[voice];
        else if(real_rate[voice] > target_rate) --real_rate[voice];
        break;
      }
      if(user.volume[voice] & VOLUME_RESET_FLAG) {
        user.volume[voice] &= ~VOLUME_RESET_FLAG;
        voice_accumulator[voice]
          = (user.waveform[voice] & WAVEFORM_SIGNED_RESET_MASK)
          == WAVEFORM_SIGNED_RESET_MASK ? 0x8000 : 0;
      }
      else {
        uint32_t nuccumulator = voice_accumulator[voice] + real_rate[voice] +1;
        if(nuccumulator >= 65536) {
          if(user.waveform[voice] & WAVEFORM_TOGGLE_INVERT_ON_CARRY_FLAG) {
            user.waveform[voice] ^= WAVEFORM_INVERT_ALL_FLAG;
          }
        }
        voice_accumulator[voice] = static_cast<uint16_t>(nuccumulator);
      }
      auto val = q6_multiply(eval_waveform(voice_accumulator[voice],
                                           user.waveform[voice]),
                             user.volume[voice] & 127);
      out_samples[user.waveform[voice] >> 6] += val;
    }
    uint8_t noise_sum = 0;
    if((user.noise_volume & VOLUME_RESET_FLAG) != 0) {
      user.noise_volume &= ~VOLUME_RESET_FLAG;
      lfsr = 1;
    }
    for(int step = 0; step < 8; ++step) {
      noise_sum += (lfsr&1);
      if(step != 7 && (user.noise_waveform & (1<<step))) continue;
      if(noise_accumulator == user.noise_period) {
        noise_accumulator = 0;
        bool feedback;
        if(user.noise_waveform & 0x80)
          feedback = ((lfsr>>6)^lfsr)&1;
        else
          feedback = ((lfsr>>1)^lfsr)&1;
        lfsr >>= 1;
        if(feedback) lfsr |= 16384;
      }
      else ++noise_accumulator;
    }
    int16_t noise = q6_multiply(noise_sum|(noise_sum<<3),
                                user.noise_volume&127);
    out_samples[3] += noise;
    ++sample_number;
  }
  void write(int addr, uint8_t value) {
    addr &= 0x1F;
    if(addr == ADDR_NOISE_PERIOD) noise_accumulator = 0;
    ram[addr] = value;
  }
  /* Helper functions that are less useful to an emulator and more useful to a
     music authoring program */
  void write_voice_rate(int voice, uint16_t value) {
    write(ADDR_RATE_LO+voice, value & 255);
    write(ADDR_RATE_HI+voice, value >> 8);
  }
  void write_voice_volume(int voice, uint8_t value) {
    write(ADDR_VOLUME+voice, value);
  }
  void write_voice_waveform(int voice, uint8_t value) {
    write(ADDR_WAVEFORM+voice, value);
  }
  void write_noise_period(uint8_t value) {
    write(ADDR_NOISE_PERIOD, value);
  }
  void write_noise_volume(uint8_t value) {
    write(ADDR_NOISE_VOLUME, value);
  }
};

#endif
