#pragma once

#include <math.h>
#include <stdint.h>

class FlangerEffect {
 public:
  int16_t process(int16_t input) {
    delayBuffer_[writeIndex_] = input;

    const float delaySamples =
        BASE_DELAY_SAMPLES + DEPTH_SAMPLES * sinf(phase_);
    float readPosition = static_cast<float>(writeIndex_) - delaySamples;
    if (readPosition < 0.0f) {
      readPosition += BUFFER_SIZE;
    }

    const uint16_t firstIndex = static_cast<uint16_t>(readPosition);
    const uint16_t secondIndex = (firstIndex + 1) % BUFFER_SIZE;
    const float fraction = readPosition - firstIndex;
    const float delayed =
        delayBuffer_[firstIndex] +
        fraction * (delayBuffer_[secondIndex] - delayBuffer_[firstIndex]);

    const float feedbackSample = input + delayed * FEEDBACK;
    delayBuffer_[writeIndex_] = clampToInt16(feedbackSample);

    phase_ += PHASE_INCREMENT;
    if (phase_ >= FLANGER_TWO_PI) {
      phase_ -= FLANGER_TWO_PI;
    }
    writeIndex_ = (writeIndex_ + 1) % BUFFER_SIZE;

    return clampToInt16(input * (1.0f - WET_MIX) + delayed * WET_MIX);
  }

 private:
  static constexpr uint16_t BUFFER_SIZE = 512;
  static constexpr float SAMPLE_RATE_HZ = 48000.0f;
  static constexpr float BASE_DELAY_SAMPLES = 3.0f * SAMPLE_RATE_HZ / 1000.0f;
  static constexpr float DEPTH_SAMPLES = 2.0f * SAMPLE_RATE_HZ / 1000.0f;
  static constexpr float MODULATION_RATE_HZ = 0.25f;
  static constexpr float PHASE_INCREMENT =
      6.28318530718f * MODULATION_RATE_HZ / SAMPLE_RATE_HZ;
  static constexpr float FLANGER_TWO_PI = 6.28318530718f;
  static constexpr float FEEDBACK = 0.25f;
  static constexpr float WET_MIX = 0.5f;

  int16_t delayBuffer_[BUFFER_SIZE] = {};
  uint16_t writeIndex_ = 0;
  float phase_ = 0.0f;

  static int16_t clampToInt16(float sample) {
    if (sample >= 32767.0f) {
      return 32767;
    }
    if (sample <= -32768.0f) {
      return -32768;
    }
    return static_cast<int16_t>(sample);
  }
};
