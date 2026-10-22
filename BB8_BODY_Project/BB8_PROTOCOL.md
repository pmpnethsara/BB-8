# BB8_PROTOCOL — Body interface (v1.0)

This file is the contract between **BB8_BODY** and the other packages. The Remote, Head and Web Dashboard firmware must follow it exactly. If anything here changes, update the version and all nodes together.

## 1. IP plan

| Node | IP | Listens on |
|---|---|---|
| Body ESP32 | 192.168.8.201 | UDP 4210 (commands), HTTP 80 |
| Head ESP32 | 192.168.8.202 | UDP 4211 (Body telemetry) |
| ESP32-CAM | 192.168.8.203 | (defined in Package 2) |
| Remote ESP32 | 192.168.8.204 | UDP 4211 (Body telemetry) |
| Router / gateway | 192.168.8.1 | — |

Wi-Fi SSID `BB8_NET`. Static IP in firmware, or the same addresses as DHCP reservations in the router.

## 2. Command format (to Body, UDP port 4210)

One command per packet, plain ASCII, comma separated:

```
BB8,<src>,<seq>,<CMD>[,arg1,arg2,...]
```

| Field | Meaning |
|---|---|
| `src` | `R` = physical Remote (only accepted from 192.168.8.204), `W` = Web Dashboard, `T` = test tool |
| `seq` | Unsigned 32-bit counter. Must increase by at least 1 for every packet from that source. A source that has been silent for 2 s may restart from any value. |

| Command | Type | Arguments | Sent by |
|---|---|---|---|
| `DRV,y,x` | continuous (send at 20 Hz, also when neutral) | y = −100..100 forward/reverse, x = −100..100 left/right | Joystick 1 / Dashboard joystick |
| `HEAD,a1,a2,a3` | continuous (≈10–20 Hz while used) | target angles in degrees for Servo 1/2/3 (Body clamps to safe limits) | Joystick 2 / Dashboard |
| `ROT,dir` | event | 1 or −1 | Joystick 1 push switch |
| `WAVE` | event | — | Joystick 2 push switch |
| `STOP` | event, **highest priority** | — | Dashboard STOP (Remote may map one too) |
| `RESET` | event | — | PB4 / Dashboard reset |
| `HB` | heartbeat | — | optional; refreshes ownership only |
| `PING` | no effect | — | test tools, requests telemetry |

Events should be sent 3 times, 30 ms apart, each with a new `seq` (UDP may lose one packet). `WAVE` and `ROT` repeats are harmless; `RESET` repeats are harmless.

Not handled by the Body: PB1 eye mode, PB2 blink (→ Head), PB3 capture (→ ESP32-CAM). The Remote sends those directly to those nodes.

## 3. Safety rules (implemented in the Body)

1. **Sequence:** older/duplicate `seq` is dropped.
2. **Ownership:** one controller at a time. The current owner keeps control while it gives non-neutral input. When it has been idle for 1.5 s, another controller takes over by sending non-neutral input.
3. **Arming:** after STOP, fail-safe, or ownership change the Body ignores movement until it receives `DRV,0,0`. The Remote satisfies this naturally (joystick centred).
4. **Fail-safe:** no valid command from the owner for 500 ms → motors ramp to stop, state `FAILSAFE`.
5. **STOP** from any source is executed immediately (active brake 300 ms) and disarms.

## 4. HTTP interface (Web Dashboard)

| Request | Result |
|---|---|
| `GET http://192.168.8.201/cmd?line=BB8,W,<seq>,DRV,50,0` | same command format; reply `OK` (200) or `REJECTED` (409) |
| `GET http://192.168.8.201/telemetry` | telemetry JSON (below) |
| `GET http://192.168.8.201/` | built-in Body test page |

CORS is open (`Access-Control-Allow-Origin: *`), so the Dashboard can be hosted anywhere on the network. While the user holds a movement control, the Dashboard must repeat `DRV` every 100 ms.

## 5. Telemetry (Body → Remote & Head, UDP 4211, 5 Hz; same JSON over HTTP)

```json
{"node":"body","fw":"BODY-1.0.0","seq":812,"up":163,"state":"ACTIVE","faults":0,
 "owner":"R","armed":1,"rssi":-58,"cmd":{"y":40,"x":0},
 "motor":{"m1":152,"m2":152,"m3":0},
 "imu":{"ok":1,"roll":2.4,"pitch":-1.1,"yaw":37.0},
 "batt":{"present":1,"v":11.86,"pct":65,"low":0,"crit":0},
 "mag":1,"servo":[90,90,90],"wave":0,"reset":0}
```

| Field | Meaning |
|---|---|
| `state` | BOOT, IDLE, ACTIVE, FAILSAFE, DISABLED |
| `faults` | bit 0 IMU, 1 battery low, 2 battery critical, 3 Wi-Fi, 4 command lost, 5 magnet off |
| `owner` | R / W / T / `-` |
| `motor` | signed PWM duty −255..255 (m1/m2 drive, m3 steering) |
| `batt.pct` | linear estimate between empty and full voltage |
| `reset` | 1 = Body is about to restart |

Remote LED2 "robot connected" = Body telemetry received within the last 1 s.

## 6. Timeouts

| Name | Value |
|---|---|
| Command timeout (fail-safe) | 500 ms |
| Ownership idle timeout | 1500 ms |
| Sequence session timeout | 2000 ms |
| Telemetry period | 200 ms |
