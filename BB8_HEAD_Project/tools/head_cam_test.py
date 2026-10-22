#!/usr/bin/env python3
"""
BB-8 Head + Camera test tool - a laptop stand-in for the Remote's PB1/PB2/PB3/PB4.
Uses the same UDP protocol as the Remote, with source 'T' (test).
Your laptop must be on the robot Wi-Fi (BB8_NET). Python 3, no extra packages.

Examples
  python head_cam_test.py listen              # show Head + Camera telemetry
  python head_cam_test.py eye 2               # design 2 (ANGRY)
  python head_cam_test.py next                # = PB1
  python head_cam_test.py blink               # = PB2
  python head_cam_test.py cap                 # = PB3
  python head_cam_test.py flash 1 | flash 0
  python head_cam_test.py demo                # cycle all eye designs + blinks
  python head_cam_test.py repeat              # PB1 sent 3x with one eid -> must advance ONE design
  python head_cam_test.py reset               # restart Head + Camera
"""
import argparse, json, socket, time

p = argparse.ArgumentParser(description="BB-8 Head/Camera UDP test tool")
p.add_argument("--head", default="192.168.8.202")
p.add_argument("--cam", default="192.168.8.203")
sub = p.add_subparsers(dest="cmd", required=True)
e = sub.add_parser("eye"); e.add_argument("n")
f = sub.add_parser("flash"); f.add_argument("on", type=int)
for name in ("next", "prev", "blink", "cap", "listen", "demo", "repeat", "reset"):
    sub.add_parser(name)
args = p.parse_args()

HEAD = (args.head, 4212)
CAM = (args.cam, 4213)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("", 4211))       # Head and Camera send telemetry back to the last test sender
sock.settimeout(0.05)
seq = int(time.time() * 100) % 2_000_000_000
eid = int(time.time()) % 60000 + 1

def send(dest, c):
    global seq
    seq += 1
    sock.sendto(f"BB8,T,{seq},{c}".encode(), dest)

def event(dest, c):
    """send one event 3x with the same eid, like the Remote"""
    global eid
    eid += 1
    for _ in range(3):
        send(dest, f"{c},{eid}")
        time.sleep(0.03)

def show(seconds):
    end = time.time() + seconds
    while time.time() < end:
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            continue
        try:
            t = json.loads(data)
        except Exception:
            print(data); continue
        if t.get("node") == "head":
            print(f"HEAD eye={t['eye']} {t['eyeName']:7s} blink={t['blink']} oled={t['oled']} "
                  f"links={t['links']} cap={t['cap']}")
        elif t.get("node") == "cam":
            print(f"CAM  camOk={t['camOk']} cap={t['cap']} capOk={t['capOk']} saved={t['saved']} "
                  f"sd={t['sd']} stream={t['stream']} rssi={t['rssi']}")

c = args.cmd
if c == "listen":
    send(HEAD, "PING"); send(CAM, "PING")
    print("listening... Ctrl+C to stop")
    try:
        while True:
            send(HEAD, "PING"); send(CAM, "PING")   # keeps us registered as monitor
            show(2)
    except KeyboardInterrupt:
        pass
elif c == "eye":   event(HEAD, f"EYE,{args.n}"); show(1)
elif c == "next":  event(HEAD, "EYE,NEXT"); show(1)
elif c == "prev":  event(HEAD, "EYE,PREV"); show(1)
elif c == "blink": event(HEAD, "BLINK"); show(1)
elif c == "cap":   event(CAM, "CAP"); show(2)
elif c == "flash": send(CAM, f"FLASH,{args.on}"); show(1)
elif c == "reset": event(HEAD, "RESET"); event(CAM, "RESET"); print("reset sent")
elif c == "repeat":
    print("PB1 test: 3 copies, one eid -> the Head must advance exactly ONE design")
    send(HEAD, "PING"); show(0.6)
    event(HEAD, "EYE,NEXT"); show(1.5)
elif c == "demo":
    for n in range(7):
        event(HEAD, f"EYE,{n}"); show(1.5)
        event(HEAD, "BLINK"); show(1.0)
    event(HEAD, "EYE,0")
