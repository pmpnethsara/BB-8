// =====================================================================
//  BB8_CAMERA - telemetry.h   (Guide: camera status)
//  ONE JSON line describing the camera. Sent by UDP to the Remote and
//  the Head (port 4211) once per second AND right after every capture,
//  returned by http://192.168.8.203/status and by /capture.
// =====================================================================
#pragma once

static const char* frameSizeName() {
  switch (CAM_FRAME_SIZE) {
    case FRAMESIZE_QVGA: return "QVGA";
    case FRAMESIZE_VGA:  return "VGA";
    case FRAMESIZE_SVGA: return "SVGA";
    case FRAMESIZE_XGA:  return "XGA";
    default:             return "custom";
  }
}

size_t buildTelemetry(char* out, size_t len) {
  int n = snprintf(out, len,
    "{\"node\":\"cam\",\"fw\":\"%s\",\"seq\":%lu,\"up\":%lu,\"camOk\":%d,"
    "\"cap\":%lu,\"capOk\":%d,\"capAge\":%ld,\"size\":%u,\"saved\":\"%s\",\"sd\":%d,"
    "\"stream\":%d,\"flash\":%d,\"res\":\"%s\",\"psram\":%d,\"rssi\":%d,\"reset\":%d}",
    FW_VERSION, (unsigned long)C.telSeq, (unsigned long)(millis() / 1000), (int)C.camOk,
    (unsigned long)C.captures, (int)C.lastCapOk,
    C.lastCapMs ? (long)((millis() - C.lastCapMs) / 1000) : -1L,
    (unsigned)C.lastSize, C.lastSaved, (int)C.sdOk,
    (int)C.streamClients, (int)C.flashOn, C.psram ? frameSizeName() : "QVGA",
    (int)C.psram, (int)C.rssi, (int)(C.resetAt != 0));
  if (n < 0) { out[0] = 0; return 0; }
  return ((size_t)n < len) ? (size_t)n : len - 1;
}
