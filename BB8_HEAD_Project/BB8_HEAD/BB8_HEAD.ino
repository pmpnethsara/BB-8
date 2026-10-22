// =====================================================================
//  SMART BB-8 ROBOT  -  PACKAGE 2  -  BB8_HEAD firmware (Head ESP32)
//  Board : ESP32 Dev Module (ESP32-WROOM-32), Arduino core 3.x
//  Libs  : Adafruit SSD1306, Adafruit GFX Library, Adafruit BusIO
//          (Library Manager). WiFi, WebServer, ESPmDNS, Wire are built in.
//
//  DATA FLOW (how the modules are interconnected)
//
//   network.h --UDP 4212 / HTTP /cmd--> protocol.h --> eyesRequestMode()
//   console.h --serial text-----------> (seq, eid)     eyesRequestBlink()
//                                                            |
//   network.h --UDP 4211 telemetry----> protocol.h --> H.bodyServo, H.bodyBattLow,
//     (Body + Camera)                                  H.camCaptures (shutter blink)
//                                                            |
//                                       eyes.h  --> H.eyeMode, H.lid, H.lookX/Y
//                                                            |
//                                       oled.h  --> OLED 1 (left) + OLED 2 (right)
//
//   telemetry.h <-- everything in H --> network.h --UDP 4211--> Remote / HTTP --> Dashboard
//
//  The loop() never uses delay(): eyes render at EYE_FPS, telemetry at
//  4 Hz, and network commands are handled every loop.
// =====================================================================

#include "config.h"
#include "state.h"
#include "eyes.h"
#include "oled.h"
#include "protocol.h"
#include "telemetry.h"
#include "network.h"
#include "console.h"
#include "system.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== SMART BB-8  HEAD  %s ===\n", FW_VERSION);
  pinMode(PIN_STATUS_LED, OUTPUT);

  oledBegin();          // OLED 1 + OLED 2
  eyesBegin();
  networkBegin();
  Serial.println("Ready. Type 'help' for test commands.");
}

void loop() {
  static uint32_t tEye = 0, tTel = 0;

  consoleUpdate();
  networkUpdate();

  if (every(tEye, 1000 / EYE_FPS))      { eyesUpdate(); oledUpdate(); }
  if (every(tTel, TELEMETRY_PERIOD_MS)) telemetryUpdate();

  statusLedUpdate();
  resetUpdate();
}
