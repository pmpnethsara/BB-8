// =====================================================================
//  BB8_REMOTE - inputs.h      (Guide: Joystick 1/2, switches, PB1-PB4)
//
//  JOYSTICKS: ADC -> smoothing -> calibrated centre -> dead-zone ->
//             expo curve -> clean values:
//               R.j1y, R.j1x  : -100..100  (drive, sent as DRV,y,x)
//               R.j2x, R.j2y  : -1..1      (head, converted in commands.h)
//  Centre is measured at every power-up (do not touch the sticks while
//  the Remote starts). The full travel (min/max) can be learned once
//  with the console command "range" and is stored in flash (NVS).
//
//  BUTTONS: active LOW (button between pin and GND, internal pull-up),
//  debounced; they produce 'pressed' / 'released' edge events.
// =====================================================================
#pragma once
#include <Preferences.h>

static const uint8_t AXIS_PIN[4] = {PIN_J1_Y, PIN_J1_X, PIN_J2_X, PIN_J2_Y};
static const bool    AXIS_INV[4] = {J1_INVERT_Y, J1_INVERT_X, J2_INVERT_X, J2_INVERT_Y};
static const char*   AXIS_NAME[4] = {"J1Y", "J1X", "J2X", "J2Y"};
static Preferences   prefs;

static float readAxisRaw(uint8_t i) {           // 4-sample average
  uint32_t s = 0;
  for (int k = 0; k < 4; k++) s += analogRead(AXIS_PIN[i]);
  return s / 4.0f;
}

static void initButton(Button& b, uint8_t pin) {
  b.pin = pin;
  pinMode(pin, INPUT_PULLUP);
  b.down = b.rawLast = (digitalRead(pin) == LOW);
}

static void updateButton(Button& b, uint32_t now) {
  bool raw = (digitalRead(b.pin) == LOW);
  if (raw != b.rawLast) { b.rawLast = raw; b.changedMs = now; }
  if (raw != b.down && now - b.changedMs >= BTN_DEBOUNCE_MS) {
    b.down = raw;
    if (raw) { b.pressed = true; b.downSinceMs = now; b.longFired = false; }
    else b.released = true;
  }
}

void loadRange() {
  prefs.begin("bb8remote", true);
  for (int i = 0; i < 4; i++) {
    char k[8];
    snprintf(k, sizeof(k), "min%d", i); R.rmin[i] = prefs.getFloat(k, 0);
    snprintf(k, sizeof(k), "max%d", i); R.rmax[i] = prefs.getFloat(k, 4095);
  }
  prefs.end();
}

void saveRange() {
  prefs.begin("bb8remote", false);
  for (int i = 0; i < 4; i++) {
    char k[8];
    snprintf(k, sizeof(k), "min%d", i); prefs.putFloat(k, R.rmin[i]);
    snprintf(k, sizeof(k), "max%d", i); prefs.putFloat(k, R.rmax[i]);
  }
  prefs.end();
}

// Measure the joystick centres (sticks must be released). ~0.5 s
void calibrateCenter() {
  float sum[4] = {0, 0, 0, 0};
  const int N = 50;
  for (int n = 0; n < N; n++) {
    for (int i = 0; i < 4; i++) sum[i] += readAxisRaw(i);
    delay(10);
  }
  R.joyFault = false;
  for (int i = 0; i < 4; i++) {
    R.center[i] = R.raw[i] = sum[i] / N;
    if (R.center[i] < CAL_CENTER_MIN || R.center[i] > CAL_CENTER_MAX) {
      R.joyFault = true;
      Serial.printf("[JOY] %s centre %.0f is IMPOSSIBLE - check VCC/GND/signal wire\n",
                    AXIS_NAME[i], R.center[i]);
    }
    // a learned range must lie around the centre, otherwise use full scale
    if (R.rmin[i] >= R.center[i] - 200) R.rmin[i] = 0;
    if (R.rmax[i] <= R.center[i] + 200) R.rmax[i] = 4095;
  }
  R.calibrated = true;
  Serial.printf("[JOY] centres J1Y=%.0f J1X=%.0f J2X=%.0f J2Y=%.0f %s\n",
                R.center[0], R.center[1], R.center[2], R.center[3],
                R.joyFault ? "-> JOYSTICK FAULT, drive disabled" : "OK");
}

