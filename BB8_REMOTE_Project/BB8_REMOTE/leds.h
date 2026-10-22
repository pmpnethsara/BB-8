// =====================================================================
//  BB8_REMOTE - leds.h        (Guide: LED logic)
//
//  LED1 POWER     : ON while the Remote runs.
//                   Fast blink = joystick fault (centre reading impossible).
//  LED2 CONNECTED : ON    = Body telemetry received within 1 s (robot linked)
//                   slow  = Wi-Fi OK but the Body is not answering
//                   fast  = Body battery CRITICAL or state FAILSAFE
//                   OFF   = no Wi-Fi
// =====================================================================
#pragma once

void ledsBegin() {
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  digitalWrite(PIN_LED1, HIGH);
  digitalWrite(PIN_LED2, LOW);
}

void ledsUpdate() {
  uint32_t now = millis();
  digitalWrite(PIN_LED1, R.joyFault ? (now / 100) % 2 : HIGH);

  bool led2;
  if (!R.netUp) led2 = false;
  else if (!linkAlive(R.body.rxMs, LINK_TIMEOUT_MS)) led2 = (now / 500) % 2;
  else if (R.body.battCrit || !strcmp(R.body.state, "FAILSAFE")) led2 = (now / 100) % 2;
  else led2 = true;
  digitalWrite(PIN_LED2, led2);
}
