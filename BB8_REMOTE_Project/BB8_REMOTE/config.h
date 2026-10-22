// =====================================================================
//  BB8_REMOTE - config.h
//  EVERY setting you are expected to change lives in this file:
//  feature switches, Wi-Fi/IP plan, pin map, joystick feel, head
//  mapping, button timing. Do not hard-code numbers in the modules.
// =====================================================================
#pragma once
#include <Arduino.h>
#include <IPAddress.h>

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "BB8_REMOTE needs the ESP32 Arduino core 3.x (Boards Manager: 'esp32 by Espressif Systems' >= 3.0.0)"
#endif

#define FW_VERSION "REMOTE-1.0.0"

// ---------------------------------------------------------------------
// 1. FEATURE SWITCHES  (guide order: Joystick 1 -> rotate switch ->
//    Joystick 2 -> wave switch -> PB1-PB4 -> OLED -> LEDs -> Wi-Fi/IP)
//    With ENABLE_WIFI 0 the Remote runs stand-alone: every input is
//    shown on the OLED and in the Serial Monitor, nothing is sent.
// ---------------------------------------------------------------------
#define ENABLE_WIFI   1
#define ENABLE_OLED   1

// ---------------------------------------------------------------------
// 2. WI-FI / IP PLAN  (must match BB8_PROTOCOL.md and the other nodes)
// ---------------------------------------------------------------------
#define WIFI_SSID        "BB8_NET"
#define WIFI_PASSWORD    "bb8password"
#define REMOTE_HOSTNAME  "bb8-remote"

#define USE_STATIC_IP  1     // the Body only accepts source 'R' from IP_REMOTE!
static const IPAddress IP_BODY    (192, 168, 8, 201);
static const IPAddress IP_HEAD    (192, 168, 8, 202);
static const IPAddress IP_CAMERA  (192, 168, 8, 203);
static const IPAddress IP_REMOTE  (192, 168, 8, 204);
static const IPAddress IP_GATEWAY (192, 168, 8, 1);
static const IPAddress IP_SUBNET  (255, 255, 255, 0);
static const IPAddress IP_DNS     (192, 168, 8, 1);

#define PORT_BODY_CMD   4210   // Body listens   (DRV / HEAD / ROT / WAVE / STOP / RESET)
#define PORT_TELEMETRY  4211   // Remote LISTENS here for Body + Head + Camera telemetry
#define PORT_HEAD_CMD   4212   // Head listens   (EYE / BLINK / HB / RESET)
#define PORT_CAM_CMD    4213   // Camera listens (CAP / RESET)

// Bench test WITHOUT the robot: send everything to a laptop running
// tools/robot_sim.py instead of the real Body/Head/Camera.
#define USE_TEST_TARGET 0
static const IPAddress IP_TEST_TARGET (192, 168, 8, 100);

// ---------------------------------------------------------------------
// 3. PIN MAP  (ESP32 Dev Module)
//    Joystick axes MUST use ADC1 pins (32-39): ADC2 does not work
//    while Wi-Fi is on. 34/35/36/39 are input-only (no pull-ups) -
//    perfect for analog axes.
// ---------------------------------------------------------------------
#define PIN_J1_Y     34    // Joystick 1 VRy : forward / reverse
#define PIN_J1_X     35    // Joystick 1 VRx : left / right
#define PIN_J1_SW    32    // Joystick 1 SW  : rotate         (internal pull-up)
#define PIN_J2_X     36    // Joystick 2 VRx : head pan / side  (pin "VP")
#define PIN_J2_Y     39    // Joystick 2 VRy : head tilt        (pin "VN")
#define PIN_J2_SW    33    // Joystick 2 SW  : head wave      (internal pull-up)

#define PIN_PB1      25    // eye design / mode
#define PIN_PB2      26    // eye blink
#define PIN_PB3      27    // camera capture
#define PIN_PB4      14    // short press = STOP, hold = full robot reset

