// =====================================================================
//  BB8_BODY - web_test_page.h
//  Small test page served at http://192.168.8.201/  (or bb8-body.local)
//  It is NOT the final Web Dashboard - it lets you test Body network
//  control (guide step 7) before the Remote is built. It uses exactly
//  the same /cmd + /telemetry endpoints the real Dashboard will use.
// =====================================================================
#pragma once

static const char WEB_TEST_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BB-8 Body test</title>
<style>
body{font-family:system-ui,sans-serif;margin:12px;background:#15181c;color:#eee}
button{font-size:17px;padding:14px 8px;margin:3px;border:0;border-radius:8px;background:#2d333b;color:#fff;touch-action:none}
#stop{background:#c62828;width:100%;font-weight:bold}
.g{display:grid;grid-template-columns:repeat(3,1fr);max-width:330px}
input{width:100%;max-width:330px}
pre{background:#000;padding:8px;font-size:12px;white-space:pre-wrap;max-width:600px}
</style></head><body>
<h3>BB-8 Body - network test page</h3>
<button id="stop" onclick="send('STOP')">STOP</button>
<div class="g">
 <span></span><button data-y="1" data-x="0">Forward</button><span></span>
 <button data-y="0" data-x="-1">Left</button><button onclick="send('ROT,1')">Rotate</button><button data-y="0" data-x="1">Right</button>
 <span></span><button data-y="-1" data-x="0">Reverse</button><span></span>
</div>
<p>Speed % <input type="range" id="spd" min="20" max="100" value="50"></p>
<p>Head servos S1 / S2 / S3</p>
<input type="range" class="hs" min="0" max="180" value="90">
<input type="range" class="hs" min="0" max="180" value="90">
<input type="range" class="hs" min="0" max="180" value="90">
<p><button onclick="send('WAVE')">Wave</button>
<button onclick="if(confirm('Restart the Body ESP32?'))send('RESET')">Reset</button></p>
<pre id="tel">waiting for telemetry...</pre>
<script>
let seq = Date.now() % 2000000000, timer = null;
function send(c){ seq++; return fetch('/cmd?line=' + encodeURIComponent('BB8,W,' + seq + ',' + c)).catch(()=>0); }
document.querySelectorAll('[data-y]').forEach(b=>{
  const go = e=>{ e.preventDefault(); const s = +document.getElementById('spd').value;
    const y = b.dataset.y * s, x = b.dataset.x * s;
    send('DRV,0,0');                                   // arm first
    clearInterval(timer); timer = setInterval(()=>send('DRV,' + y + ',' + x), 100); };
  const end = ()=>{ if(timer){ clearInterval(timer); timer = null; send('DRV,0,0'); } };
  b.addEventListener('pointerdown', go);
  ['pointerup','pointerleave','pointercancel'].forEach(ev=>b.addEventListener(ev, end));
});
let headBusy = false;
document.querySelectorAll('.hs').forEach(s=>s.oninput = ()=>{
  if(headBusy) return; headBusy = true;
  const v = [...document.querySelectorAll('.hs')].map(e=>e.value);
  send('HEAD,' + v.join(',')).finally(()=>headBusy = false);
});
setInterval(()=>fetch('/telemetry').then(r=>r.json())
  .then(j=>document.getElementById('tel').textContent = JSON.stringify(j, null, 1))
  .catch(()=>document.getElementById('tel').textContent = 'Body offline'), 500);
</script></body></html>)HTML";
