// =====================================================================
//  BB8_BODY - network.h        (Guide step 1 & 4: Wi-Fi/IP + links)
//
//  HOW THE BODY TALKS TO THE OTHER PACKAGES
//
//   Remote 192.168.8.204 --UDP 4210 (DRV/HEAD/ROT/WAVE/STOP/RESET)--> Body
//   Body 192.168.8.201  --UDP 4211 (telemetry JSON, 5 Hz)----------> Remote
//   Body                --UDP 4211 (telemetry JSON, 5 Hz)----------> Head 192.168.8.202
//   Dashboard (browser) --HTTP GET /cmd?line=...--------------------> Body
//   Dashboard           <--HTTP GET /telemetry-------------------------Body
//
//  Eye/blink (PB1/PB2) and capture (PB3) go from the Remote DIRECTLY to
//  the Head / ESP32-CAM - the Body is not in that path.
//
//  UDP is used for control because it is fast and never blocks; lost
//  packets are harmless because DRV/HEAD are re-sent 20x per second and
//  the fail-safe stops the robot if they stop arriving.
// =====================================================================
#pragma once

#if ENABLE_WIFI
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "web_test_page.h"

static WiFiUDP   udpCmd;
static WiFiUDP   udpTel;
static WebServer http(PORT_HTTP);
static bool      servicesStarted = false;
static uint32_t  wifiRetryMs = 0;
static IPAddress monitorIP;          // laptop test tool gets telemetry too
static uint32_t  monitorMs = 0;

static void noteMonitor(const IPAddress& from) {
  if (from == IP_REMOTE || from == IP_HEAD || from == IPAddress(0, 0, 0, 0)) return;
  monitorIP = from;
  monitorMs = millis();
}

// ---------------- HTTP handlers (Web Dashboard interface) ----------------
static void addCors() {
  http.sendHeader("Access-Control-Allow-Origin", "*");   // dashboard may be hosted elsewhere
  http.sendHeader("Cache-Control", "no-store");
}

static void handleRoot() {
  addCors();
  http.send(200, "text/html", WEB_TEST_PAGE);
}

static void handleTelemetryHttp() {
  char buf[640];
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
  udpCmd.begin(PORT_BODY_CMD);
  http.on("/", HTTP_GET, handleRoot);
  http.on("/telemetry", HTTP_GET, handleTelemetryHttp);
  http.on("/cmd", HTTP_ANY, handleCmdHttp);
  http.onNotFound([]() { http.send(404, "text/plain", "not found"); });
  http.begin();
  if (MDNS.begin(BODY_HOSTNAME)) MDNS.addService("http", "tcp", PORT_HTTP);
  servicesStarted = true;
  Serial.printf("[NET] UDP cmd port %d, HTTP port %d, http://%s.local\n",
                PORT_BODY_CMD, PORT_HTTP, BODY_HOSTNAME);
}

void networkBegin() {
  WiFi.persistent(false);
  WiFi.setHostname(BODY_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);              // power-save adds 100+ ms latency - keep it off
#if USE_STATIC_IP
  if (!WiFi.config(IP_BODY, IP_GATEWAY, IP_SUBNET, IP_DNS))
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

  if (up && !S.netUp) {
    Serial.printf("[NET] connected  IP=%s  RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    if (!servicesStarted) startServices();
    else { udpCmd.stop(); udpCmd.begin(PORT_BODY_CMD); }
  }
  if (!up && S.netUp) Serial.println("[NET] Wi-Fi LOST -> movement stopped");
  S.netUp = up;

  if (!up) {
    if (now - wifiRetryMs > WIFI_RETRY_MS) {
      wifiRetryMs = now;
      WiFi.reconnect();
    }
    return;
  }
  S.rssi = WiFi.RSSI();

  // read all waiting UDP command packets (max 10 per loop)
  for (int i = 0; i < 10; i++) {
    int size = udpCmd.parsePacket();
    if (size <= 0) break;
    char buf[128];
    int len = udpCmd.read(buf, sizeof(buf) - 1);
    if (len < 0) len = 0;
    buf[len] = 0;
    IPAddress from = udpCmd.remoteIP();
    if (handleCommandLine(buf, from)) noteMonitor(from);
  }

  http.handleClient();
}

static void udpSend(const IPAddress& ip, const char* data, size_t n) {
  udpTel.beginPacket(ip, PORT_TELEMETRY);
  udpTel.write((const uint8_t*)data, n);
  udpTel.endPacket();
}

void telemetryUpdate() {
  if (!S.netUp) return;
  S.telSeq++;
  char buf[640];
  size_t n = buildTelemetry(buf, sizeof(buf));
  udpSend(IP_REMOTE, buf, n);
  udpSend(IP_HEAD, buf, n);
  if (monitorMs && millis() - monitorMs < 5000) udpSend(monitorIP, buf, n);
}

#else   // ---------------- Wi-Fi disabled: stubs ----------------
void networkBegin() { Serial.println("[NET] Wi-Fi disabled in config.h (bench mode)"); }
void networkUpdate() {}
void telemetryUpdate() {}
#endif
