#pragma once

#include <stdint.h>

#include "ChorusEffect.h"
#include "DelayEffect.h"
#include "FlangerEffect.h"
#include "RoomReverbEffect.h"

class TimeEffectPresets {
 public:
  bool select(uint8_t value) {
    if (value > 7) {
      return false;
    }

    preset_ = value;
    switch (preset_) {
      case 3:
        return delay_.configure(100.0f, 0.05f, 0.3f);
      case 4:
        return delay_.configure(350.0f, 0.22f, 0.35f);
      case 5:
        return delay_.configure(600.0f, 0.4f, 0.35f);
      default:
        return true;
    }
  }

  const char *name() const {
    static const char *const names[] = {
        "Dry / bypass",
        "Subtle chorus",
        "Flanger",
        "Slapback delay",
        "Echo delay",
        "Long repeating delay",
        "Room reverb",
        "Chorus + room reverb",
    };
    return preset_ <= 7 ? names[preset_] : "Unknown";
  }

  int16_t process(int16_t input) {
    switch (preset_) {
      case 1:
        return chorus_.process(input);
      case 2:
        return flanger_.process(input);
      case 3:
      case 4:
      case 5:
        return delay_.process(input);
      case 6:
        return reverb_.process(input);
      case 7:
        return reverb_.process(chorus_.process(input));
      case 0:
      default:
        return input;
    }
  }

 private:
  uint8_t preset_ = 0;
  ChorusEffect chorus_;
  FlangerEffect flanger_;
  DelayEffect delay_;
  RoomReverbEffect reverb_;
};
