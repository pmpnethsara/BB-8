// =====================================================================
//  BB8_BODY - protocol.h       (Guide steps 4-6: commands + safety)
//
//  This is the "brain" that connects the network to the hardware.
//
//  COMMAND FORMAT (one UDP packet / one HTTP ?line= value):
//      BB8,<src>,<seq>,<CMD>[,arg1,arg2,...]
//      src : R = physical Remote, W = Web Dashboard, T = test tool
//      seq : number that must INCREASE with every packet from that source
//
//      DRV,y,x        continuous  y,x = -100..100 (fwd/rev, left/right)
//      HEAD,a1,a2,a3  continuous  servo target angles (deg)
//      HB             heartbeat (keeps ownership alive, optional)
//      ROT,dir        event       rotate (Joystick 1 push), dir 1 / -1
//      WAVE           event       head wave (Joystick 2 push)
//      STOP           event       immediate stop - ALWAYS accepted
//      RESET          event       stop, then restart the Body ESP32 (PB4)
//      PING           no effect, only asks for telemetry (test tools)
//
//  SAFETY RULES implemented here
//   1. Sequence check  : old/duplicated packets are dropped, so stale
//                        network data can never restart movement.
//   2. Ownership       : only ONE controller drives at a time
//                        (most-recent-active controller; an idle owner
//                        loses control after OWNER_TIMEOUT_MS).
//   3. Arming          : after STOP / fail-safe / owner change the robot
//                        needs a neutral DRV,0,0 before it moves again.
//   4. Fail-safe       : no valid command for CMD_TIMEOUT_MS -> STOP.
//   5. STOP priority   : STOP from any source is executed immediately.
// =====================================================================
#pragma once

static uint32_t pLastSeq[3]  = {0, 0, 0};
static uint32_t pLastRxMs[3] = {0, 0, 0};
static bool     pSeen[3]     = {false, false, false};
static bool     cmdLostLatched = false;

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
  S.rxRejected++;
  if (gVerbose) Serial.printf("[CMD] rejected: %s\n", why);
  return false;
}

// Rule 1: sequence numbers. A source silent for SESSION_TIMEOUT_MS starts a
// new session (e.g. the Remote rebooted and restarted counting from 1).
static bool sequenceOk(int idx, uint32_t seq) {
  uint32_t now = millis();
  bool newSession = !pSeen[idx] || (now - pLastRxMs[idx] > SESSION_TIMEOUT_MS);
  if (!newSession && (int32_t)(seq - pLastSeq[idx]) <= 0) return false;
  pSeen[idx] = true;
  pLastSeq[idx] = seq;
  pLastRxMs[idx] = now;
  return true;
}

// Rule 2: ownership. 'active' = the command contains real (non-neutral) input.
static bool claimControl(char src, bool active) {
  uint32_t now = millis();
  if (S.owner == src) {
    if (active) S.ownerActiveMs = now;
    S.lastValidMs = now;
    cmdLostLatched = false;
    return true;
  }
  bool ownerIdle = (S.owner == '-') || (now - S.ownerActiveMs > OWNER_TIMEOUT_MS);
  if (!ownerIdle) return false;                       // someone else is driving
  if (S.owner != '-' && !active) return false;        // idle input cannot steal control
  Serial.printf("[CTRL] control owner %c -> %c\n", S.owner, src);
  S.owner = src;
  S.ownerActiveMs = now;
  S.lastValidMs = now;
  S.armed = false;                                    // Rule 3: re-arm on owner change
  S.cmdY = S.cmdX = 0;
  cmdLostLatched = false;
  return true;
}

void doStop(char src) {
  S.cmdY = S.cmdX = 0;
  S.rotateUntil = 0;
  S.armed = false;
  S.testUntil = 0;
  S.brakeUntil = millis() + STOP_BRAKE_MS;
  Serial.printf("[CTRL] STOP from %c\n", src);
}

