#include "ars-emu.hh"
#include "et209.hh"
#include "prefs.hh"
#include "config.hh"
#include "audiocvt.hh"
#include "windower.hh"
#include "menu.hh"

#include <cmath>
#include <array>
#include <atomic>

namespace {
#if !NO_DEBUG_CORES
  constexpr int DEBUG_WINDOW_PIXEL_SIZE = 4;
  constexpr int DEBUG_WINDOW_HEIGHT = 32;
  SDL_Window* debugwindow = nullptr;
  SDL_Renderer* debugrenderer;
#endif
  enum { SYNC_NONE=0, SYNC_STATIC, SYNC_DYNAMIC, NUM_SYNC_TYPES };
  // Of setups that require a given number of channels, the "most standard"
  // setup should go first. Stereo should precede Headphones, and Quadraphonic
  // should precede 4.0 Surround.
  enum { SOUND_TYPE_MONO, SOUND_TYPE_STEREO, SOUND_TYPE_HEADPHONES,
         SOUND_TYPE_QUADRAPHONIC, SOUND_TYPE_SURROUND_31,
         SOUND_TYPE_SURROUND_40, SOUND_TYPE_SURROUND_51,
         SOUND_TYPE_SURROUND_71,
         NUM_SOUND_TYPES };
  const SN::ConstKey SOUND_TYPE_KEYS[NUM_SOUND_TYPES] = {
    "SOUND_TYPE_MONO"_Key,
    "SOUND_TYPE_STEREO"_Key,
    "SOUND_TYPE_HEADPHONES"_Key,
    "SOUND_TYPE_QUADRAPHONIC"_Key,
    "SOUND_TYPE_SURROUND_31"_Key,
    "SOUND_TYPE_SURROUND_40"_Key,
    "SOUND_TYPE_SURROUND_51"_Key,
    "SOUND_TYPE_SURROUND_71"_Key,
  };
  // 1: Mono
  // 2: Stereo
  // 3: Surround
  // 4: Surround+LFE
  const int REQUIRED_SOURCE_CHANNELS[NUM_SOUND_TYPES]={1, 2, 3, 3, 4, 3, 4, 4};
  const int REQUIRED_HARD_CHANNELS[NUM_SOUND_TYPES] = {1, 2, 2, 4, 4, 4, 6, 8};
  constexpr float SAMPLE_RATE = 47988.28125f;
  constexpr int SAMPLE_RATE_HYSTERESIS = 128;
  // These are about four cents off from the nominal sample rate. That SHOULD
  // be just small enough that it's nearly impossible to hear the difference,
  // while still being about ten times larger than the drift of even the most
  // absurdly miscalibrated sound card.
  constexpr float MINIMUM_PERMITTED_SAMPLE_RATE = SAMPLE_RATE * 0.9975f;
  constexpr float MAXIMUM_PERMITTED_SAMPLE_RATE = SAMPLE_RATE * 1.0025f;
  constexpr float ET209_OUTPUT_TO_FLOAT_SAMPLE = 1.f / 256.f;
#if __clang__
  constexpr float DAC_FILTER_COEFFICIENT = 0x1.adc011a45e08ap-2;
  constexpr float LFE_FILTER_COEFFICIENT = 0x1.f957849983096p-1;
#else
  // about 6631Hz
  constexpr float DAC_FILTER_COEFFICIENT = std::exp(-1/(SAMPLE_RATE*0.000024f));
  // 100Hz
  constexpr float LFE_FILTER_COEFFICIENT = std::exp(-1/(SAMPLE_RATE*0.001592f));
#endif
  class Mixer {
  public:
    virtual ~Mixer() {}
    virtual void operator()(const float* in, float* out) = 0;
  };
  int target_min_queue_depth, target_max_queue_depth;
  float hysteresis_accum;
  int hysteresis_count;
  float target_in_rate, cur_in_rate;
  std::unique_ptr<AudioCvt> cur_cvt, prev_cvt;
  std::unique_ptr<Mixer> mixer;
  bool autopaused;
  class AudioQueue {
  public:
    static constexpr unsigned int ELEMENT_SIZE = 384;
    static constexpr unsigned int ELEMENT_COUNT = 64;
    static constexpr unsigned int APPROX_ELEMENTS_PER_FRAME
    = (800 + ELEMENT_SIZE-1) / ELEMENT_SIZE;
  private:
    std::array<std::array<float, ELEMENT_SIZE>, ELEMENT_COUNT> elements;
    std::atomic<unsigned int> current_element_in;
    std::atomic<unsigned int> next_element_out;
    unsigned int index_within_element = 0;
  public:
    AudioQueue() : current_element_in(0), next_element_out(0) {
      for(auto& array : elements) {
        memset(&array[0], 0, ELEMENT_SIZE * sizeof(float));
      }
    }
    //// Call only when serialized ////
    void ClearQueue() {
      current_element_in = 0;
      next_element_out = 0;
      index_within_element = 0;
    }
    //// Call from PRODUCING THREAD ////
    // Samples, not frames!!
    void AddSamplesToQueue(const float* src, unsigned int sample_count) {
    restart:
      unsigned int current_element_in
        = std::atomic_load_explicit(&this->current_element_in,
                                    std::memory_order_relaxed);
      auto remaining_space_in_element = ELEMENT_SIZE - index_within_element;
      unsigned int samples_to_add;
      if(sample_count > remaining_space_in_element) {
        samples_to_add = remaining_space_in_element;
      }
      else {
        samples_to_add = sample_count;
      }
      memcpy(&elements[current_element_in][index_within_element],
             src, samples_to_add * sizeof(float));
      assert(samples_to_add <= remaining_space_in_element);
      if(samples_to_add == remaining_space_in_element) {
        unsigned int check_element_in
          = (current_element_in + 2) % ELEMENT_COUNT;
        unsigned int next_element_out
          = std::atomic_load_explicit(&this->next_element_out,
                                      std::memory_order_relaxed);
        if(check_element_in == next_element_out) {
#if !NO_DEBUG_CORES
          if(ARS::debugging_audio)
            std::cerr << SDL_GetTicks() << ": audio queue overrun!\n";
#endif
          return;
        }
        unsigned int next_element_in
          = (current_element_in + 1) % ELEMENT_COUNT;
        std::atomic_store_explicit(&this->current_element_in,
                                   next_element_in,
                                   std::memory_order_release);
        index_within_element = 0;
      }
      else
        index_within_element += samples_to_add;
      if(samples_to_add < sample_count) {
        src += samples_to_add;
        sample_count -= samples_to_add;
        goto restart;
      }
    }
#if !NO_DEBUG_CORES
    void debug_update() const {
      unsigned int current_element_in
        = std::atomic_load_explicit(&this->current_element_in,
                                    std::memory_order_relaxed);
      unsigned int next_element_out
        = std::atomic_load_explicit(&this->next_element_out,
                                    std::memory_order_relaxed);
      SDL_SetRenderDrawColor(debugrenderer, 0, 0, 0, 255);
      SDL_RenderClear(debugrenderer);
      SDL_SetRenderDrawColor(debugrenderer, 255, 255, 255, 255);
      SDL_Rect r;
      r.x = current_element_in * DEBUG_WINDOW_PIXEL_SIZE;
      r.y = 0;
      r.w = DEBUG_WINDOW_PIXEL_SIZE;
      r.h = DEBUG_WINDOW_HEIGHT / 2;
      SDL_RenderFillRect(debugrenderer, &r);
      r.x = (ELEMENT_COUNT - 1) * DEBUG_WINDOW_PIXEL_SIZE;
      r.y = DEBUG_WINDOW_HEIGHT / 2;
      r.w = DEBUG_WINDOW_PIXEL_SIZE;
      r.h = DEBUG_WINDOW_HEIGHT - DEBUG_WINDOW_HEIGHT / 2;
      SDL_RenderFillRect(debugrenderer, &r);
      SDL_SetRenderDrawColor(debugrenderer, 192, 128, 0, 255);
      if(current_element_in > next_element_out) {
        r.x = next_element_out * DEBUG_WINDOW_PIXEL_SIZE;
        r.y = 0;
        r.w = (current_element_in - next_element_out) *DEBUG_WINDOW_PIXEL_SIZE;
        r.h = DEBUG_WINDOW_HEIGHT / 2;
        SDL_RenderFillRect(debugrenderer, &r);
      }
      else if(current_element_in < next_element_out) {
        r.x = next_element_out * DEBUG_WINDOW_PIXEL_SIZE;
        r.y = 0;
        r.w = (ELEMENT_COUNT - next_element_out) *DEBUG_WINDOW_PIXEL_SIZE;
        r.h = DEBUG_WINDOW_HEIGHT / 2;
        SDL_RenderFillRect(debugrenderer, &r);
        r.x = 0;
        r.y = 0;
        r.w = current_element_in * DEBUG_WINDOW_PIXEL_SIZE;
        r.h = DEBUG_WINDOW_HEIGHT / 2;
        SDL_RenderFillRect(debugrenderer, &r);
      }
      int count;
      if(current_element_in > next_element_out)
        count = current_element_in - next_element_out;
      else if(current_element_in < next_element_out)
        count = (current_element_in + ELEMENT_COUNT) - next_element_out;
      else
        count = 0;
      if(count > target_max_queue_depth) {
        r.x = (ELEMENT_COUNT - 1 - count) * DEBUG_WINDOW_PIXEL_SIZE;
        r.y = DEBUG_WINDOW_HEIGHT / 2;
        r.w = (count - target_max_queue_depth) * DEBUG_WINDOW_PIXEL_SIZE;
        r.h = DEBUG_WINDOW_HEIGHT - DEBUG_WINDOW_HEIGHT / 2;
        SDL_SetRenderDrawColor(debugrenderer, 192, 64, 64, 255);
        SDL_RenderFillRect(debugrenderer, &r);
        count -= (count - target_max_queue_depth);
      }
      else if(target_max_queue_depth > count) {
        r.x = (ELEMENT_COUNT - 1 - target_max_queue_depth) * DEBUG_WINDOW_PIXEL_SIZE;
        r.y = DEBUG_WINDOW_HEIGHT / 2;
        r.w = (target_max_queue_depth - count) * DEBUG_WINDOW_PIXEL_SIZE;
        r.h = DEBUG_WINDOW_HEIGHT - DEBUG_WINDOW_HEIGHT / 2;
        SDL_SetRenderDrawColor(debugrenderer, 64, 64, 128, 255);
        SDL_RenderFillRect(debugrenderer, &r);
      }
      if(count > target_min_queue_depth) {
        r.x = (ELEMENT_COUNT - 1 - count) * DEBUG_WINDOW_PIXEL_SIZE;
        r.y = DEBUG_WINDOW_HEIGHT / 2;
        r.w = (count - target_min_queue_depth) * DEBUG_WINDOW_PIXEL_SIZE;
        r.h = DEBUG_WINDOW_HEIGHT - DEBUG_WINDOW_HEIGHT / 2;
        SDL_SetRenderDrawColor(debugrenderer, 64, 192, 64, 255);
        SDL_RenderFillRect(debugrenderer, &r);
        count -= (count - target_min_queue_depth);
        r.x = (ELEMENT_COUNT - 1 - count) * DEBUG_WINDOW_PIXEL_SIZE;
        r.y = DEBUG_WINDOW_HEIGHT / 2;
        r.w = count * DEBUG_WINDOW_PIXEL_SIZE;
        r.h = DEBUG_WINDOW_HEIGHT - DEBUG_WINDOW_HEIGHT / 2;
        SDL_SetRenderDrawColor(debugrenderer, 64, 128, 64, 255);
        SDL_RenderFillRect(debugrenderer, &r);
      }
      else if(count > 0) {
        r.x = (ELEMENT_COUNT - 1 - count) * DEBUG_WINDOW_PIXEL_SIZE;
        r.y = DEBUG_WINDOW_HEIGHT / 2;
        r.w = count * DEBUG_WINDOW_PIXEL_SIZE;
        r.h = DEBUG_WINDOW_HEIGHT - DEBUG_WINDOW_HEIGHT / 2;
        SDL_SetRenderDrawColor(debugrenderer, 192, 64, 64, 255);
        SDL_RenderFillRect(debugrenderer, &r);
      }
      SDL_RenderPresent(debugrenderer);
    }
#endif
    //// Call from CONSUMING THREAD ////
    int AvailableNumberOfElements() const {
      unsigned int right = std::atomic_load_explicit(&current_element_in,
                                                    std::memory_order_relaxed);
      unsigned int left = std::atomic_load_explicit(&next_element_out,
                                                    std::memory_order_relaxed);
      if(left > right) right += ELEMENT_COUNT;
      return right - left;
    }
    const float* ConsumeOneElement(bool& out_bumped) {
      unsigned int current_element_in
        = std::atomic_load_explicit(&this->current_element_in,
                                    std::memory_order_relaxed);
      unsigned int next_element_out
        = std::atomic_load_explicit(&this->next_element_out,
                                    std::memory_order_relaxed);
      if(next_element_out == current_element_in) {
        out_bumped = true;
        return &elements[(next_element_out == 0)
                         ? (ELEMENT_COUNT - 1) : (next_element_out - 1)][0];
      }
      else {
        auto ret = &elements[next_element_out][0];
        std::atomic_store_explicit(&this->next_element_out,
                                   next_element_out
                                   = (next_element_out + 1) % ELEMENT_COUNT,
                                   std::memory_order_relaxed);
        return ret;
      }
    }
  };
  std::unique_ptr<AudioQueue> audio_queue;
  constexpr float MIN_SPEAKER_SEPARATION = 1.f;
  constexpr float MAX_SPEAKER_SEPARATION = 180.f;
  constexpr int MIN_BUFFER_LEN = 64;
  constexpr int MAX_BUFFER_LEN = 2048;
  float prev_frame[4] = {0.f, 0.f, 0.f, 0.f};
  SDL_AudioDeviceID dev = 0;
  SDL_AudioSpec audiospec;
  // configuration options
  int desired_sound_type;
  float virtual_speaker_separation;
  int head_width_in_samples, desired_sdl_buffer_length, resample_quality,
    desired_sample_rate, audio_sync_type;
  // dependent on configuration
  int active_sound_type;
  const Config::Element audio_elements[] = {
    {"desired_sound_type", desired_sound_type},
    {"desired_sdl_buffer_length", desired_sdl_buffer_length},
    {"virtual_speaker_separation", virtual_speaker_separation},
    {"head_width_in_samples", head_width_in_samples},
    // don't expose this right now, since it doesn't do anything
    //{"resample_quality", resample_quality},
    {"desired_sample_rate", desired_sample_rate},
    {"audio_sync_type", audio_sync_type},
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
      if(desired_sdl_buffer_length < MIN_BUFFER_LEN)
        desired_sdl_buffer_length = MIN_BUFFER_LEN;
      else if(desired_sdl_buffer_length > MAX_BUFFER_LEN)
        desired_sdl_buffer_length = MAX_BUFFER_LEN;
      if(head_width_in_samples < 1)
        head_width_in_samples = 1;
      else if(head_width_in_samples > SAMPLE_RATE)
        head_width_in_samples = SAMPLE_RATE;
      if(desired_sample_rate > 192000) desired_sample_rate = 192000;
      else if(desired_sample_rate < 8000) desired_sample_rate = 8000;
      if(audio_sync_type < 0) audio_sync_type = 0;
      else if(audio_sync_type >= NUM_SYNC_TYPES)
        audio_sync_type = NUM_SYNC_TYPES-1;
      if(desired_sound_type < 0) desired_sound_type = 0;
      else if(desired_sound_type >= NUM_SOUND_TYPES)
        desired_sound_type = NUM_SOUND_TYPES-1;
    }
    void Save() override {
      Config::Write("Audio.utxt",
                    audio_elements, elementcount(audio_elements));
    }
    void Defaults() override {
      desired_sound_type = SOUND_TYPE_STEREO;
      virtual_speaker_separation = 90;
      head_width_in_samples = 30;
      desired_sdl_buffer_length = 1024; // about 20ms at 48KHz
      desired_sample_rate = 48000;
      resample_quality = 0; // NN when close, better otherwise
      audio_sync_type = SYNC_DYNAMIC;
    }
  } audioPrefsLogic;
  // ET209 output: Center, Right, Left, Boost
  void get_frame(float* out_frame) {
    int16_t raw_frame[4];
    ARS::apu.output_frame(raw_frame);
    switch(REQUIRED_SOURCE_CHANNELS[active_sound_type]) {
    case 1:
      // preserve volumes
      out_frame[0] = (raw_frame[0]+raw_frame[1]+raw_frame[2]+2*raw_frame[3])
        * ET209_OUTPUT_TO_FLOAT_SAMPLE * 0.5f;
      break;
    case 2:
      // do exactly what a "real" ARS would do
      out_frame[0] = (raw_frame[2]+(raw_frame[0]>>1)+raw_frame[3])
        * ET209_OUTPUT_TO_FLOAT_SAMPLE;
      out_frame[1] = (raw_frame[1]+(raw_frame[0]>>1)+raw_frame[3])
        * ET209_OUTPUT_TO_FLOAT_SAMPLE;
      out_frame[0] += (prev_frame[0] - out_frame[0]) * DAC_FILTER_COEFFICIENT;
      out_frame[1] += (prev_frame[1] - out_frame[1]) * DAC_FILTER_COEFFICIENT;
      break;
    case 3:
      // let's start rearranging things with tweezers now
      out_frame[0] = raw_frame[2] * ET209_OUTPUT_TO_FLOAT_SAMPLE;
      out_frame[1] = raw_frame[1] * ET209_OUTPUT_TO_FLOAT_SAMPLE;
      out_frame[2] = ((raw_frame[0]>>1) + raw_frame[3])
        * ET209_OUTPUT_TO_FLOAT_SAMPLE;
      out_frame[0] += (prev_frame[0] - out_frame[0]) * DAC_FILTER_COEFFICIENT;
      out_frame[1] += (prev_frame[1] - out_frame[1]) * DAC_FILTER_COEFFICIENT;
      out_frame[2] += (prev_frame[2] - out_frame[2]) * DAC_FILTER_COEFFICIENT;
      break;
    case 4:
      // the onion has blossomed
      out_frame[0] = raw_frame[2] * ET209_OUTPUT_TO_FLOAT_SAMPLE;
      out_frame[1] = raw_frame[1] * ET209_OUTPUT_TO_FLOAT_SAMPLE;
      out_frame[2] = ((raw_frame[0]>>1) + raw_frame[3])
        * ET209_OUTPUT_TO_FLOAT_SAMPLE;
      // TODO: Selectable LFE ratios
      out_frame[3] = raw_frame[3] * ET209_OUTPUT_TO_FLOAT_SAMPLE;
      out_frame[0] += (prev_frame[0] - out_frame[0]) * DAC_FILTER_COEFFICIENT;
      out_frame[1] += (prev_frame[1] - out_frame[1]) * DAC_FILTER_COEFFICIENT;
      out_frame[2] += (prev_frame[2] - out_frame[2]) * DAC_FILTER_COEFFICIENT;
      out_frame[3] += (prev_frame[3] - out_frame[3]) * LFE_FILTER_COEFFICIENT;
      break;
    }
  }
  int mix_out(const float* inp, float* outp, size_t amount_to_out) {
    const int SRCC = REQUIRED_SOURCE_CHANNELS[active_sound_type];
    const int DSTC = audiospec.channels;
    float* outp_start = outp;
    amount_to_out /= SRCC;
    while(amount_to_out-- > 0) {
      (*mixer)(inp, outp);
      inp += SRCC;
      outp += DSTC;
    }
    return outp - outp_start;
  }
  void drain_queue_dynamic(void* _ud, Uint8* _stream, int bytes) {
    AudioQueue* audio_queue = reinterpret_cast<AudioQueue*>(_ud);
    float* outp = reinterpret_cast<float*>(_stream);
    int rem = (bytes / sizeof(float))
      * REQUIRED_SOURCE_CHANNELS[active_sound_type] / audiospec.channels;
    bool bumped = false;
    int effective_depth = audio_queue->AvailableNumberOfElements();
    if(cur_in_rate < 0) {
      target_in_rate = SAMPLE_RATE;
    }
    else {
      hysteresis_accum
        += MINIMUM_PERMITTED_SAMPLE_RATE
        + (MAXIMUM_PERMITTED_SAMPLE_RATE - MINIMUM_PERMITTED_SAMPLE_RATE)
        * (effective_depth - target_min_queue_depth)
        / (target_max_queue_depth - target_min_queue_depth);
      if(++hysteresis_count >= SAMPLE_RATE_HYSTERESIS) {
        float new_rate = hysteresis_accum / SAMPLE_RATE_HYSTERESIS;
        if(new_rate < MINIMUM_PERMITTED_SAMPLE_RATE)
          new_rate = MINIMUM_PERMITTED_SAMPLE_RATE;
        else if(new_rate > MAXIMUM_PERMITTED_SAMPLE_RATE)
          new_rate = MAXIMUM_PERMITTED_SAMPLE_RATE;
        if(std::abs(new_rate - target_in_rate) >= 1)
          target_in_rate = new_rate;
        hysteresis_accum = 0;
        hysteresis_count = 0;
      }
    }
    if(cur_in_rate != target_in_rate) {
      if(cur_cvt) prev_cvt = std::move(cur_cvt);
      cur_cvt.reset();
      cur_in_rate = target_in_rate;
    }
    if(!cur_cvt) {
      if(prev_cvt) {
        auto rem_in_cvt = prev_cvt->GetNumberOfSamplesRemaining();
        if(rem_in_cvt > 0) {
          int amount_to_out = std::min(rem, rem_in_cvt);
          int outputted = mix_out(prev_cvt->GetPointerToOutput(),
                                  outp, amount_to_out);
          prev_cvt->AdvanceOutput(amount_to_out);
          outp += outputted;
          rem -= rem_in_cvt;
        }
      }
#if !NO_DEBUG_CORES
      if(ARS::debugging_audio)
        std::cerr << "New sample rate: " << target_in_rate << "\n";
#endif
      cur_cvt = MakeAudioCvt(resample_quality,
                             target_in_rate,
                             audiospec.freq,
                             AudioQueue::ELEMENT_SIZE,
                             REQUIRED_SOURCE_CHANNELS[active_sound_type]);
      if(prev_cvt) {
        if(cur_cvt->NeedsLap() || prev_cvt->NeedsLap()) {
          const float* ap = audio_queue->ConsumeOneElement(bumped);
          prev_cvt->ConvertMore(ap);
          cur_cvt->ConvertMore(ap);
          cur_cvt->CrosslapAwayFrom(*prev_cvt);
        }
        prev_cvt.reset();
      }
    }
    while(rem > 0) {
      auto rem_in_cvt = cur_cvt->GetNumberOfSamplesRemaining();
      if(rem_in_cvt == 0)
        cur_cvt->ConvertMore(audio_queue->ConsumeOneElement(bumped));
      else {
        int amount_to_out = std::min(rem, rem_in_cvt);
        int outputted = mix_out(cur_cvt->GetPointerToOutput(),
                                outp, amount_to_out);
        cur_cvt->AdvanceOutput(amount_to_out);
        outp += outputted;
        rem -= rem_in_cvt;
      }
    }
    if(bumped) {
#if !NO_DEBUG_CORES
      if(ARS::debugging_audio)
        std::cerr << SDL_GetTicks() << ": audio queue underrun!\n";
#endif
      SDL_PauseAudioDevice(dev, 1);
      autopaused = true;
    }
  }
  void drain_queue_static(void* _ud, Uint8* _stream, int bytes) {
    AudioQueue* audio_queue = reinterpret_cast<AudioQueue*>(_ud);
    float* outp = reinterpret_cast<float*>(_stream);
    int rem = (bytes / sizeof(float))
      * REQUIRED_SOURCE_CHANNELS[active_sound_type] / audiospec.channels;
    bool bumped = false;
    if(!cur_cvt) {
      cur_cvt = MakeAudioCvt(resample_quality,
                             SAMPLE_RATE,
                             audiospec.freq,
                             AudioQueue::ELEMENT_SIZE,
                             REQUIRED_SOURCE_CHANNELS[active_sound_type]);
    }
    while(rem > 0) {
      auto rem_in_cvt = cur_cvt->GetNumberOfSamplesRemaining();
      if(rem_in_cvt == 0) {
        cur_cvt->ConvertMore(audio_queue->ConsumeOneElement(bumped));
      }
      else {
        int amount_to_out = std::min(rem, rem_in_cvt);
        int outputted = mix_out(cur_cvt->GetPointerToOutput(),
                                outp, amount_to_out);
        cur_cvt->AdvanceOutput(amount_to_out);
        outp += outputted;
        rem -= rem_in_cvt;
      }
    }
    if(bumped) {
#if !NO_DEBUG_CORES
      if(ARS::debugging_audio)
        std::cerr << SDL_GetTicks() << ": audio queue underrun!\n";
#endif
      SDL_PauseAudioDevice(dev, 1);
      autopaused = true;
    }
  }
  void make_and_convert_lots_of_samples(void*, Uint8* _stream, int bytes) {
    if(!cur_cvt) {
      cur_cvt = MakeAudioCvt(resample_quality,
                             SAMPLE_RATE,
                             audiospec.freq,
                             AudioQueue::ELEMENT_SIZE,
                             REQUIRED_SOURCE_CHANNELS[active_sound_type]);
    }
    const int SRCC = REQUIRED_SOURCE_CHANNELS[active_sound_type];
    float* outp = reinterpret_cast<float*>(_stream);
    int rem = (bytes / sizeof(float))
      * REQUIRED_SOURCE_CHANNELS[active_sound_type] / audiospec.channels;
    while(rem > 0) {
      auto rem_in_cvt = cur_cvt->GetNumberOfSamplesRemaining();
      if(rem_in_cvt == 0) {
        float buf[AudioQueue::ELEMENT_SIZE];
        float* localp = buf;
        int localrem = AudioQueue::ELEMENT_SIZE / SRCC;
        while(localrem-- > 0) {
          get_frame(localp);
          localp += SRCC;
        }
        cur_cvt->ConvertMore(buf);
      }
      else {
        int amount_to_out = std::min(rem, rem_in_cvt);
        int outputted = mix_out(cur_cvt->GetPointerToOutput(),
                                outp, amount_to_out);
        cur_cvt->AdvanceOutput(amount_to_out);
        outp += outputted;
        rem -= rem_in_cvt;
      }
    }
  }
  void make_lots_of_samples(void*, Uint8* _stream, int bytes) {
    float raw_frame[4];
    float* outp = reinterpret_cast<float*>(_stream);
    int rem = bytes / sizeof(float);
    while(rem > 0) {
      get_frame(raw_frame);
      (*mixer)(raw_frame, outp);
      outp += audiospec.channels;
      rem -= audiospec.channels;
    }
  }
