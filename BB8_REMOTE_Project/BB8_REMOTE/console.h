// =====================================================================
//  BB8_REMOTE - console.h     (bench-test tool for every guide step)
//  Serial Monitor 115200 baud, line ending "Newline", type help.
// =====================================================================
#pragma once

static char    conBuf[96];
static uint8_t conLen = 0;
static bool    conWatch = false;
static uint32_t conWatchMs = 0;

void printHelp() {
  Serial.println(F(
    "\n--------------- BB8_REMOTE console ---------------\n"
    " help            this list\n"
    " status          full status summary\n"
    " watch           toggle live joystick/button line (5 Hz)\n"
    " calib           re-measure joystick centres (release both sticks!)\n"
    " range           learn full stick travel for 5 s and save it\n"
    " rangereset      forget the learned travel (full ADC scale)\n"
    " verbose         toggle printing of every packet sent\n"
    " tel             print the last Body / Head / Camera telemetry\n"
    " send <body|head|cam> <CMD...>   send one command, e.g.\n"
    "                 send head EYE,3     send cam CAP     send body WAVE\n"
    " net             network info\n"
    " reboot          restart the Remote\n"
    "--------------------------------------------------"));
}

static void printStatus() {
  Serial.printf("Joystick 1 y=%d x=%d | Joystick 2 x=%.2f y=%.2f | fault=%d\n",
                R.j1y, R.j1x, R.j2x, R.j2y, R.joyFault);
  Serial.printf("Head target %.0f %.0f %.0f\n", R.head[0], R.head[1], R.head[2]);
  Serial.printf("Buttons J1SW=%d J2SW=%d PB1=%d PB2=%d PB3=%d PB4=%d\n",
                R.j1sw.down, R.j2sw.down, R.pb1.down, R.pb2.down, R.pb3.down, R.pb4.down);
  Serial.printf("Body %s state=%s owner=%c armed=%d batt=%.2fV %d%% faults=0x%02X\n",
                linkAlive(R.body.rxMs, LINK_TIMEOUT_MS) ? "ONLINE" : "offline",
                R.body.state, R.body.owner, R.body.armed, R.body.battV, R.body.battPct, R.body.faults);
  Serial.printf("Head %s eye=%s | Camera %s captures=%lu\n",
                linkAlive(R.headNode.rxMs, LINK_TIMEOUT_MS * 2) ? "ONLINE" : "offline", R.headNode.eyeName,
                linkAlive(R.cam.rxMs, CAM_LINK_TIMEOUT_MS) ? "ONLINE" : "offline", (unsigned long)R.cam.cap);
  Serial.printf("Net up=%d RSSI=%d tx=%lu rx=%lu\n", R.netUp, R.rssi,
                (unsigned long)R.txCount, (unsigned long)R.rxCount);
}

static void consoleExec(char* line) {
  if (!strncmp(line, "send ", 5)) {
    char* save = nullptr;
    char* who = strtok_r(line + 5, " ", &save);
    char* cmd = strtok_r(nullptr, "", &save);
    if (!who || !cmd) { Serial.println("usage: send <body|head|cam> <CMD>"); return; }
    if (!strcmp(who, "body"))      sendPacket(IP_BODY, PORT_BODY_CMD, cmd);
    else if (!strcmp(who, "head")) sendPacket(IP_HEAD, PORT_HEAD_CMD, cmd);
    else if (!strcmp(who, "cam"))  sendPacket(IP_CAMERA, PORT_CAM_CMD, cmd);
    else { Serial.println("target must be body, head or cam"); return; }
    Serial.println("sent");
    return;
  }
  if (!strcmp(line, "help")) printHelp();
  else if (!strcmp(line, "status")) printStatus();
  else if (!strcmp(line, "watch")) { conWatch = !conWatch; Serial.printf("watch %s\n", conWatch ? "ON" : "OFF"); }
  else if (!strcmp(line, "calib")) calibrateCenter();
  else if (!strcmp(line, "range")) learnRange();
  else if (!strcmp(line, "rangereset")) resetRange();
  else if (!strcmp(line, "verbose")) { gVerbose = !gVerbose; Serial.printf("verbose %s\n", gVerbose ? "ON" : "OFF"); }
  else if (!strcmp(line, "tel")) {
    Serial.printf("BODY %s\nHEAD %s\nCAM  %s\n", lastBodyJson[0] ? lastBodyJson : "-",
                  lastHeadJson[0] ? lastHeadJson : "-", lastCamJson[0] ? lastCamJson : "-");
  }
  else if (!strcmp(line, "net")) {
#if ENABLE_WIFI
    Serial.printf("Wi-Fi %s  IP=%s  RSSI=%d\n", R.netUp ? "UP" : "DOWN",
                  WiFi.localIP().toString().c_str(), R.rssi);
#else
    Serial.println("Wi-Fi disabled");
#endif
  }
  else if (!strcmp(line, "reboot")) ESP.restart();
  else Serial.println("unknown - type help");
}

void consoleUpdate() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (conLen > 0) { conBuf[conLen] = 0; consoleExec(conBuf); conLen = 0; }
    } else if (conLen < sizeof(conBuf) - 1) conBuf[conLen++] = ch;
  }
  if (conWatch && millis() - conWatchMs > 200) {
    conWatchMs = millis();
    Serial.printf("[W] raw %4.0f %4.0f %4.0f %4.0f | J1 y%+4d x%+4d | J2 %+.2f %+.2f | "
                  "SW %d%d PB %d%d%d%d\n",
                  R.raw[0], R.raw[1], R.raw[2], R.raw[3], R.j1y, R.j1x, R.j2x, R.j2y,
                  R.j1sw.down, R.j2sw.down, R.pb1.down, R.pb2.down, R.pb3.down, R.pb4.down);
  }
}
