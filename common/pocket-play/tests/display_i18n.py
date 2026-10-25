#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
r"""Accept signed Pocket games in the real product simulator (Python 3 + Pillow).

From the workspace root::

    python3 local/pxa-apps/common/pocket-play/tests/display_i18n.py \
      --simulator PATH/pxsys_product_simulator \
      --packages local/artifacts/pocket-music/20261024-092424 \
      --publisher-key local/pxa-apps/.dev-signing/publisher-public.der \
      --output local/artifacts/pocket-music/RUN/display-i18n

The default matrix runs all ten apps at 296x240 and 320x240, Anarch and 2048
at 480x320, and 2048 at 240x200. --apps narrows it; --sizes overrides the base
sizes. --large-apps / --small-apps select the additional representatives.
--package APP=PATH overrides an expanded signed package, including Tiny's
chunked package. --legacy-package APP=PATH optionally creates a real old save
with the previous signed app before opening it with the current package.

Each case uses fresh isolated Host state retained under --output. Touches use
logged physical viewport geometry. Actual screenshots must match generated CJK
and ASCII font bitmaps at distant menu positions; log messages and screenshot
counts alone cannot pass. Language switches preserve the paused game's durable
storage values, then normal menu exit and a second simulator process verify the
English preference. A third process verifies switching back to Chinese.
Reports, screenshots, Host saves and logs are confined to ignored local/artifacts.
Only short UNIX control sockets use a temporary directory. No device, package
build, Host save injection, runtime patch, or alternate renderer is used.
"""

import argparse
from collections import Counter
import hashlib
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import time
import traceback
from types import SimpleNamespace
import zlib

from PIL import Image

sys.dont_write_bytecode = True
WORKSPACE = Path(__file__).resolve().parents[5]
ARTIFACTS = WORKSPACE / "local/artifacts"
APPS = ("anarch", "game-2048", "pocket-chess", "pocket-sokoban", "pet-garden",
        "crisp-arcade", "marble-islands", "lantern-grove", "harbor-cafe", "tiny-elements")
SPEC = importlib.util.spec_from_file_location(
    "pocket_display_doom_transport", WORKSPACE / "local/pxa-apps/doom/tests/simulator.py")
TRANSPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TRANSPORT)
require = TRANSPORT.require
VIEWPORT = re.compile(
    r"(?:Pocket|Anarch): viewport canvas=(\d+)x(\d+) origin=(-?\d+),(-?\d+) "
    r"size=(\d+)x(\d+) display=(\d+)x(\d+)")
FAULTS = re.compile(
    r"Guest exception|callback failed|out of bounds memory access|unreachable instruction|"
    r"PXA main-loop (?:event|wait) failed|(?:Pocket|Anarch): (?!failed to reach target FPS)[^\r\n]*failed|"
    r"exit flush timed out|rejected (?:invalid )?save|wasm trap|stack overflow|"
    r"input queue overflow|event queue overflow", re.I)


class Simulator(TRANSPORT.Simulator):
    """Reuse the original command, touch, key, wait and normal shutdown transport."""

    def __enter__(self):
        self.state.mkdir(parents=True, exist_ok=True)
        self.log_file = self.log_path.open("wb")
        argv = [str(self.args.simulator), "--package", str(self.args.package),
                "--publisher-key", str(self.args.publisher_key), "--state-root", str(self.state),
                "--width", str(self.args.width), "--height", str(self.args.height),
                "--corner-radius", "0", "--safe-insets", "0,0,0,0",
                "--pxadb-control-socket", str(self.sock)]
        self.report["processes"].append({"name": self.name, "command": argv, "log": str(self.log_path)})
        try:
            self.proc = subprocess.Popen(argv, env=dict(os.environ, SDL_VIDEODRIVER="dummy",
                SDL_AUDIODRIVER="dummy"), stdout=self.log_file, stderr=subprocess.STDOUT,
                start_new_session=True)
            self.wait(lambda: self.sock.exists(), "control socket")
            self.log_match(TRANSPORT.MEDIA)
            return self
        except BaseException:
            self.close()
            raise

    def image(self):
        payload = TRANSPORT.command(self.sock, "SCREENSHOT AFTER_PRESENT")
        header, separator, png = payload.partition(b"\n")
        require(separator and header == f"PNG {len(png)}".encode(), "Invalid screenshot framing")
        image = Image.open(io.BytesIO(png)).convert("RGB")
        require(image.size == (self.args.width, self.args.height),
                f"Physical screenshot size {image.size}, requested {self.args.width}x{self.args.height}")
        return image

    def healthy(self):
        fault = FAULTS.search(self.logs())
        require(not fault, f"Runtime fault: {fault.group() if fault else ''}; see {self.log_path}")

    def __exit__(self, exc_type, exc, tb):
        super().__exit__(exc_type, exc, tb)
        if exc_type is None:
            self.healthy()


