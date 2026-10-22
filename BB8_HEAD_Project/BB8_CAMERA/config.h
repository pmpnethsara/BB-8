// =====================================================================
//  BB8_CAMERA - config.h
//  EVERY setting you are expected to change lives in this file.
//  Board: AI-Thinker ESP32-CAM (OV2640 camera, 4 MB PSRAM, microSD slot)
// =====================================================================
#pragma once
#include <Arduino.h>
#include <IPAddress.h>

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "BB8_CAMERA needs the ESP32 Arduino core 3.x (Boards Manager: 'esp32 by Espressif Systems' >= 3.0.0)"
#endif

#define FW_VERSION "CAM-1.0.0"

// ---------------------------------------------------------------------
// 1. FEATURE SWITCHES
// ---------------------------------------------------------------------
#define ENABLE_WIFI     1
#define ENABLE_SD       1      // save every capture to the microSD card (if a card is inserted)
#define CAPTURE_FLASH   0      // 1 = fire the white flash LED (GPIO4) for each still capture

// ---------------------------------------------------------------------
// 2. WI-FI / IP PLAN  (must match BB8_PROTOCOL.md and the other nodes)
// ---------------------------------------------------------------------
#define WIFI_SSID      "BB8_NET"
#define WIFI_PASSWORD  "bb8password"
#define CAM_HOSTNAME   "bb8-cam"            // also http://bb8-cam.local

#define USE_STATIC_IP  1
static const IPAddress IP_BODY    (192, 168, 8, 201);
static const IPAddress IP_HEAD    (192, 168, 8, 202);
static const IPAddress IP_CAMERA  (192, 168, 8, 203);
static const IPAddress IP_REMOTE  (192, 168, 8, 204);
static const IPAddress IP_GATEWAY (192, 168, 8, 1);
static const IPAddress IP_SUBNET  (255, 255, 255, 0);
static const IPAddress IP_DNS     (192, 168, 8, 1);

#define PORT_CAM_CMD     4213   // camera LISTENS here for CAP / FLASH / RESET (UDP)
#define PORT_TELEMETRY   4211   // camera SENDS its status to this port on Remote + Head
#define PORT_HTTP        80     // test page, /capture, /latest.jpg, /status, /cmd
#define PORT_STREAM      81     // live MJPEG stream:  http://192.168.8.203:81/stream

#define STRICT_REMOTE_IP   1
#define ALLOW_TEST_SOURCE  1

// ---------------------------------------------------------------------
// 3. CAMERA IMAGE
// ---------------------------------------------------------------------
// Frame size for stream AND still capture (one size = no glitch when
// capturing while streaming). FRAMESIZE_QVGA 320x240, FRAMESIZE_VGA 640x480,
// FRAMESIZE_SVGA 800x600, FRAMESIZE_XGA 1024x768 (slower over Wi-Fi)
#define CAM_FRAME_SIZE     FRAMESIZE_VGA
#define CAM_JPEG_QUALITY   12      // 10..63, lower = better quality, bigger file
#define CAM_FB_COUNT       2       // 2 frame buffers in PSRAM = smooth stream
#define CAM_XCLK_HZ        20000000
#define CAM_VFLIP          0       // flip the picture if the camera is mounted upside-down
#define CAM_HMIRROR        0
#define STREAM_MAX_FPS     15      // limit to protect Wi-Fi bandwidth for robot control

#define FLASH_PRE_MS       120     // flash on this long before the still is taken
#define PIN_FLASH_LED      4       // white flash LED on the AI-Thinker board
#define PIN_RED_LED        33      // small red LED on the back, active LOW

// ---------------------------------------------------------------------
// 4. TIMING
// ---------------------------------------------------------------------
#define STATUS_PERIOD_MS   1000    // status to Remote + Head once per second
#define SESSION_TIMEOUT_MS 2000
#define WIFI_RETRY_MS      5000
#define CAM_RETRY_MS       10000   // camera init re-try if the sensor did not start
