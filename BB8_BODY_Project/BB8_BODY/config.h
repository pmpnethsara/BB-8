// =====================================================================
//  BB8_BODY - config.h
//  EVERY setting you are expected to change lives in this file:
//  feature switches, Wi-Fi/IP plan, pin map, limits and timings.
//  Do not hard-code numbers inside the modules - change them here.
// =====================================================================
#pragma once
#include <Arduino.h>
#include <IPAddress.h>

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "BB8_BODY needs the ESP32 Arduino core 3.x (Boards Manager: 'esp32 by Espressif Systems' >= 3.0.0)"
#endif

#define FW_VERSION "BODY-1.0.0"

// ---------------------------------------------------------------------
// 1. FEATURE SWITCHES  (1 = on, 0 = off)
//    Build the robot up step by step: start with everything 0 except the
//    module you are testing, then switch modules on one at a time.
//    A disabled module is replaced by a harmless stub.
// ---------------------------------------------------------------------
#define ENABLE_WIFI     1
#define ENABLE_MOTORS   1
#define ENABLE_IMU      1
#define ENABLE_BATTERY  1
#define ENABLE_MAGNET   1
#define ENABLE_SERVOS   1

// ---------------------------------------------------------------------
// 2. WI-FI / IP PLAN  (must match BB8_PROTOCOL.md and the other nodes)
// ---------------------------------------------------------------------
#define WIFI_SSID      "BB8_NET"
#define WIFI_PASSWORD  "bb8password"
#define BODY_HOSTNAME  "bb8-body"          // also http://bb8-body.local

#define USE_STATIC_IP  1                   // 0 = use router DHCP reservation instead
static const IPAddress IP_BODY    (192, 168, 8, 201);
static const IPAddress IP_HEAD    (192, 168, 8, 202);
static const IPAddress IP_CAMERA  (192, 168, 8, 203);
static const IPAddress IP_REMOTE  (192, 168, 8, 204);
static const IPAddress IP_GATEWAY (192, 168, 8, 1);
static const IPAddress IP_SUBNET  (255, 255, 255, 0);
static const IPAddress IP_DNS     (192, 168, 8, 1);

#define PORT_BODY_CMD    4210   // Body LISTENS here for UDP commands
#define PORT_TELEMETRY   4211   // Body SENDS telemetry to this port on Remote/Head
#define PORT_HTTP        80     // Web test page + /telemetry + /cmd for the Dashboard

#define STRICT_REMOTE_IP   1    // source 'R' is only accepted from IP_REMOTE
#define ALLOW_TEST_SOURCE  1    // accept source 'T' (laptop test script). Set 0 for final build.

// ---------------------------------------------------------------------
// 3. MOTORS - L298N
//    Channel A (OUT1/OUT2) = main drive (Motor 1, and Motor 2 if shared)
//    Channel B (OUT3/OUT4) = Motor 3 steering / angle mechanism
//
//    DRIVE_SHARED_CHANNEL 1: Motor 1 and Motor 2 are wired in PARALLEL on
//                            channel A (they always turn together).
//                            Check: combined current must stay < 2 A.
//    DRIVE_SHARED_CHANNEL 0: Motor 2 has its own channel on a SECOND
//                            driver (pins ENC/IN5/IN6 below).
//    IMPORTANT: remove the ENA/ENB jumpers on the L298N board.
// ---------------------------------------------------------------------
#define DRIVE_SHARED_CHANNEL 1

#define PIN_ENA   25   // PWM  - drive speed
#define PIN_IN1   26   // dir
#define PIN_IN2   27   // dir
#define PIN_ENB   13   // PWM  - steering speed
#define PIN_IN3   33   // dir
#define PIN_IN4   32   // dir
#define PIN_ENC   16   // PWM  - Motor 2 (only if DRIVE_SHARED_CHANNEL 0)
#define PIN_IN5   17
#define PIN_IN6   4

#define MOTOR1_INVERT  false   // flip if the motor turns the wrong way
#define MOTOR2_INVERT  false
#define MOTOR3_INVERT  false

