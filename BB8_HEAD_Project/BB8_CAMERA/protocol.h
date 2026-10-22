// =====================================================================
//  BB8_CAMERA - protocol.h    (Guide: capture from PB3 or Dashboard)
//
//  Same packet format as the Body and Head (BB8_PROTOCOL.md):
//      BB8,<src>,<seq>,<CMD>[,arg1,...]
//
//  CAMERA COMMANDS  (UDP port 4213, or HTTP /cmd?line=...)
//      CAP[,eid]       take a still image        (Remote PB3 / Dashboard)
//      FLASH,<0|1>     white LED torch off / on
//      PING            no effect, test tools
//      RESET[,eid]     restart the ESP32-CAM     (Remote PB4 / Dashboard)
//
//  eid = event id: the 3 UDP copies of one PB3 press carry the same eid,
//  so only ONE picture is taken.
// =====================================================================
#pragma once

static uint32_t pLastSeq[3]  = {0, 0, 0};
static uint32_t pLastRxMs[3] = {0, 0, 0};
static bool     pSeen[3]     = {false, false, false};
static uint32_t pLastEid[3]  = {0, 0, 0};

static int srcIndex(char s) {
  if (s == 'R') return 0;
  if (s == 'W') return 1;
  if (s == 'T') return 2;
  return -1;
}

static char* trimToken(char* s) {
  while (*s == ' ' || *s == '\t') s++;
  char* e = s + strlen(s);
  while (e > s && (e[-1] == ' ' || e[-1] == '\r' || e[-1] == '\n' || e[-1] == '\t')) *--e = 0;
  return s;
}

static bool rejectCmd(const char* why) {
  C.rxRejected++;
  if (gVerbose) Serial.printf("[CMD] rejected: %s\n", why);
  return false;
}

static bool sequenceOk(int idx, uint32_t seq) {
  uint32_t now = millis();
  bool newSession = !pSeen[idx] || (now - pLastRxMs[idx] > SESSION_TIMEOUT_MS);
  if (!newSession && (int32_t)(seq - pLastSeq[idx]) <= 0) return false;
  if (newSession) pLastEid[idx] = 0;
  pSeen[idx] = true;
  pLastSeq[idx] = seq;
  pLastRxMs[idx] = now;
  return true;
}

static bool newEvent(int idx, const char* eidTok) {
  if (!eidTok) return true;
  uint32_t eid = strtoul(eidTok, nullptr, 10);
  if (eid == 0) return true;
  if (eid == pLastEid[idx]) return false;
  pLastEid[idx] = eid;
  return true;
}

// 'remoteIpOk' = packet really came from the Remote IP (UDP only)
static bool handleLocked(char* line, bool remoteIpOk, bool viaSerial) {
  char* tok[8];
  int n = 0;
  char* save = nullptr;
  for (char* t = strtok_r(line, ",", &save); t && n < 8; t = strtok_r(nullptr, ",", &save))
    tok[n++] = trimToken(t);

  if (n < 4 || strcmp(tok[0], "BB8") != 0) return rejectCmd("bad format");
  char src = toupper(tok[1][0]);
  int idx = srcIndex(src);
  if (idx < 0) return rejectCmd("unknown source");
#if STRICT_REMOTE_IP
  if (src == 'R' && !viaSerial && !remoteIpOk) return rejectCmd("R from wrong IP");
#endif
#if !ALLOW_TEST_SOURCE
  if (src == 'T') return rejectCmd("test source disabled");
#endif
  if (!sequenceOk(idx, strtoul(tok[2], nullptr, 10))) return rejectCmd("old/duplicate seq");
  const char* cmd = tok[3];

  if (!strcmp(cmd, "PING") || !strcmp(cmd, "HB")) { C.rxOk++; return true; }

  if (!strcmp(cmd, "CAP")) {
    C.rxOk++;
    if (newEvent(idx, n >= 5 ? tok[4] : nullptr)) {
      xSemaphoreGive(gCmdMutex);                 // capture can take ~200 ms: release first
      cameraCapture();
      xSemaphoreTake(gCmdMutex, portMAX_DELAY);
    }
    return true;
  }

  if (!strcmp(cmd, "FLASH")) {
    setFlash(n >= 5 && atoi(tok[4]) != 0);
    C.rxOk++;
    return true;
  }

  if (!strcmp(cmd, "RESET")) {
    if (newEvent(idx, n >= 5 ? tok[4] : nullptr) && !C.resetAt) {
      C.resetAt = millis() + 400;
      C.statusNow = true;
      Serial.printf("[SYS] RESET requested by %c\n", src);
    }
    C.rxOk++;
    return true;
  }
  return rejectCmd("unknown command");
}

bool handleCommandLine(char* line, bool remoteIpOk, bool viaSerial = false) {
  xSemaphoreTake(gCmdMutex, portMAX_DELAY);
  bool ok = handleLocked(line, remoteIpOk, viaSerial);
  xSemaphoreGive(gCmdMutex);
  return ok;
}
