#!/usr/bin/env python3
"""Measure press duration and resulting charge on the same real device.

Use the isolated release-only packages prepared by prepare_jump_release_device.py.
Logs are emitted after release; no per-tick/press/MOVE logging delays the hold.
Fresh runs preserve the first platform at x=2, z=0 with the original seed.
"""
import argparse
import json
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path.cwd() / "tools/pxadb"))
import pxadb

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", required=True)
parser.add_argument("--app", required=True, choices=["pxa-jump-jump-3d", "pxa-jump-jump-3d-cpp"])
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--holds", type=float, nargs="+", default=[.15, .36, .7, .82, 1.2, 2])
parser.add_argument("--repeat", type=int, default=1)
parser.add_argument("--allow-audio", action="store_true")
args = parser.parse_args()
if args.repeat < 1 or any(not 0 < hold <= 10 for hold in args.holds):
    parser.error("repeat >= 1 and hold durations in (0, 10] seconds required")
args.output.mkdir(parents=True, exist_ok=True)
report = {"app": args.app, "diagnostic": "one log after release",
          "fps_benchmark": False, "runs": [], "passed": False}
logs = []
seen = set()
with (args.output / "serial.jsonl").open("w", buffering=1) as output:
    with pxadb.PxaDbClient(args.port, 15) as client:
        def capture(frame):
            key = (frame.kind, frame.payload)
            if key in seen:
                return
            seen.add(key)
            logs.append(frame.payload)
            output.write(json.dumps({"kind": frame.kind, "payload": frame.payload}) + "\n")
        client.log_callback = capture
        def request(command):
            output.write(json.dumps({"command": command, "host_time": time.monotonic()}) + "\n")
            return client.request(command, timeout=60)
        def pump(seconds):
            end = time.monotonic() + max(0, seconds)
            while time.monotonic() < end:
                client.pump_logs()
                time.sleep(.005)
        report["hello"] = client.hello()
        report["info"] = [frame.payload for frame in request("INFO")]
        assert not any("active=1" in frame.payload for frame in request("PACKAGES")
                       if frame.kind == "PKG"), "another app is active; preserve it"
        client.subscribe_logs()
        running = False
        try:
            request("INPUT KEY HOME")
            pump(.5)
            for repeat in range(args.repeat):
                for hold in args.holds:
                    at = len(logs)
                    request(f"PACKAGE run {args.app}")
                    running = True
                    pump(5)
                    if any("Runtime permission prompt queued:" in line and args.app in line
                           for line in logs[at:]):
                        assert args.allow_audio, "--allow-audio required on the 296x240 test panel"
                        request("INPUT TAP 214 182")
                        request("INPUT SYNC")
                        pump(3)
                        request(f"PACKAGE stop {args.app}")
                        running = False
                        pump(1)
                        request(f"PACKAGE run {args.app}")
                        running = True
                        pump(5)
                    at = len(logs)
                    start = time.monotonic()
                    request("INPUT POINTER DOWN 148 120 0")
                    request("INPUT SYNC")
                    pump(hold - (time.monotonic() - start))
                    request("INPUT POINTER UP 148 120 0")
                    request("INPUT SYNC")
                    pump(1)
                    samples = [dict((key, int(value)) for key, value in
                                    re.findall(r"(\w+)=(-?\d+)", line.split("J3RELEASE ")[1]))
                               for line in logs[at:] if "J3RELEASE " in line]
                    assert len(samples) == 1, ("missing/extra release trace", samples)
                    sample = samples[0]
                    assert sample["up"] > sample["down"] and sample["state"] == 2
                    assert abs(sample["vx_q6"] - min(15e6, 7*sample["charge_us"])) <= 30
                    assert abs(sample["vy_q6"] - min(18e6, 13500000+1.5*sample["charge_us"])) <= 30
                    expected_x = sample["vx_q6"] * (2*sample["vy_q6"]/1e6/72)
                    assert abs(sample["land_x_q6"]-expected_x) < 40 and sample["land_z_q6"] == 0
                    sample.update(repeat=repeat, requested_hold_seconds=hold,
                                  device_hold_us=sample["up"]-sample["down"],
                                  charge_to_wall_ratio=sample["charge_us"]/(sample["up"]-sample["down"]))
                    report["runs"].append(sample)
                    print(json.dumps(sample), flush=True)
                    if repeat == 0 and hold == args.holds[-1]:
                        pxadb.write_screenshot(pxadb.screenshot_capture(request("SCREENSHOT JPEG")),
                                               args.output/"after-release.jpg")
                    request(f"PACKAGE stop {args.app}")
                    running = False
                    pump(.5)
            report["passed"] = True
        finally:
            request("INPUT CANCEL")
            if running:
                request(f"PACKAGE stop {args.app}")
            (args.output/"report.json").write_text(json.dumps(report, indent=2)+"\n")
assert report["passed"]
