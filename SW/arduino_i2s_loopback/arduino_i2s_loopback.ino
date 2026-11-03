#include <Wire.h>
#include <I2S.h>

#include "hardware_config.h"
#include "ES8388Driver.h"
#include "TimeEffectPresets.h"

constexpr int SAMPLE_RATE = 48000;
constexpr int SAMPLE_BITS = 16;
constexpr int MCLK_MULTIPLIER = 256;
constexpr int STATUS_LED_PIN = 25;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr uint32_t LED_TOGGLE_INTERVAL_MS = 500;
constexpr uint32_t DEBUG_INTERVAL_MS = 1000;
constexpr uint32_t I2C_TIMEOUT_MS = 25;

static_assert((I2C_SDA_PIN < 0 && I2C_SCL_PIN < 0) ||
                  (I2C_SDA_PIN >= 0 && I2C_SCL_PIN >= 0),
              "Set both I2C_SDA_PIN and I2C_SCL_PIN, or leave both at -1.");

static_assert(I2S_BCLK_PIN >= 0 && I2S_BCLK_PIN < 47,
              "Set I2S_BCLK_PIN and reserve the following GPIO for LRCLK.");
static_assert(I2S_DOUT_PIN >= 0 && I2S_DOUT_PIN < 48,
              "Set I2S_DOUT_PIN to the verified codec SDIN GPIO.");
static_assert(I2S_DIN_PIN >= 0 && I2S_DIN_PIN < 48,
              "Set I2S_DIN_PIN to the verified codec SDOUT GPIO.");
static_assert(I2S_MCLK_PIN >= 0 && I2S_MCLK_PIN < 48,
              "Set I2S_MCLK_PIN to the verified codec MCLK GPIO.");
static_assert(DIP1_PIN >= 0 && DIP1_PIN < 48 && DIP2_PIN >= 0 &&
                  DIP2_PIN < 48 && DIP3_PIN >= 0 && DIP3_PIN < 48,
              "Set all three DIP switch pins to valid RP2350 GPIOs.");
static_assert(I2S_DOUT_PIN != I2S_DIN_PIN &&
                  I2S_DOUT_PIN != I2S_MCLK_PIN &&
                  I2S_DIN_PIN != I2S_MCLK_PIN &&
                  I2S_BCLK_PIN != I2S_DOUT_PIN &&
                  I2S_BCLK_PIN != I2S_DIN_PIN &&
                  I2S_BCLK_PIN != I2S_MCLK_PIN &&
                  I2S_BCLK_PIN + 1 != I2S_DOUT_PIN &&
                  I2S_BCLK_PIN + 1 != I2S_DIN_PIN &&
                  I2S_BCLK_PIN + 1 != I2S_MCLK_PIN &&
                  I2S_BCLK_PIN != STATUS_LED_PIN &&
                  I2S_BCLK_PIN + 1 != STATUS_LED_PIN &&
                  I2S_DOUT_PIN != STATUS_LED_PIN &&
                  I2S_DIN_PIN != STATUS_LED_PIN &&
                  I2S_MCLK_PIN != STATUS_LED_PIN,
              "I2S GPIO assignments must not overlap, including GPIO25 LED and BCLK+1 LRCLK.");

I2S audio(INPUT_PULLUP);
ES8388Driver codec(Wire1, 0x11);
TimeEffectPresets effects;
uint32_t lastLedToggleMs;
uint32_t lastDebugMs;
uint32_t framesForwarded;
uint32_t inputFramesWithSignal;
uint32_t outputWriteFailures;
uint16_t inputPeak;
uint16_t rightInputPeak;
bool ledIsOn;
bool i2sStarted;
uint8_t dipSwitchValue;

uint8_t readDipSwitchValue() {
  pinMode(DIP1_PIN, INPUT);
  pinMode(DIP2_PIN, INPUT);
  pinMode(DIP3_PIN, INPUT);
  delay(5);

  const bool dip1On = digitalRead(DIP1_PIN) == HIGH;
  const bool dip2On = digitalRead(DIP2_PIN) == HIGH;
  const bool dip3On = digitalRead(DIP3_PIN) == HIGH;
  uint8_t value = 0;
  if (dip1On) {
    value |= 1U << 0;
  }
  if (dip2On) {
    value |= 1U << 1;
  }
  if (dip3On) {
    value |= 1U << 2;
  }

  Serial.print("DIP1/DIP2/DIP3 (ON=1): ");
  Serial.print(dip1On);
  Serial.print("/");
  Serial.print(dip2On);
  Serial.print("/");
  Serial.print(dip3On);
  Serial.print("; value (DIP3..DIP1): ");
  Serial.print((dip3On ? 1 : 0));
  Serial.print((dip2On ? 1 : 0));
  Serial.print((dip1On ? 1 : 0));
  Serial.print(" (");
  Serial.print(value);
  Serial.println(").");
  return value;
}

void reportCodecError(const char *operation) {
  Serial.print("ERROR: ");
  Serial.print(operation);
  Serial.print(": ");
  Serial.print(codec.lastErrorMessage());
  Serial.print(" (register 0x");
  Serial.print(codec.failedRegister(), HEX);
  Serial.println(").");
}

bool reportCodecRegister() {
  uint8_t value;
  if (!codec.readRegister(ES8388Driver::CONTROL1, value)) {
    reportCodecError("Could not read ES8388 control register");
    return false;
  }

  Serial.print("ES8388 register 0x00 (Chip Control 1) = 0x");
  Serial.print(value, HEX);
  Serial.println(".");
  return true;
}

