#pragma once

// Replace each -1 with a verified RP2350 GPIO number for the exact board.
// The Arduino-Pico I2S implementation uses BCLK_PIN + 1 for LRCLK.
#define I2S_BCLK_PIN 2
#define I2S_DOUT_PIN 4
#define I2S_DIN_PIN 5
#define I2S_MCLK_PIN 1

// Set these to the verified GPIOs wired to the codec's I2C bus.
// Leave both at -1 to use the board core's default Wire pins.
#define I2C_SDA_PIN 38
#define I2C_SCL_PIN 39

// RP2350 GPIO wired to the ES8388 CE input.
#define CODEC_CE_PIN 37

// Three-position DIP bank with external pull-downs (ON reads HIGH).
#define DIP1_PIN 34
#define DIP2_PIN 35
#define DIP3_PIN 36