class Viewport:
    def __init__(self, values):
        (self.cw, self.ch, self.x, self.y, self.w, self.h, self.dw, self.dh) = map(int, values)

    def validate(self, app, size):
        require((self.cw, self.ch) == ((160, 120) if app == "anarch" else (288, 224)),
                f"Unexpected logical canvas: {vars(self)}")
        require((self.dw, self.dh) == size, f"Viewport uses wrong display: {vars(self)}")
        require(0 <= self.x and 0 <= self.y and self.w > 0 and self.h > 0 and
                self.x + self.w <= self.dw and self.y + self.h <= self.dh,
                f"Viewport is clipped: {vars(self)}")
        # The requested screens fit within the SDK surface cap. Check the actual
        # maximal aspect-preserving physical rectangle, independently of origin.
        ratio = min(self.dw / self.cw, self.dh / self.ch)
        require(abs(self.w - self.cw * ratio) < 1.01 and abs(self.h - self.ch * ratio) < 1.01,
                f"Surface does not fill the available display with an equal scale: {vars(self)}")
        require(abs(self.x - (self.dw - self.w) / 2) <= .5 and
                abs(self.y - (self.dh - self.h) / 2) <= .5,
                f"Viewport is not centered: {vars(self)}")
        return dict(vars(self), scale_x=self.w / self.cw, scale_y=self.h / self.ch)

    def point(self, x, y):
        # Centers stay inside the corresponding nearest-neighbor destination cell.
        return (self.x + min(self.w - 1, int((x + .5) * self.w / self.cw)),
                self.y + min(self.h - 1, int((y + .5) * self.h / self.ch)))

    def box(self, box):
        x0, y0, x1, y1 = box
        return (self.x + math.ceil(x0 * self.w / self.cw),
                self.y + math.ceil(y0 * self.h / self.ch),
                self.x + math.ceil(x1 * self.w / self.cw),
                self.y + math.ceil(y1 * self.h / self.ch))


def fonts():
    """Read the shipped font data; do not import a second UI implementation."""
    base = WORKSPACE / "local/pxa-apps/common/pocket-ui"
    cjk = {int(cp, 16): [int(row, 16) for row in rows.split(",")]
           for cp, rows in re.findall(r"\{0x([0-9a-f]+)u,\s*\{([^}]+)\}\}",
                                      (base / "glyphs.hpp").read_text())}
    ascii_source = (base / "ascii.hpp").read_text()
    names = re.search(r'ascii_names\[\]\s*=\s*"([^"\n]+)"', ascii_source).group(1)
    rows = [[int(value) for value in row.split(",")]
            for row in re.findall(r"\{([0-9,]+)\}", ascii_source)]
    require(len(names) == len(rows) and cjk, "Cannot decode shipped bitmap font tables")
    return cjk, dict(zip(names, rows))


def text_mask(value, scale, tables):
    cjk, ascii_glyphs = tables
    width = sum(6 if ord(c) < 128 else 10 for c in value) * scale
    height = (10 if any(ord(c) >= 128 for c in value) else 7) * scale
    image = Image.new("1", (width, height))
    x = 0
    for char in value:
        if char == " ":
            x += 6 * scale
            continue
        rows = ascii_glyphs.get(char.upper()) if ord(char) < 128 else cjk.get(ord(char))
        require(rows is not None, f"Expected acceptance glyph absent from font: {char!r}")
        bits = 5 if ord(char) < 128 else 10
        for y, row in enumerate(rows):
            for col in range(bits):
                if row & (1 << (bits - 1 - col)):
                    for dy in range(scale):
                        for dx in range(scale):
                            image.putpixel((x + col * scale + dx, y * scale + dy), 1)
        x += (6 if ord(char) < 128 else 10) * scale
    return image


