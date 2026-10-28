#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise real PXA UI flows and retain original screenshots under --output."""
import argparse
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time


class Simulator:
    def __init__(self, workspace, packages, output, app, width, height, scenario):
        self.output = output
        self.stem = f"{app}-{width}x{height}-{scenario}"
        states = output / "state"
        states.mkdir(exist_ok=True)
        self.state = Path(tempfile.mkdtemp(prefix="run-", dir=states))
        self.socket = f"/tmp/pxa-study-{os.getpid()}.sock"
        Path(self.socket).unlink(missing_ok=True)
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
        env.pop("PXA_SIM_NAV_MODE", None)  # Exercise the compiled default.
        command = [
            str(workspace / "build/simulator/doit-gamekit-1/pxsys_product_simulator"),
            "--package", str(packages / ("pxa-" + app)),
            "--publisher-key", str(workspace / "deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der"),
            "--state-root", str(self.state), "--pxadb-control-socket", self.socket,
            "--width", str(width), "--height", str(height), "--density-dpi", "125",
            "--locale", "zh-CN",
        ]
        self.log = (output / (self.stem + ".log")).open("w")
        self.process = subprocess.Popen(command, stdout=self.log, stderr=subprocess.STDOUT, env=env)

    def __enter__(self):
        try:
            for _ in range(80):
                if self.process.poll() is not None:
                    raise RuntimeError(f"{self.stem}: simulator exited {self.process.returncode}")
                if Path(self.socket).exists():
                    time.sleep(.8)
                    return self
                time.sleep(.1)
            raise TimeoutError(f"{self.stem}: control socket was not created")
        except BaseException:
            self.__exit__(None, None, None)
            raise

    def __exit__(self, *_):
        self.process.terminate()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.log.close()
        Path(self.socket).unlink(missing_ok=True)

    def command(self, command):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(8)
            connection.connect(self.socket)
            connection.sendall((command + "\n").encode())
            with connection.makefile("rb") as stream:
                header = stream.readline()
                if header.startswith(b"PNG "):
                    length = int(header.split()[1])
                    data = stream.read(length)
                    assert len(data) == length and data.startswith(b"\x89PNG\r\n\x1a\n")
                    return data
                assert header.strip() == b"OK", header

    def tap(self, x, y):
        self.command(f"TAP {int(x)} {int(y)}")
        time.sleep(.3)

    def screenshot(self, name):
        path = self.output / (self.stem + "-" + name + ".png")
        path.write_bytes(self.command("SCREENSHOT"))
        return path

    def stats(self):
        # The Host uses alternating generation-numbered KV snapshots. Observe
        # persistent output, rather than guessing success from a screenshot.
        snapshots = []
        for path in self.state.rglob(".pxa-kv-*"):
            data = path.read_bytes()
            offset = data.find(b"STDY")
            if offset >= 0:
                generation = struct.unpack_from("<Q", data, 8)[0]
                stats = struct.unpack_from("<II", data, offset + 12)
                snapshots.append((generation, stats))
        return max(snapshots)[1] if snapshots else (0, 0)


