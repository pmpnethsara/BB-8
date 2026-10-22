// =====================================================================
//  BB8_REMOTE - state.h
//  ONE shared structure "R" holds the live state of the Remote:
//  inputs.h WRITES the joystick/button values, commands.h SENDS from
//  them, telemetry_rx.h fills in what the robot reports, and
//  display.h / leds.h SHOW it.  Same idea as "S" in BB8_BODY.
// =====================================================================
#pragma once

struct Button {
  uint8_t  pin;
  bool     down = false;        // debounced state (true = pressed)
  bool     rawLast = false;
  uint32_t changedMs = 0;
  uint32_t downSinceMs = 0;
  bool     pressed = false;     // edge flags, cleared by commands.h
  bool     released = false;
  bool     longFired = false;   // long-press action already done
};

struct BodyInfo {               // from Body telemetry (BB8_PROTOCOL.md section 5)
  uint32_t rxMs = 0;
  char     state[10] = "-";
  char     owner = '-';
  bool     armed = false;
  uint16_t faults = 0;
  int      cmdY = 0, cmdX = 0;
  int      m1 = 0, m3 = 0;
  bool     imuOk = false;
  float    roll = 0, pitch = 0;
  bool     battPresent = false, battLow = false, battCrit = false;
  float    battV = 0;
  int      battPct = 0;
  bool     mag = false;
  float    servo[3] = {90, 90, 90};
  bool     waving = false;
};

struct HeadInfo {               // from Head telemetry
  uint32_t rxMs = 0;
  int      eye = 0;
  char     eyeName[10] = "-";
  bool     oled[2] = {false, false};
  bool     camLink = false;
};

struct CamInfo {                // from Camera status
  uint32_t rxMs = 0;
  bool     camOk = false;
  uint32_t cap = 0;
  bool     capOk = false;
  bool     sd = false;
  int      stream = 0;
};

struct RemoteState {
  // ---- joysticks ----
  bool     calibrated = false;
  bool     joyFault = false;            // centre reading impossible -> wiring problem
  float    raw[4] = {0, 0, 0, 0};       // filtered ADC: J1Y, J1X, J2X, J2Y
  float    center[4] = {2048, 2048, 2048, 2048};
  float    rmin[4] = {0, 0, 0, 0};
  float    rmax[4] = {4095, 4095, 4095, 4095};
  int      j1y = 0, j1x = 0;            // -100..100 after dead-zone/expo (sent as DRV)
  float    j2x = 0, j2y = 0;            // -1..1 (head)
  float    head[3] = {90, 90, 90};      // head target angles being sent

  // ---- buttons ----
  Button   j1sw, j2sw, pb1, pb2, pb3, pb4;

  // ---- what the robot reports ----
  BodyInfo body;
  HeadInfo headNode;
  CamInfo  cam;

  // ---- UI / events ----
  char     popup[22] = "";
  uint32_t popupUntil = 0;
  uint32_t capWaitMs = 0;               // PB3 pressed, waiting for the camera answer
  uint32_t capBaseline = 0;
  float    resetHold = 0;               // 0..1 progress while PB4 is held
  uint32_t restartAt = 0;
  bool     waveRequested = false;

  // ---- network ----
  bool     netUp = false;
  int8_t   rssi = 0;
  uint32_t txCount = 0, rxCount = 0;
};

RemoteState R;
bool gVerbose = false;   // console "verbose" prints every packet that is sent

static inline bool linkAlive(uint32_t lastMs, uint32_t timeout) {
  return lastMs != 0 && (millis() - lastMs) < timeout;
}

void showPopup(const char* text) {
  strncpy(R.popup, text, sizeof(R.popup) - 1);
  R.popup[sizeof(R.popup) - 1] = 0;
  R.popupUntil = millis() + POPUP_MS;
  Serial.printf("[UI] %s\n", text);
}
