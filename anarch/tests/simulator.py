#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
r"""Exercise signed Anarch in the real PXA product simulator (Python 3 + Pillow).

From the pxa-projects root, after building the signed package::

    python3 local/pxa-apps/anarch/tests/simulator.py \
      --simulator local/artifacts/doom/20261023-140500/simulator-build/pxsys_product_simulator \
      --package local/artifacts/anarch/BUILD/release/pxa-anarch \
      --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der \
      --output local/artifacts/anarch/RUN/simulator --capture-audio

Runs with SDL dummy video at 320x240 and isolated temporary Host storage. Uses
real held controller events, captures the original level and movement/turn/fire,
saves sound settings through the game menu, and checks load on process restart.
No device, build, protocol mock, or shared simulator state is used. PNGs, logs,
optional SDL S16 audio output, and results.json remain in --output. The Host
mixes at 16000 Hz mono; SDL disk driver may convert to another device format,
which is read from its log rather than assumed to match the Host mix format.
"""

import argparse
from array import array
from contextlib import contextmanager
import io
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
import traceback

from PIL import Image, ImageChops, ImageStat


FAULTS = re.compile(
    r"Guest exception|callback failed|out of bounds memory access|"
    r"unreachable instruction|surface create failed|"
    r"PXA main-loop (?:event|wait) failed|"
    r"Anarch: (?:event port failed|save failed|could not submit save|audio write failed)",
    re.IGNORECASE,
)
LOADED = re.compile(r"Anarch: save loaded settings=(\d+) progress=(\d+)")
# Keep UI/touch edges and the bottom HUD out of movement comparisons.
WORLD = (28, 22, 292, 190)
WEAPON = (110, 110, 318, 216)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def command(sock, text):
    with socket.socket(socket.AF_UNIX) as client:
        client.settimeout(8)
        client.connect(str(sock))
        client.sendall((text + "\n").encode())
        reply = bytearray()
        while chunk := client.recv(65536):
            reply.extend(chunk)
            require(len(reply) < 16 * 1024 * 1024, "Oversized simulator response")
    if reply.startswith(b"ERR"):
        raise RuntimeError(f"{text}: {reply.decode(errors='replace').strip()}")
    return bytes(reply)


def difference(a, b, box=None):
    if box:
        a, b = a.crop(box), b.crop(box)
    delta = ImageChops.difference(a.convert("RGB"), b.convert("RGB"))
    data = delta.tobytes()
    changed = sum(max(pixel) > 16 for pixel in zip(data[0::3], data[1::3], data[2::3]))
    return {"changed_fraction": changed / (delta.width * delta.height),
            "mean_absolute_error": sum(ImageStat.Stat(delta).mean) / 3}


def changed(report, name, before, after, box, minimum):
    result = difference(before, after, box)
    report["assertions"][name] = result
    require(result["changed_fraction"] > minimum, f"{name}: insufficient visual change: {result}")


