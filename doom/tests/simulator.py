#!/usr/bin/env python3
r"""Test signed DOOM in the actual PXA product simulator (Python 3 + Pillow).

From the pxa-projects root, after building a portable or desktop package::

    python3 local/pxa-apps/doom/tests/simulator.py \
      --simulator build/simulator/doit-gamekit-1/pxsys_product_simulator \
      --package local/artifacts/doom/BUILD/build/pxa-doom \
      --publisher-key local/pxa-apps/.dev-signing/publisher-public.der \
      --wad local/pxa-apps/.game-data/doom/files/doom1.wad \
      --output local/artifacts/doom/RUN/simulator

No device, package builder, protocol mock, or shared simulator state is used.
Each state root and control socket is temporary. Only PNG screenshots, runtime
logs, and results.json survive in --output. The default includes save/load over
process restart; --skip-persistence omits only that final restart check.
"""

import argparse
from array import array
from contextlib import contextmanager
import hashlib
import io
import json
import os
from pathlib import Path
import re
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import traceback

from PIL import Image, ImageChops, ImageStat


STATS = re.compile(
    r"DOOM state=(?P<state>-?\d+) menu=(?P<menu>\d+) tic=(?P<tic>\d+) "
    r"map=(?P<map>\d+) hp=(?P<hp>-?\d+) x=(?P<x>-?\d+) y=(?P<y>-?\d+)"
)
FAULTS = re.compile(
    r"Guest exception|callback failed|surface create failed|failed at|"
    r"PXA main-loop (?:event|wait) failed|DOOM save failed|"
    r"out of bounds memory access|unreachable instruction", re.IGNORECASE
)
MEDIA = re.compile(r"PXA SIM app-media-root=([^\r\n]+)")
# Logical 320x240 controls, from doom/controls.c and recrof/framebuffer.c.
BUTTONS = {"MENU": (19, 12), "UP": (300, 12), "DOWN": (298, 162), "OK": (294, 216)}


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
    pixels = zip(data[0::3], data[1::3], data[2::3])
    changed = sum(max(pixel) > 16 for pixel in pixels) / (delta.width * delta.height)
    return {"changed_fraction": changed, "mean_absolute_error": sum(ImageStat.Stat(delta).mean) / 3}


def missing_wad_text(image):
    # The multiline port_error message may not reach the Host log service.
    # Match its actual first line using Daniel Hepper's public-domain 8x8
    # glyphs (the same error font as recrof/font8x8.c), not text/color counts.
    glyphs = {
        " ": "0000000000000000", ".": "00000000000c0c00",
        "A": "0c1e33333f333300", "D": "1f36666666361f00",
        "I": "1e0c0c0c0c0c1e00", "M": "63777f7f6b636300",
        "N": "63676f7b73636300", "O": "1c36636363361c00",
        "W": "6363636b7f776300", "d": "3830303e33336e00",
        "f": "1c36060f06060f00", "n": "00001f3333333300",
        "o": "00001e3333331e00", "u": "0000333333336e00",
    }
    text = "No DOOM IWAD found."
    expected = Image.new("L", (len(text) * 8, 8))
    for index, char in enumerate(text):
        for y, row in enumerate(bytes.fromhex(glyphs[char])):
            for x in range(8):
                if row & (1 << x):
                    expected.putpixel((index * 8 + x, y), 255)
    actual = image.crop((8, 38, 8 + expected.width, 46)).convert("L").point(lambda value: 255 if value > 200 else 0)
    return ImageChops.difference(actual, expected).getbbox() is None


