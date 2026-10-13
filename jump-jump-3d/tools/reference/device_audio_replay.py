"""Replay the fixed opening map on Korvo without replacing saved data.

The reference log comes from simulator_platforms.py with the same map seed.
Production logs verify progress; fault injection belongs in the native tests.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, "tools/pxadb")
import pxadb

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
parser.add_argument("--reference-log", type=Path, required=True)
parser.add_argument("--jumps", type=int, default=18)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
reference = re.findall(r"J3CPP landed .*?current=(\d+) next=\d+ dx_q6=(-?\d+) dz_q6=(-?\d+)",
    args.reference_log.read_text())
assert len(reference) >= args.jumps
os.environ["PXADB_BAUD"] = "2000000"

def charge_for_distance(distance):
    low, high = 0.0, 3.0
    for _ in range(40):
        charge = (low + high) / 2
        vy = min(18.0, 13.5 + 1.5 * charge)
        rise = .55 * (1 - max(.5, 1 - .32 * charge))
        duration = (vy + math.sqrt(vy * vy - 144 * rise)) / 72
        if min(15, 7 * charge) * duration < distance:
            low = charge
        else:
            high = charge
    return (low + high) / 2

logs, rows = [], []
with pxadb.open_client("/dev/ttyUSB0", 15) as client:
    client.log_callback = lambda frame: logs.append(frame.payload)
    client.hello()
    client.subscribe_logs()

    def pump(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            client.pump_logs()
            time.sleep(.01)

    def memory(name):
        (args.output / name).write_text("\n".join(f.payload for f in client.request("MEMORY")) + "\n")

    def screenshot(name):
        capture = pxadb.screenshot_capture(client.request("SCREENSHOT JPEG AFTER_PRESENT", timeout=20))
        pxadb.write_screenshot(capture, args.output / (name + ".jpg"))

    try:
        client.request("INPUT CANCEL")
        client.request("INPUT KEY HOME")
        client.request("INPUT SWIPE 400 430 400 100 400 10")
        pump(1)
        try:
            client.request("PACKAGE stop pxa-jump-jump-3d")
        except pxadb.PxaDbRejectedError as error:
            if "package_not_running" not in str(error):
                raise
        pump(2)
        logs.clear()
        client.request("PACKAGE run pxa-jump-jump-3d")
        pump(7)
        assert any("J3CPP ready " in line for line in logs)
        memory("ready-memory.txt")
        screenshot("ready")
        distance = 2.0
        for i, (kind, dx, dz) in enumerate(reference[:args.jumps]):
            hold_ms = round(charge_for_distance(distance) * 1000)
            client.request(f"INPUT SWIPE 400 280 400 280 {hold_ms} 8")
            client.request("INPUT SYNC")
            pump(2.75)  # Flight + two-second platform reward delay.
            rows.append(dict(jump=i+1, kind=int(kind), hold_ms=hold_ms))
            print(rows[-1], flush=True)
            if int(kind) in (3, 4):
                screenshot(f"special-{i+1}")
            distance = math.hypot(int(dx), int(dz)) / 1e6
            if any("J3CPP perf " in line and "state=5" in line for line in logs):
                raise AssertionError(f"replay missed at jump {i+1}; inspect logs/screenshots")
        pump(6)
        screenshot("final")
        memory("final-memory.txt")
        perf = [line for line in logs if "J3CPP perf " in line]
        assert perf and "state=0" in perf[-1], perf[-1:]
        assert not any(term in "\n".join(logs) for term in (
            "sound preparation failed", "Physical audio write failed", "Ogg decoding stopped"))
        (args.output / "report.json").write_text(json.dumps(dict(
            jumps=rows, final_perf=perf[-1],
            deferred=[line for line in logs if "sound deferred" in line]), indent=2) + "\n")
    finally:
        client.request("INPUT CANCEL")
        (args.output / "logs.txt").write_text("\n".join(logs) + "\n")
