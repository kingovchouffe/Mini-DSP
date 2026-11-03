#pragma once

#include <stdint.h>

class DelayEffect {
 public:
  bool configure(float delayMs, float feedback, float wetMix) {
    if (!(delayMs >= 1.0f && delayMs <= MAX_DELAY_MS) ||
        !(feedback >= 0.0f && feedback <= 0.7f) ||
        !(wetMix >= 0.0f && wetMix <= 1.0f)) {
      return false;
    }

    delaySamples_ = static_cast<uint16_t>(delayMs * SAMPLE_RATE_HZ / 1000.0f);
    feedback_ = feedback;
    wetMix_ = wetMix;
    return true;
  }

  int16_t process(int16_t input) {
    const uint16_t readIndex =
        (writeIndex_ + BUFFER_SIZE - delaySamples_) % BUFFER_SIZE;
    const int16_t delayed = delayBuffer_[readIndex];
    delayBuffer_[writeIndex_] =
        clampToInt16(input + delayed * feedback_);
    writeIndex_ = (writeIndex_ + 1) % BUFFER_SIZE;

    return clampToInt16(input * (1.0f - wetMix_) + delayed * wetMix_);
  }

 private:
  static constexpr uint16_t BUFFER_SIZE = 32768;
  static constexpr float SAMPLE_RATE_HZ = 48000.0f;
  static constexpr float MAX_DELAY_MS =
      (BUFFER_SIZE - 1) * 1000.0f / SAMPLE_RATE_HZ;

  int16_t delayBuffer_[BUFFER_SIZE] = {};
  uint16_t writeIndex_ = 0;
  uint16_t delaySamples_ = 4800;
  float feedback_ = 0.05f;
  float wetMix_ = 0.3f;

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