// Console "range": move both sticks around their full circle for 5 s
void learnRange() {
  float mn[4] = {4095, 4095, 4095, 4095}, mx[4] = {0, 0, 0, 0};
  Serial.println("[JOY] move BOTH sticks in full circles for 5 seconds...");
  uint32_t t0 = millis();
  while (millis() - t0 < 5000) {
    for (int i = 0; i < 4; i++) { float v = readAxisRaw(i); mn[i] = min(mn[i], v); mx[i] = max(mx[i], v); }
    delay(5);
  }
  for (int i = 0; i < 4; i++) {
    // keep 3 % margin so full deflection always reaches 100 %
    R.rmin[i] = mn[i] + (R.center[i] - mn[i]) * 0.03f;
    R.rmax[i] = mx[i] - (mx[i] - R.center[i]) * 0.03f;
    Serial.printf("  %s min %.0f centre %.0f max %.0f\n", AXIS_NAME[i], R.rmin[i], R.center[i], R.rmax[i]);
  }
  saveRange();
  Serial.println("[JOY] range saved to flash");
}

void resetRange() {
  for (int i = 0; i < 4; i++) { R.rmin[i] = 0; R.rmax[i] = 4095; }
  saveRange();
  Serial.println("[JOY] range reset to full scale");
}

// raw ADC -> -1..1 with dead-zone and expo
static float axisValue(uint8_t i) {
  float c = R.center[i], v;
  if (R.raw[i] >= c) v = (R.raw[i] - c) / max(1.0f, R.rmax[i] - c);
  else               v = (R.raw[i] - c) / max(1.0f, c - R.rmin[i]);
  v = constrain(v, -1.0f, 1.0f);
  if (AXIS_INV[i]) v = -v;
  float dz = JOY_DEADZONE_PCT / 100.0f, a = fabsf(v);
  if (a < dz) return 0;
  a = (a - dz) / (1.0f - dz);
  a = (1.0f - JOY_EXPO) * a + JOY_EXPO * a * a * a;
  return v < 0 ? -a : a;
}

void inputsBegin() {
  analogReadResolution(12);
  for (int i = 0; i < 4; i++) analogSetPinAttenuation(AXIS_PIN[i], ADC_11db);
  initButton(R.j1sw, PIN_J1_SW);
  initButton(R.j2sw, PIN_J2_SW);
  initButton(R.pb1, PIN_PB1);
  initButton(R.pb2, PIN_PB2);
  initButton(R.pb3, PIN_PB3);
  initButton(R.pb4, PIN_PB4);
  loadRange();
}

// call every INPUT_PERIOD_MS
void inputsUpdate() {
  uint32_t now = millis();
  for (int i = 0; i < 4; i++) R.raw[i] += (readAxisRaw(i) - R.raw[i]) * JOY_FILTER;

  if (R.calibrated && !R.joyFault) {
    R.j1y = (int)lroundf(axisValue(0) * DRIVE_LIMIT_PCT);
    R.j1x = (int)lroundf(axisValue(1) * DRIVE_LIMIT_PCT);
    R.j2x = axisValue(2);
    R.j2y = axisValue(3);
  } else {
    R.j1y = R.j1x = 0;
    R.j2x = R.j2y = 0;
  }
  updateButton(R.j1sw, now);
  updateButton(R.j2sw, now);
  updateButton(R.pb1, now);
  updateButton(R.pb2, now);
  updateButton(R.pb3, now);
  updateButton(R.pb4, now);
}
