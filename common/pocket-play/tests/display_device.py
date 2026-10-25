#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
r"""Check the ten installed games' fit, touch and persistent language on pai-touch.

    python3 local/pxa-apps/common/pocket-play/tests/display_device.py \
        --port /dev/ttyACM0 --output local/artifacts/pocket-music/RUN/display-device

--apps accepts one or more application directory names (default: all ten).
Requires Python 3, Pillow, pyserial and installed signed packages. Uses the
repository PXADB client. It never installs packages or writes/deletes private
files. Pocket pause/Continue/Exit and Anarch's original menu/help/language are
used; no restart, new level, save injection or reset is performed. Normal game
checkpoints are allowed. Only the selected application is stopped on failure.
Screenshots are physical JPEG captures for human inspection, with numerical
image-change evidence; their glyphs are not claimed to be losslessly verified.
A fresh output directory below local/artifacts is required. results.json and
serial.jsonl are written on failure too. This script does not auto-detect ports.
"""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import time
import traceback
import zlib

sys.dont_write_bytecode = True
from PIL import Image, ImageChops

WORKSPACE = Path(__file__).resolve().parents[5]
ARTIFACTS = WORKSPACE / "local/artifacts"
sys.path.insert(0, str(WORKSPACE / "tools/pxadb"))
import pxadb

APPS = ("game-2048", "pocket-chess", "pocket-sokoban", "pet-garden",
        "crisp-arcade", "marble-islands", "lantern-grove", "harbor-cafe",
        "tiny-elements", "anarch")
PUBLISHER = "17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171"
SERIAL = "a4:cb:8f:d5:d4:f0"
VIEWPORT = re.compile(r"(Pocket|Anarch): viewport canvas=(\d+)x(\d+) origin=(-?\d+),(-?\d+) "
                      r"size=(\d+)x(\d+) display=(\d+)x(\d+)")
FAULTS = re.compile(
    r"Guest exception|callback failed|out of bounds memory access|unreachable instruction|"
    r"wasm trap|stack overflow|stack smashing|heap corruption|CORRUPT HEAP|"
    r"heap_caps.*(?:failed|error)|input queue overflow|event queue overflow|"
    r"(?:Pocket|Anarch): .*failed|(?:Pocket|Anarch): .*could not submit|"
    r"exit flush timed out|PXA main-loop .*failed|ASSERT|Guru Meditation|"
    r"abort\(\) was called|panic'ed|LoadProhibited|StoreProhibited", re.I)


def require(value, label):
    if not value:
        raise AssertionError(label)


