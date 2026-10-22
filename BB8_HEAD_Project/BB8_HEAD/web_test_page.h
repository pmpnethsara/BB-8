// =====================================================================
//  BB8_HEAD - web_test_page.h
//  Small test page at http://192.168.8.202/  (or bb8-head.local)
//  It is NOT the final Web Dashboard - it tests the Head + Camera
//  network control (eye design, blink, live camera, capture) using
//  exactly the endpoints the real Dashboard will use.
//  %CAM% is replaced with the camera IP from config.h.
// =====================================================================
#pragma once

static const char WEB_TEST_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BB-8 Head test</title>
<style>
body{font-family:system-ui,sans-serif;margin:12px;background:#15181c;color:#eee}
button{font-size:16px;padding:12px 10px;margin:3px;border:0;border-radius:8px;background:#2d333b;color:#fff}
button.on{background:#e67e22}
img{width:100%;max-width:480px;border-radius:8px;background:#000;min-height:120px}
pre{background:#000;padding:8px;font-size:12px;white-space:pre-wrap;max-width:600px}
</style></head><body>
<h3>BB-8 Head - network test page</h3>
<p>Eye design:</p><div id="eyes"></div>
<p><button onclick="send('BLINK')">Blink</button>
<button onclick="if(confirm('Restart the Head ESP32?'))send('RESET')">Reset Head</button></p>
<h4>Camera</h4>
<img id="cam" alt="camera stream (ESP32-CAM offline?)">
<p><button onclick="cap()">Capture</button> <span id="capst"></span>
<a id="last" href="#" target="_blank" style="color:#8cf">latest image</a></p>
<pre id="tel">waiting for telemetry...</pre>
<script>
const CAM = 'http://%CAM%';
const NAMES = ['NORMAL','HAPPY','ANGRY','SAD','SLEEPY','LOVE','LENS'];
let seq = Date.now() % 2000000000;
function send(c){ seq++; return fetch('/cmd?line=' + encodeURIComponent('BB8,W,' + seq + ',' + c)).catch(()=>0); }
const box = document.getElementById('eyes');
NAMES.forEach((n,i)=>{ const b = document.createElement('button'); b.textContent = n; b.id = 'e' + i;
  b.onclick = ()=>send('EYE,' + i); box.appendChild(b); });
document.getElementById('cam').src = CAM + ':81/stream';
document.getElementById('last').href = CAM + '/latest.jpg';
function cap(){ document.getElementById('capst').textContent = 'capturing...';
  fetch(CAM + '/capture').then(r=>r.json()).then(j=>document.getElementById('capst').textContent =
    j.capOk ? ('OK #' + j.cap + (j.saved ? ' saved ' + j.saved : '')) : 'FAILED')
  .catch(()=>document.getElementById('capst').textContent = 'camera offline'); }
setInterval(()=>fetch('/telemetry').then(r=>r.json()).then(j=>{
  document.getElementById('tel').textContent = JSON.stringify(j, null, 1);
  NAMES.forEach((n,i)=>document.getElementById('e' + i).className = (i == j.eye ? 'on' : ''));
}).catch(()=>document.getElementById('tel').textContent = 'Head offline'), 500);
</script></body></html>)HTML";
