// =====================================================================
//  BB8_CAMERA - camera.h      (Guide: still image + live stream source)
//
//  Starts the OV2640 on the AI-Thinker ESP32-CAM and implements the ONE
//  capture function used by every trigger:
//      Remote PB3 (UDP CAP)  |  Dashboard Capture (HTTP /capture)  |  console "cap"
//  The capture is kept in PSRAM as the "latest image" (/latest.jpg)
//  and, if a microSD card is inserted, saved as /bb8_00001.jpg, ...
// =====================================================================
#pragma once
#include "esp_camera.h"
#include <Preferences.h>
#if ENABLE_SD
#include "FS.h"
#include "SD_MMC.h"
#endif

// ---- AI-Thinker ESP32-CAM pin map (fixed on the board - do not change) ----
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

static uint8_t*  latestBuf = nullptr;     // copy of the last captured JPEG (PSRAM)
static size_t    latestCap = 0;           // allocated size
static uint32_t  camRetryMs = 0;
static Preferences prefs;

bool cameraInit() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = CAM_PIN_D0; c.pin_d1 = CAM_PIN_D1; c.pin_d2 = CAM_PIN_D2; c.pin_d3 = CAM_PIN_D3;
  c.pin_d4 = CAM_PIN_D4; c.pin_d5 = CAM_PIN_D5; c.pin_d6 = CAM_PIN_D6; c.pin_d7 = CAM_PIN_D7;
  c.pin_xclk = CAM_PIN_XCLK; c.pin_pclk = CAM_PIN_PCLK;
  c.pin_vsync = CAM_PIN_VSYNC; c.pin_href = CAM_PIN_HREF;
  c.pin_sccb_sda = CAM_PIN_SIOD; c.pin_sccb_scl = CAM_PIN_SIOC;
  c.pin_pwdn = CAM_PIN_PWDN; c.pin_reset = CAM_PIN_RESET;
  c.xclk_freq_hz = CAM_XCLK_HZ;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size   = C.psram ? CAM_FRAME_SIZE : FRAMESIZE_QVGA;
  c.jpeg_quality = CAM_JPEG_QUALITY;
  c.fb_count     = C.psram ? CAM_FB_COUNT : 1;
  c.fb_location  = C.psram ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  c.grab_mode    = CAMERA_GRAB_LATEST;         // always the newest frame (low latency)

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("[CAM] init FAILED 0x%x - check the camera ribbon cable\n", err);
    return false;
  }
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, CAM_VFLIP);
    s->set_hmirror(s, CAM_HMIRROR);
  }
  Serial.printf("[CAM] OV2640 ready, PSRAM %s\n", C.psram ? "yes" : "NO (QVGA only)");
  return true;
}

#if ENABLE_SD
static void sdBegin() {
  // 1-bit mode: uses GPIO 2/14/15 only, keeps GPIO4 (flash LED) free
  C.sdOk = SD_MMC.begin("/sdcard", true) && SD_MMC.cardType() != CARD_NONE;
  if (C.sdOk) Serial.printf("[SD] card %llu MB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));
  else        Serial.println("[SD] no card - captures are kept in RAM only (/latest.jpg)");
}
#endif

void cameraBegin() {
  pinMode(PIN_FLASH_LED, OUTPUT);
  digitalWrite(PIN_FLASH_LED, LOW);
  pinMode(PIN_RED_LED, OUTPUT);
  digitalWrite(PIN_RED_LED, HIGH);              // off (active LOW)
  C.psram = psramFound();
  C.camOk = cameraInit();
  camRetryMs = millis();
  prefs.begin("bb8cam", false);
  C.fileNo = prefs.getUInt("n", 0);
#if ENABLE_SD
  sdBegin();
#endif
}

// re-try a camera that failed to start (loose ribbon cable at power-up)
void cameraUpdate() {
  if (!C.camOk && millis() - camRetryMs > CAM_RETRY_MS) {
    camRetryMs = millis();
    esp_camera_deinit();
    C.camOk = cameraInit();
  }
}

void setFlash(bool on) {
  C.flashOn = on;
  digitalWrite(PIN_FLASH_LED, on ? HIGH : LOW);
}

// THE capture function. Safe to call from the loop and from the HTTP task.
bool cameraCapture() {
  if (!C.camOk) { C.lastCapOk = false; C.statusNow = true; return false; }
  xSemaphoreTake(gCapMutex, portMAX_DELAY);
  digitalWrite(PIN_RED_LED, LOW);

#if CAPTURE_FLASH
  bool torch = C.flashOn;
  digitalWrite(PIN_FLASH_LED, HIGH);
  delay(FLASH_PRE_MS);
  camera_fb_t* old = esp_camera_fb_get();       // drop a frame exposed without flash
  if (old) esp_camera_fb_return(old);
#endif
  camera_fb_t* fb = esp_camera_fb_get();
#if CAPTURE_FLASH
  if (!torch) digitalWrite(PIN_FLASH_LED, LOW);
#endif

  bool ok = false;
  if (fb && fb->format == PIXFORMAT_JPEG && fb->len > 0) {
    if (fb->len > latestCap) {                  // grow the buffer if needed
      uint8_t* nb = (uint8_t*)(C.psram ? ps_realloc(latestBuf, fb->len) : realloc(latestBuf, fb->len));
      if (nb) { latestBuf = nb; latestCap = fb->len; }
    }
    if (latestBuf && fb->len <= latestCap) {
      memcpy(latestBuf, fb->buf, fb->len);
      C.lastSize = fb->len;
      ok = true;
    }
  }
  if (fb) esp_camera_fb_return(fb);

  C.lastSaved[0] = 0;
#if ENABLE_SD
  if (ok && C.sdOk) {
    C.fileNo++;
    snprintf(C.lastSaved, sizeof(C.lastSaved), "/bb8_%05lu.jpg", (unsigned long)C.fileNo);
    File f = SD_MMC.open(C.lastSaved, FILE_WRITE);
    if (f) { f.write(latestBuf, C.lastSize); f.close(); prefs.putUInt("n", C.fileNo); }
    else   { C.lastSaved[0] = 0; C.sdOk = false; Serial.println("[SD] write failed - card removed?"); }
  }
#endif

  if (ok) C.captures++;
  C.lastCapOk = ok;
  C.lastCapMs = millis();
  C.statusNow = true;                           // tell Remote + Head right away
  digitalWrite(PIN_RED_LED, HIGH);
  xSemaphoreGive(gCapMutex);
  Serial.printf("[CAM] capture %s  #%lu  %u bytes %s\n", ok ? "OK" : "FAILED",
                (unsigned long)C.captures, (unsigned)C.lastSize, C.lastSaved);
  return ok;
}
