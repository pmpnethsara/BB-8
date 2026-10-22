// =====================================================================
//  BB8_HEAD - telemetry.h     (Guide: Head telemetry)
//
//  ONE JSON line describing the Head (eye mode, blink, displays, and
//  which other nodes the Head can hear). The same text is:
//    - sent by UDP to the Remote (port 4211) 4x per second
//    - returned by http://192.168.8.202/telemetry for the Web Dashboard
//    - printed by the "tel" console command
//  "links" gives the Dashboard the Body/Camera/Remote online state.
// =====================================================================
#pragma once

size_t buildTelemetry(char* out, size_t len) {
  bool camUp = linkAlive(H.camRxMs, CAM_LINK_TIMEOUT_MS);
  int n = snprintf(out, len,
    "{\"node\":\"head\",\"fw\":\"%s\",\"seq\":%lu,\"up\":%lu,"
    "\"eye\":%u,\"eyeName\":\"%s\",\"eyes\":%u,\"blink\":%d,\"blinks\":%lu,"
    "\"oled\":[%d,%d],\"look\":[%d,%d],"
    "\"links\":{\"body\":%d,\"cam\":%d,\"remote\":%d},"
    "\"bodyState\":\"%s\",\"cap\":%lu,\"capOk\":%d,\"rssi\":%d,\"reset\":%d}",
    FW_VERSION, (unsigned long)H.telSeq, (unsigned long)(millis() / 1000),
    (unsigned)H.eyeMode, EYE_NAMES[H.eyeMode], (unsigned)EYE_COUNT,
    (int)(H.blink != BL_NONE), (unsigned long)H.blinkCount,
    (int)H.oledOk[0], (int)H.oledOk[1], (int)lroundf(H.lookX), (int)lroundf(H.lookY),
    (int)linkAlive(H.bodyRxMs, LINK_TIMEOUT_MS), (int)camUp,
    (int)linkAlive(H.remoteRxMs, LINK_TIMEOUT_MS),
    H.bodyState, (unsigned long)H.camCaptures, (int)(camUp && H.camCaptureOk),
    (int)H.rssi, (int)(H.resetAt != 0));
  if (n < 0) { out[0] = 0; return 0; }
  return ((size_t)n < len) ? (size_t)n : len - 1;
}