#define MOTOR_PWM_FREQ    1000  // Hz - L298N works best at low frequency
#define MOTOR_PWM_BITS    8     // duty range 0..255
#define MOTOR_MIN_DUTY    70    // below this a geared motor only hums - measure yours
#define DRIVE_MAX_DUTY    230
#define STEER_MAX_DUTY    200
#define DRIVE_RAMP_PER_S  600   // duty change per second (soft start/stop for the gearbox)
#define STEER_RAMP_PER_S  1500
#define LOW_BATT_SPEED_PCT 60   // speed limit while battery is LOW
#define ROTATE_TIME_MS    1500  // Joystick-1 push = rotate for this long
#define ROTATE_DUTY       170
#define STOP_BRAKE_MS     300   // STOP command: active brake time

// ---------------------------------------------------------------------
// 4. MPU6050 (I2C)
// ---------------------------------------------------------------------
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22
#define MPU_ADDR        0x68    // 0x69 if AD0 is tied to 3.3 V
#define IMU_COMP_ALPHA  0.98f   // complementary filter: trust in gyro (0..1)

#define TILT_LIMIT_ENABLE 1     // block steering that would tilt the body further than:
#define TILT_LIMIT_DEG    25.0f
#define STEER_TILT_SIGN   1     // set -1 if "steer right" makes roll go negative

// ---------------------------------------------------------------------
// 5. BATTERY VOLTAGE SENSOR  (must use an ADC1 pin - ADC2 is blocked by Wi-Fi)
// ---------------------------------------------------------------------
#define PIN_BATT          34
#define BATT_DIVIDER      5.0f   // common "0-25 V" module = 30k/7.5k = 5:1
#define BATT_CAL          1.00f  // = multimeter reading / reported reading
#define BATT_FULL_V       12.6f  // 3S Li-ion/LiPo full. Change for your battery.
#define BATT_EMPTY_V      10.5f
#define BATT_LOW_V        11.1f
#define BATT_CRIT_V       10.6f
#define BATT_HYST_V       0.2f
#define BATT_CRIT_DELAY_MS 3000  // must stay below CRIT this long (ignores motor-start sag)
#define BATT_PRESENT_MIN_V 3.0f  // below this = no battery (USB bench power)

// ---------------------------------------------------------------------
// 6. MAGNETIC REED SWITCH  (signal only - NEVER motor current)
//    GPIO35 has no internal pull-up: fit 10k from GPIO35 to 3.3 V,
//    reed switch from GPIO35 to GND.
// ---------------------------------------------------------------------
#define PIN_REED               35
#define REED_INTERNAL_PULLUP   0     // 1 only if you move it to a pin that has one
#define REED_ACTIVE_LOW        1     // magnet present = switch closed = LOW
#define MAG_REQUIRED_TO_DRIVE  1     // 1 = motors stay disabled without the magnet
#define MAG_DEBOUNCE_MS        50

// ---------------------------------------------------------------------
// 7. MG995 HEAD SERVOS  (assumed: S1 tilt L/R, S2 tilt F/B, S3 pan)
// ---------------------------------------------------------------------
#define PIN_SERVO1  18
#define PIN_SERVO2  19
#define PIN_SERVO3  23
#define SERVO_PWM_FREQ   50
#define SERVO_PWM_BITS   14
#define SERVO_US_MIN     500    // pulse at 0 deg
#define SERVO_US_MAX     2500   // pulse at 180 deg
static const float SERVO_MIN_DEG[3]  = {45, 45, 45};    // mechanical safe limits -
static const float SERVO_MAX_DEG[3]  = {135, 135, 135}; // set these on the real head!
static const float SERVO_HOME_DEG[3] = {90, 90, 90};
#define SERVO_SPEED_DPS       120.0f  // smooth motion speed (deg/s)
#define SERVO_WAVE_SPEED_DPS  240.0f

// ---------------------------------------------------------------------
// 8. TIMING / SAFETY
// ---------------------------------------------------------------------
#define CMD_TIMEOUT_MS       500   // no valid command for this long -> STOP
#define OWNER_TIMEOUT_MS     1500  // idle controller loses ownership after this
#define SESSION_TIMEOUT_MS   2000  // silent source for this long -> new sequence session
#define TELEMETRY_PERIOD_MS  200   // 5 Hz telemetry
#define IMU_PERIOD_MS        10    // 100 Hz
#define MOTOR_PERIOD_MS      10
#define SERVO_PERIOD_MS      20
#define SENSOR_PERIOD_MS     50
#define WIFI_RETRY_MS        5000

#define PIN_STATUS_LED       2     // on-board LED
