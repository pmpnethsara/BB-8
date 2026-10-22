// =====================================================================
//  BB8_REMOTE - network.h     (Guide: Wi-Fi/IP + command transmission)
//
//  WHERE EVERY REMOTE INPUT GOES
//
//   Joystick 1        DRV,y,x  20 Hz  ---UDP 4210--> Body   192.168.8.201
//   Joystick 1 push   ROT,1 / ROT,-1  ---UDP 4210--> Body
//   Joystick 2        HEAD,a1,a2,a3   ---UDP 4210--> Body   (MG995 servos)
//   Joystick 2 push   WAVE            ---UDP 4210--> Body
//   PB4 short         STOP            ---UDP 4210--> Body
//   PB1               EYE,NEXT,eid    ---UDP 4212--> Head   192.168.8.202
//   PB2               BLINK,eid       ---UDP 4212--> Head
//   (heartbeat)       HB  2 Hz        ---UDP 4212--> Head
//   PB3               CAP,eid         ---UDP 4213--> Camera 192.168.8.203
//   PB4 hold          RESET,eid       ---> Body + Head + Camera
//
//   Body / Head / Camera telemetry    <--UDP 4211--- (OLED + LED2)
//
//  Every packet:  BB8,R,<seq>,<CMD>[,args]   seq increases by 1 each time.
// =====================================================================
#pragma once

static uint32_t gSeq = 1;

#if ENABLE_WIFI
#include <WiFi.h>
#include <WiFiUdp.h>

static WiFiUDP  udp;              // bound to 4211: receives telemetry, also used to send
static bool     udpStarted = false;
static uint32_t wifiRetryMs = 0;

void networkBegin() {
  WiFi.persistent(false);
  WiFi.setHostname(REMOTE_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);           // power-save adds 100+ ms latency to every command
#if USE_STATIC_IP
  if (!WiFi.config(IP_REMOTE, IP_GATEWAY, IP_SUBNET, IP_DNS))
    Serial.println("[NET] static IP config FAILED");
#endif
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiRetryMs = millis();
  Serial.printf("[NET] connecting to '%s' ...\n", WIFI_SSID);
}

void networkUpdate() {
  uint32_t now = millis();
  bool up = (WiFi.status() == WL_CONNECTED);
  if (up && !R.netUp) {
    Serial.printf("[NET] connected  IP=%s  RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    if (udpStarted) udp.stop();
    udp.begin(PORT_TELEMETRY);
    udpStarted = true;
#if USE_TEST_TARGET
    Serial.printf("[NET] TEST MODE: all commands go to %s (robot_sim.py)\n",
                  IP_TEST_TARGET.toString().c_str());
#endif
  }
  if (!up && R.netUp) Serial.println("[NET] Wi-Fi LOST (Body stops by itself after 0.5 s)");
  R.netUp = up;
  if (!up) {
    if (now - wifiRetryMs > WIFI_RETRY_MS) { wifiRetryMs = now; WiFi.reconnect(); }
    return;
  }
  R.rssi = WiFi.RSSI();

  for (int i = 0; i < 8; i++) {                 // read waiting telemetry
    int size = udp.parsePacket();
    if (size <= 0) break;
    char buf[768];
    int len = udp.read(buf, sizeof(buf) - 1);
    buf[len < 0 ? 0 : len] = 0;
    handleTelemetryPacket(buf);
  }
}

// Sends "BB8,R,<seq>,<cmd>" to one node
void sendPacket(const IPAddress& ip, uint16_t port, const char* cmd) {
  char line[96];
  int n = snprintf(line, sizeof(line), "BB8,R,%lu,%s", (unsigned long)gSeq++, cmd);
  if (gVerbose) Serial.printf("[TX] %s:%u  %s\n", ip.toString().c_str(), port, line);
  if (!R.netUp) return;
#if USE_TEST_TARGET
  udp.beginPacket(IP_TEST_TARGET, port);
#else
  udp.beginPacket(ip, port);
#endif
  udp.write((const uint8_t*)line, n);
  udp.endPacket();
  R.txCount++;
}

#else   // ---------------- Wi-Fi disabled: stand-alone input test ----------------
void networkBegin() { Serial.println("[NET] Wi-Fi disabled in config.h (input test mode)"); }
void networkUpdate() {}
void sendPacket(const IPAddress& ip, uint16_t port, const char* cmd) {
  if (gVerbose) Serial.printf("[TX-off] %s:%u  BB8,R,%lu,%s\n", ip.toString().c_str(), port,
                              (unsigned long)gSeq, cmd);
  gSeq++;
}
#endif