def wad_title(path):
    """Decode the supplied IWAD's TITLEPIC, giving a content-specific oracle."""
    data = path.read_bytes()
    require(12 <= len(data) <= 20 * 1024 * 1024 and data[:4] == b"IWAD", "--wad must be a valid shareware IWAD")
    count, directory = struct.unpack_from("<II", data, 4)
    require(0 < count <= 65536 and 12 <= directory <= len(data) - 16 * count, "Invalid WAD directory")
    lumps = {}
    for index in range(count):
        offset, length, name = struct.unpack_from("<II8s", data, directory + 16 * index)
        require(offset <= len(data) and length <= len(data) - offset, "Invalid WAD lump")
        lumps[name.rstrip(b"\0")] = data[offset:offset + length]
    require(b"E1M1" in lumps and b"TITLEPIC" in lumps and b"PLAYPAL" in lumps, "Use doom1.wad with E1M1, TITLEPIC and PLAYPAL")
    patch, palette = lumps[b"TITLEPIC"], lumps[b"PLAYPAL"]
    width, height = struct.unpack_from("<HH", patch)
    require((width, height) == (320, 200) and len(palette) >= 768, "Unexpected TITLEPIC dimensions/palette")
    indexed = bytearray(width * height)
    for x in range(width):
        cursor = struct.unpack_from("<I", patch, 8 + 4 * x)[0]
        for _ in range(height + 1):
            require(cursor < len(patch), "Truncated TITLEPIC column")
            top = patch[cursor]
            if top == 255:
                break
            length = patch[cursor + 1]
            require(top + length <= height and cursor + length + 4 <= len(patch), "Invalid TITLEPIC post")
            for y, color in enumerate(patch[cursor + 3:cursor + 3 + length], top):
                indexed[y * width + x] = color
            cursor += length + 4
        else:
            raise AssertionError("Unterminated TITLEPIC column")
    title = Image.frombytes("P", (width, height), bytes(indexed))
    title.putpalette(palette[:768])
    return title.convert("RGB"), hashlib.sha256(data).hexdigest()


class Simulator:
    def __init__(self, args, state, sock, name, report):
        self.args, self.state, self.sock, self.name, self.report = args, state, sock, name, report
        self.log_path = args.output / f"{name}.log"
        self.keys = set()
        self.proc = None
        self.log_file = None
        self.returncode = None
        self.forced = False

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

    def log_match(self, pattern, timeout=None, after=0):
        regex = re.compile(pattern) if isinstance(pattern, str) else pattern
        return self.wait(lambda: regex.search(self.logs(), after), f"log {regex.pattern}", timeout)

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
        self.report["processes"].append({"name": self.name, "command": argv, "log": str(self.log_path)})
        try:
            self.proc = subprocess.Popen(argv, env=env, stdout=self.log_file, stderr=subprocess.STDOUT, start_new_session=True)
            self.wait(lambda: self.sock.exists(), "control socket")
            self.log_match(MEDIA)
            return self
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.proc is not None:
            if self.proc.poll() is None:
                try:
                    for key in tuple(self.keys):
                        command(self.sock, f"KEY_UP {key}")
                    command(self.sock, "TOUCH CANCEL_ALL")
                    # Normal PXA deactivation avoids the old SDL/SIGTERM touch cleanup bug.
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
        entry = self.report["processes"][-1]
        entry.update(exit_code=self.returncode, forced_shutdown=self.forced)

    def __exit__(self, exc_type, exc, tb):
        self.close()
        if exc_type is None:
            require(self.returncode == 0 and not self.forced, f"{self.name}: abnormal shutdown ({self.returncode}); see {self.log_path}")
            require(not FAULTS.search(self.logs()), f"{self.name}: runtime fault; see {self.log_path}")

    def media_root(self):
        # desktop_media_root.h hashes PXA-APPKEY-1 + lineage root key ID +
        # uint16le(app-ID length) + app ID. Trust the actual Host binding instead
        # of inferring publisher identity from a DER key or a directory nickname.
        root = Path(self.log_match(MEDIA).group(1)).resolve()
        require(root.parent == self.state.resolve() / "media" and re.fullmatch(r"[0-9a-f]{64}", root.name), f"Unexpected app-media-root: {root}")
        require(root.is_dir(), "Logged media root does not exist")
        return root

    def key(self, name, hold=0.12, settle=0.25):
        with self.held(name):
            time.sleep(hold)
        time.sleep(settle)

    @contextmanager
    def held(self, name):
        command(self.sock, f"KEY_DOWN {name}")
        self.keys.add(name)
        try:
            yield
        finally:
            command(self.sock, f"KEY_UP {name}")
            self.keys.discard(name)

    def tap(self, x, y):
        command(self.sock, f"TOUCH DOWN 0 {x} {y}")
        try:
            time.sleep(0.12)
        finally:
            command(self.sock, f"TOUCH UP 0 {x} {y}")
        time.sleep(0.3)

    def button(self, name):
        self.tap(*BUTTONS[name])

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

    def stats(self, after=0, state=None, menu=None):
        def latest():
            for match in reversed(list(STATS.finditer(self.logs()))):
                values = {key: int(value) for key, value in match.groupdict().items()}
                if match.end() > after and (state is None or values["state"] == state) and (menu is None or values["menu"] == menu):
                    values["log_end"] = match.end()
                    return values
            return None
        return self.wait(latest, f"fresh DOOM state={state} menu={menu}", timeout=max(8, self.args.timeout))


