# Smart BB-8 — Package 1: BB8_BODY firmware guide

This folder contains the complete Body ESP32 firmware plus the tools to test it step by step, following the order in the project guide (Wi-Fi/IP → motors → movement → MPU6050 → voltage → magnet → servos → telemetry → Remote/Web → communication-loss STOP).

```
BB8_BODY_Project/
├── BB8_BODY/                 ← open BB8_BODY.ino in Arduino IDE (all files open as tabs)
│   ├── BB8_BODY.ino          main: setup() + non-blocking loop()
│   ├── config.h              ★ ALL settings: features, Wi-Fi/IP, pins, limits, timing
│   ├── state.h               shared live state "S" that connects every module
│   ├── motors.h              L298N drive + steering, PWM, ramp, brake, rotate, tilt limit
│   ├── imu.h                 MPU6050 roll/pitch/yaw (complementary filter)
│   ├── battery.h             voltage sensor, %, LOW / CRITICAL
│   ├── magnet.h              reed switch enable input (debounced)
│   ├── servos.h              3× MG995 safe limits, smooth motion, wave gesture
│   ├── protocol.h            command parser, ownership, arming, fail-safe, state machine
│   ├── telemetry.h           telemetry JSON
│   ├── network.h             Wi-Fi static IP, UDP commands/telemetry, HTTP for Dashboard
│   ├── web_test_page.h       small built-in browser test page
│   ├── console.h             Serial Monitor test commands
│   └── system.h              loop scheduler + status LED
├── tools/udp_remote_sim.py   laptop "fake Remote" for network & fail-safe tests
└── BB8_PROTOCOL.md           the contract the Remote/Head/Dashboard must follow
```

---

## 1. How the code is interconnected

Every module talks through one shared structure, `S` (in `state.h`). Nothing calls the motors directly from the network — commands always pass through the safety logic first.

```
INPUTS                          DECISION                       OUTPUTS
Remote  ─UDP 4210─┐
Dashboard ─HTTP──┼─► network.h ─► protocol.h ─► S.cmdY/cmdX ─► motors.h ─► L298N ─► M1/M2/M3
Laptop  ─UDP────┘                 (seq check,   S.state        servos.h ─► MG995 ×3
Serial Monitor ─────► console.h ─►  owner, arm,  S.owner
                                    timeout)
MPU6050 ─► imu.h ────► S.roll/pitch ─────────────► motors.h (tilt limit) + telemetry
Voltage ─► battery.h ─► S.battLow/crit ─► protocol.h (DISABLED) + motors.h (speed limit)
Reed sw ─► magnet.h ──► S.magnet ───────► protocol.h (DISABLED when absent)

S (everything) ─► telemetry.h ─► network.h ─UDP 4211─► Remote + Head   /  HTTP ─► Dashboard
```

The `loop()` never uses `delay()`. Each module runs on its own timer (IMU 100 Hz, motors 100 Hz, servos 50 Hz, sensors 20 Hz, telemetry 5 Hz), so Wi-Fi stays responsive and the fail-safe always runs.

**State machine** (in `protocol.h`, checked top to bottom every loop):

| Condition | State | Motors |
|---|---|---|
| Wi-Fi down | FAILSAFE | ramp to stop |
| Magnet absent (if required) or battery CRITICAL | DISABLED | stopped |
| No controller and commands were lost while moving | FAILSAFE | stopped |
| No controller | IDLE | stopped |
| A controller owns the robot | ACTIVE | follow commands |

---

## 2. Software setup

1. Install **Arduino IDE 2.x**.
2. File → Preferences → Additional boards manager URLs:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Boards Manager → install **esp32 by Espressif Systems, version 3.x** (the code uses the 3.x `ledcAttach()` API and stops with a clear error on 2.x).
4. Board: **ESP32 Dev Module**, Upload speed 921600, Serial Monitor **115200 baud, "Newline"**.
5. No extra libraries are needed.
6. Open `BB8_BODY/BB8_BODY.ino`. Edit `WIFI_SSID` / `WIFI_PASSWORD` and the IP plan in `config.h`.

The firmware was compile-tested on ESP32 core 3.0.7 in three configurations (shared drive channel, separate Motor 2 driver, all modules disabled).

---

## 3. Wiring (Body)

| Device | Pin | ESP32 GPIO | Notes |
|---|---|---|---|
| L298N | ENA | 25 | **remove ENA jumper** |
| | IN1 / IN2 | 26 / 27 | channel A = Motor 1 (+ Motor 2 in parallel) |
| | ENB | 13 | **remove ENB jumper** |
| | IN3 / IN4 | 33 / 32 | channel B = Motor 3 steering |
| 2nd driver (only if Motor 2 independent) | EN / IN5 / IN6 | 16 / 17 / 4 | set `DRIVE_SHARED_CHANNEL 0` |
| MPU6050 | SDA / SCL | 21 / 22 | VCC 3.3 V, AD0 → GND (0x68) |
| Voltage sensor | S | 34 | ADC1 only; "−" to GND |
| Reed switch | one side | 35 | other side → GND, **10 kΩ GPIO35 → 3.3 V** |
| MG995 #1 / #2 / #3 | signal | 18 / 19 / 23 | power from separate 5–6 V supply |
| Status LED | — | 2 | on-board LED |

