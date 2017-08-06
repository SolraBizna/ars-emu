#include "audiocvt.hh"

#include <cmath>

namespace {
  constexpr float SAMPLE_RATE = 47988.28125f;
  constexpr float MINIMUM_CLOSE_SAMPLE_RATE = SAMPLE_RATE * 0.99f;
  constexpr float MAXIMUM_CLOSE_SAMPLE_RATE = SAMPLE_RATE * 1.01f;
  template<int CHANNELS> class NNCvt : public AudioCvt {
    float dda_acc, dda_num, dda_den;
  public:
    NNCvt(int number_of_input_frames,
          float source_rate, float dest_rate)
      : AudioCvt(number_of_input_frames, CHANNELS,
                 static_cast<int>(std::ceil(number_of_input_frames
                                            *dest_rate/source_rate))),
        dda_acc(0), dda_num(source_rate), dda_den(dest_rate) {}
    ~NNCvt() {}
    void ConvertMore(const float* in) override {
      const float* inp = in;
      const float* endp = inp + number_of_input_frames * CHANNELS;
      float* outp = output_buffer;
      while(inp < endp) {
        for(int n = 0; n < CHANNELS; ++n)
          *outp++ = inp[n];
        dda_acc += dda_num;
        while(dda_acc >= dda_den && inp < endp) {
          dda_acc -= dda_den;
          inp += CHANNELS;
        }
      }
      position_in_output_buffer = output_buffer;
      end_of_output_buffer = outp;
    }
    bool NeedsLap() override {
      // the discontinuity from concatentating two NNCvts will be less than a
      // single sample in size
      return false;
    }
  };
  // Originally, we planned to use SDL_AudioCVT. As it turns out, SDL_AudioCVT
  // has numerous technical issues that prevent us from using it.
}

std::unique_ptr<AudioCvt> MakeAudioCvt(int quality_hint,
                                       float source_rate,
                                       float dest_rate,
                                       int number_of_input_samples,
                                       int input_channels) {
  if(true
     || quality_hint < 0
     || (quality_hint == 0
         && source_rate > MINIMUM_CLOSE_SAMPLE_RATE
         && source_rate < MAXIMUM_CLOSE_SAMPLE_RATE)) {
    // nearest neighbor
    switch(input_channels) {
    case 1:
      return std::make_unique<NNCvt<1>>
        (number_of_input_samples, source_rate, dest_rate);
    case 2:
      return std::make_unique<NNCvt<2>>
        (number_of_input_samples/2, source_rate, dest_rate);
    case 3:
      return std::make_unique<NNCvt<3>>
        (number_of_input_samples/3, source_rate, dest_rate);
    case 4:
      return std::make_unique<NNCvt<4>>
        (number_of_input_samples/4, source_rate, dest_rate);
    default:
      die("Internal error: wrong number of channels for MakeAudioCvt (%i)",
          input_channels);
    }
  }
  else {
    // TODO: other resamplers
  }
}
