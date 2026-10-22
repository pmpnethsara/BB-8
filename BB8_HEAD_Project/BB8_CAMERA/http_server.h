// =====================================================================
//  BB8_CAMERA - http_server.h (Guide: live camera for the Web Dashboard)
//
//  Port 80  (control)                       Port 81 (video)
//   /            small test page             /stream  live MJPEG stream
//   /capture     take a still -> JSON
//   /latest.jpg  last captured image
//   /status      camera status JSON
//   /cmd?line=   protocol command (W source)
//
//  The video stream runs in its OWN server (port 81) so that a viewer
//  watching the stream never blocks /capture or /status.
//  Dashboard: <img src="http://192.168.8.203:81/stream">
//  Note: the ESP32-CAM serves ONE stream viewer at a time.
// =====================================================================
#pragma once
#include "esp_http_server.h"

static httpd_handle_t ctrlServer = nullptr;
static httpd_handle_t streamServer = nullptr;

#define PART_BOUNDARY "bb8frame"
static const char* STREAM_TYPE  = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_SEP   = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static const char CAM_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BB-8 Camera</title>
<style>body{font-family:system-ui,sans-serif;margin:12px;background:#15181c;color:#eee}
img{width:100%;max-width:640px;border-radius:8px;background:#000}
button{font-size:16px;padding:12px;margin:3px;border:0;border-radius:8px;background:#2d333b;color:#fff}
pre{background:#000;padding:8px;font-size:12px;white-space:pre-wrap}</style></head><body>
<h3>BB-8 Camera (ESP32-CAM)</h3>
<img id="v" alt="stream">
<p><button onclick="cap()">Capture</button>
<button onclick="fl(1)">Torch on</button><button onclick="fl(0)">Torch off</button>
<a href="/latest.jpg" target="_blank" style="color:#8cf">latest image</a></p>
<pre id="s"></pre>
<script>
document.getElementById('v').src = location.protocol + '//' + location.hostname + ':81/stream';
let seq = Date.now() % 2000000000;
function fl(on){ seq++; fetch('/cmd?line=' + encodeURIComponent('BB8,W,' + seq + ',FLASH,' + on)); }
function cap(){ fetch('/capture').then(r=>r.json()).then(show); }
function show(j){ document.getElementById('s').textContent = JSON.stringify(j, null, 1); }
setInterval(()=>fetch('/status').then(r=>r.json()).then(show).catch(()=>0), 1000);
</script></body></html>)HTML";

static void cors(httpd_req_t* req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
}

static esp_err_t sendStatus(httpd_req_t* req) {
  char buf[400];
  buildTelemetry(buf, sizeof(buf));
  cors(req);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t pageHandler(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, CAM_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t statusHandler(httpd_req_t* req) { return sendStatus(req); }

static esp_err_t captureHandler(httpd_req_t* req) {   // Dashboard "Capture" button
  cameraCapture();
  return sendStatus(req);
}

static esp_err_t latestHandler(httpd_req_t* req) {
  cors(req);
  xSemaphoreTake(gCapMutex, portMAX_DELAY);
  esp_err_t r;
  if (!latestBuf || !C.lastSize) {
    xSemaphoreGive(gCapMutex);
    httpd_resp_set_status(req, "404 Not Found");
    return httpd_resp_send(req, "no image captured yet", HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=bb8_latest.jpg");
  r = httpd_resp_send(req, (const char*)latestBuf, C.lastSize);
  xSemaphoreGive(gCapMutex);
  return r;
}

static esp_err_t cmdHandler(httpd_req_t* req) {
  cors(req);
  char query[160], line[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "line", line, sizeof(line)) != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_send(req, "missing ?line=", HTTPD_RESP_USE_STRLEN);
  }
  // URL-decode (%2C -> ',' etc.)
  char dec[128];
  size_t j = 0;
  for (size_t i = 0; line[i] && j < sizeof(dec) - 1; i++) {
    if (line[i] == '%' && isxdigit((unsigned char)line[i + 1]) && isxdigit((unsigned char)line[i + 2])) {
      char hex[3] = {line[i + 1], line[i + 2], 0};
      dec[j++] = (char)strtol(hex, nullptr, 16);
      i += 2;
    } else dec[j++] = (line[i] == '+') ? ' ' : line[i];
  }
  dec[j] = 0;
  bool ok = handleCommandLine(dec, false);           // 'R' is only accepted over UDP
  if (!ok) httpd_resp_set_status(req, "409 Conflict");
  return httpd_resp_send(req, ok ? "OK" : "REJECTED", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t streamHandler(httpd_req_t* req) {
  if (!C.camOk) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_send(req, "camera not ready", HTTPD_RESP_USE_STRLEN);
  }
  esp_err_t res = httpd_resp_set_type(req, STREAM_TYPE);
  if (res != ESP_OK) return res;
  cors(req);
  C.streamClients++;
  Serial.println("[STREAM] viewer connected");
  const uint32_t minFrameMs = 1000 / STREAM_MAX_FPS;
  char part[64];
  while (true) {
    uint32_t t0 = millis();
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }
    size_t hlen = snprintf(part, sizeof(part), STREAM_PART, (unsigned)fb->len);
    res = httpd_resp_send_chunk(req, STREAM_SEP, strlen(STREAM_SEP));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, part, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;                         // viewer closed the page
    uint32_t dt = millis() - t0;
    if (dt < minFrameMs) delay(minFrameMs - dt);
  }
  C.streamClients--;
  Serial.println("[STREAM] viewer disconnected");
  return res;
}

void httpServersBegin() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = PORT_HTTP;
  cfg.ctrl_port = 32768;
  cfg.max_uri_handlers = 8;
  cfg.lru_purge_enable = true;
  struct { const char* uri; esp_err_t (*fn)(httpd_req_t*); } routes[] = {
    {"/", pageHandler}, {"/status", statusHandler}, {"/capture", captureHandler},
    {"/latest.jpg", latestHandler}, {"/cmd", cmdHandler},
  };
  if (httpd_start(&ctrlServer, &cfg) == ESP_OK) {
    for (auto& r : routes) {
      httpd_uri_t u = {};
      u.uri = r.uri; u.method = HTTP_GET; u.handler = r.fn;
      httpd_register_uri_handler(ctrlServer, &u);
    }
  }

  httpd_config_t scfg = HTTPD_DEFAULT_CONFIG();
  scfg.server_port = PORT_STREAM;
  scfg.ctrl_port = 32769;
  scfg.lru_purge_enable = true;                       // a new viewer replaces a dead one
  if (httpd_start(&streamServer, &scfg) == ESP_OK) {
    httpd_uri_t u = {};
    u.uri = "/stream"; u.method = HTTP_GET; u.handler = streamHandler;
    httpd_register_uri_handler(streamServer, &u);
  }

  Serial.printf("[HTTP] control http://%s/   stream http://%s:%d/stream\n",
                IP_CAMERA.toString().c_str(), IP_CAMERA.toString().c_str(), PORT_STREAM);
}
