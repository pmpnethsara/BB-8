#!/usr/bin/env python3
"""
BB-8 Body test tool - a laptop stand-in for the physical Remote.
Sends the same UDP protocol as the Remote, using source 'T' (test).
Your laptop must be on the robot Wi-Fi (BB8_NET). Python 3, no extra packages.

Examples
  python udp_remote_sim.py listen                 # show live Body telemetry
  python udp_remote_sim.py drive 40 0 3           # forward 40 % for 3 s, then neutral
  python udp_remote_sim.py drive 0 -60 2          # steer left for 2 s
  python udp_remote_sim.py failsafe               # drive, then go SILENT -> Body must stop
  python udp_remote_sim.py head 110 80 90
  python udp_remote_sim.py rotate 1 | wave | stop | reset
  python udp_remote_sim.py raw "BB8,T,999,DRV,0,0"   # send any line once
"""
import argparse, json, socket, time

p = argparse.ArgumentParser(description="BB-8 Body UDP test tool")
p.add_argument("--ip", default="192.168.8.201", help="Body IP")
p.add_argument("--port", type=int, default=4210, help="Body command port")
sub = p.add_subparsers(dest="cmd", required=True)
d = sub.add_parser("drive"); d.add_argument("y", type=int); d.add_argument("x", type=int)
d.add_argument("seconds", type=float, nargs="?", default=2.0)
h = sub.add_parser("head"); h.add_argument("angles", type=float, nargs=3)
r = sub.add_parser("rotate"); r.add_argument("dir", type=int, nargs="?", default=1)
for name in ("wave", "stop", "reset", "listen", "failsafe"):
    sub.add_parser(name)
w = sub.add_parser("raw"); w.add_argument("line")
args = p.parse_args()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("", 4211))          # Body sends telemetry back to the last test sender on 4211
sock.settimeout(0.005)
seq = int(time.time() * 100) % 2_000_000_000   # always larger than the previous run

def send(c):
    global seq
    seq += 1
    sock.sendto(f"BB8,T,{seq},{c}".encode(), (args.ip, args.port))

last_print = 0.0
def show_telemetry(force=False):
    global last_print
    try:
        data, _ = sock.recvfrom(2048)
    except (socket.timeout, BlockingIOError):
        return
    if not force and time.time() - last_print < 0.25:
        return
    last_print = time.time()
    try:
        t = json.loads(data)
        print(f"{t['state']:9s} owner={t['owner']} armed={t['armed']} cmd={t['cmd']} "
              f"motor={t['motor']} roll={t['imu']['roll']} batt={t['batt']['v']}V "
              f"mag={t['mag']} servo={t['servo']} faults={t['faults']}")
    except Exception:
        print(data.decode(errors="replace"))

def drive_for(y, x, seconds):
    send("DRV,0,0")                      # arm (neutral first)
    end = time.time() + seconds
    while time.time() < end:
        send(f"DRV,{y},{x}")             # continuous, 20 Hz like the Remote
        show_telemetry()
        time.sleep(0.05)

if args.cmd == "drive":
    drive_for(args.y, args.x, args.seconds)
    for _ in range(5):
        send("DRV,0,0"); time.sleep(0.05)
elif args.cmd == "failsafe":
    print("Driving forward 30 % for 2 s, then going SILENT (simulated lost Remote)...")
    drive_for(30, 0, 2.0)
    print("--- silent now: Body must ramp to stop within ~0.5 s and show FAILSAFE ---")
    end = time.time() + 3
    while time.time() < end:
        show_telemetry(force=True); time.sleep(0.01)
elif args.cmd == "head":
    a = args.angles
    for _ in range(10):
        send(f"HEAD,{a[0]:.0f},{a[1]:.0f},{a[2]:.0f}"); time.sleep(0.05)
elif args.cmd == "rotate":
    send("DRV,0,0"); time.sleep(0.05); send(f"ROT,{args.dir}")
    end = time.time() + 2
    while time.time() < end:             # keep link alive during rotate
        send("HB"); show_telemetry(); time.sleep(0.1)
elif args.cmd == "wave":
    send("WAVE")
elif args.cmd == "stop":
    send("STOP")
elif args.cmd == "reset":
    send("RESET")
elif args.cmd == "raw":
    sock.sendto(args.line.encode(), (args.ip, args.port))
elif args.cmd == "listen":
    print("Listening (Ctrl+C to quit)...")
    nxt = 0
    while True:
        if time.time() > nxt:
            send("PING"); nxt = time.time() + 1   # PING keeps telemetry coming to this PC
        show_telemetry(force=True)
        time.sleep(0.01)
print("done")