// Parses and executes one command line. Returns true if accepted.
// 'line' is modified (tokenised).  viaSerial = typed in the console.
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
  const char* cmd = tok[3];
  uint32_t now = millis();

  // Rule 5: STOP first - no ownership or sequence requirement
  if (strcmp(cmd, "STOP") == 0) {
    sequenceOk(idx, seq);
    doStop(src);
    S.rxOk++;
    return true;
  }

  if (!sequenceOk(idx, seq)) return rejectCmd("old/duplicate seq");

  if (strcmp(cmd, "PING") == 0) { S.rxOk++; return true; }

  if (strcmp(cmd, "HB") == 0) {
    if (S.owner == src) S.lastValidMs = now;      // HB never takes ownership
    S.rxOk++;
    return true;
  }

  if (strcmp(cmd, "DRV") == 0) {
    if (n < 6) return rejectCmd("DRV needs y,x");
    int y = constrain(atoi(tok[4]), -100, 100);
    int x = constrain(atoi(tok[5]), -100, 100);
    bool active = (y != 0 || x != 0);
    if (!claimControl(src, active)) return rejectCmd("not owner");
    if (!S.armed) {                                 // Rule 3
      if (!active) { S.armed = true; Serial.printf("[CTRL] armed by %c\n", src); }
      S.cmdY = S.cmdX = 0;
      S.rxOk++;
      return true;
    }
    S.cmdY = y;
    S.cmdX = x;
    S.rxOk++;
    return true;
  }

  if (strcmp(cmd, "ROT") == 0) {
    int dir = (n >= 5) ? atoi(tok[4]) : 1;
    if (!claimControl(src, true)) return rejectCmd("not owner");
    if (S.armed && S.state == ST_ACTIVE) {
      S.rotateDir = (dir >= 0) ? 1 : -1;
      S.rotateUntil = now + ROTATE_TIME_MS;
      Serial.printf("[CTRL] rotate %d\n", S.rotateDir);
    }
    S.rxOk++;
    return true;
  }

  if (strcmp(cmd, "HEAD") == 0) {
    if (n < 7) return rejectCmd("HEAD needs 3 angles");
    float a = atof(tok[4]), b = atof(tok[5]), c = atof(tok[6]);
    bool active = fabsf(a - SERVO_HOME_DEG[0]) > 3 || fabsf(b - SERVO_HOME_DEG[1]) > 3 ||
                  fabsf(c - SERVO_HOME_DEG[2]) > 3;
    if (!claimControl(src, active)) return rejectCmd("not owner");
    if (S.state != ST_FAILSAFE) servosSetHeadTargets(a, b, c);
    S.rxOk++;
    return true;
  }

  if (strcmp(cmd, "WAVE") == 0) {
    if (!claimControl(src, true)) return rejectCmd("not owner");
    if (S.state != ST_FAILSAFE) servoStartWave();
    S.rxOk++;
    return true;
  }

  if (strcmp(cmd, "RESET") == 0) {                 // PB4: controlled reset
    doStop(src);
    S.resetAt = now + 500;                         // time to send "reset" telemetry
    Serial.printf("[SYS] RESET requested by %c\n", src);
    S.rxOk++;
    return true;
  }

  return rejectCmd("unknown command");
}

// Called every loop: Rule 4 (fail-safe), fault bits and the state machine.
void controlUpdate() {
  uint32_t now = millis();

  // Rule 4: communication loss
  if (S.owner != '-' && now - S.lastValidMs > CMD_TIMEOUT_MS) {
    bool moving = S.cmdY || S.cmdX || timeNotReached(S.rotateUntil) ||
                  S.outDrive || S.outSteer;
    if (moving) {
      cmdLostLatched = true;
      Serial.printf("[SAFE] commands from %c LOST while moving -> STOP\n", S.owner);
    } else if (gVerbose) {
      Serial.printf("[CTRL] %c went quiet while idle -> released\n", S.owner);
    }
    S.owner = '-';
    S.cmdY = S.cmdX = 0;
    S.armed = false;
    S.rotateUntil = 0;
  }

  // fault bits
  uint16_t f = 0;
  if (ENABLE_IMU && !S.imuOk) f |= F_IMU;
  if (S.battLow) f |= F_BATT_LOW;
  if (S.battCrit) f |= F_BATT_CRIT;
  if (ENABLE_WIFI && !S.netUp) f |= F_WIFI;
  if (cmdLostLatched) f |= F_CMD_LOST;
  if (MAG_REQUIRED_TO_DRIVE && !S.magnet) f |= F_MAG_OFF;
  S.faults = f;

  // state machine (priority from top to bottom)
  RobotState ns;
  if (ENABLE_WIFI && !S.netUp)                  ns = ST_FAILSAFE;
  else if (MAG_REQUIRED_TO_DRIVE && !S.magnet)  ns = ST_DISABLED;
  else if (S.battCrit)                          ns = ST_DISABLED;
  else if (S.owner == '-')                      ns = cmdLostLatched ? ST_FAILSAFE : ST_IDLE;
  else                                          ns = ST_ACTIVE;

  if (ns != S.state) {
    Serial.printf("[STATE] %s -> %s\n", STATE_NAMES[S.state], STATE_NAMES[ns]);
    if (ns != ST_ACTIVE) {
      S.cmdY = S.cmdX = 0;
      S.armed = false;
      S.rotateUntil = 0;
    }
    S.state = ns;
  }

  // controlled reset (motors are already stopped by doStop)
  if (S.resetAt && (int32_t)(now - S.resetAt) >= 0) {
    motorsBrakeAll();
    Serial.println("[SYS] restarting Body ESP32...");
    Serial.flush();
    delay(100);
    ESP.restart();
  }
}
