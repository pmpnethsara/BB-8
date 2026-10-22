# Smart BB-8 — Package 3: BB8_REMOTE firmware guide

The physical Remote: two joysticks (with push switches), four push buttons, one OLED and two LEDs on an ESP32. Same structure as BB8_BODY and BB8_HEAD.

```
BB8_REMOTE_Project/
├── BB8_REMOTE/                ← open BB8_REMOTE.ino
│   ├── BB8_REMOTE.ino         main: setup() + non-blocking loop()
│   ├── config.h               ★ ALL settings: Wi-Fi/IP, pins, joystick feel, head mapping, timing
│   ├── state.h                shared live state "R"
│   ├── inputs.h               joystick ADC + calibration + dead-zone/expo, button debounce
│   ├── commands.h             DRV heartbeat, HEAD, events (ROT/WAVE/EYE/BLINK/CAP/STOP/RESET)
│   ├── network.h              Wi-Fi static IP, UDP send + telemetry receive
│   ├── telemetry_rx.h         reads Body / Head / Camera telemetry
│   ├── display.h              OLED status page + pop-ups
│   ├── leds.h                 LED1 power, LED2 connected
│   └── console.h              Serial Monitor test commands
├── tools/robot_sim.py         fake Body+Head+Camera on a laptop (test the Remote alone)
└── BB8_PROTOCOL.md            v1.1 contract for all packages
```

---

## 1. Controls

| Input | Action | Goes to |
|---|---|---|
| Joystick 1 | forward / reverse / left / right (proportional) — `DRV,y,x` 20×/s | Body |
| Joystick 1 push | short = rotate right `ROT,1`, hold 0.7 s = rotate left `ROT,-1` | Body |
| Joystick 2 | head angle → 3 MG995 targets `HEAD,a1,a2,a3` | Body |
| Joystick 2 push | head-wave gesture `WAVE` | Body |
| PB1 | next eye design `EYE,NEXT` | Head |
| PB2 | blink `BLINK` | Head |
| PB3 | capture `CAP` (OLED shows "PHOTO #n OK") | Camera |
| PB4 short | **STOP** immediately | Body |
| PB4 hold 1.5 s | **controlled full reset**: STOP → RESET Body, Head, Camera → Remote restarts | all |
| LED1 | power (fast blink = joystick wiring fault) | — |
| LED2 | ON = robot connected, slow blink = no Body answer, fast = FAILSAFE / battery critical, OFF = no Wi-Fi | — |

## 2. How the code is interconnected

```
Joysticks (ADC) ─┐
Push switches ───┼─► inputs.h ─► R.j1y/j1x, R.j2x/j2y, button edges
PB1..PB4 ────────┘                     │
                                       ▼
               commands.h ─► DRV (always, 20 Hz) / HEAD / ROT / WAVE / STOP ─UDP 4210─► Body
                          ─► EYE / BLINK / HB ──────────────────────────────UDP 4212─► Head
                          ─► CAP ───────────────────────────────────────────UDP 4213─► Camera
Body/Head/Camera telemetry ─UDP 4211─► telemetry_rx.h ─► R.body / R.headNode / R.cam
                                       ▼
                              display.h (OLED)   leds.h (LED1/LED2)
```

**Fail-safe chain.** The Remote sends `DRV` 20 times per second all the time — also `DRV,0,0` with the stick centred. This stream *is* the heartbeat. If the Remote is switched off, loses Wi-Fi, crashes or runs flat, the stream stops and the Body stops on its own within 0.5 s. After any STOP/fail-safe the Body only moves again after it has seen a centred stick (arming); the OLED shows **CENTRE JOYSTICK 1** in that case.

**Events.** One-shot actions are sent 3× (UDP may drop one). Head/Camera events carry one event id so the 3 copies count as one press.

**Joystick 2 mapping (proportional, default).** `S1 = 90 + x·10°` (lean), `S2 = 90 + y·30°` (tilt), `S3 = 90 + x·45°` (pan). Release the stick → head returns home. `HEAD_HOLD_MODE 1` = the stick moves the head and it stays where you leave it. HEAD is only sent while the stick is used (+3 packets to settle), so the Dashboard can still control the head when the Remote is idle.

---

## 3. Software setup
1. Arduino IDE 2.x, **esp32 by Espressif Systems 3.x**, board **ESP32 Dev Module**.
2. Library Manager: **Adafruit SSD1306** (+ Adafruit GFX Library, Adafruit BusIO).
3. Set `WIFI_SSID` / `WIFI_PASSWORD` in `config.h`. Keep `USE_STATIC_IP 1` — the Body accepts Remote commands only from 192.168.8.204.
4. Serial Monitor 115200, "Newline".

Compile-tested on ESP32 core 3.0.7 (default, Wi-Fi+OLED off with hold-mode head, test-target mode).

## 4. Wiring