def online_flow(workspace, packages, output, width, height, ocr):
    # Seed an ordinary private cache file. This exercises the real app's async
    # FS, JSON, UI and persistence without depending on a public API being up.
    with Simulator(workspace, packages, output, "learn-words-preschool", width, height, "online") as sim:
        private = next((sim.state / "private-files").iterdir())
        address = "https://api.dictionaryapi.dev/api/v2/entries/en/apple"
        hashed = 2166136261
        for byte in address.encode():
            hashed = ((hashed ^ byte) * 16777619) & 0xffffffff
        payload = [{"phonetic":"/ˈæp.əl/", "meanings":[{"definitions":[{"definition":"Cached entry."}]}],
                    "license":{"name":"CC BY-SA"}}]
        cache = private / f"cache-{hashed % 8}.bin"
        cache.write_text(address + "\n" + json.dumps(payload))
        original, modified = cache.read_bytes(), cache.stat().st_mtime_ns
        sim.tap(width * .75, 180)
        sim.tap(width * .75, height - 54)
        time.sleep(.5)
        sim.screenshot("cached-front")
        # A one-line cache notice sits below the card actions.
        sim.tap(width / 4, height - 72)
        sim.screenshot("answer-with-online")
        assert sim.stats() == (0, 0)
        sim.tap(width - 35, 53)
        reader = []
        for index in range(3):
            path = sim.screenshot(f"combined-reader-{index + 1}")
            if ocr:
                result = subprocess.run(["tesseract", str(path), "stdout", "-l", "chi_sim+eng", "--psm", "6"],
                                        capture_output=True, text=True, check=True)
                path.with_suffix(".txt").write_text(result.stdout)
                reader.append(result.stdout)
            sim.tap(width * .75, height - 29)
        if ocr:
            combined = "".join("".join(reader).split()).casefold()
            # The original apple card includes this English example. Match
            # clear Latin text: tiny CJK/IPA glyphs are unreliable OCR targets.
            assert "fruit" in combined and "cachedentry" in combined and "wiktionary" in combined, combined
        sim.tap(width - 30, 25)  # Return to the same revealed card.
        sim.tap(width * .75, height - 54)
        time.sleep(.3)
        assert sim.stats() == (1, 1), sim.stats()
        sim.screenshot("rated-next-card")
        assert cache.read_bytes() == original and cache.stat().st_mtime_ns == modified


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--package-root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=("smoke", "flow", "online", "all"), default="all")
    parser.add_argument("--ocr", action="store_true", help="Verify combined online/answer text using Tesseract chi_sim+eng")
    args = parser.parse_args()
    if args.ocr and not shutil.which("tesseract"):
        parser.error("--ocr requires Tesseract with chi_sim and eng language data")
    workspace = args.workspace.resolve()
    packages = (args.package_root or workspace / "local/app-output/doit-gamekit-1").resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for width, height in ((320, 240), (240, 284)):
        if args.mode in ("all", "smoke"):
            for age in ("preschool", "primary", "teen", "adult"):
                for subject in ("math", "poetry", "words", "hanzi", "cards"):
                    app = f"learn-{subject}-{age}"
                    with Simulator(workspace, packages, output, app, width, height, "smoke") as sim:
                        sim.screenshot("home")
                    rows.append(dict(app=app, viewport=[width, height], flow="startup", result="passed"))
        if args.mode in ("all", "flow"):
            with Simulator(workspace, packages, output, "learn-math-preschool", width, height, "flow") as sim:
                sim.tap(width / 4, 180)
                for index in range(10):
                    sim.tap(width / 4, 162)
                    if index == 0:
                        sim.screenshot("feedback")
                    sim.tap(width * .75, height - 25)
                sim.screenshot("completion")
                answered, correct = sim.stats()
                assert answered == 10 and 0 <= correct <= 10, (width, height, answered, correct)
                sim.tap(width * .75, height - 25)
                sim.screenshot("new-round")
                sim.tap(width / 4, 162)
                time.sleep(.3)
                assert sim.stats()[0] == 11, (width, height, sim.stats())
            rows.append(dict(app="learn-math-preschool", viewport=[width, height], flow="round/restart/save", result="passed"))
            with Simulator(workspace, packages, output, "learn-hanzi-preschool", width, height, "flow") as sim:
                sim.tap(width * .75, 180)
                sim.screenshot("front")
                sim.tap(width / 4, height - 54)
                sim.screenshot("answer")
                sim.tap(width - 95, 53)
                assert sim.stats() == (0, 0)
                sim.tap(width / 4, height - 54)
                sim.tap(width * .75, height - 54)
                time.sleep(.3)
                sim.screenshot("next-card")
                assert sim.stats() == (1, 1), (width, height, sim.stats())
            rows.append(dict(app="learn-hanzi-preschool", viewport=[width, height], flow="reveal/hide/rate/save", result="passed"))
        if args.mode in ("all", "online"):
            online_flow(workspace, packages, output, width, height, args.ocr)
            rows.append(dict(app="learn-words-preschool", viewport=[width, height], flow="cache/reveal/combined-reader/rate", result="passed"))
        print(json.dumps(dict(viewport=[width, height], passed=len(rows))), flush=True)
    (output / "results.json").write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n")
    print(f"Study simulator: {len(rows)} real runtime checks passed", flush=True)


if __name__ == "__main__":
    main()
