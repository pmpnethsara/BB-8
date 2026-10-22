// =====================================================================
//  BB8_REMOTE - telemetry_rx.h   (Guide: telemetry reception)
//
//  Everything the Remote shows on its OLED comes from three JSON
//  messages that arrive on UDP port 4211:
//     {"node":"body", ...}  Body  5 Hz  state, owner, battery, IMU, servos ...
//     {"node":"head", ...}  Head  4 Hz  eye design, OLED status ...
//     {"node":"cam",  ...}  Camera 1 Hz + after every capture
//  A tiny reader for the known fields is used (no JSON library needed).
// =====================================================================
#pragma once

static const char* jsonFind(const char* js, const char* key) {
  char pat[24];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(js, pat);
  return p ? p + strlen(pat) : nullptr;
}
static float jsonNum(const char* js, const char* key, float def = 0) {
  const char* p = jsonFind(js, key);
  return p ? atof(p) : def;
}
static bool jsonStr(const char* js, const char* key, char* out, size_t len) {
  const char* p = jsonFind(js, key);
  if (!p || *p != '"') return false;
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i < len - 1) out[i++] = *p++;
  out[i] = 0;
  return true;
}
static int jsonArray(const char* js, const char* key, float* out, int maxN) {
  const char* p = jsonFind(js, key);
  if (!p || *p != '[') return 0;
  p++;
  int i = 0;
  while (i < maxN && *p && *p != ']') {
    out[i++] = atof(p);
    while (*p && *p != ',' && *p != ']') p++;
    if (*p == ',') p++;
  }
  return i;
}

static char lastBodyJson[640], lastHeadJson[512], lastCamJson[400];   // console "tel"

void handleTelemetryPacket(const char* js) {
  char node[8];
  if (!jsonStr(js, "node", node, sizeof(node))) return;
  R.rxCount++;
  uint32_t now = millis();

  if (!strcmp(node, "body")) {
    BodyInfo& b = R.body;
    b.rxMs = now;
    jsonStr(js, "state", b.state, sizeof(b.state));
    char own[4] = "-";
    jsonStr(js, "owner", own, sizeof(own));
    b.owner = own[0];
    b.armed  = jsonNum(js, "armed") > 0;
    b.faults = (uint16_t)jsonNum(js, "faults");
    b.cmdY = (int)jsonNum(js, "y");
    b.cmdX = (int)jsonNum(js, "x");
    b.m1 = (int)jsonNum(js, "m1");
    b.m3 = (int)jsonNum(js, "m3");
    b.imuOk = jsonNum(js, "ok") > 0;
    b.roll  = jsonNum(js, "roll");
    b.pitch = jsonNum(js, "pitch");
    b.battPresent = jsonNum(js, "present") > 0;
    b.battV   = jsonNum(js, "v");
    b.battPct = (int)jsonNum(js, "pct");
    b.battLow  = jsonNum(js, "low") > 0;
    b.battCrit = jsonNum(js, "crit") > 0;
    b.mag = jsonNum(js, "mag") > 0;
    jsonArray(js, "servo", b.servo, 3);
    b.waving = jsonNum(js, "wave") > 0;
    strncpy(lastBodyJson, js, sizeof(lastBodyJson) - 1);
  }
  else if (!strcmp(node, "head")) {
    HeadInfo& h = R.headNode;
    h.rxMs = now;
    h.eye = (int)jsonNum(js, "eye");
    jsonStr(js, "eyeName", h.eyeName, sizeof(h.eyeName));
    float o[2] = {0, 0};
    jsonArray(js, "oled", o, 2);
    h.oled[0] = o[0] > 0; h.oled[1] = o[1] > 0;
    strncpy(lastHeadJson, js, sizeof(lastHeadJson) - 1);
  }
  else if (!strcmp(node, "cam")) {
    CamInfo& c = R.cam;
    c.rxMs = now;
    c.camOk = jsonNum(js, "camOk") > 0;
    c.cap   = (uint32_t)jsonNum(js, "cap");
    c.capOk = jsonNum(js, "capOk") > 0;
    c.sd    = jsonNum(js, "sd") > 0;
    c.stream = (int)jsonNum(js, "stream");
    strncpy(lastCamJson, js, sizeof(lastCamJson) - 1);

    // answer to a PB3 press?
    if (R.capWaitMs) {
      char msg[22];
      if (c.cap > R.capBaseline) {
        snprintf(msg, sizeof(msg), "PHOTO #%lu OK", (unsigned long)c.cap);
        showPopup(msg);
        R.capWaitMs = 0;
      } else if (!c.capOk && (int)jsonNum(js, "capAge", -1) == 0) {   // fresh failed attempt
        showPopup("CAPTURE FAILED");
        R.capWaitMs = 0;
      }
    }
  }
}
