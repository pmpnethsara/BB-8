# Smart BB-8 — Package 2: BB8_HEAD + BB8_CAMERA firmware guide

Package 2 has **two boards, two sketches**: the Head ESP32 (two OLED eyes) and the ESP32-CAM (live camera + capture). They follow the same structure as BB8_BODY: one `config.h` with every setting, one shared state structure, non-blocking modules, and a Serial console for bench tests.

```
BB8_HEAD_Project/
├── BB8_HEAD/                  ← Head ESP32: open BB8_HEAD.ino
│   ├── BB8_HEAD.ino           main: setup() + non-blocking loop()
│   ├── config.h               ★ ALL settings: features, Wi-Fi/IP, OLED pins, eye behaviour
│   ├── state.h                shared live state "H" + eye design list
│   ├── eyes.h                 animation: blink, design change, look/follow head, auto-blink
│   ├── oled.h                 two SSD1306 displays + the 7 eye drawings
│   ├── protocol.h             EYE / BLINK / HB / RESET parser, event-id, Body/Cam telemetry reader
│   ├── telemetry.h            Head telemetry JSON
│   ├── network.h              Wi-Fi static IP, UDP 4212 + 4211, HTTP for the Dashboard
│   ├── web_test_page.h        browser test page (eyes + camera)
│   ├── console.h              Serial Monitor test commands
│   └── system.h               scheduler, status LED, controlled reset
├── BB8_CAMERA/                ← ESP32-CAM: open BB8_CAMERA.ino
│   ├── BB8_CAMERA.ino
│   ├── config.h               ★ Wi-Fi/IP, image size/quality, SD, flash
│   ├── state.h                shared state "C"
│   ├── camera.h               OV2640 init, THE capture function, SD saving
│   ├── protocol.h             CAP / FLASH / RESET parser
│   ├── telemetry.h            camera status JSON
│   ├── http_server.h          :80 control + :81 live MJPEG stream
│   ├── network.h              Wi-Fi, UDP 4213, status to Remote + Head
│   └── console.h
├── tools/head_cam_test.py     laptop stand-in for PB1/PB2/PB3/PB4
├── eye_designs_preview.png    what the 7 designs, blink and look look like
└── BB8_PROTOCOL.md            v1.1 contract for all packages
```

---

## 1. How the code is interconnected

**Head ESP32**

```
Remote PB1/PB2 ─UDP 4212─┐
Dashboard ─────HTTP /cmd─┼─► protocol.h ──► eyesRequestMode() / eyesRequestBlink()
Serial console ──────────┘   (seq, eid)              │
Body telemetry ─UDP 4211─► protocol.h ─► H.bodyServo, H.bodyBattLow ──┐
Cam status ─────UDP 4211─► protocol.h ─► H.camCaptures (shutter blink)┤
                                                     ▼                │
                              eyes.h ─► H.eyeMode, H.lid, H.lookX/Y ◄─┘
                                                     ▼
                              oled.h ─► OLED 1 (left)  +  OLED 2 (right)
H (everything) ─► telemetry.h ─► UDP 4211 ─► Remote   /   HTTP ─► Dashboard
```

- Both eyes are drawn from the same state in the same frame → always synchronized.
- **Design change** = eyes close → design swapped while closed → eyes open (blink and return to the selected design).
- Repeated UDP copies of one button press carry one **event id**, so PB1 moves exactly one design.
- Optional behaviours (all in `config.h`): natural auto-blink, small idle eye movements, eyes follow the head servo angles (pan/tilt from Body telemetry), shutter blink on each camera capture, droopy eyelids when the Body battery is LOW.

**ESP32-CAM**

```
Remote PB3 ─UDP 4213 CAP─┐
Dashboard ─HTTP /capture─┼─► cameraCapture() ─► latest JPEG (PSRAM) ─► /latest.jpg
console "cap" ───────────┘                     ─► microSD /bb8_00001.jpg …
                                               ─► status now ─UDP 4211─► Remote (OLED) + Head (blink)
OV2640 ─► :81/stream ─► Dashboard live view
```

