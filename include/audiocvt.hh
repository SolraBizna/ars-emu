#ifndef AUDIOCVTHH
#define AUDIOCVTHH

#include "teg.hh"

#include <memory>

class AudioCvt {
protected:
  const int number_of_input_frames;
  const int maximum_number_of_output_frames;
  const bool input_is_stereo;
  float* output_buffer;
  float* position_in_output_buffer, *end_of_output_buffer;
  AudioCvt(int number_of_input_frames, bool input_is_stereo,
           int maximum_number_of_output_frames)
    : number_of_input_frames(number_of_input_frames),
      maximum_number_of_output_frames(maximum_number_of_output_frames),
      input_is_stereo(input_is_stereo),
      output_buffer(reinterpret_cast<float*>
                    (safe_malloc(maximum_number_of_output_frames
                                 * (input_is_stereo ? 2 : 1)
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
    assert(other.input_is_stereo == input_is_stereo);
    int amount_to_lap = std::min(GetNumberOfSamplesRemaining(),
                                 other.GetNumberOfSamplesRemaining());
    assert(amount_to_lap > 1);
    // it's okay that we ramp differently in left and right, the difference is
    // slight and the code is simpler and faster
    for(int i = 0; i < amount_to_lap; ++i) {
      position_in_output_buffer[i]
        = other.position_in_output_buffer[i]
        + (position_in_output_buffer[i] - other.position_in_output_buffer[i])
        * i / amount_to_lap;
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

std::unique_ptr<AudioCvt> MakeAudioCvt(int quality_hint,
                                       float source_rate,
                                       float dest_rate,
                                       int number_of_input_samples,
                                       bool input_is_stereo);

#endif
