// =====================================================================
//  BB8_BODY - imu.h            (Guide step 2: MPU6050)
//
//  Reads the MPU6050 directly over I2C (no library needed) and fuses
//  accelerometer + gyro with a complementary filter:
//     angle = 0.98 * (angle + gyroRate*dt) + 0.02 * accelAngle
//  Gyro = fast & smooth but drifts, accel = absolute but noisy.
//  Yaw comes from the gyro only, so it slowly drifts (no magnetometer).
//
//  OUTPUT: S.roll, S.pitch, S.yaw, S.imuOk
//  USED BY: motors.h (tilt limit), telemetry, Remote/Dashboard display
// =====================================================================
#pragma once

#if ENABLE_IMU
#include <Wire.h>

#define GYRO_LSB_PER_DPS 65.5f    // +-500 deg/s range
// (accel +-4 g range; only the ratio between axes is used, so no scale needed)

static float    gBias[3] = {0, 0, 0};
static uint8_t  imuErrCount = 0;
static uint32_t imuLastMs = 0, imuRetryMs = 0;

static bool mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool mpuRead(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)MPU_ADDR, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static bool mpuReadRaw(int16_t* a, int16_t* g) {
  uint8_t b[14];
  if (!mpuRead(0x3B, b, 14)) return false;
  a[0] = (int16_t)(b[0] << 8 | b[1]);
  a[1] = (int16_t)(b[2] << 8 | b[3]);
  a[2] = (int16_t)(b[4] << 8 | b[5]);
  // b[6..7] = temperature (unused)
  g[0] = (int16_t)(b[8] << 8 | b[9]);
  g[1] = (int16_t)(b[10] << 8 | b[11]);
  g[2] = (int16_t)(b[12] << 8 | b[13]);
  return true;
}

static bool imuInit() {
  uint8_t who = 0;
  if (!mpuRead(0x75, &who, 1)) return false;   // WHO_AM_I
  if (!mpuWrite(0x6B, 0x01)) return false;       // wake up, clock = gyro X PLL
  delay(50);
  mpuWrite(0x1A, 0x03);   // digital low-pass filter ~44 Hz (reduces motor vibration)
  mpuWrite(0x1B, 0x08);   // gyro  +-500 deg/s
  mpuWrite(0x1C, 0x08);   // accel +-4 g
  Serial.printf("[IMU] MPU6050 found (WHO_AM_I=0x%02X)\n", who);
  return true;
}

static void accelAngles(const int16_t* a, float& r, float& p) {
  r = atan2f((float)a[1], (float)a[2]) * RAD_TO_DEG;
  p = atan2f(-(float)a[0], sqrtf((float)a[1] * a[1] + (float)a[2] * a[2])) * RAD_TO_DEG;
}

// Robot must be STILL while this runs (~1.2 s)
void imuCalibrate() {
  if (!S.imuOk) { Serial.println("[IMU] not available"); return; }
  Serial.println("[IMU] Calibrating gyro - keep the robot still...");
  long sum[3] = {0, 0, 0};
  int16_t a[3], g[3];
  int ok = 0;
  for (int i = 0; i < 400; i++) {
    if (mpuReadRaw(a, g)) { sum[0] += g[0]; sum[1] += g[1]; sum[2] += g[2]; ok++; }
    delay(3);
  }
  if (ok > 0)
    for (int k = 0; k < 3; k++) gBias[k] = (sum[k] / (float)ok) / GYRO_LSB_PER_DPS;
  if (mpuReadRaw(a, g)) accelAngles(a, S.roll, S.pitch);
  S.yaw = 0;
  imuLastMs = millis();
  Serial.printf("[IMU] gyro bias = %.2f %.2f %.2f deg/s\n", gBias[0], gBias[1], gBias[2]);
}

void imuBegin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  S.imuOk = imuInit();
  if (S.imuOk) imuCalibrate();
  else Serial.println("[IMU] MPU6050 NOT found - check SDA/SCL/3.3V/GND and address");
}

void imuUpdate() {
  uint32_t now = millis();

  if (!S.imuOk) {                       // try to recover every 2 s
    if (now - imuRetryMs > 2000) {
      imuRetryMs = now;
      if (imuInit()) { S.imuOk = true; imuErrCount = 0; imuLastMs = now; }
    }
    return;
  }

  int16_t a[3], g[3];
  if (!mpuReadRaw(a, g)) {
    if (++imuErrCount > 5) { S.imuOk = false; Serial.println("[IMU] lost communication"); }
    return;
  }
  imuErrCount = 0;

  float dt = (now - imuLastMs) / 1000.0f;
  imuLastMs = now;
  if (dt <= 0 || dt > 0.1f) dt = IMU_PERIOD_MS / 1000.0f;

  float gx = g[0] / GYRO_LSB_PER_DPS - gBias[0];
  float gy = g[1] / GYRO_LSB_PER_DPS - gBias[1];
  float gz = g[2] / GYRO_LSB_PER_DPS - gBias[2];

  float accRoll, accPitch;
  accelAngles(a, accRoll, accPitch);

  S.roll  = IMU_COMP_ALPHA * (S.roll  + gx * dt) + (1 - IMU_COMP_ALPHA) * accRoll;
  S.pitch = IMU_COMP_ALPHA * (S.pitch + gy * dt) + (1 - IMU_COMP_ALPHA) * accPitch;
  S.yaw  += gz * dt;
  if (S.yaw > 180) S.yaw -= 360;
  if (S.yaw < -180) S.yaw += 360;
}

#else   // ---------------- IMU disabled: stubs ----------------
void imuBegin() {}
void imuUpdate() {}
void imuCalibrate() { Serial.println("[IMU] disabled in config.h"); }
#endif
