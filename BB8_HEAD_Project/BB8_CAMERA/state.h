// =====================================================================
//  BB8_CAMERA - state.h
//  ONE shared structure "C" holds the live state of the camera node.
//  The HTTP server runs in its own task, so anything both sides touch
//  is protected by the two mutexes below.
// =====================================================================
#pragma once

struct CamState {
  bool     camOk = false;          // sensor initialised
  bool     psram = false;
  bool     sdOk = false;
  bool     flashOn = false;        // torch mode (FLASH,1)
  int      streamClients = 0;        // changed only by the stream task

  uint32_t captures = 0;           // successful captures since boot
  bool     lastCapOk = false;
  uint32_t lastCapMs = 0;
  size_t   lastSize = 0;
  char     lastSaved[24] = "";     // file name on the SD card
  uint32_t fileNo = 0;             // persistent file counter (NVS)

  bool     netUp = false;
  int8_t   rssi = 0;
  uint32_t rxOk = 0, rxRejected = 0, telSeq = 0;
  bool     statusNow = false;      // send status immediately (after a capture)
  uint32_t resetAt = 0;
};

CamState C;
bool gVerbose = false;

SemaphoreHandle_t gCapMutex = nullptr;   // latest image buffer + capture
SemaphoreHandle_t gCmdMutex = nullptr;   // protocol state (UDP loop + HTTP task)
