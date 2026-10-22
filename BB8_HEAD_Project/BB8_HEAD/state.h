// =====================================================================
//  BB8_HEAD - state.h
//  ONE shared structure "H" holds the live state of the whole Head.
//  Network / console WRITE requests into it, eyes.h ANIMATES from it,
//  oled.h DRAWS it and telemetry.h REPORTS it.
//  This is how all Head modules are interconnected (same idea as the
//  "S" structure in BB8_BODY).
// =====================================================================
#pragma once

// Eye designs stored in the firmware (PB1 / Dashboard cycles through them)
enum EyeMode : uint8_t {
  EYE_NORMAL = 0, EYE_HAPPY, EYE_ANGRY, EYE_SAD, EYE_SLEEPY, EYE_LOVE, EYE_LENS,
  EYE_COUNT
};
static const char* EYE_NAMES[EYE_COUNT] = {
  "NORMAL", "HAPPY", "ANGRY", "SAD", "SLEEPY", "LOVE", "LENS"
};

enum BlinkPhase : uint8_t { BL_NONE = 0, BL_CLOSING, BL_CLOSED, BL_OPENING };

struct HeadState {
  // ---- eyes (requests from protocol.h / console, animation in eyes.h) ----
  uint8_t    eyeMode = EYE_DEFAULT_MODE;   // design currently shown
  int8_t     pendingMode = -1;             // new design waiting for the lids to close
  BlinkPhase blink = BL_NONE;
  uint32_t   blinkPhaseMs = 0;
  bool       blinkRequested = false;
  float      lid = 0;                      // 0 = open, 1 = closed (drawn by oled.h)
  float      lookX = 0, lookY = 0;         // current eye offset (pixels)
  float      lookTX = 0, lookTY = 0;       // target eye offset
  uint32_t   blinkCount = 0;

  // ---- displays ----
  bool oledOk[2] = {false, false};         // [0] = left / OLED 1, [1] = right / OLED 2
  bool testPattern = false;                // console "test" shows LEFT / RIGHT labels

  // ---- information from the other nodes ----
  uint32_t bodyRxMs = 0;                   // last Body telemetry
  char     bodyState[10] = "-";
  float    bodyServo[3] = {90, 90, 90};
  bool     bodyBattLow = false;
  uint32_t camRxMs = 0;                    // last Camera status
  uint32_t camCaptures = 0;                // capture counter reported by the camera
  bool     camCaptureOk = false;
  uint32_t remoteRxMs = 0;                 // last packet from the Remote (HB or command)

  // ---- network ----
  bool     netUp = false;
  int8_t   rssi = 0;
  uint32_t rxOk = 0, rxRejected = 0, telSeq = 0;
  uint32_t resetAt = 0;
};

HeadState H;
bool gVerbose = false;   // "verbose" console command prints rejected packets

static inline bool linkAlive(uint32_t lastMs, uint32_t timeout) {
  return lastMs != 0 && (millis() - lastMs) < timeout;
}
