// =====================================================================
//  BB8_BODY - system.h
//  Small helpers for the main loop: non-blocking timer + status LED.
// =====================================================================
#pragma once

// returns true once every 'period' ms (non-blocking scheduler)
static bool every(uint32_t& last, uint32_t period) {
  uint32_t now = millis();
  if (now - last >= period) { last = now; return true; }
  return false;
}

// fast blink = no Wi-Fi | medium = FAILSAFE/DISABLED | slow = IDLE | solid = ACTIVE
static void statusLedUpdate() {
  uint32_t period;
  if (ENABLE_WIFI && !S.netUp) period = 100;
  else if (S.state == ST_FAILSAFE || S.state == ST_DISABLED) period = 300;
  else if (S.state == ST_IDLE) period = 1000;
  else { digitalWrite(PIN_STATUS_LED, HIGH); return; }
  digitalWrite(PIN_STATUS_LED, (millis() / period) % 2);
}
