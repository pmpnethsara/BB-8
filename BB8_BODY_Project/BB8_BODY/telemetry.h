// =====================================================================
//  BB8_BODY - telemetry.h      (Guide step 4: combined telemetry)
//
//  Builds ONE JSON line describing the whole Body. The same text is:
//    - sent by UDP to the Remote and Head (port 4211) 5x per second
//    - returned by http://192.168.8.201/telemetry for the Web Dashboard
//    - printed by the "tel" console command
//  Field list is documented in BB8_PROTOCOL.md.
// =====================================================================
#pragma once

size_t buildTelemetry(char* out, size_t len) {
  int n = snprintf(out, len,
    "{\"node\":\"body\",\"fw\":\"%s\",\"seq\":%lu,\"up\":%lu,"
    "\"state\":\"%s\",\"faults\":%u,\"owner\":\"%c\",\"armed\":%d,\"rssi\":%d,"
    "\"cmd\":{\"y\":%d,\"x\":%d},"
    "\"motor\":{\"m1\":%d,\"m2\":%d,\"m3\":%d},"
    "\"imu\":{\"ok\":%d,\"roll\":%.1f,\"pitch\":%.1f,\"yaw\":%.1f},"
    "\"batt\":{\"present\":%d,\"v\":%.2f,\"pct\":%u,\"low\":%d,\"crit\":%d},"
    "\"mag\":%d,\"servo\":[%.0f,%.0f,%.0f],\"wave\":%d,\"reset\":%d}",
    FW_VERSION, (unsigned long)S.telSeq, (unsigned long)(millis() / 1000),
    STATE_NAMES[S.state], (unsigned)S.faults, S.owner, (int)S.armed, (int)S.rssi,
    (int)S.cmdY, (int)S.cmdX,
    (int)S.outDrive, (int)(DRIVE_SHARED_CHANNEL ? S.outDrive : S.outDrive2), (int)S.outSteer,
    (int)S.imuOk, S.roll, S.pitch, S.yaw,
    (int)S.battPresent, S.battV, (unsigned)S.battPct, (int)S.battLow, (int)S.battCrit,
    (int)S.magnet, S.servoPos[0], S.servoPos[1], S.servoPos[2],
    (int)S.waving, (int)(S.resetAt != 0));
  if (n < 0) { out[0] = 0; return 0; }
  return ((size_t)n < len) ? (size_t)n : len - 1;
}
