#pragma once

#include <stdint.h>

class RoomReverbEffect {
 public:
  int16_t process(int16_t input) {
    float reverberated = 0.0f;
    for (uint8_t i = 0; i < COMB_COUNT; ++i) {
      reverberated += processComb(i, input);
    }
    reverberated = diffuse(reverberated * 0.25f);

    const float output = input * (1.0f - WET_MIX) + reverberated * WET_MIX;
    return clampToInt16(output);
  }

 private:
  static constexpr uint8_t COMB_COUNT = 4;
  static constexpr uint16_t MAX_COMB_LENGTH = 2233;
  static constexpr float COMB_FEEDBACK = 0.72f;
  static constexpr float WET_MIX = 0.28f;
  static constexpr float ALLPASS_FEEDBACK = 0.5f;

  int16_t combBuffers_[COMB_COUNT][MAX_COMB_LENGTH] = {};
  uint16_t combIndices_[COMB_COUNT] = {};
  int16_t allpassBuffer1_[149] = {};
  int16_t allpassBuffer2_[211] = {};
  uint16_t allpassIndex1_ = 0;
  uint16_t allpassIndex2_ = 0;

  float processComb(uint8_t comb, int16_t input) {
    const uint16_t index = combIndices_[comb];
    const float delayed = combBuffers_[comb][index];
    combBuffers_[comb][index] = clampToInt16(input + delayed * COMB_FEEDBACK);
    combIndices_[comb] = (index + 1) % combLength(comb);
    return delayed;
  }

  uint16_t combLength(uint8_t comb) const {
    switch (comb) {
      case 0:
        return 1557;
      case 1:
        return 1777;
      case 2:
        return 1999;
      default:
        return 2233;
    }
  }

  float processAllpass(float input, int16_t *buffer, uint16_t length,
                       uint16_t &index) {
    const float delayed = buffer[index];
    const float output = delayed - ALLPASS_FEEDBACK * input;
    buffer[index] = clampToInt16(input + ALLPASS_FEEDBACK * output);
    index = (index + 1) % length;
    return output;
  }

  float diffuse(float input) {
    float output =
        processAllpass(input, allpassBuffer1_, 149, allpassIndex1_);
    return processAllpass(output, allpassBuffer2_, 211, allpassIndex2_);
  }

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