bool configureCodec() {
  Serial.println("Starting ES8388 for 48 kHz mono LIN1-to-LOUT1 audio.");
  if (!codec.configureMonoLin1ToLout1()) {
    reportCodecError("ES8388 mono configuration failed");
    return false;
  }

  if (!reportCodecRegister()) {
    return false;
  }
  Serial.println("ES8388 configured for LIN1 input and LOUT1 output.");
  return true;
}

void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  Serial.begin(115200);
  delay(1000);
  Serial.println("RP2350 <-> ES8388 I2S loopback with I2C register check");
  Serial.println("Booting; USB serial output does not wait for a monitor.");
  dipSwitchValue = readDipSwitchValue();
  if (!effects.select(dipSwitchValue)) {
    Serial.println("ERROR: Unsupported DIP switch effect preset.");
    return;
  }
  Serial.print("Selected effect preset ");
  Serial.print(dipSwitchValue);
  Serial.print(": ");
  Serial.println(effects.name());
  Serial.println("Starting I2S clocks before configuring the codec over I2C.");

  if (CODEC_CE_PIN >= 0) {
    digitalWrite(CODEC_CE_PIN, HIGH);
    pinMode(CODEC_CE_PIN, OUTPUT);
    Serial.print("ES8388 CE driven high on GPIO");
    Serial.println(CODEC_CE_PIN);
  } else {
    Serial.println("WARNING: CODEC_CE_PIN is unset; ES8388 CE is not driven by firmware.");
  }

  if (I2C_SDA_PIN >= 0) {
    if (!Wire1.setSDA(I2C_SDA_PIN) || !Wire1.setSCL(I2C_SCL_PIN)) {
      Serial.println("ERROR: Failed to set I2C pins. Check selected SDA/SCL GPIOs.");
      return;
    }
  } else {
    Serial.println("I2C using the board core's default Wire1 pins.");
  }
  Wire1.begin();
  Wire1.setTimeout(I2C_TIMEOUT_MS, false);
  Wire1.setClock(I2C_CLOCK_HZ);
  Serial.println("I2C1 initialized; requested bus clock is 100 kHz.");
  lastLedToggleMs = millis();

  if (!audio.setBCLK(I2S_BCLK_PIN) ||
      !audio.setDOUT(I2S_DOUT_PIN) ||
      !audio.setDIN(I2S_DIN_PIN) ||
      !audio.setMCLK(I2S_MCLK_PIN) ||
      !audio.setMCLKmult(MCLK_MULTIPLIER) ||
      !audio.setBitsPerSample(SAMPLE_BITS) ||
      !audio.setFrequency(SAMPLE_RATE)) {
    Serial.println("ERROR: Failed to configure I2S. Check GPIOs and settings.");
    return;
  }

  if (!audio.begin()) {
    Serial.println("ERROR: I2S start failed. Check PIO resources and GPIOs.");
    return;
  }

  if (!configureCodec()) {
    Serial.println("ERROR: Codec initialization failed; audio will not be forwarded.");
    return;
  }

  if (!codec.setDacMuted(false)) {
    reportCodecError("Failed to unmute ES8388 DAC");
    return;
  }

  i2sStarted = true;
  lastDebugMs = millis();
  Serial.println("I2S started. Processing LIN1 with the selected effect and sending it to LOUT1.");
}

void loop() {
  const uint32_t nowMs = millis();
  if (nowMs - lastLedToggleMs >= LED_TOGGLE_INTERVAL_MS) {
    ledIsOn = !ledIsOn;
    digitalWrite(STATUS_LED_PIN, ledIsOn ? HIGH : LOW);
    lastLedToggleMs = nowMs;
  }

  if (nowMs - lastDebugMs >= DEBUG_INTERVAL_MS) {
    Serial.print("Heartbeat; I2S ");
    Serial.print(i2sStarted ? "started" : "not started");
    Serial.print("; stereo frames forwarded: ");
    Serial.println(framesForwarded);
    Serial.print("LIN1 ADC nonzero frames / peak: ");
    Serial.print(inputFramesWithSignal);
    Serial.print(" / ");
    Serial.println(inputPeak);
    Serial.print("Right ADC peak: ");
    Serial.println(rightInputPeak);
    Serial.print("I2S output write failures: ");
    Serial.println(outputWriteFailures);
    Serial.print("I2S output DMA underflow: ");
    Serial.println(audio.getUnderflow() ? "yes" : "no");
    Serial.print("I2S receive buffer bytes available: ");
    Serial.println(i2sStarted ? audio.available() : 0);
    framesForwarded = 0;
    inputFramesWithSignal = 0;
    outputWriteFailures = 0;
    inputPeak = 0;
    rightInputPeak = 0;
    lastDebugMs = nowMs;
  }

  if (!i2sStarted) {
    return;
  }

  if (audio.available() < 4) {
    return;
  }

  int16_t left;
  int16_t unusedRight;
  if (!audio.read16(&left, &unusedRight)) {
    Serial.println("ERROR: I2S receive failed.");
    i2sStarted = false;
    return;
  }

  const int32_t sampleMagnitude = left < 0 ? -static_cast<int32_t>(left) : left;
  if (sampleMagnitude > 0) {
    ++inputFramesWithSignal;
  }
  if (sampleMagnitude > inputPeak) {
    inputPeak = static_cast<uint16_t>(sampleMagnitude);
  }
  const int32_t rightSampleMagnitude =
      unusedRight < 0 ? -static_cast<int32_t>(unusedRight) : unusedRight;
  if (rightSampleMagnitude > rightInputPeak) {
    rightInputPeak = static_cast<uint16_t>(rightSampleMagnitude);
  }

  const int16_t processedSample = effects.process(left);
  if (audio.write16(processedSample, processedSample) != 1) {
    ++outputWriteFailures;
    return;
  }
  ++framesForwarded;
}
