// =====================================================================
//  BB8_HEAD - oled.h          (Guide: OLED 1 -> OLED 2 -> eye graphics)
//
//  Two SSD1306 128x64 displays = left and right robot eye.
//  Library: "Adafruit SSD1306" (+ "Adafruit GFX Library", "Adafruit BusIO")
//
//  "LEFT" / OLED 1 = the eye on YOUR left when you face the robot.
//  Both eyes are drawn from the same state (H.eyeMode, H.lid, H.lookX/Y)
//  in the same frame, so they always blink and move together.
//  Asymmetric designs (ANGRY / SAD) are mirrored automatically using
//  'inner' = the side that points toward the other eye.
// =====================================================================
#pragma once
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#if OLED_SAME_BUS
  #define WIRE_R Wire
#else
  #define WIRE_R Wire1
#endif

static Adafruit_SSD1306 oledL(OLED_W, OLED_H, &Wire,   -1, OLED_I2C_HZ, OLED_I2C_HZ);
static Adafruit_SSD1306 oledR(OLED_W, OLED_H, &WIRE_R, -1, OLED_I2C_HZ, OLED_I2C_HZ);
static Adafruit_SSD1306* OLED[2] = {&oledL, &oledR};
static TwoWire*          OLED_BUS[2] = {&Wire, &WIRE_R};
static const uint8_t     OLED_ADDR[2] = {OLED_L_ADDR, OLED_R_ADDR};
static const bool        OLED_ENABLED[2] = {ENABLE_OLED_LEFT, ENABLE_OLED_RIGHT};
static const bool        OLED_FLIP[2] = {OLED_FLIP_L, OLED_FLIP_R};
static uint32_t          oledRetryMs = 0;

static bool i2cProbe(TwoWire* bus, uint8_t addr) {
  bus->beginTransmission(addr);
  return bus->endTransmission() == 0;
}

static bool oledInitOne(uint8_t i) {
  if (!OLED_ENABLED[i]) return false;
  if (!i2cProbe(OLED_BUS[i], OLED_ADDR[i])) return false;
  // periphBegin=false: the I2C buses were already started on OUR pins
  if (!OLED[i]->begin(SSD1306_SWITCHCAPVCC, OLED_ADDR[i], true, false)) return false;
  OLED[i]->setRotation(OLED_FLIP[i] ? 2 : 0);
  OLED[i]->ssd1306_command(SSD1306_SETCONTRAST);
  OLED[i]->ssd1306_command(OLED_CONTRAST);
  OLED[i]->clearDisplay();
  OLED[i]->display();
  return true;
}

void oledBegin() {
  Wire.begin(PIN_OLED_L_SDA, PIN_OLED_L_SCL, OLED_I2C_HZ);
#if !OLED_SAME_BUS
  Wire1.begin(PIN_OLED_R_SDA, PIN_OLED_R_SCL, OLED_I2C_HZ);
#endif
  for (uint8_t i = 0; i < 2; i++) {
    H.oledOk[i] = oledInitOne(i);
    if (OLED_ENABLED[i])
      Serial.printf("[OLED] %s eye (OLED %d) at 0x%02X: %s\n", i ? "RIGHT" : "LEFT", i + 1,
                    OLED_ADDR[i], H.oledOk[i] ? "OK" : "NOT FOUND - check wiring");
    else
      Serial.printf("[OLED] OLED %d disabled in config.h\n", i + 1);
  }
  oledRetryMs = millis();
}

// Scan both buses (console "i2c")
void oledScan() {
  for (uint8_t b = 0; b < (OLED_SAME_BUS ? 1 : 2); b++) {
    TwoWire* bus = b ? &Wire1 : &Wire;
    Serial.printf("I2C bus %d (SDA %d SCL %d):", b,
                  b ? PIN_OLED_R_SDA : PIN_OLED_L_SDA, b ? PIN_OLED_R_SCL : PIN_OLED_L_SCL);
    int found = 0;
    for (uint8_t a = 1; a < 127; a++) if (i2cProbe(bus, a)) { Serial.printf(" 0x%02X", a); found++; }
    Serial.println(found ? "" : " nothing found");
  }
}

// ------------------------------------------------------------------
// EYE DESIGNS  -  d = display, cx/cy = eye centre (incl. look offset),
// inner = +1 if the inner side (toward the other eye) is to the right.
// ------------------------------------------------------------------
#define EYE_W  64
#define EYE_H  44
#define EYE_R  12

static void drawNormal(Adafruit_SSD1306& d, int cx, int cy) {
  d.fillRoundRect(cx - EYE_W / 2, cy - EYE_H / 2, EYE_W, EYE_H, EYE_R, SSD1306_WHITE);
}

static void drawHappy(Adafruit_SSD1306& d, int cx, int cy) {            // ^ ^
  drawNormal(d, cx, cy);
  d.fillCircle(cx, cy + 36, 38, SSD1306_BLACK);
}

static void drawAngry(Adafruit_SSD1306& d, int cx, int cy, int inner) { // brow slopes to the inside
  drawNormal(d, cx, cy);
  int top = cy - EYE_H / 2 - 2;
  int xIn = cx + inner * (EYE_W / 2 + 2), xOut = cx - inner * (EYE_W / 2 + 2);
  d.fillTriangle(xOut, top, xIn, top, xIn, cy + 4, SSD1306_BLACK);
}