#if !NO_DEBUG_CORES
  void debug_event(SDL_Event& evt) {
    // er...
    switch(evt.type) {
    default:
      if(evt.type == Windower::UPDATE_EVENT)
        audio_queue->debug_update();
    }
  }
#endif
  class NullMixer : public Mixer {
  public:
    void operator()(const float*, float* out) override {
      memset(out, 0, sizeof(float)*audiospec.channels);
    }
  };
  class MonoMixer : public Mixer {
  public:
    void operator()(const float* in, float* out) override {
      out[0] = in[0];
    }
  };
  class StereoMixer : public Mixer {
  public:
    void operator()(const float* in, float* out) override {
      out[0] = in[0];
      out[1] = in[1];
    }
  };
  class HeadphonesMixer : public Mixer {
    float pan_filter_coefficient;
    std::vector<float> stereo_delay_buf;
    size_t stereo_delay_pos;
    float prev_stereo_samples[2];
  public:
    HeadphonesMixer() {
      /* Simulate a sort of idealized version of 3.0 surround */
      float other_speaker_angle = (virtual_speaker_separation/2)
        *3.141592653589793f/180;
      float cutoff_freq = 1000 * pow(1 / std::sin(other_speaker_angle),
                                     1.f / 0.8f);
      float tau = 1.f / (3.14159265358f * 2.0f * cutoff_freq);
      pan_filter_coefficient = std::exp(-1/(SAMPLE_RATE*tau));
      float stereo_delay
        = std::floor(head_width_in_samples
                     * std::sin(other_speaker_angle)+0.5f);
      int stereo_delay_samples;
      if(stereo_delay > SAMPLE_RATE) stereo_delay_samples = SAMPLE_RATE;
      else if(stereo_delay <= 1) stereo_delay_samples = 1;
      else stereo_delay_samples = static_cast<int>(stereo_delay);
      stereo_delay_buf.resize(stereo_delay_samples * 2);
      stereo_delay_pos = 0;
    }
    void operator()(const float* in, float* out) override {
      out[0] = in[0] + in[2]*2 + stereo_delay_buf[stereo_delay_pos+0];
      out[1] = in[1] + in[2]*2 + stereo_delay_buf[stereo_delay_pos+1];
      prev_stereo_samples[1] = in[0] + (prev_stereo_samples[1] - in[0])
        * pan_filter_coefficient;
      prev_stereo_samples[0] = in[1] + (prev_stereo_samples[0] - in[1])
        * pan_filter_coefficient;
      stereo_delay_buf[stereo_delay_pos++] = prev_stereo_samples[0];
      stereo_delay_buf[stereo_delay_pos++] = prev_stereo_samples[1];
      stereo_delay_pos += 2;
      if(stereo_delay_pos >= stereo_delay_buf.size())
        stereo_delay_pos = 0;
    }
  };
  class QuadMixer : public Mixer {
  public:
    void operator()(const float* in, float* out) override {
      out[0] = in[0] * 0.5f + in[2] * 0.5f;
      out[1] = in[1] * 0.5f + in[2] * 0.5f;
      out[2] = in[0] * 0.5f;
      out[3] = in[1] * 0.5f;
    }
  };
  class Surround31Mixer : public Mixer {
  public:
    void operator()(const float* in, float* out) override {
      out[0] = in[0];
      out[1] = in[1];
      out[2] = in[2];
      out[3] = in[3];
    }
  };
  class Surround40Mixer : public Mixer {
  public:
    void operator()(const float* in, float* out) override {
      out[0] = in[0] * 0.5f;
      out[1] = in[1] * 0.5f;
      out[2] = in[2];
      out[3] = (in[0] + in[1]) * 0.5f;
    }
  };
  class Surround51Mixer : public Mixer {
  public:
    void operator()(const float* in, float* out) override {
      out[0] = in[0] / 1.5f;
      out[1] = in[1] / 1.5f;
      out[2] = in[2];
      out[3] = in[3];
      out[4] = in[0] * 0.5f / 1.5f;
      out[5] = in[1] * 0.5f / 1.5f;
    }
  };
  class Surround71Mixer : public Mixer {
  public:
    void operator()(const float* in, float* out) override {
      out[0] = in[0] * 0.25f;
      out[1] = in[1] * 0.25f;
      out[2] = in[2];
      out[3] = in[3];
      out[4] = in[0] * 0.25f;
      out[5] = in[1] * 0.25f;
      out[6] = in[0] * 0.5f;
      out[7] = in[1] * 0.5f;
    }
  };
}

