#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Exercise signed pocket games in the real product simulator, with Pillow.

Run from the workspace root, passing --simulator, --package (expanded signed
package directory), --publisher-key, --game {2048,chess,sokoban}, and --output
(an ignored local/artifacts directory). Tests actual controller/touch frames,
paused pointer cancellation, and persisted board restoration across processes.
The simulator must support its --pxadb-control-socket interface.
"""
import argparse
import io
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import tempfile
import time
import traceback
from PIL import Image, ImageChops

FAULTS = re.compile(r"Guest exception|callback failed|out of bounds memory access|unreachable instruction|Pocket: .*failed|exit flush timed out", re.I)

def require(ok, text):
    if not ok:
        raise AssertionError(text)

def command(sock, text):
    with socket.socket(socket.AF_UNIX) as client:
        client.settimeout(8)
        client.connect(str(sock))
        client.sendall((text + "\n").encode())
        data = bytearray()
        while chunk := client.recv(65536):
            data.extend(chunk)
            require(len(data) < 16 * 1024 * 1024, "Oversized response")
    require(not data.startswith(b"ERR"), f"{text}: {data[:256]!r}")
    return bytes(data)

def difference(a, b):
    r, g, b = ImageChops.difference(a.convert("RGB"), b.convert("RGB")).split()
    maximum = ImageChops.lighter(ImageChops.lighter(r, g), b)
    return sum(maximum.histogram()[17:]) / (a.width * a.height)

class Simulator:
    def __init__(self, args, state, sock, label, report):
        self.args, self.state, self.sock, self.label, self.report = args, state, sock, label, report
        self.path = args.output / (label + ".log")
        self.proc = self.log = None
        self.closed = False
        self.keys = set()
    def logs(self):
        return self.path.read_text(errors="replace")
    def wait(self, predicate, label, timeout=30):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            value = predicate()
            if value:
                return value
            require(self.proc.poll() is None, f"Simulator exited waiting for {label}")
            require(not FAULTS.search(self.logs()), f"Runtime fault; see {self.path}")
            time.sleep(.05)
        raise TimeoutError(label)
    def __enter__(self):
        self.state.mkdir(parents=True, exist_ok=True)
        argv = [str(self.args.simulator), "--package", str(self.args.package),
                "--publisher-key", str(self.args.publisher_key), "--state-root", str(self.state),
                "--width", "320", "--height", "240", "--corner-radius", "0",
                "--safe-insets", "0,0,0,0", "--pxadb-control-socket", str(self.sock)]
        self.log = self.path.open("wb")
        self.proc = subprocess.Popen(argv, stdout=self.log, stderr=subprocess.STDOUT,
                                     env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy"), start_new_session=True)
        self.report["processes"].append({"command": argv, "log": str(self.path)})
        try:
            self.wait(lambda: self.sock.exists(), "control socket")
            self.wait(lambda: "Pocket: ready app=" in self.logs(), "Pocket startup")
            # Use the actual runtime viewport: current Hosts may fit the
            # 288x224 game canvas to a larger physical Surface. Inputs below
            # stay in game coordinates and are projected into that viewport.
            geometry = re.search(r"viewport canvas=(\d+)x(\d+) origin=(-?\d+),(-?\d+) size=(\d+)x(\d+) display=(\d+)x(\d+)", self.logs())
            require(geometry is not None, "Missing actual canvas geometry")
            self.canvas_w, self.canvas_h, self.origin_x, self.origin_y, self.viewport_w, self.viewport_h, dw, dh = map(int, geometry.groups())
            self.display_w, self.display_h = dw, dh
            require((self.canvas_w, self.canvas_h, dw, dh) == (288, 224, 320, 240), "Unexpected canvas/display geometry")
            require(self.viewport_w > 0 and self.viewport_h > 0 and self.origin_x >= 0 and self.origin_y >= 0 and
                    self.origin_x + self.viewport_w <= dw and self.origin_y + self.viewport_h <= dh,
                    "Viewport outside the display")
            time.sleep(.45)
            return self
        except BaseException:
            self.close()
            raise
    def key(self, key, settle=.4):
        command(self.sock, "KEY_DOWN " + key)
        self.keys.add(key)
        time.sleep(.07)
        command(self.sock, "KEY_UP " + key)
        self.keys.remove(key)
        time.sleep(settle)
    def pointer(self, phase, x, y):
        # Runtime mapping floors physical pixels back to logical coordinates.
        # Choose the first physical pixel inside the requested logical cell.
        px = self.origin_x + (x * self.viewport_w + self.canvas_w - 1) // self.canvas_w
        py = self.origin_y + (y * self.viewport_h + self.canvas_h - 1) // self.canvas_h
        # Physical touch reports remain inside the display even when a finger
        # leaves the application canvas; use its nearest edge for UP/CANCEL.
        px = min(max(px, 0), self.display_w - 1)
        py = min(max(py, 0), self.display_h - 1)
        command(self.sock, f"TOUCH {phase} 1 {px} {py}")
    def tap(self, x, y, settle=.4):
        self.pointer("DOWN", x, y)
        time.sleep(.07)
        self.pointer("UP", x, y)
        time.sleep(settle)
    def swipe(self, x, y, dx, dy):
        self.pointer("DOWN", x, y)
        self.pointer("MOVE", x+dx, y+dy)
        self.pointer("UP", x+dx, y+dy)
        time.sleep(.45)
    def shot(self, label):
        require(not FAULTS.search(self.logs()), f"Runtime fault; see {self.path}")
        header, separator, data = command(self.sock, "SCREENSHOT AFTER_PRESENT").partition(b"\n")
        require(separator and header == f"PNG {len(data)}".encode(), "Invalid PNG framing")
        image = Image.open(io.BytesIO(data)).convert("RGB")
        require(image.size == (320, 240), "Unexpected display geometry")
        image.save(self.args.output / f"{self.label}-{label}.png")
        return image
    def close(self):
        if self.closed:
            return
        self.closed = True
        if self.proc and self.proc.poll() is None:
            try:
                for key in self.keys:
                    command(self.sock, "KEY_UP " + key)
                command(self.sock, "TOUCH CANCEL_ALL")
                command(self.sock, "KEY HOME")
                self.proc.wait(timeout=8)
            except (OSError, AssertionError, subprocess.TimeoutExpired):
                os.killpg(self.proc.pid, signal.SIGTERM)
                self.proc.wait(timeout=3)
        if self.log:
            self.log.close()
    def __exit__(self, typ, value, tb):
        self.close()
        if typ is None:
            require(self.proc.returncode == 0, f"Abnormal simulator exit {self.proc.returncode}")
            require(not FAULTS.search(self.logs()), f"Runtime fault; see {self.path}")


def exercise(args, temp, report):
    board = {"2048": (20, 46, 200, 226), "chess": (24, 32, 216, 224), "sokoban": (20, 48, 200, 228)}[args.game]
    def cropped(sim, name):
        x0, y0, x1, y1 = board
        box = (sim.origin_x + x0 * sim.viewport_w // sim.canvas_w,
               sim.origin_y + y0 * sim.viewport_h // sim.canvas_h,
               sim.origin_x + (x1 * sim.viewport_w + sim.canvas_w - 1) // sim.canvas_w,
               sim.origin_y + (y1 * sim.viewport_h + sim.canvas_h - 1) // sim.canvas_h)
        return sim.shot(name).crop(box)
    with Simulator(args, temp / "state", temp / "one.sock", "game", report) as sim:
        require("Pocket: load completed" not in sim.logs(), "Fresh storage loaded old game")
        first = cropped(sim, "initial")
        if args.game == "2048":
            for key in ("LEFT", "UP", "RIGHT", "DOWN"):
                before = cropped(sim, "before-"+key)
                sim.key(key)
                after = cropped(sim, "after-"+key)
                if difference(before, after) > .01:
                    break
            require(difference(before, after) > .01, "Controller did not move board")
            sim.key("X")  # Simulator X maps to game B/undo.
            require(difference(before, cropped(sim, "undo")) == 0, "Undo failed to restore exact board")
            for dx, dy in ((70, 0), (0, 70), (-70, 0), (0, -70)):
                before = cropped(sim, "before-swipe")
                sim.swipe(90, 120, dx, dy)
                if difference(before, cropped(sim, "swipe")) > .01:
                    break
            require(difference(before, cropped(sim, "touch")) > .01, "Touch swipe did not move board")
        elif args.game == "chess":
            sim.key("Z")  # A/select initial e2 cursor.
            selected = cropped(sim, "selected")
            require(difference(first, selected) > .001, "Controller selection/highlights absent")
            sim.key("UP"); sim.key("UP"); sim.key("Z", settle=3.0)  # e2-e4 + bounded AI reply.
            after = cropped(sim, "ai-reply")
            require(difference(first, after) > .015, "Legal move/AI did not change pieces")
            sim.tap(246, 184)  # Y/local two players.
            sim.tap(8+3*24+12, 24+6*24+12)  # d2
            sim.tap(8+3*24+12, 24+4*24+12)  # d4
            require(difference(after, cropped(sim, "touch-move")) > .01, "Touch legal move absent")
        else:
            # Tutorial 1: player (3,4), crate (4,4), goal (6,4).
            sim.key("RIGHT")
            require(difference(first, cropped(sim, "controller-push")) > .001, "Controller push did not move crate")
            sim.key("X")  # B = undo
            require(difference(first, cropped(sim, "controller-undo")) == 0, "Undo did not restore player and crate")
            sim.swipe(80, 120, 45, 0)
            sim.swipe(80, 120, 45, 0)
            solved = cropped(sim, "touch-solved")
            sim.tap(236, 200)  # CONTINUE after solving
            require(difference(solved, cropped(sim, "next-level")) > .01, "Solved level did not unlock next board")
            report["checks"].append("push_undo_touch_swipe_solve_unlock")
        report["checks"].append("controller_and_touch")
        before = cropped(sim, "before-menu")
        if args.game == "2048":
            sim.pointer("DOWN", 80, 120)
            sim.key("ENTER"); sim.key("ENTER")
            sim.pointer("MOVE", 150, 120); sim.pointer("UP", 150, 120)
            time.sleep(.4)
            require(difference(before, cropped(sim, "cancelled-gesture")) == 0, "Cancelled pointer stream leaked after resume")
            report["checks"].append("cancelled_pointer_stays_cancelled")
        sim.tap(276 if args.game == "chess" else 252, 10)
        paused = cropped(sim, "paused")
        require(difference(before, paused) > .1, "Touch menu did not open")
        sim.tap(140, 85)
        require(difference(before, cropped(sim, "continue")) == 0, "Continue changed board")
        report["checks"].append("touch_pause_continue")
        sim.wait(lambda: "Pocket: saved" in sim.logs(), "save acknowledgment")
        saved = cropped(sim, "saved")
        sim.tap(276 if args.game == "chess" else 252, 10)
        # EXIT closes on DOWN, so the control socket may be gone before UP.
        sim.pointer("DOWN", 140, 145)
        sim.wait(lambda: sim.proc.poll() is not None, "menu EXIT")
        require(sim.proc.returncode == 0, "Menu EXIT did not stop normally")
        report["checks"].append("menu_exit_flush")
    with Simulator(args, temp / "state", temp / "two.sock", "reload", report) as sim:
        require("Pocket: load completed" in sim.logs(), "No saved-state load")
        require(difference(saved, cropped(sim, "restored")) == 0, "Restored board differs from durable board")
        report["checks"].append("saved_board_restores_across_processes")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--simulator", required=True, type=Path)
    p.add_argument("--package", required=True, type=Path)
    p.add_argument("--publisher-key", required=True, type=Path)
    p.add_argument("--game", required=True, choices=("2048", "chess", "sokoban"))
    p.add_argument("--output", required=True, type=Path)
    args = p.parse_args()
    for name in ("simulator", "package", "publisher_key", "output"):
        setattr(args, name, getattr(args, name).resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "game": args.game, "checks": [], "processes": []}
    try:
        with tempfile.TemporaryDirectory(prefix="pg-sim-") as temporary:
            exercise(args, Path(temporary), report)
        report["passed"] = True
    except BaseException:
        report["error"] = traceback.format_exc()
        raise
    finally:
        (args.output / "results.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