def decoder_module(filename="worlds_device.py"):
    path = Path(__file__).with_name(filename)
    spec = importlib.util.spec_from_file_location("display_device_" + path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(value):
    return {"bytes": len(value), "sha256": hashlib.sha256(value).hexdigest()}


def game_values(values):
    return {key: value for key, value in values.items()
            if not key.endswith((".prefs.v1", ".preferences.v1"))}


def locale_of(values, app):
    key = "anarch.preferences.v1" if app == "anarch" else "pxa-" + app + ".prefs.v1"
    raw = values.get(key)
    if raw is None:
        return "zh"
    prefix = b"AL\x01" if app == "anarch" else b"PL\x01"
    require(len(raw) == 4 and raw[:3] == prefix and raw[3] <= 1,
            f"Invalid persisted locale {key}: {raw.hex()}")
    return "en" if raw[3] else "zh"


def reload_content(app, before, after, decoder, foreground_seconds):
    """Compare real committed payloads, independently of optional startup logs.

    Turn-based saves must match exactly. Existing test decoders validate bounded
    foreground evolution for live worlds; unsupported changes fail closed.
    """
    key = "anarch.save.v1" if app == "anarch" else "pxa-" + app + ".save.v1"
    require(key in before and key in after, "Reload comparison requires both real game saves")
    old, _ = decoder.committed_world(before, key)
    new, _ = decoder.committed_world(after, key)
    evidence = {"before": digest(old), "after": digest(new), "payload_byte_identical": old == new}
    play = None
    if app in ("pet-garden", "crisp-arcade", "marble-islands"):
        play = decoder_module("play_device.py")
        a, b = (play.DECODERS[app](raw, decoder) for raw in (old, new))
    elif app in decoder.DECODERS:
        a, b = (decoder.DECODERS[app](raw) for raw in (old, new))
    else:
        # Same independent checksum/header checks used by preserve_device.py.
        for raw in (old, new):
            if app == "anarch":
                require(len(raw) == 12, "Invalid original Anarch save length")
                continue
            core = raw[12:] if app == "pocket-chess" else raw
            magic = {"game-2048": b"P204", "pocket-chess": b"PCH1", "pocket-sokoban": b"PSKB"}[app]
            require(len(core) >= 8 and core[:4] == magic, "Unexpected reload game save format")
            checksum = zlib.crc32(core[:-4]) if app == "pocket-sokoban" else decoder.fnv(core[:-4])
            require(struct.unpack_from("<I", core, len(core)-4)[0] == checksum,
                    "Reload game payload checksum invalid")
            if app == "game-2048":
                require(len(raw) == 64 and raw[4] == 1 and raw[5] <= 1 and not any(raw[6:8]) and
                        any(raw[20:36]) and max(raw[20:36]) <= 31, "Invalid 2048 reload board")
            elif app == "pocket-chess":
                require(raw[:4] == b"PCU1" and len(raw) >= 64, "Invalid chess reload UI envelope")
    if old == new:
        return dict(evidence, method="committed_payload_bytes", payload_schema_validated=True)
    if play is not None:
        play.reload_semantics(app, a, b)
        return dict(evidence, method="play_device_reload_semantics", before_state=a, after_state=b)
    require(app in decoder.DECODERS, "Reload changed game payload without a supported semantic comparison")
    bound_ms = int((foreground_seconds + 2) * 1000)
    if app == "lantern-grove":
        fields = (*decoder.GROVE_STABLE, "view", "selected")
        require(all(a[k] == b[k] for k in fields), "Grove reload lost progress or selection")
        require(b["produced_ms"] >= a["produced_ms"] and
                all(y >= x for x, y in zip(a["pending"], b["pending"])), "Grove reload lost production")
        scope = "stable growth/resources/UI; nondecreasing pending and produced time"
    elif app == "harbor-cafe":
        fields = tuple(k for k in decoder.CAFE_GROWTH if k != "total_failed") + (
            "recipe", "table", "view", "shop", "sound", "rescued", "summary", "served", "revenue", "cogs")
        require(all(a[k] == b[k] for k in fields), "Cafe reload changed inventory, growth or UI")
        delta = b["day_ms"] - a["day_ms"]
        require(0 <= delta <= bound_ms, "Cafe reload advanced outside bounded foreground time")
        expired = sum(c["stage"] != 0 and c["patience"] <= delta for c in a["customers"])
        require(b["total_failed"] == min(60000, a["total_failed"] + expired) and
                b["failed"] == min(60000, a["failed"] + expired), "Cafe reload lost customer failure history")
        scope = "stable inventory/growth/UI; bounded day time and customer expiration"
    else:
        require(not a["paused"], "Paused Elements reload must preserve exact world bytes")
        fields = ("scene_start", "scene", "material", "brush", "paused", "ui", "pending", "target",
                  "cx", "cy", "selected", "page", "confirm_yes", "muted")
        require(all(a[k] == b[k] for k in fields), "Elements reload lost scene or UI")
        require(0 <= b["steps"] - a["steps"] <= bound_ms // 50 + 2 and
                all(b[k] >= a[k] for k in ("burned", "cooled")) and
                b["badges"] & a["badges"] == a["badges"], "Elements reload lost simulation progress")
        scope = "retained scene/UI/badges; bounded live steps and nondecreasing counters; cells may evolve"
    return dict(evidence, method="world_reload_semantics", semantic_scope=scope,
                foreground_bound_ms=bound_ms, before_state=a, after_state=b)


def changed(before, after, box):
    delta = ImageChops.difference(before.crop(box), after.crop(box))
    pixels = list(delta.getdata())
    return {"changed_fraction": sum(max(pixel) > 35 for pixel in pixels) / len(pixels),
            "mean_channel_change": sum(sum(pixel) for pixel in pixels) / (3 * len(pixels)),
            "physical_box": list(box)}


class Device:
    def __init__(self, args, client, report, stream):
        self.args, self.client, self.report, self.stream = args, client, report, stream
        self.started = time.monotonic()
        self.logs, self.drops, self.faults = [], [], []
        self.last_capture = 0
        self.app = self.identity = self.case = None
        self.owned, self.viewport = False, None
        self.decoder = decoder_module()
        client.log_callback = self.record
        client.raw_callback = lambda raw: self.record(pxadb.Frame(0, "RAW", raw.decode(errors="replace")))

    def event(self, **value):
        self.stream.write(json.dumps({"seconds": round(time.monotonic() - self.started, 3), **value},
                                     ensure_ascii=False) + "\n")

    def record(self, frame):
        self.event(kind=frame.kind, payload=frame.payload)
        self.logs.append(frame.payload)
        if frame.kind == "DROP" or re.search(r"\bdropped=[1-9]\d*", frame.payload):
            self.drops.append({"log_index": len(self.logs)-1, "kind": frame.kind,
                               "payload": frame.payload, "app": self.app})
        if FAULTS.search(frame.payload):
            self.faults.append({"log_index": len(self.logs)-1, "payload": frame.payload, "app": self.app})

    def healthy(self):
        require(not self.faults, "Runtime fault: " + json.dumps(self.faults[-3:], ensure_ascii=False))

    def request(self, command):
        self.event(command=command)
        frames = self.client.request(command, timeout=30)
        self.event(response=[{"kind": f.kind, "payload": f.payload} for f in frames if f.kind != "DATA"])
        self.healthy()
        return frames

    def pump(self, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.client.pump_logs()
            self.healthy()
            time.sleep(.02)

    def until(self, predicate, label, seconds=15):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if predicate():
                return
            self.pump(.1)
        raise TimeoutError(label)

    def has(self, needle, at=0):
        return any(needle in line for line in self.logs[at:])

    def active(self):
        return [f.payload.split("\t", 1)[0] for f in self.request("PACKAGES")
                if f.kind == "PKG" and "active=1" in f.payload]

    def selected_active(self):
        return PUBLISHER + ":" + self.identity in self.active()

    def shot(self, label):
        self.pump(max(0, .4 - (time.monotonic() - self.last_capture)))
        for attempt in range(3):
            try:
                capture = pxadb.screenshot_capture(self.request("SCREENSHOT JPEG"))
                self.last_capture = time.monotonic()
                path = self.args.output / self.app / (label + ".jpg")
                pxadb.write_screenshot(capture, path)
                with Image.open(path) as source:
                    image = source.convert("RGB")
                require(image.size == (296, 240), "Expected actual 296x240 physical capture")
                require(len(set(image.getdata())) > 20, "Blank or collapsed device screenshot")
                self.case["screenshots"].append({"label": label,
                    "path": str(path.relative_to(self.args.output)), "metadata": capture.metadata,
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
                return image
            except pxadb.PxaDbError as error:
                if not pxadb.screenshot_retryable(error) or attempt == 2:
                    raise
                self.case["transport_retries"].append({"kind": "screenshot", "label": label,
                                                        "attempt": attempt+1, "error": str(error)})
                self.pump(.5)

    def geometry(self, at):
        matches = [VIEWPORT.search(line) for line in self.logs[at:]]
        matches = [match for match in matches if match]
        if not matches:
            return False
        for match in matches:
            prefix, *numbers = match.groups()
            cw, ch, x, y, w, h, dw, dh = map(int, numbers)
            require(prefix == ("Anarch" if self.app == "anarch" else "Pocket"), "Wrong viewport producer")
            require((cw, ch) == ((160, 120) if self.app == "anarch" else (288, 224)), "Wrong logical canvas")
            require((dw, dh) == (296, 240), "Wrong device display geometry")
            ratio = min(dw/cw, dh/ch)
            require(0 <= x and 0 <= y and x+w <= dw and y+h <= dh and
                    abs(w-cw*ratio) <= 1 and abs(h-ch*ratio) <= 1 and
                    abs(x-(dw-w)/2) <= .5 and abs(y-(dh-h)/2) <= .5,
                    "Viewport does not maximize centered equal-scale display fit")
            if self.app != "anarch":
                require((x, y, w, h) == (0, 5, 296, 230), "Pocket board fit regressed")
            require(w > cw and h > ch, "Game canvas was not enlarged on physical display")
        self.viewport = (cw, ch, x, y, w, h)
        self.case["viewports"].append({"canvas": [cw, ch], "origin": [x, y], "size": [w, h],
            "display": [dw, dh], "scale": [w/cw, h/ch], "source": "app_viewport_log",
            "derived": False, "log": matches[-1].group(), "touch_verified": False})
        return True

    def derive_geometry(self):
        # Expected mapping only: a JPEG's dimensions do not measure app bounds.
        cw, ch = (160, 120) if self.app == "anarch" else (288, 224)
        w, h = 296, 296 * ch // cw
        x, y = 0, (240 - h) // 2
        self.viewport = (cw, ch, x, y, w, h)
        self.case["viewports"].append({"canvas": [cw, ch], "origin": [x, y], "size": [w, h],
            "display": [296, 240], "scale": [w/cw, h/ch], "source": "expected_centered_fit",
            "derived": True, "touch_verified": False,
            "limitation": "Bounds derived from known canvas/display, not measured; mapped menu effect must verify touch"})

    def point(self, x, y):
        cw, ch, ox, oy, w, h = self.viewport
        require(0 <= x < cw and 0 <= y < ch, "Touch outside logical canvas")
        return ox + (2*x+1)*w//(2*cw), oy + (2*y+1)*h//(2*ch)

    def box(self, bounds):
        cw, ch, ox, oy, w, h = self.viewport
        x0, y0, x1, y1 = bounds
        return (ox+x0*w//cw, oy+y0*h//ch,
                ox+(x1*w+cw-1)//cw, oy+(y1*h+ch-1)//ch)

    def tap(self, x, y, settle=.25):
        px, py = self.point(x, y)
        self.case["touches"].append({"logical": [x, y], "physical": [px, py]})
        self.request(f"INPUT POINTER DOWN {px} {py} 1")
        self.request("INPUT SYNC")
        self.pump(.08)
        self.request(f"INPUT POINTER UP {px} {py} 1")
        self.request("INPUT SYNC")
        self.pump(settle)

    def stop(self):
        if self.owned and self.selected_active():
            # A previous fault must not prevent release of the selected package.
            self.event(command="PACKAGE stop " + self.identity, cleanup=True)
            self.client.request("PACKAGE stop " + self.identity, timeout=30)
            end = time.monotonic()+8
            while time.monotonic() < end:
                frames = self.client.request("PACKAGES", timeout=30)
                if not any(f.kind == "PKG" and f.payload.split("\t", 1)[0].endswith(":"+self.identity)
                           and "active=1" in f.payload for f in frames):
                    return
                self.client.pump_logs()
                time.sleep(.1)
            raise TimeoutError("Selected package did not stop")

    def launch_diagnostics(self, launch):
        at = launch["log_start_index"]
        prefix = "Anarch" if self.app == "anarch" else "Pocket"
        ready = "Anarch: ready " if self.app == "anarch" else "Pocket: ready app=" + self.identity
        loaded = "Anarch: save loaded " if self.app == "anarch" else "Pocket: load completed"
        locales = [re.search(prefix+r": language (zh|en)\b", line) for line in self.logs[at:]]
        locales = [match.group(1) for match in locales if match]
        require(all(locale == launch["expected_locale"] for locale in locales),
                "Fresh startup language log disagrees with prelaunch durable locale")
        launch.update(ready_log=self.has(ready, at), load_log=self.has(loaded, at),
            language_log=locales[-1] if locales else None,
            host_launch_completed=any("Launch completion: " in line and
                ":"+self.identity+" " in line and "status=0" in line for line in self.logs[at:]))
        observed = {"ready": launch["ready_log"], "load": launch["load_log"],
            "language": bool(locales), "viewport": any(VIEWPORT.search(line) for line in self.logs[at:])}
        launch["missing_diagnostics"] = [name for name, seen in observed.items() if not seen]
        launch["diagnostics_complete"] = all(observed.values())
        launch["dropped_events"] = len(self.drops) - launch["drop_start_index"]

    def start(self, phase, require_load=False):
        require(not self.active(), "Another package is active before launch")
        prior = self.snapshot(phase+"-prelaunch", allow_missing=True)
        key = "anarch.save.v1" if self.app == "anarch" else self.identity+".save.v1"
        require(not require_load or key in prior, "Required reload has no valid prelaunch game save")
        locale = locale_of(prior, self.app)
        launch = {"phase": phase, "attempt": 1, "log_start_index": len(self.logs),
            "drop_start_index": len(self.drops), "expected_locale": locale,
            "locale_source": "prelaunch_PXKV_or_absent_preference_default",
            "prelaunch_save_present": key in prior, "permission_allowed": False,
            "require_load": require_load, "verification_complete": False}
        self.case["launches"].append(launch)
        self._launch, self._prelaunch_values = launch, prior
        self._launch_started = time.monotonic()
        self.viewport = None
        self.owned = True
        self.request("PACKAGE run " + self.identity)
        deadline = time.monotonic()+18
        while time.monotonic() < deadline:
            prompted = any("Runtime permission prompt queued:" in line and
                ":"+self.identity+" permission=" in line for line in self.logs[launch["log_start_index"]:])
            if prompted and not launch["permission_allowed"]:
                self.shot(phase+"-permission")
                self.request("INPUT TAP 200 183")  # Host UI is in physical pixels.
                self.request("INPUT SYNC")
                launch["permission_allowed"] = True
            self.launch_diagnostics(launch)
            if (launch["ready_log"] or launch["host_launch_completed"]) and self.selected_active():
                self.pump(.3)
                self.launch_diagnostics(launch)
                if not self.geometry(launch["log_start_index"]):
                    self.derive_geometry()
                self._launch_viewport = self.case["viewports"][-1]
                self.shot(phase+"-ready")
                require(self.selected_active(), "Captured selected package is no longer active")
                launch.update(selected_package_active=True, nonblank_capture=True,
                    startup_evidence=["selected_package_active", "nonblank_physical_capture",
                        "app_ready_log" if launch["ready_log"] else "host_launch_completed"],
                    viewport_source=self._launch_viewport["source"])
                # menu() must still prove mapped touch, stable durable locale and,
                # for a reload, retained game content before acceptance is complete.
                return locale
            self.client.request("PING", timeout=5)
            self.pump(.02)
        self.launch_diagnostics(launch)
        raise AssertionError("Startup lacks selected active package plus fresh ready/Host completion: " +
                             json.dumps(launch, ensure_ascii=False))

    def confirm_launch(self, label):
        launch = self._launch
        self._launch_viewport["touch_verified"] = True
        self._launch_viewport["touch_check"] = label
        # A late viewport log must also satisfy the strict measured-log checks.
        if self.geometry(launch["log_start_index"]):
            self.case["viewports"][-1].update(touch_verified=True, touch_check=label)
            launch["late_viewport_log_validated"] = True
        self.launch_diagnostics(launch)
        values = self.stable(label+"-launch-locale", launch["expected_locale"])
        launch.update(menu_touch_verified=True, stable_durable_locale=locale_of(values, self.app))
        if launch["require_load"]:
            launch["reload_content"] = reload_content(self.app, self._prelaunch_values, values,
                self.decoder, time.monotonic()-self._launch_started)
            launch["reload_verified"] = True
        else:
            launch["reload_verified"] = None
        require(self.selected_active(), "Selected package stopped during menu verification")
        launch["verification_complete"] = True

    def snapshot(self, label, allow_missing=False):
        """Download both banks, independently decode CRC, choose max generation."""
        fs = pxadb.NormalFsClient(self.client)
        frames = self.request(f"PACKAGE paths {PUBLISHER}:{self.identity}")
        roots = [f.payload.split("\t", 1)[1] for f in frames
                 if f.kind == "PATH" and f.payload.startswith("private\t")]
        require(len(roots) == 1, "No unique selected-package private directory")
        private = roots[0]+"/.pxa-storage"
        candidates, banks = [], []
        if allow_missing and not any(kind == "D" and name == ".pxa-storage"
                                     for kind, _, name in fs.list(roots[0])):
            self.case["storage"].append({"label": label, "generation": None, "banks": [],
                "save_present": False, "locale": "zh", "absence": "no private storage directory"})
            return {}
        for kind, _, name in fs.list(private):
            if kind != "F" or name not in (".pxa-kv-a", ".pxa-kv-b"):
                continue
            path = self.args.output / self.app / (label+"-"+name.removeprefix(".pxa-")+".bin")
            for attempt in range(3):
                try:
                    fs.get(private+"/"+name, path)
                    break
                except pxadb.PxaDbError as error:
                    if "file transfer checksum mismatch" not in str(error) or attempt == 2:
                        raise
                    self.case["transport_retries"].append({"kind": "storage", "label": label,
                                                            "attempt": attempt+1, "error": str(error)})
                    self.pump(.2)
            raw = path.read_bytes()
            row = {"bank": name, "path": str(path.relative_to(self.args.output)), **digest(raw)}
            try:
                generation, values = self.decoder.host_values(raw)
                require(all(len(value) <= 960 for value in values.values()), "Device stored value exceeds 960B")
                row.update(valid=True, generation=generation)
                candidates.append((generation, values))
            except (AssertionError, UnicodeError, struct.error) as error:
                row.update(valid=False, error=str(error))
            banks.append(row)
        if allow_missing and not banks:
            self.case["storage"].append({"label": label, "generation": None, "banks": [],
                "save_present": False, "locale": "zh", "absence": "no PXKV banks"})
            return {}
        require(candidates, "No valid durable PXKV generation")
        generation, values = max(candidates, key=lambda row: row[0])
        if sum(item[0] == generation for item in candidates) > 1:
            require(all(item[1] == values for item in candidates if item[0] == generation),
                    "Conflicting PXKV banks at equal generation")
        key = "anarch.save.v1" if self.app == "anarch" else self.identity+".save.v1"
        require(allow_missing or self.app == "anarch" or key in values,
                "Paused Pocket checkpoint has no game save value")
        payload, envelope = (None, {"format": "absent"})
        if key in values:
            payload, envelope = self.decoder.committed_world(values, key)
        if self.app == "tiny-elements" and (not allow_missing or key in values):
            require(envelope["format"] == "PPC1", "Tiny requires a complete committed PPC1 save")
        self.case["storage"].append({"label": label, "generation": generation, "banks": banks,
            "save_present": key in values,
            "locale": locale_of(values, self.app), "values": {key: digest(value) for key, value in values.items()},
            "game_payload": digest(payload) if payload is not None else None, "envelope": envelope})
        self.healthy()
        return values

    def stable(self, label, locale=None):
        previous = None
        for attempt in range(6):
            values = self.snapshot(label+f"-{attempt+1}")
            if values == previous and (locale is None or locale_of(values, self.app) == locale):
                return values
            previous = values
            self.pump(.35)
        raise AssertionError(f"Paused durable values did not stabilize for {label}, expected locale={locale}")

    def menu(self, label, exercise_continue=False):
        if self.app == "anarch":
            before = self.shot(label+"-menu")
            self.tap(80, 12)
            help_image = self.shot(label+"-help")
            delta = changed(before, help_image, self.box((0, 0, 160, 110)))
            require(delta["changed_fraction"] > .02, "Anarch help touch had no visible effect")
            self.tap(80, 12)
            after = self.shot(label+"-help-closed")
            restored = changed(before, after, self.box((0, 0, 160, 110)))
            require(restored["changed_fraction"] < .02, "Anarch help touch did not restore original menu")
            self.case["checks"].append({"check": label+"_help_touch", "passed": True,
                                        "opened": delta, "closed": restored})
            self.confirm_launch(label+"_help_touch")
            return
        before = self.shot(label+"-game")
        at = len(self.logs)
        self.tap(276, 18)
        self.until(lambda: self.has("Pocket: menu opened", at), "Mapped touch did not open pause menu")
        image = self.shot(label+"-paused")
        delta = changed(before, image, self.box((56, 28, 232, 200)))
        # The Host log precedes presentation; interpreted builds can finish
        # their RGB565 frame after the first capture. Observe the completed
        # frame without repeating the touch or weakening the pixel check.
        for attempt in range(4):
            if delta["changed_fraction"] > .02:
                break
            self.pump(.8)
            image = self.shot(label+f"-paused-presented-{attempt+1}")
            delta = changed(before, image, self.box((56, 28, 232, 200)))
        require(delta["changed_fraction"] > .02, "Pause menu touch had no visible effect")
        if exercise_continue:
            at = len(self.logs)
            self.tap(140, 85)
            self.until(lambda: self.has("Pocket: menu closed", at), "Mapped Continue did not resume")
            at = len(self.logs)
            self.tap(276, 18)
            self.until(lambda: self.has("Pocket: menu opened", at), "Reopening mapped pause menu failed")
            self.shot(label+"-paused-again")
        self.case["checks"].append({"check": label+"_pause_touch", "passed": True,
                                   "continue_tested": exercise_continue, **delta})
        self.confirm_launch(label+"_pause_touch")

    def toggle(self, locale, baseline, label):
        before = self.shot(label+"-before")
        at = len(self.logs)
        self.tap(125 if self.app == "anarch" else 140, 12 if self.app == "anarch" else 175)
        prefix = "Anarch" if self.app == "anarch" else "Pocket"
        self.until(lambda: self.has(prefix+": language "+locale, at),
                   f"Missing fresh {locale} language log; dropped events remain in report", seconds=8)
        values = self.stable(label, locale)
        require(game_values(values) == game_values(baseline), "Language touch changed paused game storage values")
        require(self.app == "anarch" or not self.has("Pocket: menu closed", at),
                "Language touch unexpectedly resumed the paused game")
        image = self.shot(label+"-after")
        bounds = (0, 0, 160, 110) if self.app == "anarch" else (56, 28, 232, 200)
        delta = changed(before, image, self.box(bounds))
        require(delta["changed_fraction"] > .003, "Language touch did not visibly change menu text")
        self.case["checks"].append({"check": label, "passed": True, "locale": locale,
            "non_preference_values_byte_identical": True, "fresh_language_log": True, **delta})
        return values

    def exit(self, label):
        if self.app == "anarch":
            self.stop()  # Keep original menu selection and progress untouched.
        else:
            self.tap(140, 145)
            self.until(lambda: not self.selected_active(), "Normal pause-menu Exit did not terminate", seconds=10)
        require(not self.active(), "An application remains active after selected-app exit")
        self.healthy()
        self.case["checks"].append({"check": label, "passed": True,
                                    "method": "PACKAGE stop" if self.app == "anarch" else "touch Exit"})

    def exercise(self, app):
        self.app, self.identity = app, "pxa-"+app
        self.case = {"app": app, "identity": self.identity, "passed": False, "checks": [],
                     "launches": [], "viewports": [], "screenshots": [], "touches": [],
                     "storage": [], "transport_retries": []}
        self.report["cases"].append(self.case)
        (self.args.output / app).mkdir()
        initial_locale = self.start("first")
        self.menu("first", exercise_continue=True)
        baseline = self.stable("paused-before-language")
        require(locale_of(baseline, app) == initial_locale, "Prelaunch locale disagrees with stable paused locale")
        self.case["initial_locale"] = initial_locale
        if initial_locale == "en":
            baseline = self.toggle("zh", baseline, "normalize-existing-locale-zh")
        self.shot("chinese-menu")
        self.toggle("en", baseline, "switch-en")
        self.exit("normal-exit-en")
        # The exit checkpoint is allowed to serialize time or world progress.
        self.snapshot("after-en-exit")
        reloaded = self.start("relaunch-en", require_load=True)
        require(reloaded == "en", "English preference did not survive a fresh launch")
        self.menu("relaunch-en")
        baseline = self.stable("relaunch-paused")
        require(locale_of(baseline, app) == "en", "Relaunch lost durable English preference")
        self.toggle("zh", baseline, "switch-back-zh")
        self.exit("normal-exit-zh")
        final = self.snapshot("final-after-zh-exit")
        require(locale_of(final, app) == "zh", "Final Chinese preference is not durable")
        require(self.start("relaunch-zh", require_load=True) == "zh", "Chinese preference did not survive relaunch")
        self.menu("relaunch-zh")
        self.exit("final-exit")
        final = self.snapshot("final-stopped")
        require(locale_of(final, app) == "zh", "Final stopped storage is not Chinese")
        require(all(launch["verification_complete"] for launch in self.case["launches"]),
                "A launch lacks subsequent touch, durable locale or required reload evidence")
        self.case["checks"].append({"check": "en_and_zh_survive_distinct_launches", "passed": True})
        self.case["passed"] = True
        self.owned = False


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="PXADB port; never auto-detected")
    parser.add_argument("--output", required=True, type=Path, help="fresh directory below local/artifacts")
    parser.add_argument("--apps", nargs="+", choices=APPS, default=list(APPS), help="default: all ten installed games")
    args = parser.parse_args(argv)
    args.output = args.output.expanduser().resolve()
    require(ARTIFACTS.resolve() in args.output.parents, "--output must be below local/artifacts")
    require(not args.output.exists(), "Choose a fresh --output directory")
    require(len(set(args.apps)) == len(args.apps), "--apps contains duplicates")
    args.output.mkdir(parents=True)
    report = {"schema": "pocket-display-device-v2", "passed": False, "apps": args.apps,
              "expected_device": SERIAL, "display": [296, 240], "cases": [], "errors": [],
              "log_drops": [], "faults": [], "ended_without_active_app": False,
              "screenshot_review": "JPEG fit/glyphs retained for manual review; viewport logs validated when available; derived mapping explicitly marked and touch-verified",
               "startup_policy": "selected active + fresh app ready or Host completion + nonblank capture; menu touch and stable locale required; reload independently compares real save payloads; DROP never proves success"}
    status = 1
    device = None
    try:
        with (args.output / "serial.jsonl").open("w", buffering=1) as stream:
            with pxadb.open_client(args.port, 30) as client:
                info = pxadb.info_properties(client.device_info)
                report.update(hello=client.device_info, device=info)
                require(info.get("target") == "esp32s3" and info.get("serial", "").lower() == SERIAL,
                        "Unexpected physical board identity")
                device = Device(args, client, report, stream)
                try:
                    report["initial_packages"] = [f.payload for f in device.request("PACKAGES") if f.kind == "PKG"]
                    require(not device.active(), "Another package is already active; no packages were stopped")
                    client.subscribe_logs()
                    # Discard historical LOGSUB records, then wake/unlock the idle board.
                    device.logs.clear(); device.drops.clear(); device.faults.clear()
                    device.request("INPUT KEY HOME")
                    device.request("INPUT SWIPE 148 192 148 48 600 8")
                    device.pump(.8)
                    for app in args.apps:
                        device.exercise(app)
                    require(not device.active(), "Final active-package set is not empty")
                    report["ended_without_active_app"] = True
                    device.healthy()
                    report["passed"], status = True, 0
                finally:
                    cleanup_errors = []
                    try:
                        device.client.request("INPUT CANCEL", timeout=30)
                    except Exception:
                        cleanup_errors.append({"phase": "input_cancel", "error": traceback.format_exc()})
                    try:
                        # Bypass healthy() only for cleanup; actual faults remain FAIL.
                        if device.owned:
                            frames = device.client.request("PACKAGES", timeout=30)
                            active = any(f.kind == "PKG" and "active=1" in f.payload and
                                f.payload.split("\t", 1)[0].endswith(":"+device.identity) for f in frames)
                            if active:
                                device.client.request("PACKAGE stop "+device.identity, timeout=30)
                                deadline = time.monotonic()+8
                                while time.monotonic() < deadline:
                                    stopped = device.client.request("PACKAGES", timeout=30)
                                    if not any(f.kind == "PKG" and "active=1" in f.payload and
                                        f.payload.split("\t", 1)[0].endswith(":"+device.identity) for f in stopped):
                                        break
                                    device.client.pump_logs()
                                    time.sleep(.1)
                        frames = device.client.request("PACKAGES", timeout=30)
                        report["final_packages"] = [f.payload for f in frames if f.kind == "PKG"]
                        report["ended_without_active_app"] = not any(f.kind == "PKG" and "active=1" in f.payload for f in frames)
                    except Exception:
                        cleanup_errors.append({"phase": "selected_app_stop", "error": traceback.format_exc()})
                    report["errors"].extend(cleanup_errors)
                    report["log_drops"], report["faults"] = device.drops, device.faults
                    if cleanup_errors or device.faults or not report["ended_without_active_app"]:
                        report["passed"], status = False, 1
    except BaseException:
        report["errors"].append({"phase": "acceptance", "error": traceback.format_exc()})
        report["passed"], status = False, 1
        if device:
            report["log_drops"], report["faults"] = device.drops, device.faults
    finally:
        (args.output / "results.json").write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n")
    print(json.dumps({"passed": report["passed"], "completed_apps": [case["app"] for case in report["cases"] if case["passed"]],
                      "report": str(args.output / "results.json"), "errors": report["errors"]}, indent=2, ensure_ascii=False))
    return status


if __name__ == "__main__":
    raise SystemExit(main())
