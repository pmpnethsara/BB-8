// =====================================================================
//  BB8_BODY - magnet.h         (Guide step 2: magnetic reed switch)
//
//  3.3V --[10k]--+-- GPIO35
//                |
//            [reed switch]
//                |
//               GND          magnet near -> switch closes -> pin LOW
//
//  The reed switch is a SIGNAL only. It tells the firmware whether the
//  main system is enabled. It must never carry motor/servo current.
//  If it must cut real power, let it drive a MOSFET/relay circuit.
//
//  OUTPUT: S.magnet  (true = enabled)
//  USED BY: protocol.h -> state DISABLED when MAG_REQUIRED_TO_DRIVE
// =====================================================================
#pragma once

#if ENABLE_MAGNET
static bool     magRawLast = false;
static uint32_t magChangeMs = 0;

static bool magnetReadRaw() {
  bool level = digitalRead(PIN_REED);
  return REED_ACTIVE_LOW ? (level == LOW) : (level == HIGH);
}

void magnetBegin() {
  pinMode(PIN_REED, REED_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
  magRawLast = magnetReadRaw();
  S.magnet = magRawLast;
  Serial.printf("[MAG] magnet %s\n", S.magnet ? "PRESENT (enabled)" : "ABSENT (disabled)");
}

void magnetUpdate() {           // debounce: state must be stable for MAG_DEBOUNCE_MS
  bool raw = magnetReadRaw();
  if (raw != magRawLast) {
    magRawLast = raw;
    magChangeMs = millis();
  } else if (raw != S.magnet && millis() - magChangeMs >= MAG_DEBOUNCE_MS) {
    S.magnet = raw;
    Serial.printf("[MAG] magnet %s\n", S.magnet ? "PRESENT (enabled)" : "REMOVED (disabled)");
  }
}

#else
void magnetBegin() { S.magnet = true; }   // no sensor = always enabled
void magnetUpdate() {}
#endif
