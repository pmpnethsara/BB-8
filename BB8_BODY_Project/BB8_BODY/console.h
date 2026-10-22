// =====================================================================
//  BB8_BODY - console.h        (bench-test tool for every guide step)
//  Open Serial Monitor at 115200 baud, line ending "Newline", type help.
// =====================================================================
#pragma once

static char     conBuf[96];
static uint8_t  conLen = 0;
static bool     conWatch = false;
static uint32_t conWatchMs = 0;

void printHelp() {
  Serial.println(F(
    "\n--------------- BB8_BODY console ---------------\n"
    " help                 this list\n"
    " status               full status summary\n"
    " tel                  print one telemetry JSON line\n"
    " watch                toggle live 2 Hz status line\n"
    " verbose              toggle printing of rejected packets\n"
    " motor <1|2|3> <duty -255..255> [ms]   bench motor test (auto-stops)\n"
    "        1 = drive (M1, +M2 if shared)  2 = M2 (own driver)  3 = steering\n"
    " stop                 brake all motors now\n"
    " servo <1-3> <deg>    move one servo (clamped to safe limits)\n"
    " home                 all servos to HOME\n"
    " wave                 run head-wave gesture\n"
    " imu | calib          show angles | re-calibrate gyro (keep still)\n"
    " bat | mag | net      battery | magnet | network info\n"
    " cmd <line>           inject protocol line, e.g. cmd BB8,T,1,DRV,0,0\n"
    " reboot               restart ESP32\n"
    "------------------------------------------------"));
}

static void printStatus() {
  Serial.printf("State=%s  owner=%c  armed=%d  faults=0x%02X\n",
                STATE_NAMES[S.state], S.owner, S.armed, S.faults);
  Serial.printf("Cmd y=%d x=%d | motor M1=%d M2=%d M3=%d\n",
                S.cmdY, S.cmdX, S.outDrive, S.outDrive2, S.outSteer);
  Serial.printf("IMU ok=%d roll=%.1f pitch=%.1f yaw=%.1f\n", S.imuOk, S.roll, S.pitch, S.yaw);
  Serial.printf("Battery %.2f V %u%% present=%d low=%d crit=%d | magnet=%d\n",
                S.battV, S.battPct, S.battPresent, S.battLow, S.battCrit, S.magnet);
  Serial.printf("Servo pos %.0f %.0f %.0f  target %.0f %.0f %.0f  wave=%d\n",
                S.servoPos[0], S.servoPos[1], S.servoPos[2],
                S.servoTarget[0], S.servoTarget[1], S.servoTarget[2], S.waving);
  Serial.printf("Net up=%d RSSI=%d rx ok=%lu rejected=%lu\n",
                S.netUp, S.rssi, (unsigned long)S.rxOk, (unsigned long)S.rxRejected);
}