ET209 ARS::apu;

void ARS::init_apu() {
  if(dev != 0) SDL_CloseAudioDevice(dev);
  active_sound_type = desired_sound_type;
  do {
    SDL_AudioSpec desired;
    memset(&desired, 0, sizeof(desired));
    memset(&audiospec, 0, sizeof(audiospec));
    desired.freq = desired_sample_rate;
    desired.format = AUDIO_F32SYS;
    desired.channels = REQUIRED_HARD_CHANNELS[active_sound_type];
    desired.samples = desired_sdl_buffer_length;
    switch(audio_sync_type) {
    case SYNC_NONE:
      audio_queue.reset();
      if(desired.freq > MINIMUM_PERMITTED_SAMPLE_RATE
         && desired.freq < MAXIMUM_PERMITTED_SAMPLE_RATE)
        desired.callback = make_lots_of_samples;
      else
        desired.callback = make_and_convert_lots_of_samples;
      break;
    case SYNC_STATIC:
    case SYNC_DYNAMIC:
      if(!audio_queue) audio_queue = std::make_unique<AudioQueue>();
      else audio_queue->ClearQueue();
      desired.callback = audio_sync_type == SYNC_DYNAMIC ? drain_queue_dynamic
        : drain_queue_static;
      desired.userdata = audio_queue.get();
    }
    cur_cvt.reset();
    prev_cvt.reset();
    dev = SDL_OpenAudioDevice(nullptr, 0,
                              &desired, &audiospec,
                              SDL_AUDIO_ALLOW_FREQUENCY_CHANGE
                              |SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if(dev == 0) {
      if(active_sound_type > SOUND_TYPE_STEREO)
        active_sound_type = SOUND_TYPE_STEREO;
      else if(active_sound_type > SOUND_TYPE_MONO)
        active_sound_type = SOUND_TYPE_MONO;
      else
        break;
    }
  } while(dev == 0);
  if(dev == 0) {
    ui << SDL_GetError() << ui;
    return;
  }
  if(audiospec.channels != REQUIRED_HARD_CHANNELS[desired_sound_type]) {
    assert(audiospec.channels == 1
           || (audiospec.channels >= 2 && audiospec.channels%2 == 0));
    for(active_sound_type = 0;
        audiospec.channels != REQUIRED_HARD_CHANNELS[active_sound_type];
        ++active_sound_type) {}
  }
  if(active_sound_type != desired_sound_type) {
    ui << sn.Get("AUDIO_TYPE_DOWNGRADED_TO"_Key,
                 {sn.Get(SOUND_TYPE_KEYS[active_sound_type])}) << ui;
  }
  switch(active_sound_type) {
  case SOUND_TYPE_MONO:
    mixer = std::make_unique<MonoMixer>();
    break;
  case SOUND_TYPE_STEREO:
    mixer = std::make_unique<StereoMixer>();
    break;
  case SOUND_TYPE_HEADPHONES:
    mixer = std::make_unique<HeadphonesMixer>();
    break;
  case SOUND_TYPE_QUADRAPHONIC:
    mixer = std::make_unique<QuadMixer>();
    break;
  case SOUND_TYPE_SURROUND_31:
    mixer = std::make_unique<Surround31Mixer>();
    break;
  case SOUND_TYPE_SURROUND_40:
    mixer = std::make_unique<Surround40Mixer>();
    break;
  case SOUND_TYPE_SURROUND_51:
    mixer = std::make_unique<Surround51Mixer>();
    break;
  case SOUND_TYPE_SURROUND_71:
    mixer = std::make_unique<Surround71Mixer>();
    break;
  default:
    mixer = std::make_unique<NullMixer>();
    break;
  }
  switch(audio_sync_type) {
  case SYNC_NONE:
    break;
  case SYNC_STATIC:
    target_min_queue_depth =
      // bare minimum
      1
      // at least one buffer
      + ((audiospec.samples*static_cast<int>(SAMPLE_RATE)/audiospec.freq)+AudioQueue::ELEMENT_SIZE-1)
      / AudioQueue::ELEMENT_SIZE
      // at least one frame
      + AudioQueue::APPROX_ELEMENTS_PER_FRAME;
    target_min_queue_depth *= audiospec.channels;
    target_max_queue_depth
      = AudioQueue::ELEMENT_COUNT-2;
    break;
  case SYNC_DYNAMIC:
    cur_in_rate = -1.f;
    hysteresis_accum = 0;
    hysteresis_count = 0;
    target_min_queue_depth =
      // bare minimum
      1
      // at least one buffer
      + ((audiospec.samples*static_cast<int>(SAMPLE_RATE)/audiospec.freq)+AudioQueue::ELEMENT_SIZE-1)
      / AudioQueue::ELEMENT_SIZE
      // at least one frame
      + AudioQueue::APPROX_ELEMENTS_PER_FRAME;
    target_min_queue_depth *= audiospec.channels;
    target_max_queue_depth =
      std::min((
      // bare minimum
      2
      // at least two buffer
      + ((audiospec.samples*static_cast<int>(SAMPLE_RATE)/audiospec.freq)+AudioQueue::ELEMENT_SIZE-1)
      / AudioQueue::ELEMENT_SIZE * 2
      // at least three frame
      + AudioQueue::APPROX_ELEMENTS_PER_FRAME * 3)
               * audiospec.channels,
      AudioQueue::ELEMENT_COUNT - 2);
    break;
  }
#if !NO_DEBUG_CORES
  if(audio_sync_type == SYNC_NONE) {
    if(debugwindow != nullptr) {
      Windower::Unregister(SDL_GetWindowID(debugwindow));
      SDL_DestroyWindow(debugwindow);
      SDL_DestroyRenderer(debugrenderer);
      debugwindow = nullptr;
    }
  }
  else {
    if(debugging_audio && debugwindow == nullptr) {
      debugwindow = SDL_CreateWindow("ARS-emu Audio Queue",
                                     SDL_WINDOWPOS_UNDEFINED,
                                     SDL_WINDOWPOS_UNDEFINED,
                                     DEBUG_WINDOW_PIXEL_SIZE
                                     *AudioQueue::ELEMENT_COUNT,
                                     DEBUG_WINDOW_HEIGHT,
                                     0);
      if(debugwindow == nullptr)
        die("Couldn't create audio debugging window: %s", SDL_GetError());
      Windower::Register(SDL_GetWindowID(debugwindow), debug_event);
      debugrenderer = SDL_CreateRenderer(debugwindow, -1, 0);
      if(debugrenderer == nullptr)
        die("Couldn't create audio debugging renderer: %s", SDL_GetError());
    }
  }
#endif
  if(desired_sdl_buffer_length != audiospec.samples) {
    std::string driver = SDL_GetCurrentAudioDriver();
    if(driver == "pulseaudio"
       && audiospec.samples == desired_sdl_buffer_length / 2)
      ; // ignore it, Pulse always does that(?!)
    else
      sn.Out(std::cerr, "AUDIO_BUFFER_SIZE_MISMATCH"_Key,
             {TEG::format("%i", desired_sdl_buffer_length),
                 TEG::format("%i", static_cast<int>(audiospec.samples))});
#if !NO_DEBUG_CORES
    if(debugging_audio) {
      std::cout << "Sample rate: " << audiospec.freq << "\n";
      std::cout << "Channels: " << (int)audiospec.channels << "\n";
      std::cout << "Buffer size: " << audiospec.samples << " ("
                << audiospec.size << " bytes, "
                << ((audiospec.samples * 2000) / audiospec.freq + 1) / 2
                << "ms)\n";
      if(audio_sync_type == SYNC_DYNAMIC)
        std::cout << "Queue depth target: " << target_min_queue_depth
                  << "-" << target_max_queue_depth
                  << "/" << (AudioQueue::ELEMENT_COUNT-1) << "\n";
      else
        std::cout << "(Not using the queue; dynamic audio sync not enabled)\n";
    }
#endif
  }
  if(audio_sync_type == SYNC_NONE) {
    autopaused = false;
    SDL_PauseAudioDevice(dev, 0);
  }
  else autopaused = true;
}

void ARS::output_apu_sample() {
  if(dev <= 0 || audio_sync_type == SYNC_NONE) return;
  float samples[4];
  get_frame(samples);
  audio_queue->AddSamplesToQueue(samples, REQUIRED_SOURCE_CHANNELS[active_sound_type]);
  if(autopaused
     && audio_queue->AvailableNumberOfElements() > target_min_queue_depth) {
    autopaused = false;
    SDL_PauseAudioDevice(dev, 0);
  }
}

std::shared_ptr<Menu> Menu::createAudioMenu() {
  static std::vector<int> samplerates = {
    22050, 24000, 32000, 44100, 48000
  };
  static std::vector<std::string> samplerate_strings;
  if(samplerate_strings.size() == 0) {
    samplerate_strings.reserve(samplerates.size());
    for(auto i : samplerates) {
      samplerate_strings.emplace_back(TEG::format("%iHz", i));
    }
  }
  size_t cur_samplerate = 0;
  for(size_t i = 0; i < samplerates.size(); ++i) {
    if(samplerates[i] == desired_sample_rate) {
      cur_samplerate = i;
      break;
    }
  }
  static std::vector<std::string> sound_types;
  sound_types.reserve(NUM_SOUND_TYPES);
  for(auto& key : SOUND_TYPE_KEYS) {
    sound_types.emplace_back(sn.Get(key));
  }
  std::vector<std::shared_ptr<Menu::Item> > items;
  items.emplace_back(new Menu::Selector(sn.Get("AUDIO_MENU_SOUND_TYPE"_Key),
                                        sound_types,
                                        desired_sound_type,
                                        [](size_t opt) {
                                          desired_sound_type = opt;
                                          ARS::init_apu();
                                        }));
  items.emplace_back(new Menu::Selector(sn.Get("AUDIO_MENU_SYNC_TYPE"_Key),
                                        {sn.Get("AUDIO_MENU_SYNC_NONE"_Key),
                                         sn.Get("AUDIO_MENU_SYNC_STATIC"_Key),
                                        sn.Get("AUDIO_MENU_SYNC_DYNAMIC"_Key)},
                                        audio_sync_type,
                                        [](size_t opt) {
                                          audio_sync_type = opt;
                                          ARS::init_apu();
                                        }));
  items.emplace_back(new Menu::Selector(sn.Get("AUDIO_MENU_SAMPLERATE"_Key),
                                        samplerate_strings,
                                        cur_samplerate,
                                        [](size_t opt) {
                                          desired_sample_rate=samplerates[opt];
                                          ARS::init_apu();
                                        }));
  items.emplace_back(new Menu::Divider());
  items.emplace_back(new Menu::BackButton(sn.Get("GENERIC_MENU_FINISHED_LABEL"_Key)));
  return std::make_shared<Menu>(sn.Get("AUDIO_MENU_TITLE"_Key), items);
}
