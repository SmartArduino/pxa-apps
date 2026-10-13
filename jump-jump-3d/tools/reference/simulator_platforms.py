"""Replay centre jumps against a real AOT package built with J3_TRACE_TIMING=1.

Run from the workspace root with a private PXADB simulator service running.
The diagnostic package is never installed on a physical device.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time

sys.path.insert(0, "tools/pxadb")
import pxadb

parser = argparse.ArgumentParser()
parser.add_argument("--package", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--jumps", type=int, default=18)
parser.add_argument("--utc-offset-seconds", type=int, help="Check the effective Host zone at startup")
parser.add_argument("--endpoint", default="unix:/tmp/pxa-simulator-1000/pai-touch@jump-blocks.sock")
parser.add_argument("--control", default="/tmp/pxa-simulator-1000/pai-touch@jump-blocks.control.sock")
args = parser.parse_args()
out = args.output.resolve()
(out / "state").mkdir(parents=True, exist_ok=True)
log = out / "simulator.log"
command = [
    str(Path("build/simulator/pai-touch/pxsys_product_simulator").resolve()),
    "--package", str(args.package.resolve()),
    "--publisher-key", str(Path("local/pxa-apps/.dev-signing/publisher-public.der").resolve()),
    "--state-root", str(out / "state"), "--pxadb-control-socket", args.control,
    "--width", "800", "--height", "480", "--density-dpi", "216", "--locale", "zh-CN",
]
landing = re.compile(r"J3CPP landed now=(\d+) state=(\d+) score=(\d+) result=(\d+) current=(\d+) next=(\d+) dx_q6=(-?\d+) dz_q6=(-?\d+)")
release = re.compile(r"J3CPP release now=(\d+) charge_us=(\d+) flight_us=(\d+)")

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

rows = []
with log.open("w") as stream:
    process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
        env=os.environ | {"SDL_VIDEODRIVER": "dummy", "SDL_AUDIODRIVER": "dummy"})
    try:
        deadline = time.monotonic() + 45
        while "J3CPP ready" not in log.read_text():
            assert process.poll() is None and time.monotonic() < deadline, "initialization failed"
            time.sleep(.1)
        if args.utc_offset_seconds is not None:
            offsets=re.findall(r"J3CPP clock local=\d+:\d+ offset_s=(-?\d+)",log.read_text())
            assert offsets and int(offsets[-1])==args.utc_offset_seconds, offsets
        with pxadb.open_client(args.endpoint, 15) as client:
            distance, overhead = 2.0, .010
            for index in range(args.jumps):
                # Settle the camera, and allow each special platform's reward.
                time.sleep(2.7)
                charge = charge_for_distance(distance)
                hold_ms = max(1, round((charge - overhead) * 1000))
                client.request(f"INPUT SWIPE 400 280 400 280 {hold_ms} 2")
                client.request("INPUT SYNC")
                deadline = time.monotonic() + 5
                while len(landing.findall(log.read_text())) <= index:
                    assert time.monotonic() < deadline, "landing timed out"
                    time.sleep(.05)
                values = list(map(int, landing.findall(log.read_text())[-1]))
                stamp, state, score, result, current, next_kind, dx, dz = values
                assert state == 0 and result in (0, 1), (index, values)
                released, actual_charge, planned = map(int, release.findall(log.read_text())[-1])
                overhead = max(0, min(.1, actual_charge / 1e6 - hold_ms / 1000))
                row = dict(jump=index+1, current=current, next=next_kind, score=score,
                    charge_us=actual_charge, planned_flight_us=planned,
                    landing_callback_us=stamp-released, result=result)
                rows.append(row)
                print(row, flush=True)
                time.sleep(.5)
                cap = pxadb.screenshot_capture(client.request("SCREENSHOT", timeout=20))
                pxadb.write_screenshot(cap, out / f"jump-{index+1:02d}-kind-{current}.png")
                distance = math.hypot(dx, dz) / 1e6
            client.request("INPUT CANCEL")
    finally:
        process.terminate()
        process.wait(timeout=10)
        (out / "report.json").write_text(json.dumps(rows, indent=2) + "\n")
assert len(rows) == args.jumps
assert not any(term in log.read_text() for term in (
    "frame not accepted", "draw rejected", "sound preparation failed"))
