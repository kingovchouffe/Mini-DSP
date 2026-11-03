# ES8388 I2S loopback test proposal

## Goal

Run a simple full-duplex audio test with the external ES8388 audio codec and a
WeActStudio RP2350B board. The RP2350 receives samples from the codec ADC over
I2S and sends those same samples back to the codec DAC over I2S. This exercises
the codec input, I2S receive/transmit paths, and audio output without an
external audio interface.

This is a software proposal, not a verified pin-by-pin build guide. The checked
in hardware designs are useful references, but the main design schematic uses
an RP2350A and neither schematic establishes the WeActStudio board's header
pinout.

## Hardware references in this repository

- [`Test board/Test board.kicad_sch`](../Test%20board/Test%20board.kicad_sch)
  contains an ES8388 (`U7`) and nets named `I²S_MCLK`, `I²S_SCLK`,
  `I²S_LRCK`, `I²S_SDIN`, and `I²S_SDOUT`.
- [`HW/Main/Mini-DSP.kicad_sch`](../HW/Main/Mini-DSP.kicad_sch) contains the
  ES8388 and an RP2350A. It uses the same I2S net names.

Use the following names from the codec's perspective:

| Codec signal | Direction | Purpose |
|---|---|---|
| `I²S_SDOUT` | ES8388 to RP2350 | ADC audio samples to firmware |
| `I²S_SDIN` | RP2350 to ES8388 | Firmware samples to DAC |
| `I²S_SCLK` | Clock | Bit clock (BCLK) |
| `I²S_LRCK` | Clock | Left/right word select (WS/LRCLK) |
| `I²S_MCLK` | Clock | Codec master clock |
| `I2C_CODEC_SDA`, `I2C_CODEC_SCL` | Bidirectional / clock | ES8388 register control |
| `CODEC_CE` | RP2350 to ES8388 | Codec enable/control; confirm function and polarity for the hardware |

Confirm these directions against the selected board's schematic and the
ES8388 datasheet before connecting signals. In particular, do not connect two
outputs together.

## Proposed test

1. **Confirm the hardware interface.** Identify the exact WeActStudio
   RP2350B board revision and the ES8388 module/board. Check logic voltage,
   common ground, connector orientation, and continuity. Map each signal above
   to the actual RP2350B GPIO and codec pin from those boards' documentation.
   The KiCad labels are not a WeActStudio header pin map.
2. **Choose one I2S clock master.** For the first implementation, use the
   RP2350 as clock master only if the selected firmware can produce the
   required BCLK, LRCLK, and MCLK on available pins. Otherwise configure the
   codec as clock master and configure the RP2350 I2S receiver/transmitter as
   a slave. Never enable both masters. Confirm the ES8388's MCLK requirements
   and supported clock ratios for the chosen sample rate.
3. **Initialize the codec over I2C.** The Arduino example applies the ES8388
   User Guide's codec-startup and recording sequence, then its playback-mode
   sequence (sections 10.1-10.3), adapted for 48 kHz, stereo, 16-bit I2S with a
   256 × sample-rate MCLK. It selects LIN1/RIN1 via ADC register `0x0A`, sets
   ADC gain through `0x09`, and configures the DAC mixer and output levels.
   The DAC stays muted until I2S starts. Check that the hardware input is
   connected to LIN1/RIN1 and that output routing matches the board. The
   sketch checks every I2C write and stops before starting I2S if a register
   transaction fails.
4. **Start I2S.** The Arduino-Pico `I2S` class uses RP2350 PIO/DMA internally
   for full-duplex operation. The sketch configures stereo 16-bit samples at
   48 kHz and starts the I2S clocks after codec initialization.
5. **Process in software.** Read the DIP-selected time effect, process the
   left 16-bit sample, and write the result to both I2S output slots.
6. **Test safely.** The sketch starts with reduced DAC and line-output volume.
   Use a low-level line-level source at the codec input and connect the codec
   output to a suitable line input, oscilloscope, or headphones at low volume.
   Do not connect headphone-level output directly to an input unless that
   input's limits permit it.

This test uses a digital firmware loop between the ES8388 ADC and DAC. It does
not require a physical analog output-to-input jumper. A separate analog jumper
test can be added later, but should only be used after checking input/output
levels and grounding.

## Firmware outline

```text
initialize I2C and codec control GPIO
configure ES8388 ADC/DAC and check every control transaction
configure I2S clocks, format, and full-duplex operation
start I2S

while running:
    when at least one stereo frame is available:
        read the stereo ADC frame
        apply the selected time effect to the left sample
        write the processed sample to both DAC slots over I2S
```

Effects are separate modules: [`ChorusEffect.h`](arduino_i2s_loopback/ChorusEffect.h),
[`FlangerEffect.h`](arduino_i2s_loopback/FlangerEffect.h),
[`DelayEffect.h`](arduino_i2s_loopback/DelayEffect.h), and
[`RoomReverbEffect.h`](arduino_i2s_loopback/RoomReverbEffect.h).
[`TimeEffectPresets.h`](arduino_i2s_loopback/TimeEffectPresets.h) maps the DIP
value to the selected effect chain. Processing stays on the audio core; the
RP2350's M33 FPU handles the per-sample floating-point modulation, and a
second-core handoff would add synchronization and latency without benefit.

The preset table uses DIP3 DIP2 DIP1 order; DIP1 is the least significant bit:

| DIP3 DIP2 DIP1 | Value | Effect |
|---|---:|---|
| `000` | 0 | Dry / bypass |
| `001` | 1 | Subtle chorus |
| `010` | 2 | Flanger |
| `011` | 3 | Slapback delay (100 ms) |
| `100` | 4 | Echo delay (350 ms, light repeats) |
| `101` | 5 | Long repeating delay (600 ms) |
| `110` | 6 | Room reverb |
| `111` | 7 | Chorus + room reverb |

