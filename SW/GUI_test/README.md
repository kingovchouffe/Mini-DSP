# GC9A01 display proof of concept

This standalone Arduino-Pico sketch demonstrates the Waveshare 240x240
GC9A01 display with Arduino_GFX. It shows the selected effect from the
existing three-bit DIP switch mapping (DIP1/GP34 is bit 0, DIP2/GP35 bit 1,
DIP3/GP36 bit 2) and three concentric progress arcs driven by one
potentiometer.

## Setup

1. Install **GFX Library for Arduino** and **U8g2** from the Arduino Library
   Manager. U8g2 supplies the Helvetica-style fonts used by the sketch.
2. Select the same Arduino-Pico RP2350 board/core used by the audio sketch.
3. Open `GUI_test.ino`, verify the GPIOs below against the exact board and
   module wiring, then build and upload.

| Display/module signal | RP2350 GPIO |
|---|---:|
| SCLK / SCK | 18 |
| MOSI / SDA / DIN | 19 |
| DC | 24 |
| CS | 21 |
| RST / RES | 22 |
| BL / backlight | 23 |
| GND | GND |
| VCC | Check the module documentation |

The potentiometer is configured on **GPIO42**, matching the QFN80 setup where
this input has been verified to work. Connect its wiper to that input and its
ends to GND and 3.3 V. Pin and ADC availability depends on the exact RP2350
package and board routing; do not assume the same GPIO mapping applies to a
different package or board. The sketch explicitly sets 12-bit ADC resolution.
The three DIP inputs use the existing project's external pull-down wiring.
The sketch uses SPI0 and does not use a display MISO connection. Confirm the
display module's voltage requirements and pin labels before wiring.

## Display and responsiveness

The interface uses U8g2's Helvetica bold fonts and arcs placed close to the
screen edge. `LEVEL`, `RATE`, and `DEPTH` label the three arcs in the center;
the active effect name is shown at the bottom. These are prototype labels and
should be changed if the eventual controls have different meanings. The
numeric percentage is hidden. All three arcs mirror the same potentiometer
reading and are not independent controls. Three subtle moving wave lines
were replaced by one animated sea-like wave above the parameter names. Its
motion speed follows the potentiometer. This is a visual preview and does not
change audio effects.

The loop scans inputs every 25 ms without `delay()`. The background waves
redraw at most every 40 ms; changed arc segments and the bottom effect label
are updated independently.

Keep this proof of concept separate from the audio loop while evaluating
display timing. SPI transfers are synchronous, and drawing from the audio
processing loop can interrupt timely I2S servicing. Integration should keep
audio servicing independent and measure for audio dropouts before adding
display updates to the running firmware.