def title_assertion(actual, expected):
    # Compare a spatial grid against the supplied WAD, excluding overlay edges.
    # RGB565 quantization and gamma make exact PNG hashes inappropriate.
    scaled = expected.resize((320, 240), Image.Resampling.NEAREST)
    errors = []
    for y in range(40, 190, 3):
        for x in range(44, 265, 3):
            errors.append(sum(abs(a - b) for a, b in zip(actual.getpixel((x, y)), scaled.getpixel((x, y)))) / 3)
    mean = sum(errors) / len(errors)
    matching = sum(error < 24 for error in errors) / len(errors)
    require(mean < 24 and matching > 0.75, f"Screen does not match IWAD TITLEPIC: mean error={mean:.2f}, matching={matching:.3f}")
    return {"mean_absolute_error": mean, "matching_fraction": matching, "samples": len(errors)}


def changed(report, name, a, b, box, minimum):
    result = difference(a, b, box)
    report["assertions"][name] = result
    require(result["changed_fraction"] > minimum, f"{name}: insufficient frame change: {result}")


def discover(args, state, sock, name, report):
    with Simulator(args, state, sock, name, report) as sim:
        root = sim.media_root()
    return root


def save_file(state):
    # product_runner.c mounts private-files/<publisher_key_id>-<app_id>;
    # recrof strips the leading / from /data/doomsav0.dsg. Enumerate the
    # Host-created directory rather than assuming DER/root-key hashing rules.
    private = state / "private-files"
    roots = [p for p in private.iterdir() if p.is_dir() and re.fullmatch(r"[0-9a-f]{64}-pxa-doom", p.name)]
    require(len(roots) == 1, f"Expected one Host-created Doom private FS: {roots}")
    return roots[0] / "data" / "doomsav0.dsg"


