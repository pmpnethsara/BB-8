// =====================================================================
//  BB8_REMOTE - commands.h    (Guide: command transmission + heartbeat)
//
//  Turns the clean inputs from inputs.h into protocol commands.
//
//  CONTINUOUS (heartbeat) - the Body's fail-safe depends on these:
//    DRV,y,x is sent 20x per second ALL THE TIME, also 0,0 when the
//    stick is centred. If the Remote dies, loses Wi-Fi or its battery
//    goes flat, the DRV stream stops and the Body stops by itself
//    within 0.5 s. A centred stick (0,0) also ARMS the Body again
//    after a STOP / fail-safe, so the robot can never jump forward on
//    reconnect while the stick is pushed.
//
//  EVENTS (one-shot) - sent EVENT_REPEATS times, EVENT_GAP_MS apart,
//    each copy with a new seq. Head/Camera events carry one event id
//    (eid) so the 3 copies are executed only once.
// =====================================================================
#pragma once

struct PendingEvent {
  IPAddress ip;
  uint16_t  port;
  char      cmd[40];
  uint8_t   left = 0;
  uint32_t  nextMs = 0;
};
static PendingEvent evq[12];
static uint32_t gEid = 0;

static uint32_t newEid() { if (++gEid == 0) gEid = 1; return gEid; }

void queueEvent(const IPAddress& ip, uint16_t port, const char* cmd) {
  for (auto& e : evq) {
    if (e.left == 0) {
      e.ip = ip; e.port = port;
      strncpy(e.cmd, cmd, sizeof(e.cmd) - 1);
      e.cmd[sizeof(e.cmd) - 1] = 0;
      e.left = EVENT_REPEATS;
      e.nextMs = millis();
      return;
    }
  }
  sendPacket(ip, port, cmd);                     // queue full: send once now
}

static void eventsUpdate() {
  uint32_t now = millis();
  for (auto& e : evq) {
    if (e.left && (int32_t)(now - e.nextMs) >= 0) {
      sendPacket(e.ip, e.port, e.cmd);
      e.left--;
      e.nextMs = now + EVENT_GAP_MS;
    }
  }
}

static void sendStopNow() {                      // STOP: first copy immediately
  sendPacket(IP_BODY, PORT_BODY_CMD, "STOP");
  queueEvent(IP_BODY, PORT_BODY_CMD, "STOP");
}

void commandsBegin() {
  gEid = (esp_random() & 0xFFFF) + 1;            // different after every reboot
  for (int i = 0; i < 3; i++) R.head[i] = HEAD_HOME_DEG[i];
}

// ---------------- buttons -> events ----------------
static void handleButtons() {
  uint32_t now = millis();
  char c[40];

  // Joystick 1 push: short = ROT,1  |  held ROT_LONG_PRESS_MS = ROT,-1
  if (R.j1sw.down && !R.j1sw.longFired && now - R.j1sw.downSinceMs >= ROT_LONG_PRESS_MS) {
    R.j1sw.longFired = true;
    queueEvent(IP_BODY, PORT_BODY_CMD, "ROT,-1");
    showPopup("ROTATE LEFT");
  }
  if (R.j1sw.released) {
    if (!R.j1sw.longFired) { queueEvent(IP_BODY, PORT_BODY_CMD, "ROT,1"); showPopup("ROTATE RIGHT"); }
    R.j1sw.released = false;
  }
  R.j1sw.pressed = false;

  // Joystick 2 push: head wave gesture
  if (R.j2sw.pressed) {
    R.j2sw.pressed = false;
    queueEvent(IP_BODY, PORT_BODY_CMD, "WAVE");
    showPopup("HEAD WAVE");
  }
  R.j2sw.released = false;

  // PB1: next eye design (Head)
  if (R.pb1.pressed) {
    R.pb1.pressed = false;
    snprintf(c, sizeof(c), "EYE,NEXT,%lu", (unsigned long)newEid());
    queueEvent(IP_HEAD, PORT_HEAD_CMD, c);
    showPopup("EYE: NEXT DESIGN");
  }
  R.pb1.released = false;

  // PB2: blink (Head)
  if (R.pb2.pressed) {
    R.pb2.pressed = false;
    snprintf(c, sizeof(c), "BLINK,%lu", (unsigned long)newEid());
    queueEvent(IP_HEAD, PORT_HEAD_CMD, c);
    showPopup("BLINK");
  }
  R.pb2.released = false;

  // PB3: capture (Camera)
  if (R.pb3.pressed) {
    R.pb3.pressed = false;
    snprintf(c, sizeof(c), "CAP,%lu", (unsigned long)newEid());
    queueEvent(IP_CAMERA, PORT_CAM_CMD, c);
    R.capBaseline = R.cam.cap;
    R.capWaitMs = now;
    showPopup("CAPTURING...");
  }
  R.pb3.released = false;
  if (R.capWaitMs && now - R.capWaitMs > CAPTURE_TIMEOUT_MS) {
    R.capWaitMs = 0;
    showPopup(linkAlive(R.cam.rxMs, CAM_LINK_TIMEOUT_MS) ? "CAPTURE FAILED" : "CAMERA OFFLINE");
  }

  // PB4: press = STOP immediately; keep holding = controlled full reset
  if (R.pb4.pressed) {
    R.pb4.pressed = false;
    sendStopNow();
    showPopup("STOP");
  }
  if (R.pb4.down && !R.pb4.longFired) {
    R.resetHold = min(1.0f, (now - R.pb4.downSinceMs) / (float)RESET_HOLD_MS);
    if (R.resetHold >= 1.0f) {
      R.pb4.longFired = true;
      // the Body handles RESET as "STOP first, then restart"
      queueEvent(IP_BODY, PORT_BODY_CMD, "STOP");
      snprintf(c, sizeof(c), "RESET,%lu", (unsigned long)newEid());
      queueEvent(IP_BODY, PORT_BODY_CMD, c);
      queueEvent(IP_HEAD, PORT_HEAD_CMD, c);
      queueEvent(IP_CAMERA, PORT_CAM_CMD, c);
      showPopup("RESETTING ROBOT");
      R.popupUntil = millis() + 2500;
#if REMOTE_RESTART_ON_RESET
      R.restartAt = millis() + 2500;
#endif
    }
  }
  if (!R.pb4.down) R.resetHold = 0;
  R.pb4.released = false;
}

