// =====================================================================
//  BB8_BODY - state.h
//  ONE shared structure "S" holds the live state of the whole Body.
//  Sensor modules WRITE into it, the control logic DECIDES from it,
//  the motor/servo modules ACT on it and telemetry REPORTS it.
//  This is how all modules are interconnected.
// =====================================================================
#pragma once

enum RobotState : uint8_t { ST_BOOT = 0, ST_IDLE, ST_ACTIVE, ST_FAILSAFE, ST_DISABLED };
static const char* STATE_NAMES[] = {"BOOT", "IDLE", "ACTIVE", "FAILSAFE", "DISABLED"};

// Fault bits (sent in telemetry as one number)
#define F_IMU       (1 << 0)
#define F_BATT_LOW  (1 << 1)
#define F_BATT_CRIT (1 << 2)
#define F_WIFI      (1 << 3)
#define F_CMD_LOST  (1 << 4)
#define F_MAG_OFF   (1 << 5)

struct BodyState {
  RobotState state = ST_BOOT;
  uint16_t   faults = 0;

  // ---- control (written by protocol.h) ----
  char     owner = '-';          // 'R' remote, 'W' web, 'T' test, '-' nobody
  uint32_t lastValidMs = 0;      // last valid command from the owner
  uint32_t ownerActiveMs = 0;    // last time the owner gave non-neutral input
  bool     armed = false;        // needs a neutral (0,0) command before moving
  int8_t   cmdY = 0;             // -100..100 forward/reverse request
  int8_t   cmdX = 0;             // -100..100 left/right request
  uint32_t rotateUntil = 0;
  int8_t   rotateDir = 0;
  uint32_t brakeUntil = 0;
  uint32_t resetAt = 0;

  // ---- bench test (serial console) ----
  uint32_t testUntil = 0;
  int16_t  testDrive = 0, testDrive2 = 0, testSteer = 0;

  // ---- motor outputs (signed duty, written by motors.h) ----
  int16_t outDrive = 0, outDrive2 = 0, outSteer = 0;

  // ---- MPU6050 ----
  bool  imuOk = false;
  float roll = 0, pitch = 0, yaw = 0;

  // ---- battery ----
  bool    battPresent = false;
  float   battV = 0;
  uint8_t battPct = 0;
  bool    battLow = false, battCrit = false;

  // ---- magnet ----
  bool magnet = false;

  // ---- servos ----
  float servoPos[3]    = {90, 90, 90};
  float servoTarget[3] = {90, 90, 90};
  bool  waving = false;

  // ---- network ----
  bool     netUp = false;
  int8_t   rssi = 0;
  uint32_t rxOk = 0, rxRejected = 0, telSeq = 0;
};

BodyState S;
bool gVerbose = false;   // "verbose" console command prints rejected packets

// true while time t has not been reached yet (safe across millis() wrap)
static inline bool timeNotReached(uint32_t t) { return (int32_t)(t - millis()) > 0; }