def glyph_pixels(image, viewport, value, x, y, scale, tables):
    """Match both ink and negative space, rejecting blank, tofu and mojibake."""
    mask = text_mask(value, scale, tables)
    box = viewport.box((x, y, x + mask.width, y + mask.height))
    require(box[2] <= image.width and box[3] <= image.height, f"Text clipped: {value!r}")
    samples = {False: [], True: []}
    for py in range(box[1], box[3]):
        sy = (py - viewport.y) * viewport.ch // viewport.h - y
        for px in range(box[0], box[2]):
            sx = (px - viewport.x) * viewport.cw // viewport.w - x
            samples[bool(mask.getpixel((sx, sy)))].append(image.getpixel((px, py)))
    require(all(samples.values()), f"Glyph collapsed at this viewport: {value!r}")
    colors = {ink: Counter(values).most_common(1)[0][0] for ink, values in samples.items()}
    require(max(abs(a - b) for a, b in zip(colors[False], colors[True])) > 32,
            f"No distinct glyph ink for {value!r}: {colors}")
    accuracy = {str(ink): sum(max(abs(a - b) for a, b in zip(pixel, colors[ink])) <= 10
                             for pixel in values) / len(values)
                for ink, values in samples.items()}
    require(min(accuracy.values()) >= .97,
            f"Incorrect raster for {value!r} (blank/tofu/UTF-8 corruption/incorrect scaling): {accuracy}")
    return {"text": value, "logical_origin": [x, y], "glyph_scale": scale,
            "physical_box": box, "ink_pixels": len(samples[True]),
            "background_pixels": len(samples[False]), "matching_fraction": accuracy}


def menu_pixels(sim, viewport, locale, tables, tag):
    image = sim.shot(tag)
    if sim.args.app == "anarch":
        checks = [glyph_pixels(image, viewport, "中文 X" if locale == "zh" else "EN X",
                               112, 2, 1, tables),
                  glyph_pixels(image, viewport, "A 确认 X 语言" if locale == "zh" else "A OK  X Language",
                               4, 102, 1, tables)]
    else:
        checks = [glyph_pixels(image, viewport, "已暂停" if locale == "zh" else "PAUSED",
                               100, 43, 2, tables),
                  glyph_pixels(image, viewport, "退出" if locale == "zh" else "EXIT",
                               68, 142, 1, tables)]
    return image, checks


def gameplay_cjk(image, viewport, tables):
    """Recognize shipped CJK strokes in game pixels without a translation map.

    This additional check catches an English-only game hidden behind a localized
    shared menu. On the small downsampled screen the exact positioned menu tests
    remain authoritative; lost source pixels cannot reconstruct a whole glyph.
    """
    if viewport.w < viewport.cw or viewport.h < viewport.ch:
        return None
    pixels = [[image.getpixel((viewport.x + math.ceil(x * viewport.w / viewport.cw),
                               viewport.y + math.ceil(y * viewport.h / viewport.ch)))
               for x in range(viewport.cw)] for y in range(viewport.ch)]
    colors = Counter(pixel for row in pixels for pixel in row).most_common(24)
    signatures = {tuple(rows): chr(cp) for cp, rows in tables[0].items()
                  if 0x3400 <= cp <= 0x9fff and sum(row.bit_count() for row in rows) >= 18
                  and len(set(rows)) >= 5}
    prefixes = {rows[:3] for rows in signatures}
    found = {}
    for color, count in colors:
        if count < 12:
            continue
        rows = [int("".join("1" if pixel == color else "0" for pixel in row), 2) for row in pixels]
        for y in range(viewport.ch - 9):
            for x in range(viewport.cw - 9):
                shift = viewport.cw - 10 - x
                prefix = tuple((rows[y + dy] >> shift) & 1023 for dy in range(3))
                if prefix not in prefixes:
                    continue
                signature = prefix + tuple((rows[y + dy] >> shift) & 1023 for dy in range(3, 10))
                char = signatures.get(signature)
                if char and char not in found:
                    found[char] = {"character": char, "logical_origin": [x, y], "color": color}
        if len(found) >= 24:
            break
    return list(found.values())


def touch(sim, viewport, x, y, hold=.12):
    px, py = viewport.point(x, y)
    TRANSPORT.command(sim.sock, f"TOUCH DOWN 0 {px} {py}")
    try:
        time.sleep(hold)
    finally:
        TRANSPORT.command(sim.sock, f"TOUCH UP 0 {px} {py}")
    time.sleep(.3)


def swipe(sim, viewport, start, end):
    x, y = viewport.point(*start)
    ex, ey = viewport.point(*end)
    TRANSPORT.command(sim.sock, f"TOUCH DOWN 0 {x} {y}")
    try:
        for step in range(1, 7):
            TRANSPORT.command(sim.sock, f"TOUCH MOVE 0 {x + (ex-x)*step//6} {y + (ey-y)*step//6}")
            time.sleep(.025)
    finally:
        TRANSPORT.command(sim.sock, f"TOUCH UP 0 {ex} {ey}")
    time.sleep(.4)


