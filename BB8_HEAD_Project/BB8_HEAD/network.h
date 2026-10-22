// =====================================================================
//  BB8_HEAD - network.h       (Guide: Head Wi-Fi/IP + network commands)
//
//  HOW THE HEAD TALKS TO THE OTHER PACKAGES
//
//   Remote 192.168.8.204 --UDP 4212 (EYE / BLINK / HB / RESET)-----> Head
//   Body   192.168.8.201 --UDP 4211 (Body telemetry, 5 Hz)----------> Head
//   Camera 192.168.8.203 --UDP 4211 (camera status, 1 Hz)-----------> Head
//   Head   192.168.8.202 --UDP 4211 (Head telemetry, 4 Hz)----------> Remote
//   Dashboard (browser)  --HTTP GET /cmd?line=... /telemetry ------> Head
// =====================================================================
#pragma once

#if ENABLE_WIFI
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "web_test_page.h"

static WiFiUDP   udpCmd;      // 4212 in
static WiFiUDP   udpTel;      // 4211 in (Body/Camera) + out (to Remote)
static WebServer http(PORT_HTTP);
static bool      servicesStarted = false;
static uint32_t  wifiRetryMs = 0;
static IPAddress monitorIP;   // laptop test tool gets Head telemetry too
static uint32_t  monitorMs = 0;

static void noteMonitor(const IPAddress& from) {
  if (from == IP_REMOTE || from == IP_BODY || from == IP_CAMERA || from == IPAddress(0, 0, 0, 0)) return;
  monitorIP = from;
  monitorMs = millis();
}

static void addCors() {
  http.sendHeader("Access-Control-Allow-Origin", "*");
  http.sendHeader("Cache-Control", "no-store");
}
static void handleRoot() {
  String page(WEB_TEST_PAGE);
  page.replace("%CAM%", IP_CAMERA.toString());
  addCors();
  http.send(200, "text/html", page);
}
static void handleTelemetryHttp() {
  char buf[512];
  buildTelemetry(buf, sizeof(buf));
  addCors();
  http.send(200, "application/json", buf);
}
static void handleCmdHttp() {
  addCors();
  if (!http.hasArg("line")) { http.send(400, "text/plain", "missing ?line="); return; }
  char buf[128];
  http.arg("line").toCharArray(buf, sizeof(buf));
  bool ok = handleCommandLine(buf, http.client().remoteIP());
  http.send(ok ? 200 : 409, "text/plain", ok ? "OK" : "REJECTED");
}

static void startServices() {
  udpCmd.begin(PORT_HEAD_CMD);
  udpTel.begin(PORT_TELEMETRY);
  http.on("/", HTTP_GET, handleRoot);
  http.on("/telemetry", HTTP_GET, handleTelemetryHttp);
  http.on("/cmd", HTTP_ANY, handleCmdHttp);
  http.onNotFound([]() { http.send(404, "text/plain", "not found"); });
  http.begin();
  if (MDNS.begin(HEAD_HOSTNAME)) MDNS.addService("http", "tcp", PORT_HTTP);
  servicesStarted = true;
  Serial.printf("[NET] UDP cmd %d, telemetry %d, HTTP %d, http://%s.local\n",
                PORT_HEAD_CMD, PORT_TELEMETRY, PORT_HTTP, HEAD_HOSTNAME);
}

void networkBegin() {
  WiFi.persistent(false);
  WiFi.setHostname(HEAD_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
#if USE_STATIC_IP
  if (!WiFi.config(IP_HEAD, IP_GATEWAY, IP_SUBNET, IP_DNS))
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
  if (up && !H.netUp) {
    Serial.printf("[NET] connected  IP=%s  RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    if (!servicesStarted) startServices();
    else { udpCmd.stop(); udpCmd.begin(PORT_HEAD_CMD); udpTel.stop(); udpTel.begin(PORT_TELEMETRY); }
  }
  if (!up && H.netUp) Serial.println("[NET] Wi-Fi LOST");
  H.netUp = up;
  if (!up) {
    if (now - wifiRetryMs > WIFI_RETRY_MS) { wifiRetryMs = now; WiFi.reconnect(); }
    return;
  }
  H.rssi = WiFi.RSSI();

  for (int i = 0; i < 10; i++) {                     // eye commands
    int size = udpCmd.parsePacket();
    if (size <= 0) break;
    char buf[128];
    int len = udpCmd.read(buf, sizeof(buf) - 1);
    buf[len < 0 ? 0 : len] = 0;
    IPAddress from = udpCmd.remoteIP();
    if (handleCommandLine(buf, from)) noteMonitor(from);
  }
  for (int i = 0; i < 6; i++) {                      // Body + Camera telemetry
    int size = udpTel.parsePacket();
    if (size <= 0) break;
    char buf[768];
    int len = udpTel.read(buf, sizeof(buf) - 1);
    buf[len < 0 ? 0 : len] = 0;
    handleTelemetryPacket(buf, udpTel.remoteIP());
  }
  http.handleClient();
}

static void udpSend(const IPAddress& ip, const char* data, size_t n) {
  udpTel.beginPacket(ip, PORT_TELEMETRY);
  udpTel.write((const uint8_t*)data, n);
  udpTel.endPacket();
}

void telemetryUpdate() {
  if (!H.netUp) return;
  H.telSeq++;
  char buf[512];
  size_t n = buildTelemetry(buf, sizeof(buf));
  udpSend(IP_REMOTE, buf, n);
  if (monitorMs && millis() - monitorMs < 5000) udpSend(monitorIP, buf, n);
}

#else
void networkBegin() { Serial.println("[NET] Wi-Fi disabled in config.h (bench mode)"); }
void networkUpdate() {}
void telemetryUpdate() {}
#endif
