#!/usr/bin/env python3
"""
BB-8 robot simulator - test the Remote WITHOUT the Body, Head and Camera.

1. Give the laptop the IP 192.168.8.100 on BB8_NET (or change IP_TEST_TARGET in config.h).
2. In BB8_REMOTE/config.h set  USE_TEST_TARGET 1  and upload.
3. Run:  python robot_sim.py            (add --remote <ip> if the Remote is not .204)

The simulator listens on UDP 4210 (Body), 4212 (Head) and 4213 (Camera),
prints every command the Remote sends, and answers with fake Body/Head/Camera
telemetry so the Remote OLED and LED2 work. It also checks the Remote:
  - DRV must arrive at ~20 Hz (prints a warning if the stream stops = fail-safe test)
  - each PB1/PB2/PB3 press must produce ONE action (event id check)
Python 3, no extra packages. Ctrl+C to stop.
"""
import argparse, json, select, socket, time

p = argparse.ArgumentParser()
p.add_argument("--remote", default="192.168.8.204")
args = p.parse_args()

def bind(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("", port))
    return s

body, head, cam = bind(4210), bind(4212), bind(4213)
tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
EYES = ["NORMAL", "HAPPY", "ANGRY", "SAD", "SLEEPY", "LOVE", "LENS"]
st = dict(y=0, x=0, armed=False, state="IDLE", servo=[90, 90, 90], wave_until=0,
          eye=0, cap=0, last_drv=0.0, last_eid={}, drv_count=0, rate_t=time.time(), lost=False)

def event_new(node, eid):
    if eid is None or eid == "0":
        return True
    if st["last_eid"].get(node) == eid:
        return False
    st["last_eid"][node] = eid
    return True

def handle(node, line):
    parts = line.split(",")
    if len(parts) < 4 or parts[0] != "BB8":
        print("BAD", line); return
    cmd, a = parts[3], parts[4:]
    now = time.time()
    if node == "body":
        if cmd == "DRV":
            y, x = int(a[0]), int(a[1])
            st["last_drv"] = now; st["drv_count"] += 1
            if st["lost"]:
                print("BODY  DRV stream back"); st["lost"] = False
            if not st["armed"]:
                if y == 0 and x == 0:
                    st["armed"] = True; print("BODY  armed (stick centred)")
                y = x = 0
            if (y, x) != (st["y"], st["x"]):
                print(f"BODY  DRV y={y:+4d} x={x:+4d}")
            st["y"], st["x"] = y, x
            st["state"] = "ACTIVE"
        elif cmd == "HEAD":
            st["servo"] = [max(45, min(135, float(v))) for v in a[:3]]
            print("BODY  HEAD", a[:3])
        elif cmd in ("ROT", "WAVE"):
            print("BODY ", cmd, a)
            if cmd == "WAVE": st["wave_until"] = now + 2
        elif cmd == "STOP":
            print("BODY  STOP -> disarmed"); st["armed"] = False; st["y"] = st["x"] = 0
        elif cmd == "RESET":
            print("BODY  RESET")
    elif node == "head":
        eid = a[-1] if cmd != "HB" and len(a) >= (2 if cmd == "EYE" else 1) else None
        if cmd == "HB":
            return
        new = event_new("head", eid)
        if cmd == "EYE" and new:
            st["eye"] = (st["eye"] + 1) % 7 if a[0] == "NEXT" else int(a[0]) % 7
            print("HEAD  eye ->", EYES[st["eye"]])
        elif cmd == "BLINK" and new:
            print("HEAD  blink")
        elif not new:
            print(f"HEAD  {cmd} repeat copy ignored (same eid) - correct")
        else:
            print("HEAD ", cmd, a)
    elif node == "cam":
        eid = a[0] if a else None
        if cmd == "CAP":
            if event_new("cam", eid):
                st["cap"] += 1; print("CAM   capture #", st["cap"]); send_cam(fresh=True)
            else:
                print("CAM   CAP repeat copy ignored (same eid) - correct")
        else:
            print("CAM  ", cmd, a)

def send(obj):
    tx.sendto(json.dumps(obj, separators=(",", ":")).encode(), (args.remote, 4211))

def send_cam(fresh=False):
    send({"node": "cam", "fw": "SIM", "camOk": 1, "cap": st["cap"], "capOk": 1,
          "capAge": 0 if fresh else 5, "sd": 1, "stream": 0})

t5 = t1 = 0.0
print("robot_sim running - waiting for the Remote...")
while True:
    r, _, _ = select.select([body, head, cam], [], [], 0.02)
    for s in r:
        data, addr = s.recvfrom(256)
        node = {body: "body", head: "head", cam: "cam"}[s]
        handle(node, data.decode(errors="replace").strip())
    now = time.time()
    if st["last_drv"] and now - st["last_drv"] > 0.5 and not st["lost"]:
        st["lost"] = True; st["armed"] = False; st["state"] = "FAILSAFE"
        print("BODY  !!! DRV stream LOST > 0.5 s -> real Body would STOP (FAILSAFE)")
    if now - st["rate_t"] >= 5:
        print(f"      DRV rate {st['drv_count'] / (now - st['rate_t']):.1f} Hz (expected ~20)")
        st["drv_count"] = 0; st["rate_t"] = now
    if now - t5 >= 0.2:
        t5 = now
        m = int(st["y"] * 2.3)
        send({"node": "body", "fw": "SIM", "state": st["state"], "faults": 0, "owner": "R",
              "armed": int(st["armed"]), "cmd": {"y": st["y"], "x": st["x"]},
              "motor": {"m1": m, "m2": m, "m3": int(st["x"] * 2)},
              "imu": {"ok": 1, "roll": 1.5, "pitch": -0.5, "yaw": 0},
              "batt": {"present": 1, "v": 11.8, "pct": 62, "low": 0, "crit": 0},
              "mag": 1, "servo": st["servo"], "wave": int(now < st["wave_until"]), "reset": 0})
        send({"node": "head", "eye": st["eye"], "eyeName": EYES[st["eye"]], "oled": [1, 1], "blink": 0})
    if now - t1 >= 1:
        t1 = now
        send_cam()