def startup(sim, locale, validate=True):
    app = sim.args.app
    sim.log_match(r"Anarch: ready" if app == "anarch" else r"Pocket: ready app=pxa-" + app + r"\b")
    sim.log_match(r"(?:Pocket|Anarch): language " + locale + r"\b")
    viewport = Viewport(sim.log_match(VIEWPORT).groups())
    if validate:
        viewport.validate(app, (sim.args.width, sim.args.height))
    sim.healthy()
    return viewport


def open_menu(sim, viewport):
    if sim.args.app == "anarch":
        touch(sim, viewport, 144, 8)
    else:
        checkpoint = len(sim.logs())
        touch(sim, viewport, 276, 10)
        sim.log_match(r"Pocket: menu opened", after=checkpoint)


def gameplay(sim, viewport):
    app = sim.args.app
    before = sim.shot("before-gameplay")
    checkpoint = len(sim.logs())
    box = (0, 20, viewport.cw, viewport.ch)
    action = ""
    if app == "anarch":
        touch(sim, viewport, 32, 22)  # Fresh menu's Play item; original story intro.
        sim.key("X", settle=1.9)     # Original B skips the intro.
        sim.log_match(r"Anarch: (?:setting and initializing level|level 0 started)", after=checkpoint)
        before = sim.shot("level-before-touch")
        touch(sim, viewport, 24, 42, hold=.7)
        box = (50, 15, 125, 100)  # Exclude touch outlines and HUD.
        action = "touch Play, skip story, hold forward in original level"
    elif app in ("game-2048", "pocket-sokoban"):
        box = (4, 40, 184, 218)
        for end in ((35, 120), (100, 65), (165, 120), (100, 180)):
            swipe(sim, viewport, (100, 120), end)
            if TRANSPORT.difference(before, sim.image(), viewport.box(box))["changed_fraction"] > .005:
                break
        action = "swipe board until a valid move changes its pixels"
    elif app == "pocket-chess":
        touch(sim, viewport, 116, 180)
        touch(sim, viewport, 116, 132)
        box = (4, 28, 196, 220)
        action = "touch e2 then e4"
    elif app == "pet-garden":
        touch(sim, viewport, 24, 191)
        sim.log_match(r"pet-garden feed\b", after=checkpoint)
        action = "touch Feed"
    elif app == "crisp-arcade":
        touch(sim, viewport, 200, 140)
        sim.log_match(r"crisp launch game=(?:COLOR ROLL|彩条连击)\b", after=checkpoint)
        action = "touch launch Color Roll"
    elif app == "marble-islands":
        touch(sim, viewport, 140, 120)
        action = "touch Intro to enter the race"
    elif app == "lantern-grove":
        touch(sim, viewport, 50, 183)
        sim.log_match(r"lantern-grove collect view=0 .*collected=", after=checkpoint)
        action = "touch collect grove resources"
    elif app == "harbor-cafe":
        time.sleep(.8)
        touch(sim, viewport, 155, 115)
        touch(sim, viewport, 143, 179)
        sim.log_match(r"harbor event=2 .*table=0", after=checkpoint)
        action = "touch first table and accept order"
    else:
        require(app == "tiny-elements", f"No gameplay acceptance for {app}")
        touch(sim, viewport, 144, 195)
        before = sim.shot("guide-dismissed")
        checkpoint = len(sim.logs())
        touch(sim, viewport, 137, 199)
        sim.log_match(r"Elements: material selected", after=checkpoint)
        action = "dismiss guide, touch select water"
    after = sim.shot("after-gameplay")
    delta = TRANSPORT.difference(before, after, viewport.box(box))
    require(delta["changed_fraction"] > .001,
            f"{app}: gameplay touch has no visible effect in mapped surface: {delta}")
    return {"action": action, "physical_comparison_box": viewport.box(box), **delta}


def host_values(raw):
    """Read-only PXKV validation, including CRC, generation and record bounds."""
    require(len(raw) >= 26, "Short Host storage generation")
    magic, version, reserved, generation, length, crc = struct.unpack_from("<4sHHQII", raw)
    body = raw[24:]
    require(magic == b"PXKV" and version == 1 and reserved == 0 and generation > 0 and
            length == len(body) and zlib.crc32(body) == crc, "Invalid Host storage header/CRC")
    count = struct.unpack_from("<H", body)[0]
    values, offset, previous = {}, 2, b""
    for _ in range(count):
        require(offset < len(body), "Missing Host key length")
        size = body[offset]
        offset += 1
        require(1 <= size <= 64 and offset + size + 2 <= len(body), "Invalid Host key bounds")
        key = body[offset:offset + size]
        offset += size
        require(key > previous, "Host keys are not sorted")
        previous = key
        size = struct.unpack_from("<H", body, offset)[0]
        offset += 2
        require(size <= 2048 and offset + size <= len(body), "Invalid Host value bounds")
        values[key.decode("ascii")] = body[offset:offset + size]
        offset += size
    require(offset == len(body), "Trailing Host storage bytes")
    return generation, values


