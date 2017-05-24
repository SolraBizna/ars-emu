#include "ars-emu.hh"
#include "et209.hh"

#include <cmath>

namespace {
  constexpr float SAMPLE_RATE = 47988.28125f;
  constexpr float ET209_OUTPUT_TO_FLOAT_SAMPLE = 1.f / 256.f;
  constexpr float FILTER_COEFFICIENT = std::exp(-1/(SAMPLE_RATE*0.000024f));
  constexpr unsigned int QBUF_LEN = 256;
  constexpr unsigned int QBUF_THROTTLE = 1536 * sizeof(float);
  static const SDL_AudioSpec AUDIO_SPEC = {
    47988, // in SDL, these are always integers...
    AUDIO_F32SYS,
    1,
    0,
    512, // buffer a bit less than a frame
    512*sizeof(float),
    0,
    nullptr,
    nullptr
  };
  float prev_sample = 0.f;
  std::array<float, QBUF_LEN> qbuf;
  unsigned int qbuf_pos = 0;
  SDL_AudioDeviceID dev = 0;
}

ET209 ARS::apu;

void ARS::init_apu() {
  if(dev != 0) SDL_CloseAudioDevice(dev);
  dev = SDL_OpenAudioDevice(nullptr, 0, &AUDIO_SPEC, nullptr, 0);
  if(dev == 0)
    ui << SDL_GetError() << ui;
  else
    SDL_PauseAudioDevice(dev, 0);
}

void ARS::output_apu_sample() {
  if(dev <= 0) return;
  float new_sample = apu.output_sample() * ET209_OUTPUT_TO_FLOAT_SAMPLE;
  qbuf[qbuf_pos++] = prev_sample = new_sample + (prev_sample - new_sample)
    * FILTER_COEFFICIENT;
  // TODO: more aggressive shortening
  if(qbuf_pos == QBUF_LEN) {
    unsigned int throttle = SDL_GetQueuedAudioSize(dev) / QBUF_THROTTLE;
    SDL_QueueAudio(dev, qbuf.data(), (QBUF_LEN - throttle) * sizeof(float));
    qbuf_pos = 0;
  }
}
