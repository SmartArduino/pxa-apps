#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Test an installed signed pocket game on pai-touch using PXADB and Pillow.

Run from the workspace root with --port, --game and --output. No firmware
flashing, package removal or save deletion. The chosen game is stopped at the
end. Requires no other active application. Idle-lock testing waits without
input and checks lifecycle transitions, zero locked frames, then board restore.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import time
import traceback
import zlib
sys.dont_write_bytecode = True
from PIL import Image
from simulator import difference, require
sys.path.insert(0, str(Path.cwd() / "tools/pxadb"))
import pxadb

PUBLISHER = "17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171"
IDS = {"2048": "pxa-game-2048", "chess": "pxa-pocket-chess", "sokoban": "pxa-pocket-sokoban"}
FAULTS = re.compile(r"Guest exception|callback failed|Pocket: .*failed|exit flush timed out|ASSERT|Guru Meditation", re.I)

class Device:
    def __init__(self, args, client, report, stream):
        self.args, self.client, self.report, self.stream = args, client, report, stream
        self.identity = IDS[args.game]
        self.logs = []
        self.ox, self.oy = 4, 8
        self.viewport_width, self.viewport_height = 288, 224
        self.started = time.monotonic()
        self.last_capture = 0.0
        client.log_callback = self.record
        client.raw_callback = lambda raw: self.record(pxadb.Frame(0, "RAW", raw.decode(errors="replace")))
    def record(self, frame):
        self.logs.append(frame.payload)
        self.stream.write(json.dumps({"seconds": time.monotonic()-self.started, "kind": frame.kind, "payload": frame.payload})+"\n")
    def request(self, text):
        self.stream.write(json.dumps({"command": text})+"\n")
        frames = self.client.request(text, timeout=30)
        self.stream.write(json.dumps({"response": [{"kind": f.kind, "payload": f.payload} for f in frames if f.kind != "DATA"]})+"\n")
        return frames
    def pump(self, seconds):
        end = time.monotonic()+seconds
        while time.monotonic() < end:
            self.client.pump_logs()
            time.sleep(.02)
    def until(self, predicate, label, seconds=20):
        end = time.monotonic()+seconds
        while time.monotonic() < end:
            if predicate():
                return
            self.pump(.1)
        raise TimeoutError(label)
    def start(self):
        at = len(self.logs)
        self.request("PACKAGE run "+self.identity)
        self.until(lambda: any("Pocket: ready app="+self.identity in line for line in self.logs[at:]), "game ready")
        self.pump(.6)
        require(self.configure_viewport(at), "Missing device viewport diagnostic")
    def configure_viewport(self, at=0):
        pattern = r"Pocket: viewport canvas=288x224 origin=(-?\d+),(-?\d+) size=(\d+)x(\d+) display=(\d+)x(\d+)"
        matches = [re.search(pattern, line) for line in self.logs[at:]]
        matches = [match for match in matches if match]
        if not matches:
            return False
        self.ox, self.oy, self.viewport_width, self.viewport_height, width, height = map(int, matches[-1].groups())
        require((width, height) == (296, 240), "Wrong board viewport display")
        require((self.ox, self.oy, self.viewport_width, self.viewport_height) == (0, 5, 296, 230), "Unexpected fitted device geometry")
        self.report.setdefault("viewports", []).append({"origin": [self.ox, self.oy], "size": [self.viewport_width, self.viewport_height], "display": [width, height]})
        return True
    def physical(self, x, y):
        return (self.ox + (2*x+1)*self.viewport_width//576,
                self.oy + (2*y+1)*self.viewport_height//448)
    def pointer(self, phase, x, y):
        px, py = self.physical(x, y)
        self.request(f"INPUT POINTER {phase} {px} {py} 1")
        self.request("INPUT SYNC")
    def tap(self, x, y, settle=.45):
        self.pointer("DOWN", x, y); self.pump(.06)
        self.pointer("UP", x, y); self.pump(settle)
    def swipe(self, dx, dy):
        x1, y1 = self.physical(90, 120)
        x2, y2 = self.physical(90+dx, 120+dy)
        self.request(f"INPUT SWIPE {x1} {y1} {x2} {y2} 200 4")
        self.request("INPUT SYNC"); self.pump(.6)
    def shot(self, label):
        require(not any(FAULTS.search(line) for line in self.logs), "Device runtime fault")
        # Firmware capture requests have a 250 ms minimum interval.
        self.pump(max(0.0, .35-(time.monotonic()-self.last_capture)))
        capture = pxadb.screenshot_capture(self.request("SCREENSHOT JPEG"))
        self.last_capture = time.monotonic()
        path = self.args.output/(label+".jpg")
        pxadb.write_screenshot(capture, path)
        require((capture.metadata["width"], capture.metadata["height"]) == ("296", "240"), "Wrong board display")
        self.report.setdefault("screenshots", {})[label] = capture.metadata
        return Image.open(path).convert("RGB")
    def board(self, label):
        x1, y1, x2, y2 = {"2048": (4,38,184,218), "chess": (8,24,200,216), "sokoban": (4,40,184,220)}[self.args.game]
        left, top = self.physical(x1, y1)
        right, bottom = self.physical(x2, y2)
        return self.shot(label).crop((left, top, right, bottom))
    def saved(self, label):
        # Read both host KV generations and validate length/CRC before selecting.
        fs = pxadb.NormalFsClient(self.client)
        paths = self.request(f"PACKAGE paths {PUBLISHER}:{self.identity}")
        private_root = next(f.payload.split("\t", 1)[1] for f in paths
                            if f.kind == "PATH" and f.payload.startswith("private\t"))
        private = private_root+"/.pxa-storage"
        candidates = []
        for kind, _, name in fs.list(private):
            if kind != "F" or not name.startswith(".pxa-kv-"):
                continue
            path = self.args.output/(label+"-"+name.removeprefix(".pxa-"))
            fs.get(private+"/"+name, path)
            raw = path.read_bytes()
            if len(raw) < 24:
                continue
            magic, version, _, generation, size, crc = struct.unpack("<4sHHQII", raw[:24])
            body = raw[24:]
            if magic != b"PXKV" or version != 1 or len(body) != size or zlib.crc32(body) != crc:
                continue
            count = struct.unpack_from("<H", body)[0]; at = 2; values = {}
            for _ in range(count):
                size = body[at]; at += 1; key = body[at:at+size]; at += size
                size = struct.unpack_from("<H", body, at)[0]; at += 2
                values[key.decode()] = body[at:at+size]; at += size
            require(at == len(body), "Invalid KV body")
            candidates.append((generation, values))
        require(candidates, "No valid host storage generation")
        generation, values = max(candidates, key=lambda row: row[0])
        payload = values[self.identity+".save.v1"]
        self.report.setdefault("storage", {})[label] = {"generation": generation, "size": len(payload), "sha256": hashlib.sha256(payload).hexdigest()}
        return payload
    def menu_exit(self):
        self.tap(276, 10)
        self.pointer("DOWN", 140, 145)
        def stopped():
            return any(f.kind == "PKG" and ":"+self.identity+"\t" in f.payload and "active=0" in f.payload for f in self.request("PACKAGES"))
        self.until(stopped, "menu EXIT", seconds=8)
        self.request("INPUT CANCEL")
    def lifecycle(self, expected):
        at = len(self.logs)
        def transition(foreground):
            return any("UI lifecycle:" in line and f"foreground={foreground}" in line and f"display_interactive={foreground}" in line for line in self.logs[at:])
        deadline = time.monotonic()+self.args.lock_timeout
        keepalive = time.monotonic()+10
        while not transition(0) and time.monotonic() < deadline:
            self.pump(.1)
            if time.monotonic() >= keepalive:
                self.request("INFO"); keepalive += 10
        require(transition(0), "Idle-lock background transition missing")
        self.pump(1)
        self.request("PERF CLEAR"); self.request("PERF START"); self.pump(3)
        paused = pxadb.info_properties(self.request("PERF STOP")[-1].payload)
        self.report["locked_perf"] = paused
        require(int(paused["raster"]) == 0 and int(paused["present"]) == 0, "Game kept drawing while locked")
        self.shot("locked")
        at = len(self.logs)
        self.request("PERF CLEAR"); self.request("PERF START")
        self.request("INPUT TAP 148 120"); self.pump(.3)
        self.request("INPUT SWIPE 148 192 148 48 600 8")
        self.until(lambda: transition(1), "unlock foreground transition", seconds=15)
        self.pump(.6)
        resumed = pxadb.info_properties(self.request("PERF STOP")[-1].payload)
        self.report["resumed_perf"] = resumed
        require(int(resumed["present"]) > 0, "No frame after unlock")
        require(difference(expected, self.board("unlocked")) < .005, "Unlock changed board")
        self.report["checks"].append("idle_lock_zero_frames_and_resume_same_board")


def exercise(d):
    require(not any(f.kind == "PKG" and "active=1" in f.payload for f in d.request("PACKAGES")), "Another app is active")
    d.client.subscribe_logs()
    d.report["input"] = [f.payload for f in d.request("INPUT CAPABILITIES")]
    d.request("INPUT KEY HOME")
    d.request("INPUT SWIPE 148 192 148 48 600 8"); d.pump(.5)
    d.logs.clear()  # LOGSUB replays history from apps run before this test.
    d.start()
    # Reset only the selected game's current board using its own UI.
    d.tap(276, 10); d.tap(140, 115)
    first = d.board("initial")
    if d.args.game == "2048":
        for dx, dy in ((-70, 0), (0, -70), (70, 0), (0, 70)):
            before = d.board("before-swipe")
            d.swipe(dx, dy)
            after = d.board("swipe")
            if difference(before, after) > .01:
                break
        require(difference(before, after) > .01, "Touch swipe did not move board")
        d.tap(236, 156)
        require(difference(before, d.board("undo")) < .005, "Undo did not restore board")
        for dx, dy in ((70, 0), (0, 70), (-70, 0), (0, -70)):
            d.swipe(dx, dy)
        d.pump(.5)
        payload = d.saved("played")
        require(payload[:4] == b"P204" and struct.unpack_from("<I", payload, 16)[0] > 0, "No durable 2048 moves")
        d.report["checks"].append("touch_swipe_undo_and_durable_moves")
    elif d.args.game == "chess":
        # The reset keeps mode; force AI if a prior test left local two-player.
        payload = d.saved("reset")
        if payload[4]: d.tap(246, 184)
        d.tap(8+4*24+12, 24+6*24+12); d.tap(8+4*24+12, 24+4*24+12)
        d.pump(4)
        payload = d.saved("ai-played")
        require(payload[:4] == b"PCU1" and struct.unpack_from("<I", payload, 16)[0] >= 2, "Human move/AI reply not saved")
        d.tap(246, 184)
        d.tap(8+3*24+12, 24+6*24+12); d.tap(8+3*24+12, 24+4*24+12)
        d.pump(.5)
        payload = d.saved("local-played")
        require(payload[4] == 1 and struct.unpack_from("<I", payload, 16)[0] >= 3, "Touch local move not saved")
        require(difference(first, d.board("played")) > .015, "Chess board did not change")
        d.report["checks"].append("touch_e2e4_ai_reply_local_d2d4_and_durable_ply")
    else:
        d.tap(85, 121)  # Tutorial 1: push the adjacent crate right.
        require(difference(first, d.board("pushed")) > .001, "Sokoban touch did not push")
        payload = d.saved("touch-push")
        require(payload[:4] == b"PSKB" and struct.unpack_from("<I", payload, 8)[0] == 1, "Sokoban push not durable")
        d.tap(236, 106)
        require(difference(first, d.board("undo")) < .003, "Sokoban undo did not restore crate")
        d.swipe(50, 0); d.swipe(50, 0)
        payload = d.saved("solved")
        require(struct.unpack_from("<Q", payload, 16)[0] & 1, "Tutorial completion not durable")
        d.tap(236, 200)
        payload = d.saved("next-level")
        require(payload[5] == 1 and payload[6] >= 1, "Sokoban next level not unlocked")
        d.report["checks"].append("touch_push_undo_solve_unlock_and_durable_progress")
    expected = d.board("before-menu")
    d.tap(276, 10)
    require(difference(expected, d.board("paused")) > .1, "Menu missing")
    d.tap(140, 85)
    require(difference(expected, d.board("continue")) < .005, "Continue changed board")
    d.report["checks"].append("touch_menu_and_continue")
    if not d.args.skip_idle_lock:
        d.lifecycle(expected)
    d.menu_exit()
    final = d.saved("after-exit")
    at = len(d.logs); d.start()
    require(any("Pocket: load completed" in line for line in d.logs[at:]), "Save not loaded on relaunch")
    require(difference(expected, d.board("restored")) < .005, "Relaunch board differs")
    require(d.saved("reloaded") == final, "Relaunch changed durable payload")
    d.report["checks"].append("menu_exit_and_durable_board_restoration")
    d.menu_exit()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", required=True)
    p.add_argument("--game", required=True, choices=IDS)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--lock-timeout", type=float, default=180)
    p.add_argument("--skip-idle-lock", action="store_true")
    args = p.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "game": args.game, "checks": []}
    with (args.output/"serial.jsonl").open("w", buffering=1) as stream:
        with pxadb.open_client(args.port, 30) as client:
            report["hello"] = client.device_info
            require(pxadb.info_properties(client.device_info).get("target") == "esp32s3", "Expected ESP32-S3")
            d = Device(args, client, report, stream)
            try:
                exercise(d)
                require(not any(FAULTS.search(line) for line in d.logs), "Runtime fault")
                report["passed"] = True
            except BaseException:
                report["error"] = traceback.format_exc()
                raise
            finally:
                try:
                    d.request("INPUT CANCEL")
                    active = any(f.kind == "PKG" and ":"+d.identity+"\t" in f.payload and "active=1" in f.payload for f in d.request("PACKAGES"))
                    if active: d.request("PACKAGE stop "+d.identity)
                finally:
                    (args.output/"results.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
