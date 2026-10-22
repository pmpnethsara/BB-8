// =====================================================================
//  BB8_HEAD - eyes.h          (Guide: eye designs, eye mode, blink)
//
//  The ANIMATION brain of the eyes. It never touches the displays -
//  it only updates H.lid (eyelid position), H.lookX/Y (where the eyes
//  look) and H.eyeMode (which design). oled.h then draws exactly the
//  same picture on BOTH displays, so the two eyes are always in sync.
//
//  INPUT : eyesRequestMode()  <- PB1 / Dashboard / console
//          eyesRequestBlink() <- PB2 / Dashboard / console / auto / capture
//          H.bodyServo[]      <- Body telemetry (eyes follow the head)
//  OUTPUT: H.eyeMode, H.lid, H.lookX, H.lookY
//
//  Changing design: the eyes close, the design is swapped while they
//  are closed, then they open again ("blink and return to the
//  selected eye design").
// =====================================================================
#pragma once

static uint32_t nextAutoBlinkMs = 0;
static uint32_t nextIdleLookMs = 0;

static uint32_t randBetween(uint32_t a, uint32_t b) { return a + (esp_random() % (b - a + 1)); }

void eyesRequestBlink() {
  if (H.blink != BL_NONE) return;          // already blinking - ignore repeats
  H.blink = BL_CLOSING;
  H.blinkPhaseMs = millis();
  H.blinkCount++;
}

// n = design number, or -1 = next, -2 = previous
void eyesRequestMode(int n) {
  int base = (H.pendingMode >= 0) ? H.pendingMode : H.eyeMode;
  if (n == -1) n = (base + 1) % EYE_COUNT;
  else if (n == -2) n = (base + EYE_COUNT - 1) % EYE_COUNT;
  if (n < 0 || n >= EYE_COUNT) return;
  if (n == H.eyeMode && H.pendingMode < 0) return;
  H.pendingMode = n;
  Serial.printf("[EYE] design -> %d %s\n", n, EYE_NAMES[n]);
  if (H.blink == BL_NONE) eyesRequestBlink();   // swap happens while the lids are closed
  // if a blink is already OPENING, the swap waits for the next blink (started below)
}

void eyesBegin() {
  H.eyeMode = (EYE_DEFAULT_MODE < EYE_COUNT) ? EYE_DEFAULT_MODE : 0;
  nextAutoBlinkMs = millis() + randBetween(AUTO_BLINK_MIN_MS, AUTO_BLINK_MAX_MS);
  nextIdleLookMs = millis() + 1000;
}

static void updateBlink(uint32_t now) {
  uint32_t t = now - H.blinkPhaseMs;
  switch (H.blink) {
    case BL_NONE:
      H.lid = 0;
      if (H.pendingMode >= 0) eyesRequestBlink();      // a mode change is waiting
      break;
    case BL_CLOSING:
      H.lid = min(1.0f, t / (float)BLINK_CLOSE_MS);
      if (t >= BLINK_CLOSE_MS) { H.blink = BL_CLOSED; H.blinkPhaseMs = now; H.lid = 1; }
      break;
    case BL_CLOSED:
      H.lid = 1;
      if (H.pendingMode >= 0) {                        // swap design while closed
        H.eyeMode = H.pendingMode;
        H.pendingMode = -1;
      }
      if (t >= BLINK_HOLD_MS) { H.blink = BL_OPENING; H.blinkPhaseMs = now; }
      break;
    case BL_OPENING:
      H.lid = 1.0f - min(1.0f, t / (float)BLINK_OPEN_MS);
      if (t >= BLINK_OPEN_MS) { H.blink = BL_NONE; H.lid = 0; }
      break;
  }
}

static void updateLook(uint32_t now) {
  bool headMoved = false;
#if FOLLOW_HEAD_SERVOS
  if (linkAlive(H.bodyRxMs, LINK_TIMEOUT_MS)) {
    float pan  = H.bodyServo[HEAD_PAN_SERVO]  - HEAD_HOME_DEG;
    float tilt = H.bodyServo[HEAD_TILT_SERVO] - HEAD_HOME_DEG;
    if (fabsf(pan) > 3 || fabsf(tilt) > 3) {
      headMoved = true;
      H.lookTX = constrain(HEAD_PAN_SIGN  * pan  / HEAD_PAN_RANGE_DEG  * LOOK_MAX_X, -LOOK_MAX_X, LOOK_MAX_X);
      H.lookTY = constrain(HEAD_TILT_SIGN * tilt / HEAD_TILT_RANGE_DEG * LOOK_MAX_Y, -LOOK_MAX_Y, LOOK_MAX_Y);
      nextIdleLookMs = now + IDLE_LOOK_MIN_MS;
    }
  }
#endif
#if IDLE_LOOK
  if (!headMoved && (int32_t)(now - nextIdleLookMs) >= 0) {
    nextIdleLookMs = now + randBetween(IDLE_LOOK_MIN_MS, IDLE_LOOK_MAX_MS);
    if (esp_random() % 3 == 0) { H.lookTX = 0; H.lookTY = 0; }        // often look straight
    else {
      H.lookTX = (int)(esp_random() % (2 * LOOK_MAX_X + 1)) - LOOK_MAX_X;
      H.lookTY = ((int)(esp_random() % (2 * LOOK_MAX_Y + 1)) - LOOK_MAX_Y) / 2;
    }
  }
#else
  if (!headMoved) { H.lookTX = 0; H.lookTY = 0; }
#endif
  H.lookX += (H.lookTX - H.lookX) * LOOK_SMOOTH;
  H.lookY += (H.lookTY - H.lookY) * LOOK_SMOOTH;
}

// call at EYE_FPS
void eyesUpdate() {
  uint32_t now = millis();
#if AUTO_BLINK
  if ((int32_t)(now - nextAutoBlinkMs) >= 0) {
    nextAutoBlinkMs = now + randBetween(AUTO_BLINK_MIN_MS, AUTO_BLINK_MAX_MS);
    eyesRequestBlink();
  }
#endif
  updateBlink(now);
  updateLook(now);
}

// Eyelid amount actually drawn: blink, plus a droop when the Body battery is LOW
float eyesDrawLid() {
  float lid = H.lid;
#if LOW_BATT_SLEEPY
  if (H.bodyBattLow && linkAlive(H.bodyRxMs, LINK_TIMEOUT_MS) && H.eyeMode != EYE_SLEEPY)
    lid = max(lid, 0.35f);
#endif
  return lid;
}