**Power rules**

- Use a separate buck converter (12 V → 5 V, ≥1 A) for the ESP32 VIN. Do not rely on the L298N's 5 V regulator.
- MG995 servos: separate 5–6 V BEC/buck rated **≥ 5 A** (each MG995 can draw ~2.5 A at stall). Never from the ESP32 pins.
- **All grounds connected**: battery −, L298N GND, servo supply −, ESP32 GND.
- Reed switch carries only the 3.3 V signal current. If it must switch main power, let it (or the ESP32) drive a MOSFET/relay.
- **L298N limit:** ~2 A per channel and ~2 V voltage drop. If Motor 1 + Motor 2 in parallel draw more than 2 A together at stall, use a second channel/driver (`DRIVE_SHARED_CHANNEL 0`) or a stronger driver (e.g. BTS7960). Measure stall current before final wiring.

---

## 4. Step-by-step coding & test procedure

Each step: change the feature switches in `config.h`, upload, run the listed tests, and do not move on until the pass check is met. Keep the robot on a stand with wheels off the ground until Step 9.

### Step 1 — Wi-Fi / IP
**config.h:** `ENABLE_WIFI 1`, all others `0`.

1. Upload, open Serial Monitor. You should see `[NET] connected IP=192.168.8.201`.
2. From a laptop on `BB8_NET`: `ping 192.168.8.201`.
3. Open `http://192.168.8.201/` (or `http://bb8-body.local/`) — the test page shows telemetry.
4. Turn the router off and on: the console shows `Wi-Fi LOST` and then reconnects by itself.

**Pass:** fixed IP every boot, page loads, automatic reconnect. LED: fast blink = no Wi-Fi, slow blink = IDLE.

### Step 2 — Motor tests
**config.h:** add `ENABLE_MOTORS 1`. Set `MAG_REQUIRED_TO_DRIVE 0` for now (magnet not yet enabled).

Serial Monitor:
```
motor 1 120        → drive forward 1.5 s, then auto-stop
motor 1 -120       → drive reverse
motor 3 150        → steering one way
motor 3 -150       → steering other way
motor 1 200 3000   → 3 s run
stop               → active brake
```
- If a direction is wrong, flip `MOTOR1_INVERT` / `MOTOR3_INVERT` (do not rewire).
- Find the minimum duty where the motor really turns (not just hums) and put it in `MOTOR_MIN_DUTY`.

**Pass:** each motor turns both ways, stops on time, `stop` brakes, nothing gets hot.

### Step 3 — Movement logic (network control)
Same config. On the laptop:
```
python tools/udp_remote_sim.py drive 40 0 3     # forward
python tools/udp_remote_sim.py drive -40 0 2    # reverse
python tools/udp_remote_sim.py drive 0 60 2     # steer
python tools/udp_remote_sim.py rotate 1
python tools/udp_remote_sim.py stop
```
Or press and hold the buttons on the web test page.

Check the safety logic:
- Motors ramp up/down smoothly (`DRIVE_RAMP_PER_S`).
- Sending `DRV,40,0` without a previous `DRV,0,0` does **not** move the robot (arming):
  `cmd BB8,T,1,DRV,40,0` in the console → accepted but no movement.
- While the laptop drives, the web page cannot take over until the laptop is idle for 1.5 s (ownership).

**Pass:** all directions, rotate and STOP work from the network.

### Step 4 — MPU6050
**config.h:** `ENABLE_IMU 1`. Keep the robot still while it boots (gyro calibration ~1 s).
```
imu        → roll / pitch / yaw
watch      → live line every 0.5 s (type watch again to stop)
calib      → re-calibrate (robot still)
```
Tilt the body by hand: roll and pitch follow smoothly and return to ~0 when level. Yaw slowly drifts — that is normal for an MPU6050 without a magnetometer.

Tilt limit: steer to one side and tilt the body the same way past 25°. Steering must stop. If it blocks the wrong side, set `STEER_TILT_SIGN -1`.

**Pass:** stable angles, `imu.ok=1`, no `[IMU] lost` messages while motors run (if there are, shorten the I2C wires / twist SDA+SCL with GND).

### Step 5 — Voltage sensor
**config.h:** `ENABLE_BATTERY 1`. Set `BATT_FULL_V`, `BATT_EMPTY_V`, `BATT_LOW_V`, `BATT_CRIT_V` for your actual battery (defaults are for 3S Li-ion 12.6 V).

Calibration:
1. Measure the battery with a multimeter, e.g. 12.10 V.
2. Type `bat`, e.g. it shows 11.80 V.
3. Set `BATT_CAL = 12.10 / 11.80 = 1.025`, upload, check again (±0.1 V).

On USB only (no battery) it reports `present=0` and no warnings.

