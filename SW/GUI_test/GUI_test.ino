#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>

constexpr int8_t TFT_DC_PIN = 24;
constexpr int8_t TFT_CS_PIN = 21;
constexpr int8_t TFT_SCK_PIN = 18;
constexpr int8_t TFT_MOSI_PIN = 19;
constexpr int8_t TFT_RST_PIN = 22;
constexpr int8_t TFT_BL_PIN = 23;
constexpr int8_t POT_PIN = 42;
constexpr int8_t DIP1_PIN = 34;
constexpr int8_t DIP2_PIN = 35;
constexpr int8_t DIP3_PIN = 36;

constexpr uint32_t INPUT_SCAN_INTERVAL_MS = 25;
constexpr uint32_t WAVE_FRAME_INTERVAL_MS = 40;
constexpr uint8_t DISPLAY_ROTATION = 0;
constexpr uint16_t ADC_MAX_VALUE = 4095;
constexpr int16_t SCREEN_CENTER_X = 120;
constexpr int16_t GAUGE_CENTER_Y = 120;
constexpr float ARC_START_DEGREES = 135.0f;
constexpr float ARC_SWEEP_DEGREES = 270.0f;
constexpr int16_t WAVE_LEFT = 64;
constexpr int16_t WAVE_RIGHT = 176;
constexpr int16_t WAVE_TOP = 65;
constexpr int16_t WAVE_BASELINE_Y = 100;
constexpr int16_t WAVE_CENTER_Y = 83;
constexpr int16_t WAVE_AMPLITUDE = 11;
constexpr uint8_t WAVE_SEGMENTS = 28;
constexpr uint16_t WAVE_COLOR = 0x07FF;
constexpr uint16_t BACKGROUND_COLOR = 0x0861;
constexpr uint16_t TEXT_COLOR = 0xFFFF;
constexpr uint16_t MUTED_COLOR = 0x7BEF;
constexpr uint16_t TRACK_COLOR = 0x2128;

struct GaugeArc {
  int16_t outerRadius;
  int16_t innerRadius;
  uint16_t color;
};

constexpr GaugeArc GAUGE_ARCS[] = {
    {116, 109, 0x07FF},
    {100, 93, 0xA01F},
    {84, 77, 0xFD20},
};
constexpr size_t GAUGE_ARC_COUNT = sizeof(GAUGE_ARCS) / sizeof(GAUGE_ARCS[0]);

const char *const parameterNames[] = {
    "LEVEL",
    "RATE",
    "DEPTH",
};

Arduino_DataBus *bus =
    new Arduino_RPiPicoSPI(TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN,
                           GFX_NOT_DEFINED, spi0);
Arduino_GFX *gfx =
    new Arduino_GC9A01(bus, TFT_RST_PIN, DISPLAY_ROTATION, true);

const char *const effectNames[] = {
    "Dry / bypass",
    "Subtle chorus",
    "Flanger",
    "Slapback delay",
    "Echo delay",
    "Long delay",
    "Room reverb",
    "Chorus + reverb",
};

uint32_t lastInputScanMs;
uint32_t lastWaveFrameMs;
uint16_t lastPotPercent = UINT16_MAX;
uint8_t lastEffect = UINT8_MAX;
float wavePhase;
bool displayReady;

uint8_t readEffectPreset() {
  const bool dip1On = digitalRead(DIP1_PIN) == HIGH;
  const bool dip2On = digitalRead(DIP2_PIN) == HIGH;
  const bool dip3On = digitalRead(DIP3_PIN) == HIGH;
  return static_cast<uint8_t>((dip1On ? 1U : 0U) |
                              (dip2On ? 2U : 0U) |
                              (dip3On ? 4U : 0U));
}

void drawCenteredText(const char *text, int16_t y, const uint8_t *font,
                      uint16_t color) {
  int16_t x1;
  int16_t y1;
  uint16_t textWidth;
  uint16_t textHeight;
  gfx->setFont(font);
  gfx->setTextSize(1);
  gfx->setTextColor(color);
  gfx->getTextBounds(text, 0, y, &x1, &y1, &textWidth, &textHeight);
  gfx->setCursor((gfx->width() - textWidth) / 2 - x1, y);
  gfx->print(text);
}

float angleForPercent(uint16_t percent) {
  return ARC_START_DEGREES + ARC_SWEEP_DEGREES * percent / 100.0f;
}

void drawGaugeTracks() {
  const float arcEnd = ARC_START_DEGREES + ARC_SWEEP_DEGREES;
  for (size_t i = 0; i < GAUGE_ARC_COUNT; ++i) {
    gfx->fillArc(SCREEN_CENTER_X, GAUGE_CENTER_Y, GAUGE_ARCS[i].outerRadius,
                 GAUGE_ARCS[i].innerRadius, ARC_START_DEGREES, arcEnd,
                 TRACK_COLOR);
  }
}