def gameplay(args, tmp, report, expected_title):
    state, sock = tmp / "game", tmp / "game.sock"
    media = discover(args, state, sock, "game-discover", report)
    destination = media / "doom1.wad"
    destination.write_bytes(args.wad.read_bytes())
    report["media_root"] = str(media)
    with Simulator(args, state, sock, "game", report) as sim:
        require(sim.media_root() == media, "Media binding changed across relaunch")
        sim.log_match(r"DOOM ready IWAD=doom1\.wad")
        time.sleep(0.4)
        title = sim.shot("title")
        report["assertions"]["wad_titlepic"] = title_assertion(title, expected_title)
        # ENTER is controller Start, not the game's confirm key. Z is A.
        sim.key("ENTER")
        menu = sim.shot("main-menu")
        changed(report, "title_to_menu", title, menu, (40, 45, 270, 215), 0.03)
        sim.key("Z")
        episode = sim.shot("episode")
        changed(report, "new_game_episode", menu, episode, (40, 45, 270, 215), 0.02)
        sim.key("Z")
        skill = sim.shot("skill")
        changed(report, "episode_to_skill", episode, skill, (40, 45, 270, 215), 0.02)
        # Skill starts at Hurt Me Plenty; choose the easiest for repeatable movement.
        sim.key("UP")
        sim.key("UP")
        sim.key("Z", settle=0.8)
        first = sim.stats(state=0, menu=0)
        require(first["map"] == 1 and first["hp"] > 0, f"No live E1M1 player: {first}")
        start = sim.shot("level")
        changed(report, "title_to_level", title, start, (40, 40, 270, 190), 0.35)
        with sim.held("UP"):
            time.sleep(0.9)
        forward = sim.stats(after=first["log_end"], state=0, menu=0)
        moved = sim.shot("forward")
        delta_up = (forward["x"] - first["x"], forward["y"] - first["y"])
        require(sum(v * v for v in delta_up) > 16, f"UP did not move player: {first} -> {forward}")
        changed(report, "movement_view", start, moved, (45, 40, 265, 150), 0.08)
        with sim.held("DOWN"):
            time.sleep(0.9)
        backward = sim.stats(after=forward["log_end"], state=0, menu=0)
        delta_down = (backward["x"] - forward["x"], backward["y"] - forward["y"])
        require(sum(v * v for v in delta_down) > 16 and sum(a * b for a, b in zip(delta_up, delta_down)) < 0,
                f"DOWN did not reverse movement: up={delta_up} down={delta_down}")
        require(backward["tic"] > forward["tic"] > first["tic"], "Game tics did not advance")
        report["assertions"]["movement"] = {"initial": first, "forward": forward, "backward": backward, "up_delta": delta_up, "down_delta": delta_down}
        before_turn = sim.shot("before-turn")
        sim.key("LEFT", hold=0.35)
        turned_left = sim.shot("left-turn")
        sim.key("RIGHT", hold=0.35)
        turned_right = sim.shot("right-turn")
        changed(report, "controller_left_turn", before_turn, turned_left, (45, 40, 265, 150), 0.08)
        changed(report, "controller_right_turn", turned_left, turned_right, (45, 40, 265, 150), 0.08)
        idle = sim.shot("before-fire")
        fire_frames = []
        with sim.held("Z"):
            for index in range(5):
                time.sleep(0.16)
                fire_frames.append(sim.shot(f"fire-{index}"))
        time.sleep(0.5)
        after_fire = sim.shot("after-fire")
        weapon_changes = [difference(idle, frame, (120, 100, 210, 201))["changed_fraction"] for frame in fire_frames]
        require(max(weapon_changes) > 0.025, f"Controller A did not animate the weapon: {weapon_changes}")
        report["assertions"]["fire_weapon"] = weapon_changes
        changed(report, "fire_ammo_hud", idle, after_fire, (0, 202, 43, 239), 0.015)
        # Pause/menu via Start. World position must stay fixed while tics advance.
        checkpoint = len(sim.logs())
        sim.key("ENTER")
        paused = sim.stats(after=checkpoint, state=0, menu=1)
        paused_again = sim.stats(after=paused["log_end"], state=0, menu=1)
        require((paused["x"], paused["y"]) == (paused_again["x"], paused_again["y"]) and paused_again["tic"] > paused["tic"], "Menu did not pause the world")
        report["assertions"]["menu_pauses_world"] = {"first": paused, "second": paused_again}
        hidden = sim.shot("controller-menu")
        # Tap the MENU hit target itself: revealing must consume this touch,
        # otherwise Escape would close the menu and the menu=1 check would fail.
        checkpoint = len(sim.logs())
        sim.button("MENU")
        revealed = sim.shot("touch-overlay")
        changed(report, "touch_reveals_controls", hidden, revealed, (0, 0, 40, 40), 0.08)
        still_paused = sim.stats(after=checkpoint, state=0, menu=1)
        report["assertions"]["first_touch_consumed"] = still_paused
        # X is controller B: leave menu, hide overlay, then reveal it again.
        checkpoint = len(sim.logs())
        sim.key("X")
        resumed = sim.stats(after=checkpoint, state=0, menu=0)
        report["assertions"]["controller_b_resumes"] = resumed
        sim.tap(160, 120)
        sim.button("MENU")
        touch_menu = sim.shot("touch-menu")
        sim.button("DOWN")
        down_menu = sim.shot("touch-down")
        sim.button("UP")
        up_menu = sim.shot("touch-up")
        changed(report, "touch_menu_down", touch_menu, down_menu, (55, 60, 100, 122), 0.025)
        changed(report, "touch_menu_up", down_menu, up_menu, (55, 60, 100, 122), 0.025)
        # Main menu index is New Game again; three touch DOWNs select Save Game.
        for _ in range(3):
            sim.button("DOWN")
        sim.button("OK")
        slots = sim.shot("save-slots")
        changed(report, "touch_ok_save_menu", up_menu, slots, (40, 40, 270, 205), 0.04)
        sim.button("OK")  # Slot zero: port pre-fills PXA SAVE 1.
        sim.shot("save-name")
        checkpoint = len(sim.logs())
        sim.button("OK")
        sim.log_match(r"DOOM saved /data/doomsav0\.dsg", after=checkpoint)
        saved = save_file(state)
        blob = saved.read_bytes()
        require(len(blob) > 1024 and blob[:24].split(b"\0", 1)[0] == b"PXA SAVE 1", "Missing/invalid Doom slot-zero save header")
        require(not (saved.parent / "doom-pending.tmp").exists(), "Save left a pending temporary file")
        saved_state = sim.stats(after=checkpoint, state=0, menu=0)
        sim.shot("saved-game")
        digest = hashlib.sha256(blob).hexdigest()
        report["assertions"]["save"] = {"private_path": str(saved.relative_to(state)), "bytes": len(blob), "sha256": digest, "state": saved_state}
    if args.capture_audio:
        path = args.output / "game-audio.s16"
        require(path.is_file(), "SDL audio output was not opened")
        pcm = array("h", path.read_bytes())  # SDL AUDIO_S16SYS, same host process.
        nonzero = sum(value != 0 for value in pcm)
        peak = max((abs(value) for value in pcm), default=0)
        require(len(pcm) >= 16000 and nonzero > 512 and peak > 64,
                f"No audible game samples in Host mixer output: n={len(pcm)}, nonzero={nonzero}, peak={peak}")
        report["assertions"]["audio_output"] = {"path": str(path), "rate": 16000,
            "channels": 1, "format": "S16SYS", "samples": len(pcm),
            "nonzero_samples": nonzero, "peak": peak}
    if args.skip_persistence:
        report["assertions"]["relaunch_load"] = {"skipped": True, "reason": "--skip-persistence"}
        return
    require(hashlib.sha256(saved.read_bytes()).hexdigest() == digest, "Save changed on normal shutdown")
    with Simulator(args, state, sock, "reload", report) as sim:
        sim.log_match(r"DOOM ready IWAD=doom1\.wad")
        sim.key("ENTER")
        sim.key("DOWN")
        sim.key("DOWN")
        sim.key("Z")
        sim.shot("load-slots")
        checkpoint = len(sim.logs())
        sim.key("Z", settle=0.8)
        loaded = sim.stats(after=checkpoint, state=0, menu=0)
        require(loaded["hp"] > 0 and loaded["map"] == saved_state["map"], f"Save did not restore a live player: {loaded}")
        require(abs(loaded["x"] - saved_state["x"]) <= 2 and abs(loaded["y"] - saved_state["y"]) <= 2,
                f"Save did not restore position: saved={saved_state}, loaded={loaded}")
        require(hashlib.sha256(saved.read_bytes()).hexdigest() == digest, "Loading changed save bytes")
        sim.shot("loaded-level")
        report["assertions"]["relaunch_load"] = {"passed": True, "loaded": loaded, "save_sha256": digest}


