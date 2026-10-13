"""Verify a retained result on Korvo, then restart without losing saved data.

Run from the workspace root. If needed, a fresh run and a short jump prepare
the result screen. The default 60-second wait exceeds the old
automatic resolution downgrade interval. It does not change device settings.
"""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, "tools/pxadb")
import pxadb

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
parser.add_argument("--seconds", type=float, default=60)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
os.environ["PXADB_BAUD"] = "2000000"
logs = []

with pxadb.open_client("/dev/ttyUSB0", 15) as client:
    client.log_callback = lambda frame: logs.append(frame.payload)
    client.subscribe_logs()

    def pump(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            client.pump_logs()
            time.sleep(.01)

    def latest_perf():
        lines = [line for line in logs if "J3CPP perf " in line]
        assert lines, "missing game performance counters"
        return {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", lines[-1])}

    def screenshot(name):
        capture = pxadb.screenshot_capture(client.request("SCREENSHOT JPEG AFTER_PRESENT", timeout=20))
        pxadb.write_screenshot(capture, args.output / (name + ".jpg"))

    pump(6)
    before = latest_perf()
    if before["state"] != 5:
        client.request("INPUT KEY HOME")
        client.request("INPUT SWIPE 400 430 400 100 400 10")
        client.request("PACKAGE stop pxa-jump-jump-3d")
        pump(2)
        client.request("PACKAGE run pxa-jump-jump-3d")
        pump(6)
        client.request("INPUT SWIPE 400 280 400 280 360 2")
        client.request("INPUT SYNC")
        pump(8)
        before=latest_perf()
    assert before["state"] == 5, before
    ready = [line for line in logs if "J3CPP ready " in line]
    if ready:
        assert "render=800,480 scale=0" in ready[-1], ready[-1]
    screenshot("before")
    marker = len(logs)
    started = time.monotonic()
    while time.monotonic() - started < args.seconds:
        pump(min(10, args.seconds - (time.monotonic() - started)))
        print("result retained", round(time.monotonic() - started), "seconds", flush=True)
    after = latest_perf()
    assert after["state"] == 5 and after["submitted"] == before["submitted"], (before, after)
    assert not any("J3CPP ready " in line for line in logs[marker:]), "renderer recreated on static result"
    screenshot("after")

    # The retained framebuffer must survive a real background/foreground cycle.
    client.request("INPUT KEY HOME")
    client.request("INPUT SWIPE 400 430 400 100 400 10")
    pump(1)
    client.request("PACKAGE run pxa-jump-jump-3d")
    pump(3)
    screenshot("foreground")
    client.request("INPUT TAP 400 280")
    client.request("INPUT SYNC")
    pump(6)
    restarted = latest_perf()
    assert restarted["state"] == 0 and restarted["score"] == 0, restarted
    assert restarted["submitted"] > after["submitted"], "restart did not resume rendering"
    screenshot("restarted")
    client.request("INPUT POINTER DOWN 400 280 0")
    client.request("INPUT SYNC")
    pump(.7)
    client.request("INPUT POINTER UP 400 280 0")
    client.request("INPUT SYNC")
    pump(6)
    landed = latest_perf()
    assert landed["state"] == 0 and landed["score"] > 0, landed
    screenshot("landed")
    client.request("INPUT CANCEL")
    (args.output / "logs.txt").write_text("\n".join(logs) + "\n")
    report = dict(seconds=args.seconds, before=before, after=after,
        restarted=restarted, landed=landed, renderer_recreated=False)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