void updateGaugeArcs(uint16_t oldPercent, uint16_t newPercent) {
  if (oldPercent == UINT16_MAX) {
    oldPercent = 0;
  }
  if (oldPercent == newPercent) {
    return;
  }

  const float oldAngle = angleForPercent(oldPercent);
  const float newAngle = angleForPercent(newPercent);
  const float wedgeStart = oldAngle < newAngle ? oldAngle : newAngle;
  const float wedgeEnd = oldAngle < newAngle ? newAngle : oldAngle;

  for (size_t i = 0; i < GAUGE_ARC_COUNT; ++i) {
    gfx->fillArc(SCREEN_CENTER_X, GAUGE_CENTER_Y, GAUGE_ARCS[i].outerRadius,
                 GAUGE_ARCS[i].innerRadius, wedgeStart, wedgeEnd,
                 newPercent > oldPercent ? GAUGE_ARCS[i].color : TRACK_COLOR);
  }
}

void drawEffect(uint8_t effect) {
  gfx->fillRect(25, 208, 190, 28, BACKGROUND_COLOR);
  drawCenteredText(effectNames[effect], 230, u8g2_font_helvB10_tf,
                   GAUGE_ARCS[0].color);
}

void drawParameterNames() {
  constexpr int16_t FIRST_LABEL_Y = 128;
  constexpr int16_t LABEL_SPACING = 20;
  for (size_t i = 0; i < GAUGE_ARC_COUNT; ++i) {
    drawCenteredText(parameterNames[i],
                     FIRST_LABEL_Y + static_cast<int16_t>(i) * LABEL_SPACING,
                     u8g2_font_helvB08_tf, GAUGE_ARCS[i].color);
  }
}

void drawBackgroundWaves(uint16_t ratePercent) {
  constexpr float WAVE_TWO_PI = 6.28318530718f;
  const uint32_t nowMs = millis();
  const uint32_t elapsedMs = nowMs - lastWaveFrameMs;
  if (elapsedMs < WAVE_FRAME_INTERVAL_MS) {
    return;
  }

  const float rateHz = 0.25f + 3.75f * ratePercent / 100.0f;
  wavePhase += WAVE_TWO_PI * rateHz * elapsedMs / 1000.0f;
  if (wavePhase >= WAVE_TWO_PI) {
    wavePhase = fmodf(wavePhase, WAVE_TWO_PI);
  }
  lastWaveFrameMs = nowMs;

  gfx->fillRect(WAVE_LEFT, WAVE_TOP, WAVE_RIGHT - WAVE_LEFT + 1,
                WAVE_BASELINE_Y - WAVE_TOP + 1, BACKGROUND_COLOR);
  gfx->drawFastHLine(WAVE_LEFT, WAVE_BASELINE_Y,
                     WAVE_RIGHT - WAVE_LEFT + 1, TRACK_COLOR);

  int16_t previousX = WAVE_LEFT;
  int16_t previousY = static_cast<int16_t>(
      WAVE_CENTER_Y - WAVE_AMPLITUDE * sinf(wavePhase));
  for (uint8_t i = 1; i <= WAVE_SEGMENTS; ++i) {
    const float position = static_cast<float>(i) / WAVE_SEGMENTS;
    const int16_t x = static_cast<int16_t>(
        WAVE_LEFT + position * (WAVE_RIGHT - WAVE_LEFT));
    const int16_t y = static_cast<int16_t>(
        WAVE_CENTER_Y -
        WAVE_AMPLITUDE * sinf(wavePhase + position * WAVE_TWO_PI));
    gfx->drawLine(previousX, previousY, x, y, WAVE_COLOR);
    previousX = x;
    previousY = y;
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(POT_PIN, INPUT);

  pinMode(DIP1_PIN, INPUT);
  pinMode(DIP2_PIN, INPUT);
  pinMode(DIP3_PIN, INPUT);

  if (TFT_BL_PIN >= 0) {
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
  }

  if (!gfx->begin()) {
    Serial.println("ERROR: GC9A01 initialization failed.");
    return;
  }
  displayReady = true;

  gfx->fillScreen(BACKGROUND_COLOR);
  drawGaugeTracks();
  drawBackgroundWaves(0);
  drawParameterNames();
  lastEffect = readEffectPreset();
  drawEffect(lastEffect);

  Serial.println("GC9A01 GUI test ready.");
}

void loop() {
  if (!displayReady) {
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastInputScanMs < INPUT_SCAN_INTERVAL_MS) {
    return;
  }
  lastInputScanMs = nowMs;

  const uint16_t rawValue = static_cast<uint16_t>(analogRead(POT_PIN));
  const uint16_t percent =
      static_cast<uint16_t>((static_cast<uint32_t>(rawValue) * 100U +
                             ADC_MAX_VALUE / 2U) /
                            ADC_MAX_VALUE);
  const uint8_t effect = readEffectPreset();

  if (effect != lastEffect) {
    drawEffect(effect);
    lastEffect = effect;
  }
  if (percent != lastPotPercent) {
    updateGaugeArcs(lastPotPercent, percent);
    lastPotPercent = percent;
  }
  drawBackgroundWaves(percent);
}