def storage(state):
    candidates = []
    for name in (".pxa-kv-a", ".pxa-kv-b"):
        for pattern in ("app-data/*/" + name, "private-files/*/.pxa-storage/" + name):
            for path in state.glob(pattern):
                try:
                    generation, values = host_values(path.read_bytes())
                    candidates.append((generation, values, path))
                except (OSError, AssertionError, UnicodeError, struct.error):
                    continue  # The other committed generation remains authoritative.
    if not candidates:
        return None
    return max(candidates, key=lambda value: value[0])


def game_values(values):
    return {key: value for key, value in values.items()
            if not key.endswith((".prefs.v1", ".preferences.v1"))}


def saved(sim):
    previous, stable_since = None, time.monotonic()
    def current():
        nonlocal previous, stable_since
        value = storage(sim.state)
        if not value or not game_values(value[1]):
            return None
        contents = game_values(value[1])
        if contents != previous:
            previous, stable_since = contents, time.monotonic()
        return value if time.monotonic() - stable_since >= .3 else None
    return sim.wait(current, "durable stable paused game save")


def storage_summary(snapshot):
    generation, values, path = snapshot
    return {"generation": generation, "file": str(path), "values": {
        key: {"size": len(value), "sha256": hashlib.sha256(value).hexdigest()}
        for key, value in values.items()}}


def language(sim, viewport, locale, baseline):
    checkpoint = len(sim.logs())
    touch(sim, viewport, 132 if sim.args.app == "anarch" else 144,
          7 if sim.args.app == "anarch" else 176)
    sim.log_match(r"(?:Pocket|Anarch): language " + locale + r"\b", after=checkpoint)
    expected = (b"AL\x01" if sim.args.app == "anarch" else b"PL\x01") + bytes([locale == "en"])

    def persisted():
        snapshot = storage(sim.state)
        prefs = [value for key, value in snapshot[1].items()
                 if key.endswith((".prefs.v1", ".preferences.v1"))] if snapshot else []
        return snapshot if prefs == [expected] else None
    snapshot = sim.wait(persisted, f"durable {locale} language preference")
    require(sim.args.app == "anarch" or "Pocket: menu closed" not in sim.logs()[checkpoint:],
            "Language toggle resumed gameplay instead of remaining in the pause menu")
    require(game_values(snapshot[1]) == game_values(baseline[1]),
            "Language switch changed the paused game's original save values")
    return snapshot


def exit_menu(sim, viewport):
    if sim.args.app == "anarch":
        # Original-menu rows include Continue/Map only in the current level,
        # and Load only when an actual completed-level checkpoint exists.
        current = storage(sim.state)
        original = current[1].get("anarch.save.v1", b"\0") if current else b"\0"
        row = 5 if "Anarch: level 0 started" in sim.logs() else 3
        row += int(bool(original[0] >> 4))
        px, py = viewport.point(80, 22 + row * 12)
        TRANSPORT.command(sim.sock, f"TOUCH DOWN 0 {px} {py}")
    else:
        # The Exit row acts on DOWN and may finish before a corresponding UP.
        px, py = viewport.point(144, 146)
        TRANSPORT.command(sim.sock, f"TOUCH DOWN 0 {px} {py}")
    sim.proc.wait(timeout=max(8, sim.args.timeout))
    require(sim.proc.returncode == 0, f"Menu exit failed: {sim.proc.returncode}")
    sim.healthy()


