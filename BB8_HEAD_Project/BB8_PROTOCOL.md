# BB8_PROTOCOL — full robot interface (v1.1)

v1.1 = v1.0 (Body) + Head, Camera and Remote. **The Body part is unchanged**, so BB8_BODY 1.0.0 works as it is. Replace the old `BB8_PROTOCOL.md` in the Body folder with this file so all packages share one copy.

## 1. IP plan

| Node | IP | Listens on | Firmware |
|---|---|---|---|
| Body ESP32 | 192.168.8.201 | UDP 4210 (commands), HTTP 80 | BB8_BODY |
| Head ESP32 | 192.168.8.202 | UDP 4212 (eye commands), UDP 4211 (telemetry in), HTTP 80 | BB8_HEAD |
| ESP32-CAM | 192.168.8.203 | UDP 4213 (camera commands), HTTP 80, stream HTTP 81 | BB8_CAMERA |
| Remote ESP32 | 192.168.8.204 | UDP 4211 (telemetry in) | BB8_REMOTE |
| Router | 192.168.8.1 | — | — |

Wi-Fi SSID `BB8_NET`, password `bb8password` (change in every `config.h`). Static IP in firmware, or the same addresses as DHCP reservations.

## 2. Packet format (all command ports)

```
BB8,<src>,<seq>,<CMD>[,arg1,arg2,...]
```

| Field | Meaning |
|---|---|
| `src` | `R` physical Remote (accepted **only** from 192.168.8.204 over UDP), `W` Web Dashboard, `T` laptop test tool |
| `seq` | Unsigned counter, must increase with every packet from that source. A source silent for 2 s may restart from any value. |
| `eid` | (Head/Camera events) event id. The 3 UDP copies of one button press carry the SAME eid, so the node executes the event once. Missing or 0 = always execute (fine for HTTP). |

One-shot events are sent 3×, 30 ms apart, each copy with a new `seq`.

## 3. Commands

### Body — UDP 4210 / `http://192.168.8.201/cmd?line=`
| Command | Type | Sent by |
|---|---|---|
| `DRV,y,x` (−100..100) | continuous 20 Hz, also 0,0 | Joystick 1 / Dashboard |
| `HEAD,a1,a2,a3` (deg) | continuous ~15 Hz while used | Joystick 2 / Dashboard |
| `ROT,1` / `ROT,-1` | event | Joystick 1 push (short / long) |
| `WAVE` | event | Joystick 2 push |
| `STOP` | event, highest priority | PB4 short press / Dashboard |
| `RESET` | event | PB4 hold / Dashboard |
| `HB`, `PING` | — | optional |

### Head — UDP 4212 / `http://192.168.8.202/cmd?line=`
| Command | Meaning | Sent by |
|---|---|---|
| `EYE,<0-6>[,eid]` | select design 0 NORMAL, 1 HAPPY, 2 ANGRY, 3 SAD, 4 SLEEPY, 5 LOVE, 6 LENS | Dashboard |
| `EYE,NEXT[,eid]` / `EYE,PREV[,eid]` | next / previous design | PB1 |
| `BLINK[,eid]` | blink both eyes | PB2 / Dashboard |
| `HB` | Remote heartbeat (2 Hz) → Head reports `links.remote` | Remote |
| `RESET[,eid]` | restart Head ESP32 | PB4 hold / Dashboard |
| `PING` | no effect | tools |

### Camera — UDP 4213 / `http://192.168.8.203/cmd?line=`
| Command | Meaning | Sent by |
|---|---|---|
| `CAP[,eid]` | take a still image | PB3 / Dashboard |
| `FLASH,0|1` | white LED torch off / on | Dashboard |
| `RESET[,eid]` | restart ESP32-CAM | PB4 hold / Dashboard |
| `PING` | no effect | tools |

Camera HTTP: `/` test page, `/capture` (capture + status JSON), `/latest.jpg`, `/status`, live stream `http://192.168.8.203:81/stream` (one viewer at a time).

## 4. Telemetry (UDP 4211, JSON, field `node` tells the sender)

| From | To | Rate | Also on HTTP |
|---|---|---|---|
| Body `{"node":"body",...}` | Remote + Head | 5 Hz | `192.168.8.201/telemetry` |
| Head `{"node":"head",...}` | Remote | 4 Hz | `192.168.8.202/telemetry` |
| Camera `{"node":"cam",...}` | Remote + Head | 1 Hz + right after each capture | `192.168.8.203/status` |

Body fields: see v1.0 (state, faults, owner, armed, cmd, motor, imu, batt, mag, servo, wave, reset).

Head:
```json
{"node":"head","fw":"HEAD-1.0.0","seq":51,"up":13,"eye":0,"eyeName":"NORMAL","eyes":7,
 "blink":0,"blinks":4,"oled":[1,1],"look":[3,-1],
 "links":{"body":1,"cam":1,"remote":1},"bodyState":"IDLE","cap":2,"capOk":1,"rssi":-55,"reset":0}
```
`links` gives the Dashboard the online state of Body, Camera and Remote in one request.

Camera:
```json
{"node":"cam","fw":"CAM-1.0.0","seq":20,"up":20,"camOk":1,"cap":2,"capOk":1,"capAge":0,
 "size":28512,"saved":"/bb8_00002.jpg","sd":1,"stream":1,"flash":0,"res":"VGA","psram":1,"rssi":-60,"reset":0}
```
`capAge` = seconds since the last capture attempt (−1 = never).

## 5. Safety rules (Body, unchanged)
Sequence check · one owner at a time (1.5 s idle hand-over) · arming with `DRV,0,0` · 500 ms command-loss STOP · STOP always first.
The Remote keeps these working by sending `DRV` 20×/s **always**. Remote off / Wi-Fi lost / Remote frozen → Body stops within 0.5 s.

## 6. Timeouts
| Name | Value |
|---|---|
| Body command timeout | 500 ms |
| Ownership idle | 1500 ms |
| Sequence session | 2000 ms |
| Remote "connected" (LED2) | Body telemetry < 1 s |
| Head: Body/Remote online | < 1.5 s |
| Camera online | < 3 s |
| Capture answer timeout (Remote) | 3 s |
