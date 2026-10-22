// =====================================================================
//  BB8_HEAD - config.h
//  EVERY setting you are expected to change lives in this file:
//  feature switches, Wi-Fi/IP plan, OLED wiring, eye behaviour, timing.
//  Do not hard-code numbers inside the modules - change them here.
// =====================================================================
#pragma once
#include <Arduino.h>
#include <IPAddress.h>

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "BB8_HEAD needs the ESP32 Arduino core 3.x (Boards Manager: 'esp32 by Espressif Systems' >= 3.0.0)"
#endif

#define FW_VERSION "HEAD-1.0.0"

// ---------------------------------------------------------------------
// 1. FEATURE SWITCHES  (1 = on, 0 = off)
//    Guide order: Head Wi-Fi -> OLED 1 -> OLED 2 -> eye graphics ->
//    eye mode -> blink -> network eye commands -> Head telemetry.
// ---------------------------------------------------------------------
#define ENABLE_WIFI        1
#define ENABLE_OLED_LEFT   1     // OLED 1
#define ENABLE_OLED_RIGHT  1     // OLED 2

// ---------------------------------------------------------------------
// 2. WI-FI / IP PLAN  (must match BB8_PROTOCOL.md and the other nodes)
// ---------------------------------------------------------------------
#define WIFI_SSID      "BB8_NET"
#define WIFI_PASSWORD  "bb8password"
#define HEAD_HOSTNAME  "bb8-head"          // also http://bb8-head.local

#define USE_STATIC_IP  1                   // 0 = use router DHCP reservation instead
static const IPAddress IP_BODY    (192, 168, 8, 201);
static const IPAddress IP_HEAD    (192, 168, 8, 202);
static const IPAddress IP_CAMERA  (192, 168, 8, 203);
static const IPAddress IP_REMOTE  (192, 168, 8, 204);
static const IPAddress IP_GATEWAY (192, 168, 8, 1);
static const IPAddress IP_SUBNET  (255, 255, 255, 0);
static const IPAddress IP_DNS     (192, 168, 8, 1);

#define PORT_TELEMETRY   4211   // Head LISTENS here for Body + Camera telemetry
                                // and SENDS its own telemetry to this port on the Remote
#define PORT_HEAD_CMD    4212   // Head LISTENS here for eye commands (EYE / BLINK ...)
#define PORT_HTTP        80     // test page + /telemetry + /cmd for the Dashboard

#define STRICT_REMOTE_IP   1    // source 'R' is only accepted from IP_REMOTE
#define ALLOW_TEST_SOURCE  1    // accept source 'T' (laptop test script). Set 0 for final build.

// ---------------------------------------------------------------------
// 3. TWO OLED EYES  (SSD1306 0.96" 128x64, I2C, 4 pins: GND VCC SCL SDA)
//
//    OLED_SAME_BUS 0 (default, recommended):
//       each OLED has its OWN I2C bus, so both can keep address 0x3C
//       (most cheap modules cannot change address easily).
//         LEFT  eye (OLED 1): SDA 21, SCL 22  (I2C bus 0 = Wire)
//         RIGHT eye (OLED 2): SDA 32, SCL 33  (I2C bus 1 = Wire1)
//    OLED_SAME_BUS 1:
//       both on SDA 21 / SCL 22. Then ONE module must be changed to
//       address 0x3D (move the address resistor on the back) and
//       OLED_R_ADDR set to 0x3D.
// ---------------------------------------------------------------------
#define OLED_SAME_BUS     0
#define PIN_OLED_L_SDA    21
#define PIN_OLED_L_SCL    22
#define PIN_OLED_R_SDA    32
#define PIN_OLED_R_SCL    33
#define OLED_L_ADDR       0x3C
#define OLED_R_ADDR       0x3C      // 0x3D when OLED_SAME_BUS 1
#define OLED_I2C_HZ       800000    // lower to 400000 if you see garbage / flicker
#define OLED_W            128
#define OLED_H            64
#define OLED_FLIP_L       0         // 1 = rotate that display 180 deg (mounted upside-down)
#define OLED_FLIP_R       0
#define OLED_CONTRAST     255       // 0..255 (lower = dimmer, less current)
#define OLED_RETRY_MS     5000      // a missing OLED is re-tried this often

// ---------------------------------------------------------------------
// 4. EYE BEHAVIOUR
// ---------------------------------------------------------------------
#define EYE_DEFAULT_MODE     0        // design shown after boot (0 = NORMAL)
#define EYE_FPS              30       // render rate of both displays
#define BLINK_CLOSE_MS       80       // eyelids closing
#define BLINK_HOLD_MS        40       // fully closed
#define BLINK_OPEN_MS        110      // eyelids opening

#define AUTO_BLINK           1        // natural random blinking
#define AUTO_BLINK_MIN_MS    2500
#define AUTO_BLINK_MAX_MS    7000

#define IDLE_LOOK            1        // small random eye movements when the head is still
#define IDLE_LOOK_MIN_MS     1500
#define IDLE_LOOK_MAX_MS     4000
#define LOOK_MAX_X           20       // max eye shift in pixels
#define LOOK_MAX_Y           10
#define LOOK_SMOOTH          0.25f    // 0..1 per frame (higher = faster eye motion)

#define FOLLOW_HEAD_SERVOS   1        // eyes look where the head servos point (Body telemetry)
#define HEAD_PAN_SERVO       2        // index in Body "servo":[s1,s2,s3]  (2 = S3 pan)
#define HEAD_TILT_SERVO      1        // (1 = S2 tilt forward/back)
#define HEAD_HOME_DEG        90.0f
#define HEAD_PAN_RANGE_DEG   45.0f    // servo offset from home that gives full LOOK_MAX_X
#define HEAD_TILT_RANGE_DEG  30.0f
#define HEAD_PAN_SIGN        1        // set -1 if the eyes look the wrong way
#define HEAD_TILT_SIGN       1

#define CAPTURE_BLINK        1        // eyes blink like a shutter when the camera captures
#define LOW_BATT_SLEEPY      1        // eyelids droop while the Body battery is LOW

// ---------------------------------------------------------------------
// 5. TIMING
// ---------------------------------------------------------------------
#define TELEMETRY_PERIOD_MS  250      // Head telemetry 4 Hz to the Remote
#define LINK_TIMEOUT_MS      1500     // Body / Remote considered offline after this
#define CAM_LINK_TIMEOUT_MS  3000     // Camera sends status once per second
#define SESSION_TIMEOUT_MS   2000     // silent source -> new sequence session
#define WIFI_RETRY_MS        5000

#define PIN_STATUS_LED       2        // on-board LED
