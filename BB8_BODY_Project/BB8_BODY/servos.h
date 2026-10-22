// =====================================================================
//  BB8_BODY - servos.h         (Guide step 3: three MG995 servos)
//
//  Uses the ESP32 hardware LEDC PWM (50 Hz, 14-bit) - no Servo library,
//  no jitter from Wi-Fi.  Pulse 500..2500 us = 0..180 deg.
//  Every target is clamped to SERVO_MIN_DEG/SERVO_MAX_DEG, and the servo
//  moves toward it at SERVO_SPEED_DPS (smooth motion, lower current peaks).
//
//  INPUT : HEAD targets (Joystick 2 / Dashboard), WAVE event
//  OUTPUT: servo PWM, S.servoPos[], S.servoTarget[], S.waving
//  POWER : MG995 stall ~2.5 A each -> separate 5-6 V supply >= 5 A,
//          grounds connected to ESP32 GND. Never from the ESP32 3V3/5V pin.
// =====================================================================
#pragma once

#if ENABLE_SERVOS
static const uint8_t SERVO_PINS[3] = {PIN_SERVO1, PIN_SERVO2, PIN_SERVO3};

// Head-wave gesture: target angles for S1,S2,S3 and how long to hold each frame
struct WaveFrame { float a[3]; uint16_t holdMs; };
static const WaveFrame WAVE_SEQ[] = {
  {{ 90,  75,  90}, 300},
  {{115,  75,  70}, 350},
  {{ 65,  75, 110}, 350},
  {{115,  75,  70}, 350},
  {{ 65,  75, 110}, 350},
  {{ 90,  90,  90}, 300},
};
static const uint8_t WAVE_LEN = sizeof(WAVE_SEQ) / sizeof(WAVE_SEQ[0]);
static uint8_t  waveIdx = 0;
static uint32_t waveStepMs = 0;
static float    preWave[3];

static uint32_t servoDuty(float deg) {
  float us = SERVO_US_MIN + (deg / 180.0f) * (SERVO_US_MAX - SERVO_US_MIN);
  return (uint32_t)(us * ((1UL << SERVO_PWM_BITS) - 1) / 20000.0f);   // 20 ms period
}

static float clampServo(uint8_t i, float deg) {
  return constrain(deg, SERVO_MIN_DEG[i], SERVO_MAX_DEG[i]);
}

void servoSetTarget(uint8_t i, float deg) {
  if (i > 2) return;
  S.servoTarget[i] = clampServo(i, deg);
}

void servosSetHeadTargets(float a, float b, float c) {
  if (S.waving) return;                 // gesture has priority until finished
  servoSetTarget(0, a);
  servoSetTarget(1, b);
  servoSetTarget(2, c);
}

void servosHome() {
  for (uint8_t i = 0; i < 3; i++) servoSetTarget(i, SERVO_HOME_DEG[i]);
}

static void applyWaveFrame(uint8_t idx) {
  for (uint8_t i = 0; i < 3; i++) S.servoTarget[i] = clampServo(i, WAVE_SEQ[idx].a[i]);
  waveStepMs = millis();
}

void servoStartWave() {
  if (S.waving) return;
  for (uint8_t i = 0; i < 3; i++) preWave[i] = S.servoTarget[i];
  S.waving = true;
  waveIdx = 0;
  applyWaveFrame(0);
  Serial.println("[SRV] wave gesture start");
}

void servosBegin() {
  for (uint8_t i = 0; i < 3; i++) {
    S.servoPos[i] = S.servoTarget[i] = SERVO_HOME_DEG[i];
    ledcAttach(SERVO_PINS[i], SERVO_PWM_FREQ, SERVO_PWM_BITS);
    ledcWrite(SERVO_PINS[i], servoDuty(S.servoPos[i]));
  }
  Serial.println("[SRV] 3 servos at HOME");
}

void servosUpdate() {
  static uint32_t lastMs = millis();
  uint32_t now = millis();
  float dt = (now - lastMs) / 1000.0f;
  lastMs = now;

  // gesture sequencer
  if (S.waving && now - waveStepMs >= WAVE_SEQ[waveIdx].holdMs) {
    if (++waveIdx >= WAVE_LEN) {
      S.waving = false;
      for (uint8_t i = 0; i < 3; i++) S.servoTarget[i] = preWave[i];
      Serial.println("[SRV] wave gesture done");
    } else {
      applyWaveFrame(waveIdx);
    }
  }

  // smooth motion toward target
  float maxStep = (S.waving ? SERVO_WAVE_SPEED_DPS : SERVO_SPEED_DPS) * dt;
  for (uint8_t i = 0; i < 3; i++) {
    float diff = S.servoTarget[i] - S.servoPos[i];
    if (fabsf(diff) <= maxStep) S.servoPos[i] = S.servoTarget[i];
    else S.servoPos[i] += (diff > 0 ? maxStep : -maxStep);
    ledcWrite(SERVO_PINS[i], servoDuty(S.servoPos[i]));
  }
}

#else
void servosBegin() {}
void servosUpdate() {}
void servoSetTarget(uint8_t, float) {}
void servosSetHeadTargets(float, float, float) {}
void servosHome() {}
void servoStartWave() {}
#endif
