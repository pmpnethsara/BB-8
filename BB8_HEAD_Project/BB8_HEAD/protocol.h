// =====================================================================
//  BB8_HEAD - protocol.h      (Guide: PB1/PB2 commands + Web eye commands)
//
//  Same packet format as the Body (see BB8_PROTOCOL.md):
//      BB8,<src>,<seq>,<CMD>[,arg1,...]
//      src : R = physical Remote, W = Web Dashboard, T = test tool
//      seq : must INCREASE with every packet from that source
//
//  HEAD COMMANDS  (UDP port 4212, or HTTP /cmd?line=...)
//      EYE,<n>[,eid]      select eye design n (0..6)
//      EYE,NEXT[,eid]     next design      (Remote PB1)
//      EYE,PREV[,eid]     previous design
//      BLINK[,eid]        blink both eyes  (Remote PB2)
//      HB                 Remote heartbeat (Head reports "remote online")
//      PING               no effect, test tools
//      RESET[,eid]        restart the Head ESP32 (Remote PB4 / Dashboard)
//
//  eid = EVENT ID. Events are sent 3x over UDP so one lost packet does
//  not matter. All 3 copies carry the SAME eid, so the Head executes
//  the event only once (otherwise PB1 would jump 3 designs).
//
//  This file also reads the TELEMETRY the Head receives from the Body
//  (head servo angles, battery) and the Camera (capture counter).
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
  H.rxRejected++;
  if (gVerbose) Serial.printf("[CMD] rejected: %s\n", why);
  return false;
}

static bool sequenceOk(int idx, uint32_t seq) {
  uint32_t now = millis();
  bool newSession = !pSeen[idx] || (now - pLastRxMs[idx] > SESSION_TIMEOUT_MS);
  if (!newSession && (int32_t)(seq - pLastSeq[idx]) <= 0) return false;
  if (newSession) pLastEid[idx] = 0;                 // sender may have rebooted
  pSeen[idx] = true;
  pLastSeq[idx] = seq;
  pLastRxMs[idx] = now;
  return true;
}

// true = this event is new and must be executed. eid 0 / missing = always new.
static bool newEvent(int idx, const char* eidTok) {
  if (!eidTok) return true;
  uint32_t eid = strtoul(eidTok, nullptr, 10);
  if (eid == 0) return true;
  if (eid == pLastEid[idx]) return false;            // repeat copy of the same event
  pLastEid[idx] = eid;
  return true;
}

bool handleCommandLine(char* line, const IPAddress& from, bool viaSerial = false) {
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
  if (src == 'R' && !viaSerial && !(from == IP_REMOTE)) return rejectCmd("R from wrong IP");
#endif
#if !ALLOW_TEST_SOURCE
  if (src == 'T') return rejectCmd("test source disabled");
#endif
  uint32_t seq = strtoul(tok[2], nullptr, 10);
  if (!sequenceOk(idx, seq)) return rejectCmd("old/duplicate seq");
  if (src == 'R') H.remoteRxMs = millis();

  const char* cmd = tok[3];

  if (!strcmp(cmd, "PING") || !strcmp(cmd, "HB")) { H.rxOk++; return true; }

  if (!strcmp(cmd, "EYE")) {
    if (n < 5) return rejectCmd("EYE needs a design");
    if (newEvent(idx, n >= 6 ? tok[5] : nullptr)) {
      if (!strcasecmp(tok[4], "NEXT"))      eyesRequestMode(-1);
      else if (!strcasecmp(tok[4], "PREV")) eyesRequestMode(-2);
      else {
        int m = atoi(tok[4]);
        if (m < 0 || m >= EYE_COUNT) return rejectCmd("EYE design out of range");
        eyesRequestMode(m);
      }
    }
    H.rxOk++;
    return true;
  }

  if (!strcmp(cmd, "BLINK")) {
    if (newEvent(idx, n >= 5 ? tok[4] : nullptr)) eyesRequestBlink();
    H.rxOk++;
    return true;
  }

  if (!strcmp(cmd, "RESET")) {
    if (newEvent(idx, n >= 5 ? tok[4] : nullptr) && !H.resetAt) {
      H.resetAt = millis() + 400;                    // time to send "reset" telemetry
      Serial.printf("[SYS] RESET requested by %c\n", src);
    }
    H.rxOk++;
    return true;
  }

  return rejectCmd("unknown command");
}

// ------------------------------------------------------------------
// Minimal JSON readers for the known telemetry fields (no library).
// They find the first  "key":  in the text and read what follows.
// ------------------------------------------------------------------
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

// Body / Camera telemetry arriving on UDP 4211
void handleTelemetryPacket(const char* js, const IPAddress& from) {
  char node[8];
  if (!jsonStr(js, "node", node, sizeof(node))) return;
  if (!strcmp(node, "body")) {
    H.bodyRxMs = millis();
    jsonStr(js, "state", H.bodyState, sizeof(H.bodyState));
    jsonArray(js, "servo", H.bodyServo, 3);
    H.bodyBattLow = jsonNum(js, "low") > 0;
  } else if (!strcmp(node, "cam")) {
    bool seenBefore = (H.camRxMs != 0);
    H.camRxMs = millis();
    uint32_t caps = (uint32_t)jsonNum(js, "cap");
    H.camCaptureOk = jsonNum(js, "capOk") > 0;
#if CAPTURE_BLINK
    if (seenBefore && caps > H.camCaptures) eyesRequestBlink();   // shutter blink
#endif
    H.camCaptures = caps;
  }
}
