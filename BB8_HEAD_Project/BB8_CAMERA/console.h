// =====================================================================
//  BB8_CAMERA - console.h
//  Serial Monitor 115200 baud, "Newline". The ESP32-CAM has no USB:
//  use the USB-serial (FTDI / ESP32-CAM-MB) adapter used for uploading.
// =====================================================================
#pragma once

static char    conBuf[96];
static uint8_t conLen = 0;

static void consoleExec(char* line) {
  if (strncmp(line, "cmd ", 4) == 0) {
    bool ok = handleCommandLine(line + 4, false, true);
    Serial.println(ok ? "-> accepted" : "-> REJECTED (type 'verbose')");
    return;
  }
  if (!strcmp(line, "help"))
    Serial.println(F("\n--- BB8_CAMERA console ---\n"
                     " status | tel | cap | flash on | flash off | verbose | net | reboot\n"
                     " cmd <line>   e.g. cmd BB8,T,1,CAP"));
  else if (!strcmp(line, "status") || !strcmp(line, "tel")) {
    char b[400]; buildTelemetry(b, sizeof(b)); Serial.println(b);
  }
  else if (!strcmp(line, "cap")) cameraCapture();
  else if (!strcmp(line, "flash on")) setFlash(true);
  else if (!strcmp(line, "flash off")) setFlash(false);
  else if (!strcmp(line, "verbose")) { gVerbose = !gVerbose; Serial.printf("verbose %d\n", gVerbose); }
  else if (!strcmp(line, "net")) {
#if ENABLE_WIFI
    Serial.printf("Wi-Fi %s IP=%s RSSI=%d stream viewers=%d\n", C.netUp ? "UP" : "DOWN",
                  WiFi.localIP().toString().c_str(), C.rssi, (int)C.streamClients);
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
}