// ---------------- Joystick 2 -> three head angles ----------------
static bool computeHead(float dt) {
  float x = R.j2x, y = R.j2y;
  bool used = (x != 0 || y != 0);
#if HEAD_HOLD_MODE
  R.head[0] += x * (HEAD_X_TO_S1_DEG / 45.0f) * HEAD_HOLD_RATE_DPS * dt;
  R.head[1] += y * (HEAD_Y_TO_S2_DEG / 45.0f) * HEAD_HOLD_RATE_DPS * dt;
  R.head[2] += x * (HEAD_X_TO_S3_DEG / 45.0f) * HEAD_HOLD_RATE_DPS * dt;
#else
  R.head[0] = HEAD_HOME_DEG[0] + x * HEAD_X_TO_S1_DEG;
  R.head[1] = HEAD_HOME_DEG[1] + y * HEAD_Y_TO_S2_DEG;
  R.head[2] = HEAD_HOME_DEG[2] + x * HEAD_X_TO_S3_DEG;
#endif
  for (int i = 0; i < 3; i++) R.head[i] = constrain(R.head[i], HEAD_MIN_DEG[i], HEAD_MAX_DEG[i]);
  return used;
}

// call every loop
void commandsUpdate() {
  static uint32_t tDrv = 0, tHead = 0, tHb = 0;
  static uint8_t  headTail = 0;                  // extra HEAD packets after release
  uint32_t now = millis();

  handleButtons();
  eventsUpdate();

  if (!R.calibrated) return;

  // DRV heartbeat to the Body - never stops while the Remote is alive
  if (now - tDrv >= DRV_PERIOD_MS) {
    tDrv = now;
    char c[24];
    if (R.restartAt) snprintf(c, sizeof(c), "DRV,0,0");             // resetting: stay neutral
    else snprintf(c, sizeof(c), "DRV,%d,%d", R.j1y, R.j1x);          // joyFault -> 0,0 already
    sendPacket(IP_BODY, PORT_BODY_CMD, c);
  }

  // HEAD while Joystick 2 is used, then a few packets to settle at home
  if (now - tHead >= HEAD_PERIOD_MS) {
    float dt = (now - tHead) / 1000.0f;
    tHead = now;
    bool used = computeHead(min(dt, 0.2f));
    if (used) headTail = 3;
    if ((used || headTail) && !R.restartAt && !R.joyFault) {
      if (!used) headTail--;
      char c[40];
      snprintf(c, sizeof(c), "HEAD,%.0f,%.0f,%.0f", R.head[0], R.head[1], R.head[2]);
      sendPacket(IP_BODY, PORT_BODY_CMD, c);
    }
  }

  // heartbeat to the Head (it reports "remote online" to the Dashboard)
  if (now - tHb >= HEAD_HB_PERIOD_MS) {
    tHb = now;
    sendPacket(IP_HEAD, PORT_HEAD_CMD, "HB");
  }
}