---

## 2. Software setup

1. Arduino IDE 2.x, Boards Manager **esp32 by Espressif Systems 3.x** (same as the Body).
2. Library Manager: **Adafruit SSD1306** (accept the dependencies **Adafruit GFX Library** and **Adafruit BusIO**). The camera needs no extra library.
3. Head: board **ESP32 Dev Module**, 115200 baud, "Newline".
4. Camera: board **AI Thinker ESP32-CAM**, Partition Scheme **Huge APP (3MB No OTA)**. Upload with an ESP32-CAM-MB board or a 3.3 V/5 V USB-serial adapter (IO0 to GND while uploading, then remove and press RST).
5. Edit `WIFI_SSID` / `WIFI_PASSWORD` in **both** `config.h` files.

Compile-tested on ESP32 core 3.0.7: Head (default, same-bus OLEDs, Wi-Fi off, all features off) and Camera (default, SD off + flash on, Wi-Fi off).

---

## 3. Wiring

### Head ESP32
| Device | Pin | ESP32 GPIO | Notes |
|---|---|---|---|
| OLED 1 (left eye) | SDA / SCL | 21 / 22 | VCC 3.3 V, GND. I2C bus 0 |
| OLED 2 (right eye) | SDA / SCL | 32 / 33 | VCC 3.3 V, GND. I2C bus 1 |
| Status LED | — | 2 | on-board |

Two buses = both OLEDs can keep address 0x3C (no soldering). If you prefer one bus (`OLED_SAME_BUS 1`) one module must be changed to 0x3D. "Left" = the eye on **your** left when you face the robot.

