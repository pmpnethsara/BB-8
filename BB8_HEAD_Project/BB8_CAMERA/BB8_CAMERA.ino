// =====================================================================
//  SMART BB-8 ROBOT  -  PACKAGE 2  -  BB8_CAMERA firmware (ESP32-CAM)
//  Board : "AI Thinker ESP32-CAM", Arduino core 3.x
//          Tools -> Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
//          Tools -> PSRAM: "Enabled" (if the menu is shown)
//  Libs  : none extra (esp_camera, esp_http_server, SD_MMC are built in)
//
//  DATA FLOW
//   Remote PB3 --UDP CAP----> protocol.h --+
//   Dashboard --HTTP /capture---------------+--> camera.h cameraCapture()
//   console "cap" --------------------------+        |-> latest image (PSRAM) -> /latest.jpg
//                                                     |-> microSD /bb8_00001.jpg
//                                                     '-> C.captures++ -> status now
//   OV2640 --> http_server.h :81/stream --> Dashboard live view
//   telemetry.h --UDP 4211--> Remote (OLED "CAM") + Head (shutter blink)
// =====================================================================

#include "config.h"
#include "state.h"
#include "camera.h"
#include "protocol.h"
#include "telemetry.h"
#include "http_server.h"
#include "network.h"
#include "console.h"

static bool every(uint32_t& last, uint32_t period) {
  uint32_t now = millis();
  if (now - last >= period) { last = now; return true; }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== SMART BB-8  CAMERA  %s ===\n", FW_VERSION);
  gCapMutex = xSemaphoreCreateMutex();
  gCmdMutex = xSemaphoreCreateMutex();
  cameraBegin();
  networkBegin();
  Serial.println("Ready. Type 'help'.");
}

void loop() {
  static uint32_t tTel = 0;
  consoleUpdate();
  networkUpdate();
  cameraUpdate();

  if (C.statusNow || every(tTel, STATUS_PERIOD_MS)) {
    C.statusNow = false;
    tTel = millis();
    telemetryUpdate();
  }

  // red LED: slow blink while Wi-Fi is down (it also flashes on each capture)
  static bool ledWasBlinking = false;
  if (!C.netUp) { digitalWrite(PIN_RED_LED, (millis() / 500) % 2); ledWasBlinking = true; }
  else if (ledWasBlinking) { digitalWrite(PIN_RED_LED, HIGH); ledWasBlinking = false; }

  if (C.resetAt && (int32_t)(millis() - C.resetAt) >= 0) {
    Serial.println("[SYS] restarting ESP32-CAM...");
    Serial.flush();
    setFlash(false);
    delay(50);
    ESP.restart();
  }
  delay(2);   // let the camera / HTTP tasks run
}
