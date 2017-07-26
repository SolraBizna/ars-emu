#include "ars-emu.hh"
#include "et209.hh"

#include <cmath>
#include <array>

namespace {
  constexpr float SAMPLE_RATE = 47988.28125f;
  constexpr float ET209_OUTPUT_TO_FLOAT_SAMPLE = 1.f / 256.f;
#if __clang__
  constexpr float FILTER_COEFFICIENT = 0x1.adc011a45e08ap-2;
#else
  constexpr float FILTER_COEFFICIENT = std::exp(-1/(SAMPLE_RATE*0.000024f));
#endif
  constexpr unsigned int QBUF_LEN = 256;
  //constexpr unsigned int QBUF_THROTTLE = 2048 * sizeof(float);
  constexpr unsigned int QBUF_THROTTLE = 4096 * sizeof(float);
  constexpr unsigned int UNPAUSE_THRESHOLD = 1024 * sizeof(float);
  float prev_sample = 0.f;
  std::array<float, QBUF_LEN> qbuf;
  unsigned int qbuf_pos = 0;
  SDL_AudioDeviceID dev = 0;
  bool autopaused = false;
}

ET209 ARS::apu;

void ARS::init_apu() {
  if(dev != 0) SDL_CloseAudioDevice(dev);
  SDL_version version;
  SDL_GetVersion(&version);
  SDL_AudioSpec desired;
  memset(&desired, 0, sizeof(desired));
  // in SDL, sample rates are always integers...
  desired.freq = static_cast<int>(SAMPLE_RATE);
  desired.format = AUDIO_F32SYS;
  desired.channels = 1;
  if(version.major > 2
     || (version.major == 2 && version.minor > 0)
     || (version.major == 2 && version.minor == 0 && version.patch > 5)) {
    desired.samples = 512;
    desired.size = 512*sizeof(float);
  }
  else {
    // work around SDL bug 3685
    desired.size = 8192;
    desired.samples = 8192/sizeof(float);
  }
  dev = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
  if(dev == 0)
    ui << SDL_GetError() << ui;
  else
    autopaused = true;
}

void ARS::output_apu_sample() {
  if(dev <= 0) return;
  float new_sample = apu.output_sample() * ET209_OUTPUT_TO_FLOAT_SAMPLE;
  qbuf[qbuf_pos++] = prev_sample = new_sample + (prev_sample - new_sample)
    * FILTER_COEFFICIENT;
  // TODO: more aggressive shortening
  if(qbuf_pos == QBUF_LEN) {
    //unsigned int throttle = SDL_GetQueuedAudioSize(dev) / QBUF_THROTTLE;
    //SDL_QueueAudio(dev, qbuf.data(), (QBUF_LEN - throttle) * sizeof(float));
    if(!autopaused && SDL_GetQueuedAudioSize(dev) == 0) {
      autopaused = true;
      SDL_PauseAudioDevice(dev, 1);
    }
    if(SDL_GetQueuedAudioSize(dev) <= QBUF_THROTTLE)
      SDL_QueueAudio(dev, qbuf.data(), QBUF_LEN * sizeof(float));
    qbuf_pos = 0;
    if(autopaused) {
      if(SDL_GetQueuedAudioSize(dev) >= UNPAUSE_THRESHOLD) {
        autopaused = false;
        SDL_PauseAudioDevice(dev, 0);
      }
    }
  }
}
