#include "ars-emu.hh"
#include "et209.hh"
#include "prefs.hh"
#include "config.hh"

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
  constexpr unsigned int QBUF_LEN = 512;
  //constexpr unsigned int QBUF_THROTTLE = 2048 * sizeof(float);
  constexpr unsigned int QBUF_THROTTLE = 8192 * sizeof(float);
  constexpr unsigned int UNPAUSE_THRESHOLD = 1024 * sizeof(float);
  constexpr float MIN_SPEAKER_SEPARATION = 1.f;
  constexpr float MAX_SPEAKER_SEPARATION = 180.f;
  float prev_sample[2] = {0.f, 0.f};
  // used for the headphones filter
  float delayed_sample[2] = {0.f, 0.f};
  std::vector<float> stereo_delay_buf;
  size_t stereo_delay_pos;
  std::array<float, QBUF_LEN> qbuf;
  unsigned int qbuf_pos = 0;
  SDL_AudioDeviceID dev = 0;
  bool autopaused = false;
  // configuration options
  bool stereo, headphones;
  float virtual_speaker_separation;
  int head_width_in_samples;
  // dependent on configuration
  float pan_filter_coefficient;
  const Config::Element audio_elements[] = {
    {"stereo", stereo},
    {"headphones", headphones},
    {"virtual_speaker_separation", virtual_speaker_separation},
    {"head_width_in_samples", head_width_in_samples},
  };
  class AudioPrefsLogic : public PrefsLogic {
  protected:
    void Load() override {
      Config::Read("Audio.utxt",
                   audio_elements, elementcount(audio_elements));
      if(virtual_speaker_separation < MIN_SPEAKER_SEPARATION)
        virtual_speaker_separation = MIN_SPEAKER_SEPARATION;
      else if(virtual_speaker_separation > MAX_SPEAKER_SEPARATION)
        virtual_speaker_separation = MAX_SPEAKER_SEPARATION;
      if(head_width_in_samples < 1)
        head_width_in_samples = 1;
      else if(head_width_in_samples > SAMPLE_RATE)
        head_width_in_samples = SAMPLE_RATE;
    }
    void Save() override {
      Config::Write("Audio.utxt",
                    audio_elements, elementcount(audio_elements));
    }
    void Defaults() override {
      stereo = true;
      headphones = false;
      virtual_speaker_separation = 60;
      head_width_in_samples = 30;
    }
  } audioPrefsLogic;
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
  desired.channels = stereo ? 2 : 1;
  if(version.major > 2
     || (version.major == 2 && version.minor > 0)
     || (version.major == 2 && version.minor == 0 && version.patch > 5)) {
    desired.samples = 512*desired.channels;
    desired.size = 512*desired.channels*sizeof(float);
  }
  else {
    // work around SDL bug 3685
    desired.size = 8192;
    desired.samples = 8192/sizeof(float);
  }
  if(stereo && headphones) {
    float cutoff_freq = 1000 * pow(1 / std::sin(virtual_speaker_separation
                                                *3.14159265358f/360),// NOT 180
                                   1.f / 0.8f);
    float tau = 1.f / (3.14159265358f * 2.0f * cutoff_freq);
    pan_filter_coefficient = std::exp(-1/(SAMPLE_RATE*tau));
    float stereo_delay
      = std::floor(head_width_in_samples * std::sin(virtual_speaker_separation
                                                    *3.14159265358f/360)+0.5f);
    int stereo_delay_samples;
    if(stereo_delay > SAMPLE_RATE) stereo_delay_samples = SAMPLE_RATE;
    else if(stereo_delay <= 1) stereo_delay_samples = 1;
    else stereo_delay_samples = static_cast<int>(stereo_delay);
    stereo_delay_buf.clear();
    stereo_delay_buf.resize(stereo_delay_samples * 2);
  }
  dev = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
  if(dev == 0)
    ui << SDL_GetError() << ui;
  else
    autopaused = true;
}

void ARS::output_apu_sample() {
  if(dev <= 0) return;
  if(stereo) {
    int16_t raw_sample[2];
    float new_sample[2];
    apu.output_stereo_sample(raw_sample);
    new_sample[0] = raw_sample[0] * ET209_OUTPUT_TO_FLOAT_SAMPLE;
    new_sample[1] = raw_sample[1] * ET209_OUTPUT_TO_FLOAT_SAMPLE;
    qbuf[qbuf_pos++] =
      prev_sample[0] = new_sample[0] + (prev_sample[0] - new_sample[0])
      * FILTER_COEFFICIENT;
    qbuf[qbuf_pos++] =
      prev_sample[1] = new_sample[1] + (prev_sample[1] - new_sample[1])
      * FILTER_COEFFICIENT;
    if(headphones) {
      delayed_sample[1] = prev_sample[0] + (delayed_sample[1] - prev_sample[0])
        * pan_filter_coefficient;
      delayed_sample[0] = prev_sample[1] + (delayed_sample[0] - prev_sample[1])
        * pan_filter_coefficient;
      qbuf[qbuf_pos-2] = (qbuf[qbuf_pos-2]+stereo_delay_buf[stereo_delay_pos])
        * 0.5f;
      stereo_delay_buf[stereo_delay_pos++] = delayed_sample[0];
      qbuf[qbuf_pos-1] = (qbuf[qbuf_pos-1]+stereo_delay_buf[stereo_delay_pos])
        * 0.5f;
      stereo_delay_buf[stereo_delay_pos++] = delayed_sample[1];
      if(stereo_delay_pos >= stereo_delay_buf.size())
        stereo_delay_pos = 0;
    }
  }
  else {
    float new_sample = apu.output_mono_sample() * ET209_OUTPUT_TO_FLOAT_SAMPLE;
    qbuf[qbuf_pos++] =
      prev_sample[0] = new_sample + (prev_sample[0] -new_sample)
      * FILTER_COEFFICIENT;
  }
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
