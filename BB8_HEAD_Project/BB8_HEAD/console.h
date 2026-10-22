// =====================================================================
//  BB8_HEAD - console.h       (bench-test tool for every guide step)
//  Serial Monitor 115200 baud, line ending "Newline", type help.
// =====================================================================
#pragma once

static char    conBuf[96];
static uint8_t conLen = 0;

void printHelp() {
  Serial.println(F(
    "\n--------------- BB8_HEAD console ---------------\n"
    " help               this list\n"
    " status             full status summary\n"
    " tel                print one telemetry JSON line\n"
    " verbose            toggle printing of rejected packets\n"
    " i2c                scan both I2C buses for the OLEDs\n"
    " test               toggle LEFT/RIGHT test pattern on the OLEDs\n"
    " eye <0-6>          select eye design\n"
    " next | prev        next / previous design (= PB1)\n"
    " blink              blink both eyes (= PB2)\n"
    " look <x> <y>       hold the eyes at an offset (look 0 0 = release)\n"
    " list               list eye designs\n"
    " net                network info\n"
    " cmd <line>         inject protocol line, e.g. cmd BB8,T,1,EYE,2\n"
    " reboot             restart ESP32\n"
    "------------------------------------------------"));
}

static void printStatus() {
  Serial.printf("Eye %u %s  pending=%d  lid=%.2f  look=%.0f,%.0f  blinks=%lu\n",
                H.eyeMode, EYE_NAMES[H.eyeMode], H.pendingMode, H.lid, H.lookX, H.lookY,
                (unsigned long)H.blinkCount);
  Serial.printf("OLED left=%d right=%d  test=%d\n", H.oledOk[0], H.oledOk[1], H.testPattern);
  Serial.printf("Links body=%d (%s) cam=%d (cap %lu) remote=%d\n",
                linkAlive(H.bodyRxMs, LINK_TIMEOUT_MS), H.bodyState,
                linkAlive(H.camRxMs, CAM_LINK_TIMEOUT_MS), (unsigned long)H.camCaptures,
                linkAlive(H.remoteRxMs, LINK_TIMEOUT_MS));
  Serial.printf("Net up=%d RSSI=%d rx ok=%lu rejected=%lu\n",
                H.netUp, H.rssi, (unsigned long)H.rxOk, (unsigned long)H.rxRejected);
}

static void consoleExec(char* line) {
  if (strncmp(line, "cmd ", 4) == 0) {
    bool ok = handleCommandLine(line + 4, IPAddress(0, 0, 0, 0), true);
    Serial.println(ok ? "-> accepted" : "-> REJECTED (type 'verbose' to see why)");
    return;
  }
  char* save = nullptr;
  char* c  = strtok_r(line, " ", &save);
  char* a1 = strtok_r(nullptr, " ", &save);
  char* a2 = strtok_r(nullptr, " ", &save);
  if (!c) return;

  if (!strcmp(c, "help")) printHelp();
  else if (!strcmp(c, "status")) printStatus();
  else if (!strcmp(c, "tel")) { char b[512]; buildTelemetry(b, sizeof(b)); Serial.println(b); }
  else if (!strcmp(c, "verbose")) { gVerbose = !gVerbose; Serial.printf("verbose %s\n", gVerbose ? "ON" : "OFF"); }
  else if (!strcmp(c, "i2c")) oledScan();
  else if (!strcmp(c, "test")) { H.testPattern = !H.testPattern; Serial.printf("test pattern %s\n", H.testPattern ? "ON" : "OFF"); }
  else if (!strcmp(c, "eye")) {
    if (!a1) { Serial.println("usage: eye <0-6>"); return; }
    eyesRequestMode(atoi(a1));
  }
  else if (!strcmp(c, "next")) eyesRequestMode(-1);
  else if (!strcmp(c, "prev")) eyesRequestMode(-2);
  else if (!strcmp(c, "blink")) eyesRequestBlink();
  else if (!strcmp(c, "look")) {
    if (!a1 || !a2) { Serial.println("usage: look <x> <y>"); return; }
    H.lookTX = constrain(atoi(a1), -LOOK_MAX_X, LOOK_MAX_X);
    H.lookTY = constrain(atoi(a2), -LOOK_MAX_Y, LOOK_MAX_Y);
    Serial.println("(idle look will take over again after a few seconds)");
  }
  else if (!strcmp(c, "list")) for (int i = 0; i < EYE_COUNT; i++) Serial.printf(" %d %s\n", i, EYE_NAMES[i]);
  else if (!strcmp(c, "net")) {
#if ENABLE_WIFI
    Serial.printf("Wi-Fi %s  IP=%s  RSSI=%d\n", H.netUp ? "UP" : "DOWN",
                  WiFi.localIP().toString().c_str(), H.rssi);
#else
    Serial.println("Wi-Fi disabled");
#endif
  }
  else if (!strcmp(c, "reboot")) { delay(100); ESP.restart(); }
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
}
