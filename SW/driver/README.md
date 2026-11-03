# Mini-DSP audio driver

`ES8388Driver.h` contains the reusable I2C register access and the known-good
mono guitar codec setup used by `arduino_i2s_loopback`. The folder is an
Arduino library.

## Use from a sketch

Install `SW/driver` as an Arduino library (or build with
`arduino-cli compile --libraries SW ...`). Include the driver and create an
instance using the selected I2C bus. The address is a 7-bit I2C address; the
default is `0x11` for this board's CE-high configuration.

```cpp
#include <ES8388Driver.h>

ES8388Driver codec(Wire1, 0x11);
```

Configure the I2C bus and start the MCU I2S clocks before configuring the
ES8388 in slave mode. Then initialize the mono LIN1-to-LOUT1 path:

```cpp
if (!codec.configureMonoLin1ToLout1()) {
  Serial.print(codec.lastErrorMessage());
  Serial.print(" at register 0x");
  Serial.println(codec.failedRegister(), HEX);
  return;
}

if (!codec.setDacMuted(false)) {
  Serial.println(codec.lastErrorMessage());
  return;
}
```

The helper configures 16-bit, 48 kHz-compatible I2S registers, selects LIN1,
routes and powers the left DAC to LOUT1 (DAC power register `0x04` = `0x60`),
and follows the ES8388 startup power sequence. It leaves the DAC muted until
`setDacMuted(false)`.

Other useful operations:

- `readRegister()` / `writeRegister()` for register-level access.
- `setAdcGainDb()` for 0-24 dB in 3 dB steps.
- `setLout1Volume()` for an ES8388 LOUT1 volume register value from `0x00` to
  `0x21` (`0x1E` is 0 dB).
- `setDacMuted(true)` to mute; `setDacMuted(false)` to unmute.

All methods that perform I2C return `false` on failure. `lastErrorMessage()`
and `failedRegister()` provide details for diagnostics.

## Add a guitar effect

The effects are implemented separately in
[`arduino_i2s_loopback/ChorusEffect.h`](../arduino_i2s_loopback/ChorusEffect.h),
[`arduino_i2s_loopback/FlangerEffect.h`](../arduino_i2s_loopback/FlangerEffect.h),
[`arduino_i2s_loopback/DelayEffect.h`](../arduino_i2s_loopback/DelayEffect.h),
and
[`arduino_i2s_loopback/RoomReverbEffect.h`](../arduino_i2s_loopback/RoomReverbEffect.h).
[`arduino_i2s_loopback/TimeEffectPresets.h`](../arduino_i2s_loopback/TimeEffectPresets.h)
maps the three DIP inputs to the eight presets documented in `SW/README.md`.
The switch value is sampled at startup, so a change requires a restart.
Effects process the mono LIN1 sample between `audio.read16()` and
`audio.write16()`; no digital overdrive is applied.

The longest delay uses a fixed 32,768-sample mono buffer (64 KB). The effects
run at 48 kHz on the existing audio core, using the RP2350 M33 FPU; the second
core is not needed for this processing load.