## Arduino-Pico example

[`arduino_i2s_loopback/arduino_i2s_loopback.ino`](arduino_i2s_loopback/arduino_i2s_loopback.ino)
shows blocking stereo 16-bit capture and processing using the `I2S` class in the
[Arduino-Pico core](https://github.com/earlephilhower/arduino-pico). Its I2S
implementation uses PIO and DMA. The example expects the RP2350 to generate
BCLK, LRCLK, and MCLK, with the ES8388 configured as the I2S clock slave.

Before compiling, set `I2S_BCLK_PIN`, `I2S_DOUT_PIN`, `I2S_DIN_PIN`, and
`I2S_MCLK_PIN` in
[`hardware_config.h`](arduino_i2s_loopback/hardware_config.h) to verified
RP2350 GPIO numbers. This core uses the GPIO immediately after BCLK for LRCLK,
so both pins must be available and wired correctly. Pin numbers are deliberately
not guessed from the KiCad schematics. Keep all four signals on distinct,
correctly mapped GPIOs.

The three-position DIP switch is read once during startup: DIP1/GP34 is bit 0,
DIP2/GP35 is bit 1, and DIP3/GP36 is bit 2. The switches have external
pull-downs, so firmware uses plain `INPUT` and ON reads HIGH. The startup serial
log prints each switch state, the three-bit value, its decimal equivalent,
and the selected preset name. Changing the switch position takes effect after
the next restart.

Set `I2C_SDA_PIN` and `I2C_SCL_PIN` in the same file to the verified GPIOs
wired to the codec's I2C bus. On the WeAct Studio RP2350B, GPIO38/39 are I2C1
pins, so the sketch uses `Wire1` for the codec bus. The bus is explicitly set
to a requested 100 kHz after `Wire1.begin()`. Set `CODEC_CE_PIN` to the
verified RP2350 GPIO wired to the ES8388 CE input; the sketch drives it high
before starting I2C. Leave it at `-1` only if CE is already pulled high by the
hardware. The sketch resets and configures the ES8388 ADC and DAC in separate
stages, following the guide's startup, recording, and playback sequences. It
holds DAC mute until I2S starts, then unmutes it at reduced analog output
volume. It reads register `0x00` (Chip Control 1) once as a diagnostic. The
register read uses the datasheet's register-address write followed by a
repeated-start read, with a 25 ms timeout. The sketch reads the register
directly, without a separate address-only probe; this avoids the Arduino-Pico
core's special bit-banged zero-length probe path. The register-address
transaction sends the chip address and register address, not register data;
the datasheet shows ACKs for the address phases and omits the ACK only after
register data. During the read, the master terminates the one-byte transfer
with a NACK as normal. Although
the sketch requests 100 kHz, verify SCL with an oscilloscope or logic
analyzer; bus capacitance, rise time, clock stretching, or board/core clocking
can affect the measured waveform. I2C standard-mode permits clock rates up to
100 kHz, so a stable 62 kHz SCL rate is also within the standard-mode limit.

The ES8388 control pins are pin 27 `CDATA/SDA` and pin 28 `CCLK/SCL`; connect
the controller's SDA to pin 27 and SCL to pin 28. The RP2350B I2C1 GPIO roles
are fixed for this pair (GPIO38 SDA, GPIO39 SCL), so do not swap the firmware
pin definitions to compensate for crossed codec-side nets. The ES8388 net
labels at the codec pins have been corrected in both KiCad schematics; an
already-routed PCB must also be checked and rerouted if its copper follows the
old crossed net assignments.

The I2S settings are 48 kHz, stereo, 16-bit samples, and
256 × sample-rate MCLK. The I2S example expects the RP2350 to be the sole
clock master. The sketch configures the ES8388 as a clock slave and enables its
ADC and DAC paths over I2C. It applies the mono input effects in the MCU and
sends the result to both DAC slots; it does not use a codec-internal loopback
or require an analog jumper.

Install/select the Arduino-Pico board core for the RP2350B, open the sketch
folder, set the verified GPIO macros, then build and upload. Start with low
codec gain/volume and validate clocks and framing with a logic analyzer before
connecting an audio source or listening equipment.

The sketch blinks the board LED on GPIO25, and prints startup/errors,
register reads, and a once-per-second frame count to `Serial`. In the
Arduino-Pico Tools menu, select the default **Pico SDK** USB stack (which
provides USB CDC `Serial`) and open the board's USB serial port at 115200 baud.
The sketch continues even if no serial monitor is connected. GPIO25 must be
the LED pin on the exact board and must not be assigned to any I2S signal.

## Pass criteria and diagnostics

- The codec address responds, all configuration writes are acknowledged, and
  register `0x00` can be read after initialization.
- A logic analyzer shows stable MCLK, BCLK, and LRCLK with the configured
  frequency and correct I2S framing.
- A low-level input tone is audible or measurable at the codec output without
  periodic gaps, channel swaps, or obvious distortion.
- Silence at the input produces silence (apart from the codec's noise floor).
- Firmware reports codec-init and I2S receive failures, along with a
  once-per-second count of forwarded stereo frames; it must not continue after
  a codec initialization error.

For fault isolation, first check codec I2C/control and clocks, then verify the
ADC data stream, then verify known samples sent directly to the DAC, and only
then enable the ADC-to-DAC software loop.

## Open hardware checks before implementation

- Exact WeActStudio RP2350B board and header pin mapping.
- Whether the codec is a separate module or the repository's ES8388 circuit.
- I2S clock-master selection and the source/routing of MCLK.
- Codec reset/enable polarity and I2C address for the assembled hardware.
- Physical input/output connector type, voltage range, and safe monitoring
  method.
