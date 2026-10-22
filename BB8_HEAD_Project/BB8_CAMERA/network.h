// =====================================================================
//  BB8_CAMERA - network.h     (Guide: Camera Wi-Fi/IP)
//
//   Remote 192.168.8.204 --UDP 4213 (CAP / FLASH / RESET)----------> Camera
//   Camera 192.168.8.203 --UDP 4211 (status, 1 Hz + after capture)-> Remote + Head
//   Dashboard (browser)  --HTTP :80 /capture /status /cmd ---------> Camera
//   Dashboard (browser)  <-HTTP :81 /stream (live MJPEG)------------ Camera
// =====================================================================
#pragma once

#if ENABLE_WIFI
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>

static WiFiUDP  udpCmd;
static WiFiUDP  udpTel;
static bool     servicesStarted = false;
static uint32_t wifiRetryMs = 0;
static IPAddress monitorIP;
static uint32_t  monitorMs = 0;

void networkBegin() {
  WiFi.persistent(false);
  WiFi.setHostname(CAM_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                         // essential for a smooth stream
#if USE_STATIC_IP
  if (!WiFi.config(IP_CAMERA, IP_GATEWAY, IP_SUBNET, IP_DNS))
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
  if (up && !C.netUp) {
    Serial.printf("[NET] connected  IP=%s  RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    if (!servicesStarted) {
      udpCmd.begin(PORT_CAM_CMD);
      httpServersBegin();
      if (MDNS.begin(CAM_HOSTNAME)) MDNS.addService("http", "tcp", PORT_HTTP);
      servicesStarted = true;
    } else { udpCmd.stop(); udpCmd.begin(PORT_CAM_CMD); }
  }
  if (!up && C.netUp) Serial.println("[NET] Wi-Fi LOST");
  C.netUp = up;
  if (!up) {
    if (now - wifiRetryMs > WIFI_RETRY_MS) { wifiRetryMs = now; WiFi.reconnect(); }
    return;
  }
  C.rssi = WiFi.RSSI();

  for (int i = 0; i < 6; i++) {
    int size = udpCmd.parsePacket();
    if (size <= 0) break;
    char buf[128];
    int len = udpCmd.read(buf, sizeof(buf) - 1);
    buf[len < 0 ? 0 : len] = 0;
    IPAddress from = udpCmd.remoteIP();
    if (handleCommandLine(buf, from == IP_REMOTE) &&
        !(from == IP_REMOTE || from == IP_HEAD || from == IP_BODY)) {
      monitorIP = from; monitorMs = millis();      // laptop test tool gets status too
    }
  }
}

static void udpSend(const IPAddress& ip, const char* data, size_t n) {
  udpTel.beginPacket(ip, PORT_TELEMETRY);
  udpTel.write((const uint8_t*)data, n);
  udpTel.endPacket();
}

void telemetryUpdate() {
  if (!C.netUp) return;
  C.telSeq++;
  char buf[400];
  size_t n = buildTelemetry(buf, sizeof(buf));
  udpSend(IP_REMOTE, buf, n);
  udpSend(IP_HEAD, buf, n);
  if (monitorMs && millis() - monitorMs < 5000) udpSend(monitorIP, buf, n);
}

#else
void networkBegin() { Serial.println("[NET] Wi-Fi disabled in config.h"); }
void networkUpdate() {}
void telemetryUpdate() {}
#endif
