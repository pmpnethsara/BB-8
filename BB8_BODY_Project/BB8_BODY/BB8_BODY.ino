// =====================================================================
//  SMART BB-8 ROBOT  -  PACKAGE 1  -  BB8_BODY firmware
//  Board : ESP32 Dev Module (ESP32-WROOM-32), Arduino core 3.x
//  Libs  : none extra (WiFi, WebServer, ESPmDNS, Wire are built in)
//
//  DATA FLOW (how the modules are interconnected)
//
//   network.h ──UDP/HTTP text──► protocol.h ──► S.cmdY/cmdX, S.owner,
//   console.h ──serial text─────►  (rules:      S.armed, S.state, events
//                                   seq, owner,        │
//                                   arm, timeout)      ▼
//   imu.h ─────► S.roll/pitch/yaw ──────────────► motors.h  ──► L298N
//   battery.h ─► S.battV/low/crit ──► protocol.h  servos.h  ──► 3x MG995
//   magnet.h ──► S.magnet ──────────► (state)          │
//                                                      ▼
//   telemetry.h ◄──────────── everything in S ─── network.h ──► Remote/Head/Web
//
//  The loop() never blocks: each module runs on its own timer, so
//  Wi-Fi, sensors, motors and servos all stay responsive.
// =====================================================================

#include "config.h"
#include "state.h"
#include "motors.h"
#include "imu.h"
#include "battery.h"
#include "magnet.h"
#include "servos.h"
#include "protocol.h"
#include "telemetry.h"
#include "network.h"
#include "console.h"
#include "system.h"


void setup() {
  // Motors FIRST: force every H-bridge pin to a safe OFF state immediately
  motorsBegin();

  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== SMART BB-8  BODY  %s ===\n", FW_VERSION);
  Serial.printf("Drive mode: %s\n", DRIVE_SHARED_CHANNEL ?
                "M1+M2 shared on L298N channel A" : "M2 on second driver");
  pinMode(PIN_STATUS_LED, OUTPUT);

  servosBegin();
  imuBegin();          // keep the robot still: gyro calibration
  batteryBegin();
  magnetBegin();
  networkBegin();

  S.state = ST_IDLE;
  Serial.println("Ready. Type 'help' for test commands.");
}

void loop() {
  static uint32_t tImu = 0, tSen = 0, tMot = 0, tSrv = 0, tTel = 0;

  consoleUpdate();                 // serial test commands
  networkUpdate();                 // Wi-Fi + incoming UDP/HTTP commands
  controlUpdate();                 // fail-safe, faults, state machine

  if (every(tImu, IMU_PERIOD_MS))        imuUpdate();
  if (every(tSen, SENSOR_PERIOD_MS))     { batteryUpdate(); magnetUpdate(); }
  if (every(tMot, MOTOR_PERIOD_MS))      motorsUpdate();
  if (every(tSrv, SERVO_PERIOD_MS))      servosUpdate();
  if (every(tTel, TELEMETRY_PERIOD_MS))  telemetryUpdate();

  statusLedUpdate();
}
