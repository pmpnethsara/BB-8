// =====================================================================
//  BB8_HEAD - system.h
//  Small helpers for the main loop: non-blocking timer, status LED and
//  the controlled reset (PB4 / Dashboard).
// =====================================================================
#pragma once

// returns true once every 'period' ms (non-blocking scheduler)
static bool every(uint32_t& last, uint32_t period) {
  uint32_t now = millis();
  if (now - last >= period) { last = now; return true; }
  return false;
}

// fast blink = no Wi-Fi | slow blink = Wi-Fi OK, Body not heard | solid = Body linked
static void statusLedUpdate() {
  uint32_t period;
  if (ENABLE_WIFI && !H.netUp) period = 100;
  else if (!linkAlive(H.bodyRxMs, LINK_TIMEOUT_MS)) period = 1000;
  else { digitalWrite(PIN_STATUS_LED, HIGH); return; }
  digitalWrite(PIN_STATUS_LED, (millis() / period) % 2);
}

static void resetUpdate() {
  if (H.resetAt && (int32_t)(millis() - H.resetAt) >= 0) {
    for (uint8_t i = 0; i < 2; i++) if (H.oledOk[i]) { OLED[i]->clearDisplay(); OLED[i]->display(); }
    Serial.println("[SYS] restarting Head ESP32...");
    Serial.flush();
    delay(50);
    ESP.restart();
  }
}