static void consoleExec(char* line) {
  // "cmd ..." passes the rest of the line to the protocol parser untouched
  if (strncmp(line, "cmd ", 4) == 0) {
    bool ok = handleCommandLine(line + 4, IPAddress(0, 0, 0, 0), true);
    Serial.println(ok ? "-> accepted" : "-> REJECTED (type 'verbose' to see why)");
    return;
  }

  char* save = nullptr;
  char* c  = strtok_r(line, " ", &save);
  char* a1 = strtok_r(nullptr, " ", &save);
  char* a2 = strtok_r(nullptr, " ", &save);
  char* a3 = strtok_r(nullptr, " ", &save);
  if (!c) return;

  if (!strcmp(c, "help")) printHelp();
  else if (!strcmp(c, "status")) printStatus();
  else if (!strcmp(c, "tel")) { char b[640]; buildTelemetry(b, sizeof(b)); Serial.println(b); }
  else if (!strcmp(c, "watch")) { conWatch = !conWatch; Serial.printf("watch %s\n", conWatch ? "ON" : "OFF"); }
  else if (!strcmp(c, "verbose")) { gVerbose = !gVerbose; Serial.printf("verbose %s\n", gVerbose ? "ON" : "OFF"); }
  else if (!strcmp(c, "motor")) {
    if (!ENABLE_MOTORS) { Serial.println("motors disabled in config.h"); return; }
    if (S.owner != '-') { Serial.println("a network controller is active - stop it first"); return; }
    if (!a1 || !a2) { Serial.println("usage: motor <1|2|3> <duty> [ms]"); return; }
    int m = atoi(a1);
    int duty = constrain(atoi(a2), -255, 255);
    uint32_t ms = a3 ? constrain(atoi(a3), 100, 5000) : 1500;
    S.testDrive = S.testDrive2 = S.testSteer = 0;
    if (m == 1) { S.testDrive = duty; if (DRIVE_SHARED_CHANNEL) S.testDrive2 = duty; }
    else if (m == 2) {
      if (DRIVE_SHARED_CHANNEL) { Serial.println("M2 shares channel A - use motor 1"); return; }
      S.testDrive2 = duty;
    }
    else if (m == 3) S.testSteer = duty;
    else { Serial.println("motor must be 1, 2 or 3"); return; }
    S.testUntil = millis() + ms;
    Serial.printf("TEST motor %d duty %d for %lu ms\n", m, duty, (unsigned long)ms);
  }
  else if (!strcmp(c, "stop")) doStop('S');
  else if (!strcmp(c, "servo")) {
    if (!a1 || !a2) { Serial.println("usage: servo <1-3> <deg>"); return; }
    int i = atoi(a1) - 1;
    if (i < 0 || i > 2) { Serial.println("servo must be 1..3"); return; }
    servoSetTarget(i, atof(a2));
    Serial.printf("servo %d target %.0f deg (after safe-limit clamp)\n", i + 1, S.servoTarget[i]);
  }
  else if (!strcmp(c, "home")) servosHome();
  else if (!strcmp(c, "wave")) servoStartWave();
  else if (!strcmp(c, "imu")) Serial.printf("IMU ok=%d roll=%.1f pitch=%.1f yaw=%.1f\n", S.imuOk, S.roll, S.pitch, S.yaw);
  else if (!strcmp(c, "calib")) imuCalibrate();
  else if (!strcmp(c, "bat")) Serial.printf("Battery %.2f V  %u%%  present=%d low=%d crit=%d\n",
                                            S.battV, S.battPct, S.battPresent, S.battLow, S.battCrit);
  else if (!strcmp(c, "mag")) Serial.printf("Magnet %s\n", S.magnet ? "PRESENT" : "ABSENT");
  else if (!strcmp(c, "net")) {
#if ENABLE_WIFI
    Serial.printf("Wi-Fi %s  IP=%s  RSSI=%d  owner=%c\n", S.netUp ? "UP" : "DOWN",
                  WiFi.localIP().toString().c_str(), S.rssi, S.owner);
#else
    Serial.println("Wi-Fi disabled");
#endif
  }
  else if (!strcmp(c, "reboot")) { motorsBrakeAll(); delay(100); ESP.restart(); }
  else Serial.println("unknown - type help");
}

void consoleUpdate() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (conLen > 0) { conBuf[conLen] = 0; consoleExec(conBuf); conLen = 0; }
    } else if (conLen < sizeof(conBuf) - 1) {
      conBuf[conLen++] = ch;
    }
  }
  if (conWatch && millis() - conWatchMs > 500) {
    conWatchMs = millis();
    Serial.printf("[W] %s own=%c y=%d x=%d M1=%d M3=%d | r=%.1f p=%.1f | %.2fV | mag=%d | srv %.0f %.0f %.0f\n",
                  STATE_NAMES[S.state], S.owner, S.cmdY, S.cmdX, S.outDrive, S.outSteer,
                  S.roll, S.pitch, S.battV, S.magnet,
                  S.servoPos[0], S.servoPos[1], S.servoPos[2]);
  }
}
