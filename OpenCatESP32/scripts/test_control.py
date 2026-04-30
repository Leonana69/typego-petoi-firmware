#!/usr/bin/env python3
"""Quick test harness for the OpenCatESP32 /control endpoint.

Sends timed walk / rotate commands and stops cleanly on Ctrl+C. Stdlib only —
no `pip install` needed.

Usage:
  python3 test_control.py walk                  # walk forward 3s
  python3 test_control.py walk 5                # walk forward 5s
  python3 test_control.py walk 5 --vx 0.3       # ...with bigger vx
  python3 test_control.py rotate                # rotate left 2s
  python3 test_control.py rotate 3 --vyaw -0.3  # rotate right 3s
  python3 test_control.py square                # 4-sided patrol
  python3 test_control.py stand
  python3 test_control.py rest
  python3 test_control.py stop
"""

import argparse
import json
import signal
import sys
import time
import urllib.error
import urllib.request

ROBOT = "http://192.168.0.235"
TIMEOUT = 1.0          # seconds for the HTTP call itself
RETRY_BUSY_DELAY = 0.3 # if firmware says 409 busy, wait this long and retry once


def post(command, **fields):
    """POST {"command": <command>, **fields} to /control. Retries once on 409."""
    body = json.dumps({"command": command, **fields}).encode()
    req = urllib.request.Request(
        f"{ROBOT}/control",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    for attempt in (1, 2):
        try:
            with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
                return json.loads(r.read() or b"{}")
        except urllib.error.HTTPError as e:
            if e.code == 409 and attempt == 1:
                time.sleep(RETRY_BUSY_DELAY)
                continue
            sys.stderr.write(f"HTTP {e.code}: {e.read().decode(errors='replace')}\n")
            return None
        except urllib.error.URLError as e:
            sys.stderr.write(f"Network error talking to {ROBOT}: {e}\n")
            return None


def safe_stop(*_):
    post("stop")
    sys.exit(0)


def stand_up():
    print("stand_up")
    post("stand_up")
    time.sleep(2.0)  # let the up-skill finish before commanding a gait


def walk(duration, vx):
    print(f"walk vx={vx} for {duration}s")
    post("nav", vx=vx)
    time.sleep(duration)
    post("stop")


def rotate(duration, vyaw):
    side = "left" if vyaw > 0 else "right"
    print(f"rotate {side} vyaw={vyaw} for {duration}s")
    post("nav", vyaw=vyaw)
    time.sleep(duration)
    post("stop")


def square(sides, side_duration, turn_duration, vx, vyaw):
    stand_up()
    for i in range(sides):
        print(f"--- side {i + 1}/{sides} ---")
        walk(side_duration, vx)
        time.sleep(0.5)
        rotate(turn_duration, vyaw)
        time.sleep(0.5)
    print("sit_down")
    post("sit_down")


def main():
    global ROBOT
    p = argparse.ArgumentParser(description="OpenCatESP32 /control test harness.")
    p.add_argument("--robot", default=ROBOT, help=f"robot base URL (default {ROBOT})")
    sub = p.add_subparsers(dest="cmd", required=True)

    w = sub.add_parser("walk", help="walk forward N seconds, then stop")
    w.add_argument("duration", nargs="?", type=float, default=3.0)
    w.add_argument("--vx", type=float, default=0.2)

    r = sub.add_parser("rotate", help="rotate in place N seconds, then stop")
    r.add_argument("duration", nargs="?", type=float, default=2.0)
    r.add_argument("--vyaw", type=float, default=0.3,
                   help="positive=left, negative=right (rad/s)")

    sq = sub.add_parser("square", help="walk an N-sided patrol")
    sq.add_argument("--sides", type=int, default=4)
    sq.add_argument("--side-duration", type=float, default=4.0)
    sq.add_argument("--turn-duration", type=float, default=3.0)
    sq.add_argument("--vx", type=float, default=0.2)
    sq.add_argument("--vyaw", type=float, default=0.3)

    sub.add_parser("stand", help="stand up")
    sub.add_parser("rest", help="sit down (servos relax)")
    sub.add_parser("stop", help="stop motion (load rest skill)")

    args = p.parse_args()
    ROBOT = args.robot

    signal.signal(signal.SIGINT, safe_stop)
    signal.signal(signal.SIGTERM, safe_stop)

    if args.cmd == "walk":
        stand_up()
        walk(args.duration, args.vx)
    elif args.cmd == "rotate":
        stand_up()
        rotate(args.duration, args.vyaw)
    elif args.cmd == "square":
        square(args.sides, args.side_duration, args.turn_duration,
               args.vx, args.vyaw)
    elif args.cmd == "stand":
        post("stand_up")
    elif args.cmd == "rest":
        post("sit_down")
    elif args.cmd == "stop":
        post("stop")


if __name__ == "__main__":
    main()