class Simulator:
    def __init__(self, args, state, sock, name, report):
        self.args, self.state, self.sock, self.name, self.report = args, state, sock, name, report
        self.log_path = args.output / f"{name}.log"
        self.keys = set()
        self.proc = self.log_file = None
        self.returncode = None
        self.forced = False
        self.closed = False
        self.entry = {"name": name, "log": str(self.log_path)}

    def logs(self):
        return self.log_path.read_text(errors="replace") if self.log_path.exists() else ""

    def wait(self, predicate, description, timeout=None):
        deadline = time.monotonic() + (self.args.timeout if timeout is None else timeout)
        while True:
            value = predicate()
            if value:
                return value
            if self.proc.poll() is not None:
                raise RuntimeError(f"{self.name} exited ({self.proc.returncode}) waiting for {description}; see {self.log_path}")
            fault = FAULTS.search(self.logs())
            if fault:
                raise AssertionError(f"{self.name}: {fault.group()}; see {self.log_path}")
            if time.monotonic() >= deadline:
                raise TimeoutError(f"{self.name}: timed out waiting for {description}; see {self.log_path}")
            time.sleep(0.05)

    def log_match(self, pattern, after=0):
        regex = re.compile(pattern) if isinstance(pattern, str) else pattern
        return self.wait(lambda: regex.search(self.logs(), after), f"log {regex.pattern}")

    def __enter__(self):
        self.state.mkdir(parents=True, exist_ok=True)
        self.log_file = self.log_path.open("wb")
        argv = [str(self.args.simulator), "--package", str(self.args.package),
                "--publisher-key", str(self.args.publisher_key), "--state-root", str(self.state),
                "--width", "320", "--height", "240", "--corner-radius", "0",
                "--safe-insets", "0,0,0,0", "--pxadb-control-socket", str(self.sock)]
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
        if self.args.capture_audio:
            env.update(SDL_AUDIODRIVER="disk", SDL_DISKAUDIODELAY="32",
                       SDL_DISKAUDIOFILE=str(self.args.output / f"{self.name}-audio.s16"))
        self.entry["command"] = argv
        self.report["processes"].append(self.entry)
        try:
            self.proc = subprocess.Popen(argv, env=env, stdout=self.log_file,
                                         stderr=subprocess.STDOUT, start_new_session=True)
            self.wait(lambda: self.sock.exists(), "control socket")
            self.log_match(r"Anarch: ready ")
            time.sleep(3.1)  # Allow original initialization/menu frames to settle.
            return self
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.closed:
            return
        self.closed = True
        if self.proc is not None:
            if self.proc.poll() is None:
                try:
                    for key in tuple(self.keys):
                        command(self.sock, f"KEY_UP {key}")
                    command(self.sock, "TOUCH CANCEL_ALL")
                    # Normal deactivation avoids the old SDL/SIGTERM cleanup bug.
                    command(self.sock, "KEY HOME")
                    self.proc.wait(timeout=8)
                except (OSError, RuntimeError, subprocess.TimeoutExpired):
                    self.forced = True
                    if self.proc.poll() is None:
                        os.killpg(self.proc.pid, signal.SIGTERM)
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.forced = True
                os.killpg(self.proc.pid, signal.SIGKILL)
                self.proc.wait(timeout=3)
            self.returncode = self.proc.returncode
        if self.log_file:
            self.log_file.close()
        self.entry.update(exit_code=self.returncode, forced_shutdown=self.forced)

    def __exit__(self, exc_type, exc, tb):
        self.close()
        if exc_type is None:
            require(self.returncode == 0 and not self.forced,
                    f"{self.name}: abnormal shutdown ({self.returncode}); see {self.log_path}")
            fault = FAULTS.search(self.logs())
            require(not fault, f"{self.name}: runtime fault {fault.group() if fault else ''}; see {self.log_path}")

    @contextmanager
    def held(self, name):
        command(self.sock, f"KEY_DOWN {name}")
        self.keys.add(name)
        try:
            yield
        finally:
            command(self.sock, f"KEY_UP {name}")
            self.keys.discard(name)

    def key(self, name, hold=0.12, settle=0.2):
        with self.held(name):
            time.sleep(hold)
        time.sleep(settle)

    @contextmanager
    def touched(self, x, y):
        command(self.sock, f"TOUCH DOWN 0 {x} {y}")
        try:
            yield
        finally:
            command(self.sock, f"TOUCH UP 0 {x} {y}")

    def touch(self, x, y, hold=0.12, settle=0.2):
        with self.touched(x, y):
            time.sleep(hold)
        time.sleep(settle)

    def image(self):
        payload = command(self.sock, "SCREENSHOT AFTER_PRESENT")
        header, separator, png = payload.partition(b"\n")
        require(separator and header == f"PNG {len(png)}".encode(), "Invalid screenshot framing")
        image = Image.open(io.BytesIO(png)).convert("RGB")
        require(image.size == (320, 240), f"Unexpected screen size: {image.size}")
        return image

    def shot(self, name):
        image = self.image()
        path = self.args.output / f"{self.name}-{name}.png"
        image.save(path)
        self.report["screenshots"].append(str(path))
        return image


def check_audio(args, report):
    path = args.output / "game-audio.s16"
    require(path.is_file(), "SDL disk output was not opened")
    raw = path.read_bytes()
    require(len(raw) % 2 == 0, "Truncated S16SYS audio sample")
    logged = re.search(r"format=(S16LE|S16BE) channels=(\d+) freq=(\d+)",
                       (args.output / "game.log").read_text(errors="replace"))
    require(logged, "SDL disk driver did not report a supported S16 device format")
    audio_format, channels, rate = logged.groups()
    channels, rate = int(channels), int(rate)
    pcm = array("h", raw)
    if (audio_format == "S16LE") != (sys.byteorder == "little"):
        pcm.byteswap()
    nonzero = sum(value != 0 for value in pcm)
    peak = max((abs(value) for value in pcm), default=0)
    result = {"path": str(path), "rate": rate, "channels": channels, "format": audio_format,
              "host_mix_rate": 16000, "host_mix_channels": 1,
              "samples": len(pcm), "nonzero_samples": nonzero, "peak": peak}
    report["assertions"]["host_audio"] = result
    require(len(pcm) >= rate * channels and nonzero > 512 and peak > 64,
            f"No audible Anarch output from the Host mixer: {result}")