### ESP32-CAM
All camera pins are fixed on the board. Only power: **5 V and GND** (≥ 500 mA, stable — brown-outs are the #1 ESP32-CAM problem). microSD optional (FAT32).

### Power
Head ESP32 + 2 OLEDs ≈ 150 mA, ESP32-CAM up to ≈ 300 mA peaks (flash LED more). Feed both from a 5 V buck converter on the robot battery, **separate from the MG995 servo supply**, all grounds common.

---

## 4. Step-by-step coding & test procedure (guide Package 2 order)

### Step 1 — Head Wi-Fi
`config.h`: `ENABLE_WIFI 1`, `ENABLE_OLED_LEFT 0`, `ENABLE_OLED_RIGHT 0`. Upload. Serial: `[NET] connected IP=192.168.8.202`. Laptop: `ping 192.168.8.202`, open `http://192.168.8.202/`.
**Pass:** fixed IP, page loads, reconnects after router restart. LED: fast blink = no Wi-Fi.

### Step 2 — OLED 1
`ENABLE_OLED_LEFT 1`. Console: `i2c` → bus 0 shows `0x3C`. Boot message `[OLED] LEFT eye (OLED 1) at 0x3C: OK`. `test` → shows LEFT.
**Pass:** eye visible, no flicker. Garbage/flicker → `OLED_I2C_HZ 400000`, shorter wires.

### Step 3 — OLED 2
`ENABLE_OLED_RIGHT 1`. `i2c` → bus 1 shows `0x3C`. `test` → LEFT on your left, RIGHT on your right (swap the connectors if not). Upside-down → `OLED_FLIP_L/R 1`.
**Pass:** both displays work together; unplug one → `[OLED] OLED 2 LOST`, plug back → `back online`.

### Step 4 — Eye graphics & eye mode
`list`, then `eye 0` … `eye 6`, `next`, `prev`. Compare with `eye_designs_preview.png`. ANGRY must look like "\ /" and SAD like "/ \" seen from the front — if mirrored, swap the OLEDs.
**Pass:** all 7 designs on both eyes, change happens through a quick blink.

### Step 5 — Blink
`blink` several times; watch auto-blink every 2.5–7 s (`AUTO_BLINK`). Both eyes must close and open at exactly the same time.
**Pass:** synchronized blink, returns to the selected design.

### Step 6 — ESP32-CAM Wi-Fi
Upload BB8_CAMERA. Serial (via the adapter): `[CAM] OV2640 ready`, `[NET] connected IP=192.168.8.203`. Red LED on the back blinks while Wi-Fi is down.
**Pass:** `ping 192.168.8.203`, `http://192.168.8.203/` opens.

### Step 7 — Still image
Console `cap` or `http://192.168.8.203/capture` → JSON `"capOk":1`. Open `/latest.jpg`. With a microSD card: `"sd":1` and files `/bb8_00001.jpg` … (numbering continues after reboot). Upside-down picture → `CAM_VFLIP 1` / `CAM_HMIRROR 1`.
**Pass:** sharp image every time; 20 captures in a row without a failure.

### Step 8 — Live stream
`http://192.168.8.203:81/stream` in a browser. Leave it running 10 minutes. Capture while streaming must still work.
**Pass:** stable stream ≈ 10–15 fps at VGA; no reboot. Choppy → `CAM_FRAME_SIZE FRAMESIZE_QVGA` or move the router closer / use the external-antenna version.

### Step 9 — Network eye/camera commands
Laptop:
```
python tools/head_cam_test.py listen
python tools/head_cam_test.py next        # PB1
python tools/head_cam_test.py blink       # PB2
python tools/head_cam_test.py cap         # PB3 -> eyes shutter-blink
python tools/head_cam_test.py repeat      # 3 copies, one eid -> ONE design step
python tools/head_cam_test.py demo
```
And the Head test page `http://192.168.8.202/` (eye buttons, blink, live camera, capture).
**Pass:** every command works once per press; camera capture makes the eyes blink.

### Step 10 — Head telemetry
`tel` on the Head, or `listen` above. With the Body running: `links.body = 1`, `bodyState` follows the Body, head servo movement moves the eyes (`FOLLOW_HEAD_SERVOS`; wrong direction → `HEAD_PAN_SIGN -1`). Camera on: `links.cam = 1`, `cap` counts.
**Pass:** all fields correct, Head LED solid when the Body is heard.

### Step 11 — Reset
`python tools/head_cam_test.py reset` → Head eyes go dark and come back, camera restarts, both back on the network within ~5 s.

---

## 5. Notes for the Remote and Dashboard developers
- PB1 → `EYE,NEXT,<eid>` and PB2 → `BLINK,<eid>` to 192.168.8.202:4212; PB3 → `CAP,<eid>` to 192.168.8.203:4213. Send 3× with the same eid.
- Remote sends `HB` to the Head 2×/s so the Head can report `links.remote`.
- Dashboard: `<img src="http://192.168.8.203:81/stream">`, `fetch('http://192.168.8.203/capture')`, eye control via `http://192.168.8.202/cmd?line=BB8,W,<seq>,EYE,<n>`, all connection states via `http://192.168.8.202/telemetry` → `links`. CORS is open on both.

## 6. Troubleshooting
| Symptom | Fix |
|---|---|
| `[OLED] … NOT FOUND` | `i2c` scan; SDA/SCL swapped? VCC 3.3 V? Module address 0x3D → set `OLED_x_ADDR` |
| Both eyes show the same "LEFT" | both on the same bus — check OLED 2 is on GPIO 32/33 |
| ESP32-CAM `init FAILED 0x105/0x20004` | camera ribbon not fully latched; re-seat it |
| ESP32-CAM keeps rebooting ("Brownout") | weak 5 V supply or thin wires; use ≥ 1 A, short wires, 470 µF capacitor at 5V/GND |
| Stream stops when the Dashboard opens a 2nd viewer | normal: one stream viewer at a time |
| `R from wrong IP` (type `verbose`) | Remote is not at 192.168.8.204 |
| PB1 jumps several designs | sender is not reusing the same eid for its 3 copies |