**Pass:** voltage within 0.1 V, % sensible, LOW lowers speed to 60 %, CRITICAL (held 3 s) disables driving until reboot. Test with a bench power supply if available.

### Step 6 — Magnetic input
**config.h:** `ENABLE_MAGNET 1`, set `MAG_REQUIRED_TO_DRIVE 1` again.

`mag` → bring the magnet close / remove it. Console shows `[MAG] magnet PRESENT` / `REMOVED`. Without the magnet the state is `DISABLED` and drive commands do not move the motors. If it works the opposite way, change `REED_ACTIVE_LOW`.

**Pass:** clean switching (no flicker), drive blocked when absent.

### Step 7 — Servo 1/2/3 tests
**config.h:** `ENABLE_SERVOS 1`. **Remove the servo horns / disconnect the head linkage the first time.**

1. Boot: all servos go to 90°.
2. Find safe limits on the real mechanism with the limits temporarily wide (0–180):
   `servo 1 80`, `servo 1 70` … step 5° until just before the mechanism touches. Note min/max for each servo.
3. Write them in `SERVO_MIN_DEG` / `SERVO_MAX_DEG` (and the neutral pose in `SERVO_HOME_DEG`), upload.
4. Test: `servo 2 0` → moves only to its safe minimum. `home`. `wave`.
5. Adjust the wave frames in `servos.h` (`WAVE_SEQ`) to make a nice gesture for your head design.

**Pass:** smooth movement, never past the limits, no brown-out reset of the ESP32 while all three move (if it resets, the servo supply is too weak or not separate).

### Step 8 — Combined telemetry
All modules enabled. Commands `tel`, `status`, and on the laptop:
```
python tools/udp_remote_sim.py listen
```
Check every field changes when you act on the robot: tilt → `imu`, magnet → `mag`, servo → `servo`, drive → `motor`, battery → `batt`. Field list: `BB8_PROTOCOL.md`.

**Pass:** all values correct and updating 5×/s.

### Step 9 — Remote / Web command test
1. Web test page: drive, steer, rotate, head sliders, wave, STOP, reset.
2. Laptop tool: same commands.
3. When Package 3 is ready: the real Remote must send exactly the format in `BB8_PROTOCOL.md` from 192.168.8.204 (source `R`).
4. Ownership test: drive with the Remote; press Forward on the web page — rejected. Leave the Remote centred 2 s; web page now drives. STOP from either side always works.

**Pass:** both controllers work, never fight, STOP always wins.

### Step 10 — Communication-loss STOP test (most important)
Wheels off the ground.

| Test | Expected |
|---|---|
| `python tools/udp_remote_sim.py failsafe` | motors ramp to 0 within ~0.5 s, state `FAILSAFE`, console `[SAFE] ... LOST while moving` |
| Hold Forward on the web page, then close the browser tab | stops within ~0.5 s |
| Drive with the Remote, then switch the Remote off | stops within ~0.5 s |
| Drive, then switch the router off | `Wi-Fi LOST`, stops |
| After any of the above, push the joystick forward again | **no movement** until joystick is centred once (arming) |
| PB4 / RESET while driving | stops first, then Body restarts |

**Pass:** every row behaves as listed. Only then mount the Body permanently (project step 12 — save this firmware version, the IP plan and `BB8_PROTOCOL.md`).

---

## 5. Notes for the Remote and Dashboard developers

- Send `DRV,y,x` at 20 Hz **all the time**, including `0,0` when centred. That keeps ownership and the fail-safe alive and arms the Body.
- Joystick 1 dead-zone and calibration belong in the Remote; the Body receives clean −100..100 values.
- Joystick 2 → convert to three angles in the Remote and send `HEAD,a1,a2,a3`. The Body clamps to safe limits anyway.
- Joystick push switches → `ROT,1` and `WAVE` as events (send 3×, new seq each).
- Remote LED2 = Body telemetry received in the last 1 s. Remote OLED reads `state`, `batt`, `owner`, `wave`, `imu` from the telemetry JSON.
- Dashboard: use `/cmd?line=...` and poll `/telemetry` (2–5 Hz). Repeat DRV every 100 ms while a control is held.

---

## 6. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Compile error "needs ESP32 Arduino core 3.x" | update the esp32 board package to 3.x |
| ESP32 resets when motors/servos start | brown-out: separate supplies, thicker wires, common ground, 470–1000 µF capacitor on servo supply |
| Motor only hums at low speed | raise `MOTOR_MIN_DUTY` |
| `R from wrong IP` rejections (type `verbose`) | Remote is not at 192.168.8.204 |
| `old/duplicate seq` rejections | sender is not increasing `seq` |
| Battery reading jumps when motors run | normal sag; check divider ground goes to the ESP32 GND close to the pin |
| Upload fails with L298N connected | GPIO 12/15 are not used, so check that the board is in boot mode (hold BOOT) |
| Robot stops every few seconds | controller sends too slowly (< 2 Hz) → send DRV at 20 Hz |