def run_case(args, temporary, app, width, height, report, tables):
    label = f"{app}-{width}x{height}"
    result = {"app": app, "display": [width, height], "passed": False}
    report["cases"].append(result)
    output = args.output / label
    require(not output.exists(), f"Case output already exists; choose a fresh --output: {output}")
    output.mkdir()
    state = output / "state"
    child = SimpleNamespace(**{**vars(args), "app": app, "width": width, "height": height,
                               "package": args.package_map[app], "output": output,
                               "capture_audio": False})
    result["package"] = str(child.package)
    result["manifest_sha256"] = hashlib.sha256((child.package / "manifest.pxm").read_bytes()).hexdigest()
    result["state_root"] = str(state)
    try:
        with Simulator(child, state, temporary / (label + "-a.sock"), "zh-first", report) as sim:
            viewport = startup(sim, "zh")
            result["viewport"] = viewport.validate(app, (width, height))
            initial = sim.shot("ready-zh")
            result["gameplay_chinese_glyphs"] = gameplay_cjk(initial, viewport, tables)
            if result["gameplay_chinese_glyphs"] is not None:
                require(len(result["gameplay_chinese_glyphs"]) >= 2,
                        "Default game UI contains fewer than two recognizable Chinese glyphs")
            result["gameplay"] = gameplay(sim, viewport)
            open_menu(sim, viewport)
            if app == "anarch":
                checkpoint = len(sim.logs())
                touch(sim, viewport, 30, 58)  # Normal Sound menu action creates original save.
                sim.log_match(r"Anarch: progress/settings saved", after=checkpoint)
            else:
                sim.log_match(r"Pocket: saved")
            baseline = saved(sim)
            zh, checks = menu_pixels(sim, viewport, "zh", tables, "menu-zh")
            result["chinese_glyphs"] = checks
            result["storage_before_language"] = storage_summary(baseline)
            snapshot = language(sim, viewport, "en", baseline)
            en, checks = menu_pixels(sim, viewport, "en", tables, "menu-en")
            result["english_glyphs"] = checks
            # A cropped, static pause menu excludes changing gameplay and toasts.
            box = viewport.box((0, 0, 160, 120) if app == "anarch" else (48, 32, 240, 190))
            delta = TRANSPORT.difference(zh, en, box)
            require(delta["changed_fraction"] > .005, f"Language switch did not change menu pixels: {delta}")
            result["language_pixel_change"] = delta
            result["storage_english"] = storage_summary(snapshot)
            result["game_save_unchanged_by_language"] = True
            exit_menu(sim, viewport)
        with Simulator(child, state, temporary / (label + "-b.sock"), "en-reload", report) as sim:
            viewport = startup(sim, "en")
            sim.log_match(r"Anarch: save loaded" if app == "anarch" else r"Pocket: load completed")
            reloaded = sim.shot("ready-en-reloaded")
            result["gameplay_english_cjk_glyphs"] = gameplay_cjk(reloaded, viewport, tables)
            if result["gameplay_chinese_glyphs"] is not None:
                require(len(result["gameplay_english_cjk_glyphs"]) < len(result["gameplay_chinese_glyphs"]),
                        "Reloaded English preference did not change the game's Chinese UI glyphs")
            if app == "anarch":
                # Restart opens the original menu directly; no level has started.
                pass
            else:
                open_menu(sim, viewport)
                # A loaded, unchanged game may have no dirty save to submit.
                # Its existing Host-committed values remain the baseline.
            _, result["reloaded_english_glyphs"] = menu_pixels(sim, viewport, "en", tables, "menu-en-reloaded")
            baseline = saved(sim)
            snapshot = language(sim, viewport, "zh", baseline)
            _, result["switched_back_chinese_glyphs"] = menu_pixels(sim, viewport, "zh", tables, "menu-zh-restored")
            result["storage_chinese"] = storage_summary(snapshot)
            exit_menu(sim, viewport)
        with Simulator(child, state, temporary / (label + "-c.sock"), "zh-reload", report) as sim:
            viewport = startup(sim, "zh")
            sim.log_match(r"Anarch: save loaded" if app == "anarch" else r"Pocket: load completed")
            if app != "anarch":
                open_menu(sim, viewport)
            _, result["reloaded_chinese_glyphs"] = menu_pixels(sim, viewport, "zh", tables, "menu-zh-reloaded")
        result["passed"] = True
        print(f"PASS {label}: physical fit, gameplay touch, CJK raster, zh/en/zh persistence", flush=True)
    except Exception:
        result["error"] = traceback.format_exc()
        raise


