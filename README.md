# Mini-DSP

Mini-DSP is an audio-effects hardware and firmware project built around the
ES8388 audio codec and Raspberry Pi RP2350.

## Hardware

The KiCad designs include the main RP2350A/ES8388 board in
[`HW/Main`](HW/Main/) and an ES8388 test board in
[`Test board`](Test%20board/). They provide schematic and PCB references for
the codec, I2S audio interface, and supporting circuitry. See
[`HW/Main/README.md`](HW/Main/README.md) for hardware notes; confirm board
revisions and pin mappings before wiring.

## Software

[`SW/arduino_i2s_loopback`](SW/arduino_i2s_loopback/) contains an Arduino-Pico
full-duplex I2S sketch for the RP2350 and ES8388. It captures the mono input,
applies a time-based effect, and sends the result to the codec output. A
three-way DIP switch selects among bypass, chorus, flanger, delay, and reverb
presets. The effects and preset mapping are in separate header files.

See [`SW/README.md`](SW/README.md) for setup, wiring considerations, presets,
and build instructions.