static void drawSad(Adafruit_SSD1306& d, int cx, int cy, int inner) {   // brow slopes to the outside
  drawNormal(d, cx, cy);
  int top = cy - EYE_H / 2 - 2;
  int xIn = cx + inner * (EYE_W / 2 + 2), xOut = cx - inner * (EYE_W / 2 + 2);
  d.fillTriangle(xIn, top, xOut, top, xOut, cy + 2, SSD1306_BLACK);
}

static void drawSleepy(Adafruit_SSD1306& d, int cx, int cy) {           // half-closed
  drawNormal(d, cx, cy);
  d.fillRect(cx - EYE_W / 2 - 2, cy - EYE_H / 2 - 2, EYE_W + 4, EYE_H / 2 + 4, SSD1306_BLACK);
  d.drawFastHLine(cx - EYE_W / 2, cy + 2, EYE_W, SSD1306_WHITE);
}

static void drawLove(Adafruit_SSD1306& d, int cx, int cy) {             // pulsing heart
  float beat = 1.0f + 0.08f * sinf(millis() / 120.0f);
  int r = (int)(14 * beat), off = (int)(13 * beat);
  d.fillCircle(cx - off, cy - 6, r, SSD1306_WHITE);
  d.fillCircle(cx + off, cy - 6, r, SSD1306_WHITE);
  d.fillTriangle(cx - off - r + 1, cy - 2, cx + off + r - 1, cy - 2, cx, cy + (int)(26 * beat), SSD1306_WHITE);
}

static void drawLens(Adafruit_SSD1306& d, int cx, int cy, int inner) {  // BB-8 style camera lens
  d.fillCircle(cx, cy, 28, SSD1306_WHITE);
  d.fillCircle(cx, cy, 24, SSD1306_BLACK);
  d.fillCircle(cx, cy, 13, SSD1306_WHITE);
  d.fillCircle(cx, cy, 10, SSD1306_BLACK);
  d.fillCircle(cx - inner * 7, cy - 8, 3, SSD1306_WHITE);              // highlight
}

static void drawEyelids(Adafruit_SSD1306& d, int cx, int cy, float lid) {
  if (lid <= 0.01f) return;
  const int reach = 31;                                // lids travel from +-reach to the centre
  int upper = cy - reach + (int)(lid * reach);
  int lower = cy + reach - (int)(lid * reach);
  if (upper > 0) d.fillRect(0, 0, OLED_W, upper, SSD1306_BLACK);
  if (lower < OLED_H) d.fillRect(0, lower, OLED_W, OLED_H - lower, SSD1306_BLACK);
  if (lid >= 0.95f) d.drawFastHLine(cx - EYE_W / 2, cy, EYE_W, SSD1306_WHITE);  // closed-eye line
}

static void drawTestPattern(Adafruit_SSD1306& d, uint8_t i) {
  d.drawRect(0, 0, OLED_W, OLED_H, SSD1306_WHITE);
  d.setTextColor(SSD1306_WHITE);
  d.setTextSize(2);
  d.setCursor(i ? 34 : 40, 14);
  d.print(i ? "RIGHT" : "LEFT");
  d.setTextSize(1);
  d.setCursor(40, 40);
  d.printf("OLED %d", i + 1);
}

static void drawEye(uint8_t i) {
  Adafruit_SSD1306& d = *OLED[i];
  d.clearDisplay();
  if (H.testPattern) { drawTestPattern(d, i); d.display(); return; }
  int inner = (i == 0) ? +1 : -1;                      // left eye: inside is to the right
  int cx = OLED_W / 2 + (int)lroundf(H.lookX);
  int cy = OLED_H / 2 + (int)lroundf(H.lookY);
  switch (H.eyeMode) {
    case EYE_HAPPY:  drawHappy(d, cx, cy); break;
    case EYE_ANGRY:  drawAngry(d, cx, cy, inner); break;
    case EYE_SAD:    drawSad(d, cx, cy, inner); break;
    case EYE_SLEEPY: drawSleepy(d, cx, cy); break;
    case EYE_LOVE:   drawLove(d, cx, cy); break;
    case EYE_LENS:   drawLens(d, cx, cy, inner); break;
    default:         drawNormal(d, cx, cy); break;
  }
  drawEyelids(d, cx, cy, eyesDrawLid());
  d.display();
}

// call at EYE_FPS: draws both eyes and re-tries missing displays
void oledUpdate() {
  for (uint8_t i = 0; i < 2; i++) if (H.oledOk[i]) drawEye(i);

  if (millis() - oledRetryMs > OLED_RETRY_MS) {        // hot-plug / loose wire check
    oledRetryMs = millis();
    for (uint8_t i = 0; i < 2; i++) {
      if (!OLED_ENABLED[i]) continue;
      bool present = i2cProbe(OLED_BUS[i], OLED_ADDR[i]);
      if (H.oledOk[i] && !present) {
        H.oledOk[i] = false;
        Serial.printf("[OLED] OLED %d LOST\n", i + 1);
      } else if (!H.oledOk[i] && present) {
        H.oledOk[i] = oledInitOne(i);
        if (H.oledOk[i]) Serial.printf("[OLED] OLED %d back online\n", i + 1);
      }
    }
  }
}