| Device | Pin | ESP32 GPIO | Notes |
|---|---|---|---|
| Joystick 1 | VRy / VRx | 34 / 35 | ADC1 (works with Wi-Fi) |
| | SW | 32 | internal pull-up |
| | +5V / GND | **3.3 V** / GND | **3.3 V**, not 5 V — the ESP32 ADC max is ~3.1 V |
| Joystick 2 | VRx / VRy | 36 (VP) / 39 (VN) | ADC1 |
| | SW | 33 | internal pull-up |
| | +5V / GND | **3.3 V** / GND | |
| PB1 / PB2 / PB3 / PB4 | one leg | 25 / 26 / 27 / 14 | other leg → GND |
| OLED SSD1306 | SDA / SCL | 21 / 22 | VCC 3.3 V, GND, address 0x3C |
| LED1 (power) | anode via 330 Ω | 18 | cathode → GND |
| LED2 (connected) | anode via 330 Ω | 19 | cathode → GND |

Power: USB power bank, or 1S Li-ion (3.7 V) + 5 V boost into VIN, or 2×AA… anything that gives the ESP32 a stable 5 V on VIN (≈ 150 mA). Add a power switch in series with the battery.

## 5. Step-by-step coding & test procedure (guide Package 3 order)

For steps 1–6 set `ENABLE_WIFI 0` (the Remote runs alone; nothing is sent). Use `watch` (live line 5×/s) and `verbose` (prints every packet that *would* be sent).

### Step 1 — Joystick 1
Power up **without touching the sticks** (centre calibration). `watch`: raw values ~1800–2100 at rest; push forward → `J1 y` goes to +100, back → −100, right → `x` +100. Wrong sign → `J1_INVERT_Y` / `J1_INVERT_X`. Centre must read 0 (dead-zone). Not reaching ±100 → `range`, move both sticks in full circles for 5 s (saved to flash).
**Pass:** smooth 0 → ±100, steady 0 at rest, LED1 not blinking.

### Step 2 — Rotate switch
`verbose` on. Click J1 → `ROT,1` ×3, pop-up ROTATE RIGHT. Hold > 0.7 s → `ROT,-1`.
**Pass:** exactly one action per press, no double-triggers.

### Step 3 — Joystick 2
`watch` → `J2` −1..+1. `verbose` → `HEAD,a1,a2,a3` while used, home angles 3× after release. Check direction against the real head later (`HEAD_*_DEG` signs).

### Step 4 — Wave switch
Click J2 → `WAVE` ×3, pop-up HEAD WAVE.

### Step 5 — PB1–PB4
PB1 → `EYE,NEXT,<eid>` (3 copies, same eid). PB2 → `BLINK,<eid>`. PB3 → `CAP,<eid>` (pop-up CAPTURING… then CAMERA OFFLINE because nothing answers). PB4 short → `STOP`; hold → progress bar, then `RESET` to all three and the Remote restarts.

### Step 6 — OLED and LEDs
OLED shows the status page; every event shows a pop-up. LED1 solid. Disconnect a joystick VCC wire and reboot → JOYSTICK FAULT, LED1 blinks fast, DRV stays 0,0.

### Step 7 — Wi-Fi/IP
`ENABLE_WIFI 1`. `[NET] connected IP=192.168.8.204`. OLED line 2 shows BODY OFFLINE until the robot is on; LED2 blinks slowly.

### Step 8 — Command transmission (without the robot)
Laptop on BB8_NET with IP 192.168.8.100. `USE_TEST_TARGET 1`, upload, run `python tools/robot_sim.py`. Every input appears in the simulator; the OLED shows the fake Body/Head/Camera data; LED2 is ON; the simulator reports **DRV rate ≈ 20 Hz** and confirms repeat copies are ignored. Set `USE_TEST_TARGET 0` afterwards.

### Step 9 — Telemetry reception (with the robot)
Body + Head + Camera running. OLED: B/H/C boxes filled, battery, state IDLE → ACTIVE when you drive, motor values, head angles, eye name, CAM count. `tel` prints the three last JSON lines.

### Step 10 — Heartbeat / fail-safe test (wheels off the ground)
| Test | Expected |
|---|---|
| Drive forward, switch the Remote off | Body stops within ~0.5 s (`[SAFE] … LOST`) |
| Drive, pull the router power | Body stops; Remote LED2 off, OLED "WiFi connecting…" |
| Remote back on while the stick is pushed | **no movement**, OLED "CENTRE JOYSTICK 1"; centre → drives normally |
| PB4 short while driving | stops immediately |
| PB4 hold while driving | stops, then Body/Head/Camera/Remote restart |
| Dashboard drives, Remote idle | Remote does not interfere; touching Joystick 1 is refused until the Dashboard is idle 1.5 s |

**Pass:** every row as listed → Package 3 complete.

## 6. Troubleshooting
| Symptom | Fix |
|---|---|
| Joystick values jump / stuck at 4095 when Wi-Fi starts | axis on an ADC2 pin — use 32–39 only |
| Full deflection reached at half travel | joystick powered from 5 V → use 3.3 V |
| Robot creeps with the stick released | raise `JOY_DEADZONE_PCT`, re-run `calib` with sticks released |
| Body ignores the Remote, `R from wrong IP` | Remote IP is not 192.168.8.204 (static IP / DHCP reservation) |
| Robot stops every few seconds | weak Wi-Fi: Remote too far / power-save; check DRV rate with robot_sim |
| Buttons trigger twice | raise `BTN_DEBOUNCE_MS` to 50 |
| OLED blank | `[OLED] NOT FOUND` → SDA 21 / SCL 22 / 3.3 V; some modules are 0x3D |
