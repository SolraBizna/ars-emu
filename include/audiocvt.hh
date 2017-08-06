#ifndef AUDIOCVTHH
#define AUDIOCVTHH

#include "teg.hh"

#include <memory>

class AudioCvt {
protected:
  const int number_of_input_frames;
  const int maximum_number_of_output_frames;
  const int input_channels;
  float* output_buffer;
  float* position_in_output_buffer, *end_of_output_buffer;
  AudioCvt(int number_of_input_frames, int input_channels,
           int maximum_number_of_output_frames)
    : number_of_input_frames(number_of_input_frames),
      maximum_number_of_output_frames(maximum_number_of_output_frames),
      input_channels(input_channels),
      output_buffer(reinterpret_cast<float*>
                    (safe_malloc(maximum_number_of_output_frames
                                 * input_channels
                                 * sizeof(float)))),
      position_in_output_buffer(nullptr),
      end_of_output_buffer(nullptr) {}
  void AdjustOutputBuffer(size_t new_size) {
    output_buffer = reinterpret_cast<float*>
      (safe_realloc(output_buffer, new_size));
  }
public:
  virtual ~AudioCvt() {}
  virtual void ConvertMore(const float* in) = 0;
  virtual bool NeedsLap() = 0;
  void CrosslapAwayFrom(AudioCvt& other) {
    assert(other.input_channels == input_channels);
    int amount_to_lap = std::min(GetNumberOfSamplesRemaining(),
                                 other.GetNumberOfSamplesRemaining());
    assert(amount_to_lap > 1);
    float* ap = position_in_output_buffer;
    const float* bp = other.position_in_output_buffer;
    for(int i = 0; i < amount_to_lap; ++i) {
      for(int j = 0; j < input_channels; ++j) {
        *ap = *bp + (*ap - *bp) * i / amount_to_lap;
        ++ap;
        ++bp;
      }
    }
  }
  int GetNumberOfSamplesRemaining() const {
    return (end_of_output_buffer - position_in_output_buffer);
  }
  const float* GetPointerToOutput() const {
    return position_in_output_buffer;
  }
  void AdvanceOutput(int amount_to_advance) {
    assert(GetNumberOfSamplesRemaining() >= amount_to_advance);
    position_in_output_buffer += amount_to_advance;
  }
};

// supported channel counts: 1 2 3 4
std::unique_ptr<AudioCvt> MakeAudioCvt(int quality_hint,
                                       float source_rate,
                                       float dest_rate,
                                       int number_of_input_samples,
                                       bool input_is_stereo);

#endif
