#pragma once

#include <math.h>
#include <stdint.h>

class ChorusEffect {
 public:
  static constexpr float DEFAULT_RATE_HZ = 0.8f;
  static constexpr float DEFAULT_DEPTH_MS = 3.0f;
  static constexpr float DEFAULT_MIX = 0.25f;

  bool setRateHz(float rateHz) {
    if (!(rateHz >= 0.1f && rateHz <= 5.0f)) {
      return false;
    }
    phaseIncrement_ = CHORUS_TWO_PI * rateHz / SAMPLE_RATE_HZ;
    return true;
  }

  bool setDepthMs(float depthMs) {
    if (!(depthMs >= 0.5f && depthMs <= 5.0f)) {
      return false;
    }
    depthSamples_ = depthMs * (SAMPLE_RATE_HZ / 1000.0f);
    return true;
  }

  bool setMix(float mix) {
    if (!(mix >= 0.0f && mix <= 1.0f)) {
      return false;
    }
    mix_ = mix;
    return true;
  }

  int16_t process(int16_t input) {
    delayBuffer_[writeIndex_] = input;

    const float delaySamples =
        BASE_DELAY_SAMPLES + depthSamples_ * sinf(phase_);
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

    phase_ += phaseIncrement_;
    if (phase_ >= CHORUS_TWO_PI) {
      phase_ -= CHORUS_TWO_PI;
    }
    writeIndex_ = (writeIndex_ + 1) % BUFFER_SIZE;

    const float output =
        (1.0f - mix_) * input + mix_ * delayed;
    if (output >= 32767.0f) {
      return 32767;
    }
    if (output <= -32768.0f) {
      return -32768;
    }
    return static_cast<int16_t>(output);
  }

 private:
  static constexpr uint16_t BUFFER_SIZE = 1024;
  static constexpr float SAMPLE_RATE_HZ = 48000.0f;
  static constexpr float BASE_DELAY_MS = 12.0f;
  static constexpr float BASE_DELAY_SAMPLES =
      BASE_DELAY_MS * (SAMPLE_RATE_HZ / 1000.0f);
  static constexpr float CHORUS_TWO_PI = 6.28318530718f;

  int16_t delayBuffer_[BUFFER_SIZE] = {};
  uint16_t writeIndex_ = 0;
  float phase_ = 0.0f;
  float depthSamples_ = DEFAULT_DEPTH_MS * (SAMPLE_RATE_HZ / 1000.0f);
  float mix_ = DEFAULT_MIX;
  float phaseIncrement_ = CHORUS_TWO_PI * DEFAULT_RATE_HZ / SAMPLE_RATE_HZ;
};
