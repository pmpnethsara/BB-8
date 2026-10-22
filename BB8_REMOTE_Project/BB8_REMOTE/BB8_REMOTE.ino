// =====================================================================
//  SMART BB-8 ROBOT  -  PACKAGE 3  -  BB8_REMOTE firmware
//  Board : ESP32 Dev Module (ESP32-WROOM-32), Arduino core 3.x
//  Libs  : Adafruit SSD1306, Adafruit GFX Library, Adafruit BusIO
//          (Library Manager). WiFi, Wire, Preferences are built in.
//
//  DATA FLOW (how the modules are interconnected)
//
//   Joystick 1/2 (ADC) ─┐
//   J1/J2 push switches ┼─► inputs.h ─► R.j1y/j1x, R.j2x/j2y, button events
//   PB1..PB4 ───────────┘                       │
//                                               ▼
//                          commands.h ─► DRV 20 Hz, HEAD, ROT, WAVE, STOP ─► Body
//                                    ─► EYE / BLINK / HB ─────────────────► Head
//                                    ─► CAP ──────────────────────────────► Camera
//                                    ─► RESET (PB4 hold) ─────────────► all three
//                                               │ (network.h, UDP)
//   Body / Head / Camera telemetry ─UDP 4211─► telemetry_rx.h ─► R.body / R.headNode / R.cam
//                                               │
//                                    display.h (OLED)  +  leds.h (LED1 / LED2)
//
//  The loop() never blocks (except the 0.5 s centre calibration at boot).
// =====================================================================

#include "config.h"
#include "state.h"
#include "inputs.h"
#include "telemetry_rx.h"
#include "network.h"
#include "commands.h"
#include "display.h"
#include "leds.h"
#include "console.h"

static bool every(uint32_t& last, uint32_t period) {
  uint32_t now = millis();
  if (now - last >= period) { last = now; return true; }
  return false;
}

void setup() {
  ledsBegin();                 // LED1 on = powered
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== SMART BB-8  REMOTE  %s ===\n", FW_VERSION);

  inputsBegin();
  displayBegin();              // "Calibrating... do not touch sticks"
  delay(400);
  calibrateCenter();           // joystick centres
  commandsBegin();
  networkBegin();
  Serial.println("Ready. Type 'help' for test commands.");
}

void loop() {
  static uint32_t tIn = 0, tOled = 0;

  consoleUpdate();
  networkUpdate();             // Wi-Fi + incoming telemetry
  if (every(tIn, INPUT_PERIOD_MS)) inputsUpdate();
  commandsUpdate();            // DRV heartbeat, HEAD, button events
  if (every(tOled, OLED_PERIOD_MS)) displayUpdate();
  ledsUpdate();

  if (R.restartAt && (int32_t)(millis() - R.restartAt) >= 0) {
    Serial.println("[SYS] full reset done - restarting Remote");
    Serial.flush();
    ESP.restart();
  }
}