def world_state(app, snapshot):
    # Import only the independent binary decoders, never its device transport.
    spec = importlib.util.spec_from_file_location("pocket_display_world_decoders",
                                                Path(__file__).with_name("worlds_device.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    payload, envelope = module.committed_world(snapshot[1], "pxa-" + app + ".save.v1")
    state = module.DECODERS[app](payload)
    return payload, envelope, state, module


def run_legacy_case(args, temporary, app, package, report, tables):
    """Generate an old save through gameplay; open the same Host state in new code."""
    require(app in ("lantern-grove", "harbor-cafe", "tiny-elements"),
            "Legacy semantic decoders cover Grove, Cafe and Tiny Elements")
    result = {"app": app, "old_package": str(package), "passed": False}
    report["legacy_cases"].append(result)
    output = args.output / ("legacy-" + app)
    require(not output.exists(), f"Legacy output already exists: {output}")
    output.mkdir()
    state = output / "state"
    child = SimpleNamespace(**{**vars(args), "app": app, "width": 320, "height": 240,
                               "package": package, "output": output, "capture_audio": False})
    try:
        with Simulator(child, state, temporary / ("old-" + app + ".sock"), "old-save", report) as sim:
            sim.log_match(r"Pocket: ready app=pxa-" + app + r"\b")
            # The prior runtime has a fixed 288x224 surface centered at 16,8.
            # Prefer a logged fitted rectangle when an older package has it.
            matches = list(VIEWPORT.finditer(sim.logs()))
            viewport = Viewport(matches[-1].groups() if matches else (288, 224, 16, 8, 288, 224, 320, 240))
            result["old_gameplay"] = gameplay(sim, viewport)
            open_menu(sim, viewport)
            sim.log_match(r"Pocket: saved")
            saved(sim)
            sim.shot("paused-normal-save")
            exit_menu(sim, viewport)
        old = storage(state)
        require(old is not None, "Old signed package produced no committed storage")
        payload, envelope, before, decoder = world_state(app, old)
        require(not any(key.endswith((".prefs.v1", ".preferences.v1")) for key in old[1]),
                "Legacy package already contains locale preferences; use the actual previous package")
        if app == "tiny-elements":
            require(payload[43] == 0, "Old Tiny reserved byte 43 must be zero")
        result.update(old_save=storage_summary(old), old_envelope=envelope,
                      old_payload_sha256=hashlib.sha256(payload).hexdigest())
        child.package = args.package_map[app]
        result["new_package"] = str(child.package)
        with Simulator(child, state, temporary / ("migrate-" + app + ".sock"), "new-load", report) as sim:
            viewport = startup(sim, "zh")
            sim.log_match(r"Pocket: load completed")
            open_menu(sim, viewport)
            checkpoint = len(sim.logs())
            # Pausing flushes a pending normal save; polling committed generations
            # avoids assuming that a log line corresponds to a particular bank.
            def migrated():
                current = storage(state)
                return current if current and current[0] > old[0] else None
            current = sim.wait(migrated, "new runtime's durable loaded game save")
            raw, new_envelope, after, _ = world_state(app, current)
            fields = (decoder.GROVE_STABLE if app == "lantern-grove" else
                      decoder.CAFE_GROWTH if app == "harbor-cafe" else
                      ("scene", "badges", "material", "brush", "paused", "ui", "pending",
                       "target", "selected", "page", "confirm_yes"))
            differences = [name for name in fields if before[name] != after[name]]
            require(not differences, f"Old gameplay progress lost during locale migration: {differences}")
            if app == "tiny-elements":
                require(after["muted"] == 0 and raw[43] == 0,
                        "Old Tiny reserved byte did not migrate to audible muted=0")
                require(after["steps"] >= before["steps"], "Tiny old scene reset instead of loading")
                result["reserved_43_to_muted_zero"] = True
            _, checks = menu_pixels(sim, viewport, "zh", tables, "old-save-new-chinese-menu")
            result.update(preserved_fields=list(fields), new_save=storage_summary(current),
                          new_envelope=new_envelope, chinese_glyphs=checks)
            language(sim, viewport, "en", current)
            _, result["english_glyphs"] = menu_pixels(sim, viewport, "en", tables, "old-save-new-english-menu")
            require("Pocket: menu closed" not in sim.logs()[checkpoint:], "Language toggle resumed the game")
            exit_menu(sim, viewport)
        with Simulator(child, state, temporary / ("legacy-reload-" + app + ".sock"), "migrated-reload", report) as sim:
            viewport = startup(sim, "en")
            sim.log_match(r"Pocket: load completed")
            open_menu(sim, viewport)
            _, result["reloaded_english_glyphs"] = menu_pixels(sim, viewport, "en", tables, "old-save-en-reloaded")
            exit_menu(sim, viewport)
        result["passed"] = True
        print(f"PASS legacy {app}: old signed app normal save, preserved progress, language persistence", flush=True)
    except Exception:
        result["error"] = traceback.format_exc()
        raise


def package_override(value):
    app, separator, path = value.partition("=")
    if not separator or app not in APPS or not path:
        raise argparse.ArgumentTypeError("Expected APP=PATH for one of the ten Pocket apps")
    return app, Path(path).expanduser().resolve()


def screen(value):
    match = re.fullmatch(r"(\d+)x(\d+)", value)
    if not match:
        raise argparse.ArgumentTypeError("Expected WIDTHxHEIGHT")
    width, height = map(int, match.groups())
    if not (160 <= width <= 480 and 120 <= height <= 320):
        raise argparse.ArgumentTypeError("Acceptance screens must be within 160x120..480x320")
    return width, height


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--simulator", type=Path, required=True)
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--publisher-key", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--apps", nargs="+", choices=APPS, default=list(APPS))
    parser.add_argument("--sizes", nargs="+", type=screen, default=[(296, 240), (320, 240)])
    parser.add_argument("--large-apps", nargs="*", choices=APPS, default=["anarch", "game-2048"])
    parser.add_argument("--small-apps", nargs="*", choices=APPS, default=["game-2048"])
    parser.add_argument("--package", action="append", type=package_override, default=[])
    parser.add_argument("--legacy-package", action="append", type=package_override, default=[],
                        help="Optional prior signed package; compatibility uses real normal saves")
    parser.add_argument("--legacy-only", action="store_true", help="Run only requested old-package migration cases")
    parser.add_argument("--fail-fast", action="store_true", help="Stop after the first failed case")
    parser.add_argument("--timeout", type=float, default=30)
    args = parser.parse_args()
    args.simulator = args.simulator.expanduser().resolve()
    args.publisher_key = args.publisher_key.expanduser().resolve()
    args.packages = args.packages.expanduser().resolve()
    args.output = args.output.expanduser().resolve()
    require(args.output.is_relative_to(ARTIFACTS.resolve()), "--output must be below workspace local/artifacts")
    require(subprocess.run(["git", "check-ignore", "-q", str(args.output)], cwd=WORKSPACE).returncode == 0,
            "--output must be ignored by Git")
    require(args.simulator.is_file() and os.access(args.simulator, os.X_OK), "Missing executable simulator")
    require(args.publisher_key.is_file(), "Missing publisher DER public key")
    require(math.isfinite(args.timeout) and 1 <= args.timeout <= 120, "--timeout must be within 1..120 seconds")
    require(len(set(args.apps)) == len(args.apps), "Repeated --apps")
    require(len(dict(args.package)) == len(args.package), "Repeated --package override")
    require(len(dict(args.legacy_package)) == len(args.legacy_package), "Repeated --legacy-package override")
    require(not args.legacy_only or args.legacy_package, "--legacy-only requires --legacy-package")
    for app, package in args.legacy_package:
        require(app in args.apps and app in ("lantern-grove", "harbor-cafe", "tiny-elements"),
                "Legacy cases must select Grove, Cafe or Tiny in --apps")
        require((package / "manifest.pxm").is_file(), f"Missing old signed package: {package}")
    args.package_map = {app: args.packages / app / "package" / ("pxa-" + app) for app in args.apps}
    args.package_map.update(dict(args.package))
    for app in args.apps:
        require((args.package_map[app] / "manifest.pxm").is_file(), f"Missing expanded signed package for {app}: {args.package_map[app]}")
    matrix = [(app, *size) for app in args.apps for size in args.sizes]
    matrix += [(app, 480, 320) for app in args.large_apps if app in args.apps]
    matrix += [(app, 240, 200) for app in args.small_apps if app in args.apps]
    matrix = [] if args.legacy_only else list(dict.fromkeys(matrix))
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "matrix": matrix, "cases": [], "legacy_cases": [], "processes": [], "screenshots": [],
              "simulator": str(args.simulator), "publisher_key": str(args.publisher_key),
              "save_injection": False,
              "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "font_sha256": {name: hashlib.sha256((WORKSPACE / "local/pxa-apps/common/pocket-ui" / name).read_bytes()).hexdigest()
                              for name in ("glyphs.hpp", "ascii.hpp")}}
    try:
        tables = fonts()
        with tempfile.TemporaryDirectory(prefix="pocket-display-") as temporary:
            for app, width, height in matrix:
                try:
                    run_case(args, Path(temporary), app, width, height, report, tables)
                except Exception:
                    print(f"FAIL {app}-{width}x{height}: {report['cases'][-1].get('error', traceback.format_exc())}", file=sys.stderr, flush=True)
                    if args.fail_fast:
                        raise
                (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
            for app, package in args.legacy_package:
                try:
                    run_legacy_case(args, Path(temporary), app, package, report, tables)
                except Exception:
                    print(f"FAIL legacy {app}: {report['legacy_cases'][-1].get('error', traceback.format_exc())}", file=sys.stderr, flush=True)
                    if args.fail_fast:
                        raise
                (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        report["passed"] = all(case["passed"] for case in report["cases"] + report["legacy_cases"])
        return 0 if report["passed"] else 1
    except Exception:
        report["error"] = traceback.format_exc()
        print(report["error"], file=sys.stderr)
        return 1
    finally:
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