def gameplay(args, temporary, report):
    state = temporary / "state"
    with Simulator(args, state, temporary / "game.sock", "game", report) as sim:
        require(not LOADED.search(sim.logs()), "Fresh isolated Host unexpectedly loaded an old save")
        menu = sim.shot("menu")
        checkpoint = len(sim.logs())
        sim.key("Z")  # Default NEWGAME enters the original story intro.
        intro = sim.shot("intro")
        changed(report, "newgame_intro", menu, intro, None, 0.05)
        sim.key("X", settle=1.9)  # Intro B/cancel enters level zero, then 1.5 s start animation.
        report["assertions"]["controller_cancel_intro"] = {"used_game_b": True}
        sim.log_match(r"Anarch: setting and initializing level", after=checkpoint)
        level = sim.shot("level")
        colors = len(level.crop(WORLD).getcolors(320 * 240) or [])
        require(colors > 30, f"No full-color original level: {colors} world colors")
        report["assertions"]["original_level"] = {"world_colors": colors, "level_initialized": True}
        changed(report, "intro_to_level", intro, level, WORLD, 0.20)

        sim.key("UP", hold=0.8)
        forward = sim.shot("forward")
        changed(report, "controller_forward", level, forward, WORLD, 0.03)
        sim.key("DOWN", hold=0.8)
        backward = sim.shot("backward")
        changed(report, "controller_backward", forward, backward, WORLD, 0.03)
        sim.key("LEFT", hold=0.35)
        left = sim.shot("left")
        changed(report, "controller_left_turn", backward, left, WORLD, 0.06)
        sim.key("RIGHT", hold=0.35)
        right = sim.shot("right")
        changed(report, "controller_right_turn", left, right, WORLD, 0.06)

        # Look for the original knife attack animation while A remains held.
        idle = sim.shot("before-fire")
        changes = []
        with sim.held("Z"):
            for index in range(6):
                time.sleep(0.11)
                changes.append(difference(idle, sim.shot(f"fire-{index}"), WEAPON)["changed_fraction"])
        report["assertions"]["controller_fire"] = {"weapon_changed_fractions": changes}
        require(max(changes) > 0.015, f"A did not animate the original weapon: {changes}")

        # Dedicated jump and shoulder-strafe keys must affect the world, too.
        sim.key("A", hold=0.18, settle=0)
        jump = sim.shot("jump")
        changed(report, "controller_jump", right, jump, WORLD, 0.015)
        time.sleep(0.6)
        before_strafe = sim.shot("before-strafe")
        sim.key("Q", hold=0.45)
        strafe_left = sim.shot("strafe-left")
        changed(report, "controller_strafe_left", before_strafe, strafe_left, WORLD, 0.02)
        sim.key("W", hold=0.45)
        strafe_right = sim.shot("strafe-right")
        changed(report, "controller_strafe_right", strafe_left, strafe_right, WORLD, 0.02)
        sim.key("S")  # Next weapon is valid even when only the knife has ammo.

        # A neutral touch makes outlines visible before comparisons; exclude
        # all outlined controls from the movement crop to avoid false positives.
        sim.touch(160, 220)
        touch_idle = sim.shot("touch-idle")
        sim.touch(48, 84, hold=0.65)
        touch_forward = sim.shot("touch-forward")
        changed(report, "touch_forward", touch_idle, touch_forward, (100, 22, 252, 190), 0.03)
        sim.touch(48, 152, hold=0.65)
        touch_back = sim.shot("touch-backward")
        changed(report, "touch_backward", touch_forward, touch_back, (100, 22, 252, 190), 0.03)
        touch_changes = []
        with sim.touched(288, 112):
            for index in range(4):
                time.sleep(0.11)
                touch_changes.append(difference(touch_back, sim.shot(f"touch-fire-{index}"), WEAPON)["changed_fraction"])
        report["assertions"]["touch_fire"] = {"weapon_changed_fractions": touch_changes}
        require(max(touch_changes) > 0.015, f"Touch fire did not animate the weapon: {touch_changes}")
        sim.touch(288, 12)
        touch_menu = sim.shot("touch-menu")
        changed(report, "touch_menu", touch_back, touch_menu, None, 0.03)
        sim.touch(160, 44)  # Tap the first localized menu row: Continue / 继续.
        time.sleep(0.3)
        sim.key("S")  # Controller input hides touch outlines again.

        before_menu = sim.shot("before-menu")
        sim.key("ENTER")
        start_menu = sim.shot("start-menu")
        changed(report, "controller_start_menu", before_menu, start_menu, None, 0.03)
        sim.key("Z")  # CONTINUE is selected at index zero; original menu resumes with A.
        resumed = sim.shot("resumed")
        changed(report, "controller_continue", start_menu, resumed, None, 0.03)
        with sim.held("TAB"):
            sim.key("DOWN")  # Original mandatory C+DOWN opens the menu.
        c_menu = sim.shot("c-menu")
        changed(report, "controller_c_down_menu", resumed, c_menu, None, 0.03)
        sim.key("UP", hold=0.6)
        sim.key("Z")
        sim.key("BACK")  # Host back from gameplay must open menu, keeping the app alive.
        sim.shot("system-back-menu")
        require(sim.proc.poll() is None, "Host BACK closed active gameplay")

        # Fresh level zero has CONTINUE, MAP, NEWGAME, SOUND; LOAD is absent until
        # a later-level checkpoint is saved. UP pins index zero without wrapping.
        sim.key("UP", hold=1.0)
        for _ in range(3):
            sim.key("DOWN")
        sound_before = sim.shot("sound-before")
        checkpoint = len(sim.logs())
        sim.key("Z")
        sim.log_match(r"Anarch: sound changed", after=checkpoint)
        sim.log_match(r"Anarch: progress/settings saved", after=checkpoint)
        sound_after = sim.shot("sound-after")
        changed(report, "sound_setting_visual", sound_before, sound_after, None, 0.001)
        # The pinned core starts at settings 0x03; one SOUND activation wraps
        # its low two bits to zero, preserving the remaining default bits.
        report["assertions"]["save_settings"] = {"saved": True, "expected_settings": 0}
        report["assertions"]["runtime_faults"] = {"found": False}

    if args.capture_audio:
        check_audio(args, report)
    with Simulator(args, state, temporary / "reload.sock", "reload", report) as sim:
        loaded = sim.log_match(LOADED)
        settings, progress = map(int, loaded.groups())
        require(settings == 0, f"Sound setting was not restored across restart: {settings}")
        sim.log_match(r"Anarch: settings loaded")
        sim.shot("loaded-menu")
        report["assertions"]["relaunch_load"] = {"passed": True, "settings": settings, "progress": progress,
                                                  "same_host_storage": True}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    for name in ("simulator", "package", "publisher-key", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--capture-audio", action="store_true", help="Require audible S16 PCM at SDL's logged device rate/channels")
    parser.add_argument("--timeout", type=float, default=25, help="Seconds per startup/log wait (default: 25)")
    args = parser.parse_args(argv)
    for name in ("simulator", "package", "publisher_key", "output"):
        setattr(args, name, getattr(args, name).expanduser().resolve())
    require(args.timeout > 0, "--timeout must be positive")
    require(args.simulator.is_file() and os.access(args.simulator, os.X_OK), f"Simulator unavailable: {args.simulator}")
    require(args.package.is_dir() and (args.package / "manifest.pxm").is_file(), f"Signed package unavailable: {args.package}")
    require(args.publisher_key.is_file(), f"Publisher public key unavailable: {args.publisher_key}")
    args.output.mkdir(parents=True, exist_ok=True)
    require(not any(args.output.iterdir()), f"--output must be empty: {args.output}")
    report = {"passed": False, "simulator": str(args.simulator), "package": str(args.package),
              "publisher_key": str(args.publisher_key), "assertions": {}, "processes": [], "screenshots": []}
    began = time.monotonic()
    try:
        with tempfile.TemporaryDirectory(prefix="pxa-anarch-") as directory:
            gameplay(args, Path(directory), report)
        report["passed"] = True
    except Exception as error:
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        print(f"FAIL: {error}", file=sys.stderr, flush=True)
    finally:
        report["elapsed_seconds"] = round(time.monotonic() - began, 3)
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{'PASS' if report['passed'] else 'FAIL'}: {args.output / 'results.json'}", flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
