// =====================================================================
//  BB8_BODY - battery.h        (Guide step 2: voltage sensor)
//
//  Battery(+) -> voltage sensor module -> GPIO34 (ADC1).
//  Vbatt = Vpin * BATT_DIVIDER * BATT_CAL
//  analogReadMilliVolts() uses the ESP32's factory ADC calibration.
//  With a 5:1 divider the max measurable voltage is ~16 V (3.3 V pin limit).
//
//  OUTPUT: S.battV, S.battPct, S.battLow, S.battCrit, S.battPresent
//  USED BY: motors.h (low = speed limit), protocol.h (crit = DISABLED)
// =====================================================================
#pragma once

#if ENABLE_BATTERY
static uint32_t battCritSince = 0;

static float readBatteryVolts() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_BATT);
  return (sum / 8.0f) / 1000.0f * BATT_DIVIDER * BATT_CAL;
}

void batteryBegin() {
  pinMode(PIN_BATT, INPUT);
  analogReadResolution(12);
  S.battV = readBatteryVolts();
  S.battPresent = S.battV > BATT_PRESENT_MIN_V;
  Serial.printf("[BAT] %.2f V %s\n", S.battV, S.battPresent ? "" : "(no battery - USB bench mode)");
}

void batteryUpdate() {
  float v = readBatteryVolts();
  bool present = v > BATT_PRESENT_MIN_V;

  if (!present) {                 // USB bench power: no battery warnings
    S.battPresent = false;
    S.battV = v; S.battPct = 0;
    S.battLow = false; battCritSince = 0;
    return;
  }
  if (!S.battPresent) S.battV = v; // battery just connected: reset filter
  S.battPresent = true;

  S.battV = S.battV * 0.9f + v * 0.1f;   // smoothing filter
  float pct = (S.battV - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
  S.battPct = (uint8_t)constrain(pct, 0.0f, 100.0f);

  // LOW warning with hysteresis
  if (!S.battLow && S.battV < BATT_LOW_V) S.battLow = true;
  else if (S.battLow && S.battV > BATT_LOW_V + BATT_HYST_V) S.battLow = false;

  // CRITICAL: must persist (motor start-up sag is ignored). Latched until reboot,
  // because a resting battery "recovers" and would otherwise oscillate.
  if (S.battV < BATT_CRIT_V) {
    if (battCritSince == 0) battCritSince = millis();
    else if (!S.battCrit && millis() - battCritSince > BATT_CRIT_DELAY_MS) {
      S.battCrit = true;
      Serial.printf("[BAT] CRITICAL %.2f V -> drive disabled (recharge + reboot)\n", S.battV);
    }
  } else {
    battCritSince = 0;
  }
}

#else
void batteryBegin() {}
void batteryUpdate() {}
#endif