def negative_case(args, tmp, report, malformed):
    name = "malformed-wad" if malformed else "absent-wad"
    state, sock = tmp / name, tmp / f"{name}.sock"
    if malformed:
        root = discover(args, state, sock, name + "-discover", report)
        (root / "doom1.wad").write_bytes(b"IWAD" + struct.pack("<II", 1, 0x7fffffff))
    with Simulator(args, state, sock, name, report) as sim:
        expected = "Invalid IWAD directory." if malformed else "No DOOM IWAD found."
        if malformed:
            sim.log_match(re.escape(expected))
        else:
            sim.wait(lambda: missing_wad_text(sim.image()), "rendered 'No DOOM IWAD found.' error")
        time.sleep(0.35)
        screen = sim.shot("error")
        if not malformed:
            require(missing_wad_text(screen), "Missing-WAD diagnostic text changed")
        require("DOOM ready" not in sim.logs() and not STATS.search(sim.logs()), f"{name}: invalid resources reached the game loop")
        # port_error draws white explanatory text in the body, with no game HUD.
        text_pixels = sum(value > 200 for value in screen.crop((8, 35, 312, 100)).convert("L").tobytes())
        require(text_pixels > 80, f"{name}: error explanation was not rendered")
        blank = Image.new("RGB", screen.size)
        require(difference(screen, blank, (0, 110, 320, 230))["changed_fraction"] < 0.03, f"{name}: unexpected gameplay/HUD below error text")
        report["assertions"][name] = {"expected_text": expected, "logged": expected in sim.logs(), "exact_text_pixels": not malformed, "text_pixels": text_pixels, "game_loop_started": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    for name in ("simulator", "package", "publisher-key", "wad", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=25, help="Maximum seconds per startup/state wait (default: 25)")
    parser.add_argument("--skip-persistence", action="store_true", help="Skip save loading across simulator relaunch")
    parser.add_argument("--capture-audio", action="store_true",
                        help="Capture SDL disk audio and require nonzero Host mixer output (16000 Hz mono S16SYS)")
    args = parser.parse_args(argv)
    for name in ("simulator", "package", "publisher_key", "wad", "output"):
        setattr(args, name, getattr(args, name).expanduser().resolve())
    require(args.timeout > 0, "--timeout must be positive")
    require(args.simulator.is_file() and os.access(args.simulator, os.X_OK), f"Simulator executable unavailable: {args.simulator}")
    require(args.package.is_dir() and (args.package / "manifest.pxm").is_file(), f"Signed package directory unavailable: {args.package}")
    require(args.publisher_key.is_file(), f"Publisher public key unavailable: {args.publisher_key}")
    require(args.wad.is_file(), f"Shareware IWAD unavailable: {args.wad}")
    args.output.mkdir(parents=True, exist_ok=True)
    require(not any(args.output.iterdir()), f"--output must be empty: {args.output}")
    report = {"passed": False, "simulator": str(args.simulator), "package": str(args.package),
              "publisher_key": str(args.publisher_key), "wad": str(args.wad), "assertions": {},
              "cases": {}, "processes": [], "screenshots": []}
    began = time.monotonic()
    try:
        expected_title, report["wad_sha256"] = wad_title(args.wad)
        with tempfile.TemporaryDirectory(prefix="pxa-doom-") as directory:
            tmp = Path(directory)
            for name, run in (("gameplay", lambda: gameplay(args, tmp, report, expected_title)),
                              ("absent-wad", lambda: negative_case(args, tmp, report, False)),
                              ("malformed-wad", lambda: negative_case(args, tmp, report, True))):
                try:
                    run()
                    report["cases"][name] = {"passed": True}
                    print(f"PASS {name}", flush=True)
                except Exception as error:
                    report["cases"][name] = {"passed": False, "error": str(error), "traceback": traceback.format_exc()}
                    print(f"FAIL {name}: {error}", file=sys.stderr, flush=True)
            report["passed"] = all(case["passed"] for case in report["cases"].values())
    except Exception as error:
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
    finally:
        report["elapsed_seconds"] = round(time.monotonic() - began, 3)
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{'PASS' if report['passed'] else 'FAIL'}: {args.output / 'results.json'}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