#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22
#define OLED_ADDR    0x3C
#define OLED_FLIP    0     // 1 = rotate the display 180 deg

#define PIN_LED1     18    // LED1 power      (330 ohm series resistor)
#define PIN_LED2     19    // LED2 connected  (330 ohm series resistor)

// ---------------------------------------------------------------------
// 4. JOYSTICK FEEL
// ---------------------------------------------------------------------
#define J1_INVERT_Y   0    // set 1 if pushing forward drives backward
#define J1_INVERT_X   0
#define J2_INVERT_X   0
#define J2_INVERT_Y   0
#define JOY_DEADZONE_PCT  8      // centre dead-zone (% of travel)
#define JOY_EXPO          0.35f  // 0 = linear, 1 = very soft centre (fine low-speed control)
#define JOY_FILTER        0.40f  // 0..1 smoothing of the ADC (higher = faster, noisier)
#define DRIVE_LIMIT_PCT   100    // maximum drive command sent to the Body
#define CAL_CENTER_MIN    800    // centre reading outside 800..3300 = wiring fault
#define CAL_CENTER_MAX    3300

// ---------------------------------------------------------------------
// 5. HEAD (Joystick 2 -> three MG995 target angles, sent as HEAD,a1,a2,a3)
//    Assumed like the Body: S1 = tilt left/right, S2 = tilt fwd/back, S3 = pan.
//    Angle = HOME + stick(-1..1) x gain. Use negative gains to reverse.
//    The Body clamps every angle to ITS safe limits anyway.
// ---------------------------------------------------------------------
static const float HEAD_HOME_DEG[3] = {90, 90, 90};   // same as SERVO_HOME_DEG in the Body
static const float HEAD_MIN_DEG[3]  = {45, 45, 45};   // same as SERVO_MIN_DEG in the Body
static const float HEAD_MAX_DEG[3]  = {135, 135, 135};
#define HEAD_X_TO_S1_DEG   10.0f   // side stick also leans the head a little
#define HEAD_Y_TO_S2_DEG   30.0f   // up/down stick -> tilt forward/back
#define HEAD_X_TO_S3_DEG   45.0f   // side stick -> pan
#define HEAD_HOLD_MODE     0       // 0 = proportional (spring back to home)
                                   // 1 = hold: stick moves the head, release keeps the angle
#define HEAD_HOLD_RATE_DPS 90.0f   // speed in hold mode

// ---------------------------------------------------------------------
// 6. TIMING
// ---------------------------------------------------------------------
#define INPUT_PERIOD_MS     10     // read joysticks/buttons at 100 Hz
#define DRV_PERIOD_MS       50     // DRV to the Body at 20 Hz - ALWAYS, also 0,0 (heartbeat)
#define HEAD_PERIOD_MS      66     // HEAD at 15 Hz while Joystick 2 is used
#define HEAD_HB_PERIOD_MS   500    // HB to the Head ("Remote online")
#define OLED_PERIOD_MS      100    // display refresh 10 Hz
#define EVENT_REPEATS       3      // one-shot events are sent 3x ...
#define EVENT_GAP_MS        30     // ... 30 ms apart (UDP may lose one)
#define BTN_DEBOUNCE_MS     30
#define ROT_LONG_PRESS_MS   700    // J1 push: short = rotate right (ROT,1), long = left (ROT,-1)
#define RESET_HOLD_MS       1500   // PB4 held this long = full robot reset
#define REMOTE_RESTART_ON_RESET 1  // the Remote restarts itself too after a full reset
#define LINK_TIMEOUT_MS     1000   // LED2 "connected" = Body telemetry within this time
#define CAM_LINK_TIMEOUT_MS 3000
#define CAPTURE_TIMEOUT_MS  3000   // no camera answer -> "CAPTURE FAILED"
#define POPUP_MS            1200   // OLED event message time
#define WIFI_RETRY_MS       5000
