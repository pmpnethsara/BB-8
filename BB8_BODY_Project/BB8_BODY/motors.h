// =====================================================================
//  BB8_BODY - motors.h        (Guide step 1: motor tests + movement)
//
//  L298N logic per channel:
//     IN_A  IN_B  EN(PWM)   result
//      1     0    duty      forward
//      0     1    duty      reverse
//      0     0    0         coast (free stop)
//      1     1    255       active brake
//
//  INPUT : S.state, S.cmdY/cmdX (from protocol), S.rotate*, S.brakeUntil,
//          S.test* (serial console), S.roll (tilt limit), S.battLow
//  OUTPUT: PWM/direction pins, S.outDrive/outDrive2/outSteer
// =====================================================================
#pragma once

struct HBridge { uint8_t en, inA, inB; bool invert; };

#if ENABLE_MOTORS

static const HBridge BR_DRIVE = {PIN_ENA, PIN_IN1, PIN_IN2, MOTOR1_INVERT};
static const HBridge BR_STEER = {PIN_ENB, PIN_IN3, PIN_IN4, MOTOR3_INVERT};
#if !DRIVE_SHARED_CHANNEL
static const HBridge BR_DRIVE2 = {PIN_ENC, PIN_IN5, PIN_IN6, MOTOR2_INVERT};
#endif

static void bridgeBegin(const HBridge& b) {
  pinMode(b.inA, OUTPUT);
  pinMode(b.inB, OUTPUT);
  digitalWrite(b.inA, LOW);
  digitalWrite(b.inB, LOW);
  ledcAttach(b.en, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
  ledcWrite(b.en, 0);
}

// duty: -255..255  (0 = coast)
static void bridgeWrite(const HBridge& b, int16_t duty) {
  if (b.invert) duty = -duty;
  if (duty > 255) duty = 255;
  if (duty < -255) duty = -255;
  if (duty == 0) {
    ledcWrite(b.en, 0);
    digitalWrite(b.inA, LOW);
    digitalWrite(b.inB, LOW);
    return;
  }
  digitalWrite(b.inA, duty > 0 ? HIGH : LOW);
  digitalWrite(b.inB, duty < 0 ? HIGH : LOW);
  ledcWrite(b.en, (uint32_t)abs(duty));
}

static void bridgeBrake(const HBridge& b) {
  digitalWrite(b.inA, HIGH);
  digitalWrite(b.inB, HIGH);
  ledcWrite(b.en, 255);
}

// -100..100 %  ->  signed duty, skipping the dead band where motors only hum
static int16_t pctToDuty(int pct, int maxDuty) {
  if (pct == 0) return 0;
  int d = map(abs(pct), 1, 100, MOTOR_MIN_DUTY, maxDuty);
  return pct > 0 ? d : -d;
}

static int16_t rampToward(int16_t cur, int16_t target, float perSec, float dt) {
  int step = (int)(perSec * dt);
  if (step < 1) step = 1;
  if (target > cur) return (cur + step > target) ? target : cur + step;
  if (target < cur) return (cur - step < target) ? target : cur - step;
  return cur;
}

static void writeAll() {
  bridgeWrite(BR_DRIVE, S.outDrive);
  bridgeWrite(BR_STEER, S.outSteer);
#if !DRIVE_SHARED_CHANNEL
  bridgeWrite(BR_DRIVE2, S.outDrive2);
#endif
}

void motorsBrakeAll() {
  S.outDrive = S.outDrive2 = S.outSteer = 0;
  bridgeBrake(BR_DRIVE);
  bridgeBrake(BR_STEER);
#if !DRIVE_SHARED_CHANNEL
  bridgeBrake(BR_DRIVE2);
#endif
}

void motorsBegin() {
  bridgeBegin(BR_DRIVE);
  bridgeBegin(BR_STEER);
#if !DRIVE_SHARED_CHANNEL
  bridgeBegin(BR_DRIVE2);
#endif
}

void motorsUpdate() {
  static uint32_t lastMs = millis();
  uint32_t now = millis();
  float dt = (now - lastMs) / 1000.0f;
  lastMs = now;

  // (a) Bench test from the serial console - direct, no ramp, auto-timeout
  if (timeNotReached(S.testUntil)) {
    S.outDrive = S.testDrive;
    S.outDrive2 = S.testDrive2;
    S.outSteer = S.testSteer;
    writeAll();
    return;
  }

  // (b) STOP command -> active brake for a short time
  if (timeNotReached(S.brakeUntil)) {
    motorsBrakeAll();
    return;
  }

  // (c) Normal control - targets are only non-zero in ACTIVE state
  int16_t tD = 0, tD2 = 0, tS = 0;
  if (S.state == ST_ACTIVE) {
    if (timeNotReached(S.rotateUntil)) {
#if DRIVE_SHARED_CHANNEL
      // one drive channel: rotate = full steering lock + slow drive (tight turn)
      tS = S.rotateDir * STEER_MAX_DUTY;
      tD = ROTATE_DUTY;
      tD2 = tD;
#else
      // independent drive motors: spin in place
      tD = S.rotateDir * ROTATE_DUTY;
      tD2 = -tD;
#endif
    } else {
      int scale = S.battLow ? LOW_BATT_SPEED_PCT : 100;
      tD = pctToDuty(S.cmdY * scale / 100, DRIVE_MAX_DUTY);
      tD2 = tD;
      tS = pctToDuty(S.cmdX, STEER_MAX_DUTY);
    }

#if ENABLE_IMU && TILT_LIMIT_ENABLE
    // steering may not push the body past the tilt limit
    if (S.imuOk) {
      float tilt = S.roll * STEER_TILT_SIGN;
      if (tilt >  TILT_LIMIT_DEG && tS > 0) tS = 0;
      if (tilt < -TILT_LIMIT_DEG && tS < 0) tS = 0;
    }
#endif
  }

  // Ramp (also gives a soft stop when state leaves ACTIVE)
  S.outDrive  = rampToward(S.outDrive,  tD,  DRIVE_RAMP_PER_S, dt);
  S.outDrive2 = rampToward(S.outDrive2, tD2, DRIVE_RAMP_PER_S, dt);
  S.outSteer  = rampToward(S.outSteer,  tS,  STEER_RAMP_PER_S, dt);
  writeAll();
}

#else   // ---------------- motors disabled: stubs ----------------
void motorsBegin() {}
void motorsUpdate() {}
void motorsBrakeAll() {}
#endif
