#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Accept an installed Pocket Play world on the physical pai-touch board.

Example, from any directory:
    python3 /path/to/pxa-projects/local/pxa-apps/common/pocket-play/tests/worlds_device.py \
        --game lantern-grove --output /path/to/pxa-projects/local/artifacts/pocket-worlds/20261024-063000/grove

Requires the installed signed app and the existing PXADB/Pillow dependencies.
Uses physical touch, normal menus and read-only private-storage transfers; never
installs packages, edits saves, clears a world, or deletes existing progress.
Normal collection, purchases, serving and additive painting remain in the save.
Idle lock runs by default. --skip-idle-lock explicitly omits that acceptance.
The script stops only its selected app and writes results.json on failure too.
"""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import struct
import sys
import time
import traceback
import zlib

sys.dont_write_bytecode = True
WORKSPACE = Path(__file__).resolve().parents[5]
ARTIFACTS = WORKSPACE / "local/artifacts"
HELPERS = WORKSPACE / "local/pxa-apps/common/pocket-game/tests"
IDS = {game: "pxa-" + game for game in ("lantern-grove", "harbor-cafe", "tiny-elements")}
SERIAL = "a4:cb:8f:d5:d4:f0"
FAULTS = re.compile(
    r"Guest exception|callback failed|out of bounds memory access|unreachable instruction|"
    r"wasm trap|stack overflow|input queue overflow|event queue overflow|"
    r"Pocket: .*failed|exit flush timed out|ASSERT|Guru Meditation", re.I)


def require(condition, label):
    if not condition:
        raise AssertionError(label)


def transport():
    # The frozen helper imports simulator and pxadb by name. Resolve both from
    # this source file, rather than inheriting the caller's working directory.
    sys.path[:0] = [str(HELPERS), str(WORKSPACE / "tools/pxadb")]
    spec = importlib.util.spec_from_file_location("pocket_worlds_device_helper", HELPERS / "device.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.IDS.update(IDS)
    return module


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def host_values(raw):
    """Independently validate PXKV length, CRC, sorted keys and record bounds."""
    require(len(raw) >= 26, "PXKV short header/body")
    magic, version, reserved, generation, size, crc = struct.unpack_from("<4sHHQII", raw)
    body = raw[24:]
    require(magic == b"PXKV" and version == 1 and reserved == 0 and generation > 0, "PXKV header")
    require(len(body) == size and zlib.crc32(body) == crc, "PXKV length/CRC")
    count = struct.unpack_from("<H", body)[0]
    offset, previous, values = 2, b"", {}
    for _ in range(count):
        require(offset < len(body), "PXKV missing key length")
        length = body[offset]
        offset += 1
        require(1 <= length <= 64 and offset + length + 2 <= len(body), "PXKV key bounds")
        key = body[offset:offset+length]
        offset += length
        require(key > previous and re.fullmatch(rb"[A-Za-z][A-Za-z0-9_.-]*", key), "PXKV key order/format")
        previous = key
        length = struct.unpack_from("<H", body, offset)[0]
        offset += 2
        require(offset + length <= len(body) and length <= 2048, "PXKV value bounds")
        values[key.decode("ascii")] = body[offset:offset+length]
        offset += length
    require(offset == len(body), "PXKV trailing bytes")
    return generation, values


def committed_world(values, key):
    require(key in values, "No committed game save key")
    head = values[key]
    if head[:4] != b"PPC1":
        return head, {"format": "direct"}
    require(len(head) == 24 and head[4] == 1 and head[5] <= 1 and not any(head[20:24]), "PPC1 header")
    size, checksum, generation = struct.unpack_from("<HIQ", head, 6)
    require(0 < size <= 1792 and generation > 0, "PPC1 size/generation")
    bank = head[5]
    chunks = []
    for chunk in range((size + 895) // 896):
        name = f"{key}.b{bank}.c{chunk}"
        require(name in values, "Missing committed PPC1 chunk: " + name)
        data = values[name]
        require(len(data) == min(896, size - 896*chunk), "PPC1 chunk size")
        chunks.append(data)
    payload = b"".join(chunks)
    require(len(payload) == size and fnv(payload) == checksum, "PPC1 assembled FNV/length")
    return payload, {"format": "PPC1", "bank": bank, "generation": generation, "chunks": len(chunks),
                     "maximum_stored_value_bytes": max(map(len, values.values()))}


def checked_payload(raw, magic, size):
    require(len(raw) == size, f"{magic!r}: wrong payload size {len(raw)}")
    require(raw[:4] == magic and raw[4] == 1, f"{magic!r}: wrong magic/version")
    require(struct.unpack_from("<I", raw, size-4)[0] == fnv(raw[:-4]), f"{magic!r}: FNV mismatch")


def decode_grove(raw):
    checked_payload(raw, b"LGV1", 128)
    require(raw[5:8] == bytes(3) and not any(raw[80:124]), "LGV1 reserved bytes")
    s = {name: list(struct.unpack_from("<3I", raw, offset))
         for name, offset in (("resources", 8), ("pending", 20), ("fraction", 32))}
    s["level"] = list(raw[44:48])
    s.update(zip(("goals", "sound", "view", "selected"), raw[48:52]))
    s.update(zip(("collected", "taps", "cycle_ms"), struct.unpack_from("<3I", raw, 52)))
    s["produced_ms"], s["last_wall"] = struct.unpack_from("<2Q", raw, 64)
    capacity = 120 + 1500*s["level"][1] + 3000*s["level"][3]
    require(all(v <= 999999 for v in s["resources"]), "LGV1 resources range")
    require(all(v <= capacity for v in s["pending"]), "LGV1 basket range")
    require(all(v < 1000000 for v in s["fraction"]), "LGV1 fractional range")
    require(all(v <= maximum for v, maximum in zip(s["level"], (5, 4, 4, 3))), "LGV1 level range")
    require(s["sound"] <= 1 and s["view"] <= 3 and s["selected"] <= 3, "LGV1 UI range")
    require(s["collected"] <= 999999 and s["taps"] <= 999999 and s["cycle_ms"] < 120000,
            "LGV1 progress range")
    require(s["produced_ms"] <= 315360000000, "LGV1 produced time range")
    require(s["last_wall"] == 0 or 1704067200 <= s["last_wall"] < 4133980800, "LGV1 UTC range")
    goals = s["goals"]
    require((goals & (goals+1)) == 0, "LGV1 nonsequential goals")
    level, collected = s["level"], s["collected"]
    achieved = (collected >= 10, level[0] >= 2, level[1] >= 1, collected >= 60,
                level[2] >= 1, level[3] >= 1, level[0] >= 4 and level[1] >= 2,
                level[2] >= 3 and level[3] >= 2 and collected >= 1800)
    require(all(not (goals & (1 << g)) or achieved[g] for g in range(8)), "LGV1 unearned goal")
    return s


def decode_cafe(raw):
    checked_payload(raw, b"HCFE", 160)
    require(raw[5] == 0 and struct.unpack_from("<H", raw, 6)[0] == 160, "HCFE header")
    require(not any(raw[98:156]), "HCFE reserved bytes")
    s = {"coins": struct.unpack_from("<I", raw, 8)[0]}
    s.update(zip(("day", "paid_day", "total_served", "total_failed", "stars", "debt"),
                 struct.unpack_from("<6H", raw, 12)))
    s["stock"] = list(struct.unpack_from("<3H", raw, 24))
    s["level"] = list(raw[30:33])
    s.update(zip(("recipe", "table", "view", "shop", "sound", "rescued", "summary"), raw[33:40]))
    s.update(zip(("day_ms", "spawn_ms", "seed", "cook_ms"), struct.unpack_from("<4I", raw, 40)))
    s["cook_recipe"], s["ready_recipe"] = raw[56:58]
    s["served"], s["failed"] = struct.unpack_from("<2H", raw, 58)
    s["revenue"], s["cogs"] = struct.unpack_from("<2I", raw, 62)
    s["bonus"], s["fee"] = struct.unpack_from("<2H", raw, 70)
    s["customers"] = []
    for at in (74, 82, 90):
        require(raw[at+3] == 0, "HCFE customer reserved byte")
        c = dict(zip(("stage", "recipe", "skin"), raw[at:at+3]))
        c["patience"] = struct.unpack_from("<I", raw, at+4)[0]
        require(c["stage"] <= 2 and c["recipe"] <= 2 and c["skin"] <= 3 and c["patience"] <= 60000,
                "HCFE customer range")
        require((c["stage"] != 0 and c["patience"] > 0) or
                (c["stage"] == c["recipe"] == c["skin"] == c["patience"] == 0), "HCFE customer consistency")
        s["customers"].append(c)
    require(s["coins"] <= 999999 and 1 <= s["day"] <= 999 and s["paid_day"] <= s["day"], "HCFE cash/day range")
    require(s["total_served"] <= 60000 and s["total_failed"] <= 60000 and s["stars"] <= 999 and s["debt"] <= 1000,
            "HCFE cumulative range")
    require(all(v <= 99 for v in s["stock"]) and all(v <= 3 for v in s["level"]), "HCFE inventory/level range")
    for name, maximum in (("recipe", 2), ("table", 2), ("view", 3), ("shop", 3), ("sound", 1),
                          ("rescued", 1), ("summary", 1), ("day_ms", 150000), ("spawn_ms", 12000),
                          ("cook_ms", 2400), ("cook_recipe", 3), ("ready_recipe", 3), ("served", 1000),
                          ("failed", 1000), ("revenue", 10000), ("cogs", 10000)):
        require(s[name] <= maximum, "HCFE range: " + name)
    require((s["cook_recipe"] == 3) == (s["cook_ms"] == 0), "HCFE cooking consistency")
    require(s["cook_recipe"] == 3 or s["ready_recipe"] == 3, "HCFE cooking and ready overlap")
    require(s["bonus"] in (0, 8) and s["fee"] in (0, 10), "HCFE settlement range")
    if s["summary"]:
        require(s["day_ms"] == 150000 and s["paid_day"] == s["day"] and
                s["cook_recipe"] == s["ready_recipe"] == 3 and
                all(c["stage"] == 0 for c in s["customers"]), "HCFE closed-day consistency")
    else:
        require(s["day_ms"] < 150000, "HCFE active day consistency")
    return s


def decode_elements(raw):
    checked_payload(raw, b"TEL1", 1780)
    require(raw[5:8] == bytes((72, 48, 0)) and raw[43] <= 1 and not any(raw[44:48]), "TEL1 geometry/reserved bytes")
    s = dict(zip(("rng", "steps", "scene_start", "burned", "cooled"), struct.unpack_from("<5I", raw, 8)))
    s["accumulator"] = struct.unpack_from("<H", raw, 28)[0]
    s.update(zip(("scene", "badges", "material", "brush", "paused", "ui", "pending", "target",
                  "cx", "cy", "selected", "page", "confirm_yes"), raw[30:43]))
    s["muted"] = raw[43]
    require(s["rng"] != 0 and s["accumulator"] < 50 and s["badges"] & 0xf0 == 0, "TEL1 RNG/time/badges")
    for name, maximum in (("scene", 4), ("material", 9), ("brush", 2), ("paused", 1), ("ui", 3),
                          ("pending", 2), ("target", 4), ("cx", 71), ("cy", 47), ("selected", 4),
                          ("page", 1), ("confirm_yes", 1)):
        require(s[name] <= maximum, "TEL1 range: " + name)
    require((s["ui"] == 3) == (s["pending"] != 0), "TEL1 confirmation consistency")
    s["cells"] = [nibble for byte in raw[48:1776] for nibble in (byte & 15, byte >> 4)]
    require(all(v < 10 for v in s["cells"]), "TEL1 invalid material nibble")
    s["counts"] = [s["cells"].count(m) for m in range(10)]
    return s


DECODERS = {"lantern-grove": decode_grove, "harbor-cafe": decode_cafe, "tiny-elements": decode_elements}
GROVE_STABLE = ("resources", "level", "goals", "sound", "collected", "taps")
CAFE_GROWTH = ("coins", "day", "paid_day", "total_served", "total_failed", "stars", "debt", "stock", "level")


class Session:
    def __init__(self, d, module):
        self.d, self.module, self.report = d, module, d.report
        self.game = d.args.game
        self.paused, self.index = False, 0

    def check(self, condition, label, **evidence):
        row = {"check": label, "passed": bool(condition), **evidence}
        self.report["checks"].append(row)
        require(condition, label)

    def unavailable(self, label, reason):
        self.report.setdefault("unavailable", []).append({"check": label, "reason": reason})

    def same(self, a, b, fields, label):
        different = [name for name in fields if a[name] != b[name]]
        self.check(not different, label, different_fields=different)

    def pause(self):
        if not self.paused:
            at = len(self.d.logs)
            self.d.tap(276, 18, settle=.2)
            self.d.until(lambda: any("Pocket: menu opened" in line for line in self.d.logs[at:]), "menu opened")
            self.paused = True

    def resume(self):
        if self.paused:
            at = len(self.d.logs)
            self.d.tap(140, 85, settle=.15)
            self.d.until(lambda: any("Pocket: menu closed" in line for line in self.d.logs[at:]), "menu continue")
            self.paused = False

    def saved_world(self, tag, marker_only=False):
        """Validate durable generations; optionally inspect a pre-write marker."""
        fs = self.module.pxadb.NormalFsClient(self.d.client)
        frames = self.d.request(f"PACKAGE paths {self.module.PUBLISHER}:{self.d.identity}")
        private = next(f.payload.split("\t", 1)[1] for f in frames
                       if f.kind == "PATH" and f.payload.startswith("private\t")) + "/.pxa-storage"
        candidates = []
        for kind, _, name in fs.list(private):
            if kind != "F" or name not in (".pxa-kv-a", ".pxa-kv-b"):
                continue
            path = self.d.args.output / (tag + "-" + name.removeprefix(".pxa-"))
            fs.get(private + "/" + name, path)
            try:
                generation, values = host_values(path.read_bytes())
            except (AssertionError, UnicodeDecodeError, struct.error) as error:
                self.report.setdefault("invalid_host_generations", []).append({"file": path.name, "error": str(error)})
                continue
            candidates.append((generation, values))
        if marker_only and not candidates:
            return {"generation": 0, "logical_generation": 0}
        require(candidates, "No valid host storage generation")
        generation, values = max(candidates, key=lambda row: row[0])
        key = self.d.identity + ".save.v1"
        if marker_only:
            logical = committed_world(values, key)[1].get("generation", 0) if key in values else 0
            return {"generation": generation, "logical_generation": logical}
        payload, envelope = committed_world(values, key)
        require(all(len(value) <= 960 for value in values.values()), "ESP stored value exceeds 960 bytes")
        if self.game == "tiny-elements":
            require(envelope["format"] == "PPC1", "Tiny Elements must use committed PPC1 chunks")
        self.report.setdefault("storage", {})[tag] = {"generation": generation, "size": len(payload),
            "sha256": hashlib.sha256(payload).hexdigest(), "envelope": envelope}
        return payload

    def read(self, label):
        self.index += 1
        tag = f"{self.index:03d}-{label}"
        for attempt in range(5):
            try:
                raw = self.saved_world(tag)
                break
            except self.module.pxadb.PxaDbError as error:
                if "file transfer checksum mismatch" not in str(error) or attempt == 4:
                    raise
                self.report.setdefault("storage_read_retries", []).append(
                    {"label": tag, "attempt": attempt+1, "error": str(error)})
                self.d.pump(.2)
        state = DECODERS[self.game](raw)
        (self.d.args.output / (tag + ".save")).write_bytes(raw)
        (self.d.args.output / (tag + ".json")).write_text(json.dumps(state, indent=2) + "\n")
        self.last_kv = {path.name.removeprefix(tag + "-"): hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in self.d.args.output.glob(tag + "-kv-*")}
        self.report.setdefault("snapshots", []).append({"label": tag, "sha256": hashlib.sha256(raw).hexdigest(),
                                                        "kv_sha256": self.last_kv, "valid": True})
        return state, raw

    def snapshot(self, label):
        self.pause()
        self.d.pump(.8)
        return self.read(label)[0]

    def start(self):
        at = len(self.d.logs)
        self.d.request("PACKAGE run " + self.d.identity)
        self.d.pump(.75)
        # A permission prompt blocks guest initialization. Only touch Allow for
        # a fresh, selected-app prompt, never replayed LOGSUB history.
        def prompted():
            return any("Runtime permission prompt queued:" in line and
                       ":" + self.d.identity + " permission=" in line for line in self.d.logs[at:])
        def ready():
            return any("Pocket: ready app=" + self.d.identity in line for line in self.d.logs[at:])
        def audio_ready():
            return any("Pocket: audio ready 16000 Hz mono" in line for line in self.d.logs[at:])
        self.d.until(lambda: prompted() or ready() or audio_ready() or
                     any(re.fullmatch(r"dropped=[1-9][0-9]*", line) for line in self.d.logs[at:]),
                     "selected app launch, media permission or explicit diagnostic loss")
        # Permission acquisition may arrive after game readiness. A host DROP
        # can lose the audio-ready diagnostic even when its service is active.
        try:
            self.d.until(lambda: prompted() or audio_ready(), "media permission or audio ready", seconds=8)
        except TimeoutError:
            require(any(re.fullmatch(r"dropped=[1-9][0-9]*", line) for line in self.d.logs[at:]),
                    "Audio startup unobserved without a host log-drop diagnostic")
        permission = prompted()
        if permission:
            self.d.shot(f"launch-{len(self.report.get('launches', []))}-permission")
            self.d.request("INPUT TAP 200 183")  # Host permission UI uses physical pixels.
            self.d.pump(.6)
            self.d.until(audio_ready, "audio ready after permission", seconds=8)
        audio_observed = audio_ready()
        self.d.pump(.25)
        ready_lines = [line for line in self.d.logs[at:] if "Pocket: ready app=" + self.d.identity in line]
        viewport_observed = self.d.configure_viewport(at)
        if not viewport_observed:
            require(any(re.fullmatch(r"dropped=[1-9][0-9]*", line) for line in self.d.logs[at:]),
                    "Missing viewport without a host log-drop diagnostic")
            # This helper requires the 296x240 board; fit is verified again by
            # actual menu touches and the screenshots below after diagnostic loss.
            self.d.ox, self.d.oy = 0, 5
            self.d.viewport_width, self.d.viewport_height = 296, 230
            self.report.setdefault("viewport_log_drops", []).append({"origin": [0, 5], "size": [296, 230]})
        self.d.until(lambda: self.active(), "selected package active")
        self.paused = False
        self.report.setdefault("launches", []).append({"permission_prompt": permission, "audio_ready": audio_observed,
                                                       "ready_log_observed": bool(ready_lines)})
        self.d.shot(f"launch-{len(self.report['launches'])}")
        if not ready_lines or not audio_observed:
            # Require independent active-package, real menu touch and valid
            # newly committed save evidence when startup diagnostics are lost.
            # Missing audio observations remain explicitly absent in reports.
            require(any(re.fullmatch(r"dropped=[1-9][0-9]*", line) for line in self.d.logs[at:]),
                    "Missing startup diagnostic without a nonzero PXADB DROP")
            before = self.saved_world(f"launch-{len(self.report['launches'])}-before-menu", marker_only=True)
            for attempt in range(10):
                self.snapshot(f"startup-with-dropped-diagnostics-{attempt}")
                after = self.report["storage"][self.report["snapshots"][-1]["label"]]
                advanced = after["generation"] > before["generation"]
                if self.game == "tiny-elements":
                    advanced = advanced and after["envelope"]["generation"] > before["logical_generation"]
                if advanced:
                    break
                self.d.pump(.2)
            require(advanced, "Menu checkpoint did not commit a newer complete save")
            self.report.setdefault("startup_log_drops", []).append({"package_active": True, "audio_ready": audio_observed,
                "ready_log_observed": bool(ready_lines), "screenshot_captured": True,
                "touch_menu_verified": True, "independent_save_valid": True,
                "before_marker": before, "after_generation": after["generation"],
                "after_envelope": after["envelope"], "new_commit_verified": True})

    def active(self):
        return any(f.kind == "PKG" and ":"+self.d.identity+"\t" in f.payload and "active=1" in f.payload
                   for f in self.d.request("PACKAGES"))

    def foreground_perf(self, label="foreground"):
        self.resume()
        self.d.pump(.4)
        self.d.request("PERF CLEAR")
        self.d.request("PERF START")
        self.d.pump(3)
        props = self.module.pxadb.info_properties(self.d.request("PERF STOP")[-1].payload)
        samples = {}
        self.report.setdefault("perf", {})[label] = {"properties": props, "intervals_us": samples}
        for kind in ("raster", "present"):
            count = int(props[kind])
            require(int(props[kind+"_overflow"]) == 0, label + ": PERF overflow")
            values = []
            for offset in range(0, count, 16):
                row = self.module.pxadb.info_properties(self.d.request(f"PERF READ {kind} {offset}")[-1].payload)
                chunk = [int(v) for v in row["values"].split(",") if v]
                require(int(row["offset"]) == offset and len(chunk) == int(row["count"]), "Malformed PERF block")
                values.extend(chunk)
            require(len(values) == count and all(v > 0 for v in values), "Malformed PERF intervals")
            samples[kind] = values
        require(int(props["surface_changed"]) == 0, label + ": surface changed")
        self.check(int(props["present"]) > 0 and bool(samples["present"]), label + "_presents_frames")
        fps = 1e6*len(samples["present"])/sum(samples["present"])
        self.report["perf"][label]["display_fps"] = fps
        self.d.shot(label)

    def exit(self):
        self.pause()
        self.d.pointer("DOWN", 140, 145)
        self.d.until(lambda: not self.active(), "menu EXIT completed", seconds=8)
        self.d.request("INPUT CANCEL")
        self.paused = False

    def grove_home(self, state):
        if state["view"] == 0:
            return state
        # The touch-only HUD Back target is x184..255, y21..41.
        self.resume()
        self.d.tap(220, 30, settle=.1)
        after = self.snapshot("grove-touch-return-home")
        self.same(state, after, GROVE_STABLE, "grove_touch_back_preserves_growth")
        self.check(after["view"] == 0, "grove_return_home")
        return after

    def cafe_home(self, state):
        if state["view"] != 0 and not state["summary"]:
            self.resume()
            self.d.tap(70, 202, settle=.1)
            state = self.snapshot("cafe-home")
            self.check(state["view"] == 0, "cafe_touch_back")
        if state["summary"]:
            previous = state
            self.resume()
            self.d.tap(143, 190, settle=.1)
            state = self.snapshot("cafe-next-day")
            self.same(previous, state, ("coins", "stock", "level", "total_served", "total_failed", "stars", "debt"),
                      "cafe_next_day_preserves_business_growth")
        return state


def grove_price(state, item):
    n = state["level"][item]
    return ((8*n*n, 4*n, 3*n if n > 2 else 0), (10+12*n, 0, 3+3*n),
            (18+80*n*n, 8+24*n*n, 4+10*n*n), (22+160*n*n, 12+80*n*n, 6+35*n*n))[item]


def grove_play(s):
    before = s.grove_home(s.snapshot("grove-initial"))
    s.resume()
    s.d.pump(.8)
    s.d.tap(50, 183, settle=.1)
    after = s.snapshot("grove-collected")
    s.check(after["taps"] == min(999999, before["taps"]+1) and after["collected"] >= before["collected"],
            "grove_real_collect_increments_taps_and_collection")
    s.check(all(b >= a for a, b in zip(before["resources"], after["resources"])), "grove_collect_adds_resources")
    s.resume()
    s.d.pointer("DOWN", 50, 183)
    s.d.request("INPUT CANCEL")
    # CANCEL terminates the host gesture; a later UP is an invalid INPUT request.
    cancel = s.snapshot("grove-cancel")
    s.check(cancel["taps"] == after["taps"] and cancel["resources"] == after["resources"], "grove_cancel_does_not_collect")
    candidates = [i for i, maximum in enumerate((5, 4, 4, 3))
                  if cancel["level"][i] < maximum and all(a >= b for a, b in zip(cancel["resources"], grove_price(cancel, i)))]
    if candidates:
        item = candidates[0]
        price = grove_price(cancel, item)
        s.resume()
        s.d.tap(140, 183, settle=.1)
        s.d.tap(140, 60+34*item, settle=.1)
        upgraded = s.snapshot("grove-upgraded")
        levels = cancel["level"].copy()
        levels[item] += 1
        s.check(upgraded["level"] == levels and upgraded["resources"] == [v-p for v, p in zip(cancel["resources"], price)],
                "grove_real_upgrade_deducts_price_and_increases_level", item=item, price=price)
        s.grove_home(upgraded)
    else:
        s.unavailable("grove_upgrade", "Every upgrade is maxed or unaffordable with the preserved resources")
    s.foreground_perf()


def cafe_play(s):
    state = s.cafe_home(s.snapshot("cafe-initial"))
    s.foreground_perf()
    state = s.snapshot("cafe-orders")
    # Preserve existing food. Finish it if a matching accepted customer exists;
    # an orphaned tray is reported rather than thrown away by the test.
    if state["cook_recipe"] != 3:
        s.resume()
        s.d.pump(2.6)
        state = s.snapshot("cafe-existing-ready")
    if state["ready_recipe"] != 3:
        matches = [i for i, c in enumerate(state["customers"]) if c["stage"] == 2 and c["recipe"] == state["ready_recipe"]]
        if matches:
            s.resume()
            s.d.tap(155+48*matches[0], 115, settle=.05)
            s.d.tap(143, 179, settle=.1)
            sold = s.snapshot("cafe-existing-sale")
            s.check(sold["total_served"] == min(60000, state["total_served"]+1), "cafe_existing_food_served")
            state = sold
        else:
            s.unavailable("cafe_new_order_cycle", "Preserved ready tray has no accepted matching customer; no food discarded")
    if state["ready_recipe"] == 3:
        # Seek a new order, including when a prior save contains accepted orders.
        for _ in range(8):
            if any(c["stage"] == 1 for c in state["customers"]) or state["summary"]:
                break
            s.resume()
            s.d.pump(2)
            state = s.snapshot("cafe-await-new-order")
        state = s.cafe_home(state)
        orders = [i for i, c in enumerate(state["customers"]) if c["stage"] == 1]
        if orders:
            table = orders[0]
            recipe = state["customers"][table]["recipe"]
            ingredients = ((1, 0, 0), (1, 1, 0), (0, 0, 2))[recipe]
            # Buy only missing ingredients through the actual pantry UI.
            for item, needed in enumerate(ingredients):
                if state["stock"][item] >= needed:
                    continue
                pack_price = (8, 4, 4)[item]
                if state["coins"] < pack_price:
                    break
                s.resume()
                s.d.tap(164, 200, settle=.05)
                s.d.tap(140, 69+29*item, settle=.1)
                state = s.snapshot("cafe-required-restock")
                state = s.cafe_home(state)
            if all(v >= need for v, need in zip(state["stock"], ingredients)):
                s.resume()
                s.d.tap(155+48*table, 115, settle=.05)
                s.d.tap(143, 179, settle=.1)
                accepted = s.snapshot("cafe-accepted")
                s.check(accepted["customers"][table]["stage"] == 2 and accepted["recipe"] == recipe,
                        "cafe_real_order_accepted", table=table, recipe=recipe)
                s.resume()
                s.d.tap(143, 179, settle=.05)
                cooking = s.snapshot("cafe-cooking")
                s.check(cooking["stock"] == [v-n for v, n in zip(accepted["stock"], ingredients)] and
                        cooking["cogs"] == min(10000, accepted["cogs"] + (2, 3, 2)[recipe]),
                        "cafe_real_cooking_consumes_ingredients", ingredients=ingredients)
                s.check(cooking["cook_recipe"] == recipe or cooking["ready_recipe"] == recipe, "cafe_cooking_or_ready_recipe")
                s.resume()
                s.d.pump(2.6)
                ready = s.snapshot("cafe-ready")
                s.check(ready["ready_recipe"] == recipe and ready["cook_recipe"] == 3, "cafe_cooking_becomes_ready")
                s.resume()
                s.d.tap(143, 179, settle=.1)
                sold = s.snapshot("cafe-sold")
                sale = sold["revenue"] - ready["revenue"]
                repayment = min(ready["debt"], sale)
                s.check(sold["total_served"] == min(60000, ready["total_served"]+1) and sold["ready_recipe"] == 3 and
                        (9, 14, 10)[recipe]+ready["level"][2] <= sale <= (9, 14, 10)[recipe]+ready["level"][2]+1 and
                        sold["coins"] == min(999999, ready["coins"]+sale-repayment) and sold["debt"] == ready["debt"]-repayment,
                        "cafe_real_sale_updates_cash_revenue_and_credit", sale=sale)
                state = sold
            else:
                s.unavailable("cafe_new_order_cycle", "Preserved inventory and cash cannot fund the new recipe")
        else:
            s.unavailable("cafe_new_order_cycle", "No new unaccepted customer before day end; preserved orders retained")
    state = s.cafe_home(state)
    packs = [i for i, price in enumerate((8, 4, 4)) if state["stock"][i] <= 95 and state["coins"] >= price]
    if packs:
        item = packs[0]
        before = state
        s.resume()
        s.d.tap(164, 200, settle=.05)
        s.d.tap(140, 69+29*item, settle=.1)
        state = s.snapshot("cafe-restocked")
        expected = before["stock"].copy()
        expected[item] += 4
        s.check(state["stock"] == expected and state["coins"] == before["coins"]-(8, 4, 4)[item], "cafe_real_four_unit_restock")
        state = s.cafe_home(state)
    else:
        s.unavailable("cafe_restock", "Pantry full or preserved cash below every pack price")
    upgrades = [i for i, level in enumerate(state["level"]) if level < 3 and state["coins"] >= 20+22*level+6*level*level]
    if upgrades:
        item = upgrades[0]
        level = state["level"][item]
        cost = 20+22*level+6*level*level
        before = state
        s.resume()
        s.d.tap(240, 200, settle=.05)
        s.d.tap(140, 68+42*item, settle=.1)
        state = s.snapshot("cafe-upgraded")
        expected = before["level"].copy()
        expected[item] += 1
        s.check(state["level"] == expected and state["coins"] == before["coins"]-cost, "cafe_real_growth_upgrade", item=item, price=cost)
        s.cafe_home(state)
    else:
        s.unavailable("cafe_upgrade", "All lines maxed or preserved cash below every available upgrade")


def elements_play(s):
    state = s.snapshot("elements-initial")
    if state["ui"]:
        s.resume()
        # HELP/TASKS close below their rows; CONFIRM uses its explicit CANCEL.
        s.d.tap(203 if state["ui"] == 3 else 140, 179 if state["ui"] == 3 else 196, settle=.1)
        state = s.snapshot("elements-world")
    s.check(state["ui"] == 0, "elements_touch_world_without_replacement")
    if not state["paused"]:
        s.resume()
        s.d.tap(267, 51, settle=.1)
        state = s.snapshot("elements-paused")
    s.check(state["paused"] == 1, "elements_world_paused_for_exact_painting")
    # Use a one-cell brush and only empty cells in an open vertical corridor.
    s.resume()
    for _ in range((3-state["brush"]) % 3):
        s.d.tap(267, 75, settle=.05)
    if state["page"]:
        s.d.tap(267, 199, settle=.05)
    state = s.snapshot("elements-single-cell-brush")
    s.check(state["brush"] == 0 and state["page"] == 0, "elements_touch_brush_and_palette")
    locations = []
    for y in range(3, 30):
        for x in range(4, 68):
            if all(state["cells"][yy*72+xx] == 0 for yy in range(y, y+9) for xx in range(x-2, x+3)):
                if all(abs(x-px) > 5 or abs(y-py) > 10 for px, py in locations):
                    locations.append((x, y))
            if len(locations) == 2:
                break
        if len(locations) == 2:
            break
    if not locations:
        s.unavailable("elements_additive_paint", "Preserved world has no empty falling corridor; no existing cells overwritten")
    else:
        materials = [1, 2] if state["scene"] in (0, 1) and len(locations) > 1 else [2]
        previous = state
        for (x, y), material in zip(locations, materials):
            s.resume()
            s.d.tap(94 if material == 1 else 137, 199, settle=.05)
            s.d.pointer("DOWN", 31+3*x, 41+3*y)
            s.d.pump(.15)
            s.d.request("INPUT CANCEL")
            painted = s.snapshot("elements-painted-" + str(material))
            expected = previous["cells"].copy()
            expected[y*72+x] = material
            s.check(painted["cells"] == expected and painted["steps"] == previous["steps"] and
                    painted["rng"] == previous["rng"], "elements_real_additive_paint_" + str(material), cell=[x, y])
            s.d.pump(.3)
            stable = s.read("elements-cancel-stable")[0]
            s.same(painted, stable, ("cells", "rng", "steps"), "elements_cancel_stops_paused_brush_" + str(material))
            previous = painted
        s.resume()
        s.d.tap(267, 51, settle=.05)
        s.d.pump(1)
        s.d.tap(267, 51, settle=.05)
        evolved = s.snapshot("elements-evolved")
        s.check(evolved["steps"] > previous["steps"] and evolved["cells"] != previous["cells"] and
                evolved["rng"] != previous["rng"], "elements_real_world_evolves_after_unpause")
        s.check(all(evolved["cells"][y*72+x] != m for (x, y), m in zip(locations, materials)),
                "elements_cancel_does_not_repaint_after_ticks")
    # Actual presentation intervals require a live world, even after a paused save.
    current = s.snapshot("elements-before-perf")
    if current["paused"]:
        s.resume()
        s.d.tap(267, 51, settle=.05)
    s.foreground_perf()


def menu_behavior(s):
    before = s.snapshot("menu-before")
    s.d.shot("menu-open")
    s.d.pump(2)
    during = s.read("menu-held")[0]
    # Cafe/Tiny can compare the committed payload while held. Grove synchronizes
    # production on serialization, so its next fresh save is the actual proof.
    if s.game != "lantern-grove":
        s.same(before, during, tuple(before), "menu_committed_world_stable")
    s.resume()
    s.d.pump(.15)
    after = s.snapshot("menu-resumed")
    if s.game == "lantern-grove":
        s.same(before, after, GROVE_STABLE, "grove_menu_keeps_growth")
        s.check(after["produced_ms"] >= min(315360000000, before["produced_ms"]+1500), "grove_monotonic_income_includes_menu_time")
        s.report["rtc"] = {"valid_stamp": bool(after["last_wall"]), "last_wall": after["last_wall"]}
    elif s.game == "harbor-cafe":
        elapsed = after["day_ms"]-before["day_ms"]
        s.same(before, after, CAFE_GROWTH, "cafe_menu_keeps_inventory_and_growth")
        s.check(0 <= elapsed < 1500, "cafe_menu_excludes_paused_elapsed", active_day_ms=elapsed, held_seconds=2)
    else:
        s.check(0 <= after["steps"]-before["steps"] < 30, "elements_menu_excludes_paused_steps",
                active_steps=after["steps"]-before["steps"], held_seconds=2)
    s.d.shot("menu-continued")


def idle_lock(s):
    s.resume()
    before = s.snapshot("before-idle-lock")
    s.resume()
    at = len(s.d.logs)
    def transition(value):
        return any("UI lifecycle:" in line and f"foreground={value}" in line and
                   f"display_interactive={value}" in line for line in s.d.logs[at:])
    deadline = time.monotonic()+s.d.args.lock_timeout
    keepalive = time.monotonic()+10
    while not transition(0) and time.monotonic() < deadline:
        s.d.pump(.1)
        if time.monotonic() >= keepalive:
            s.d.request("INFO")
            keepalive += 10
    s.check(transition(0), "idle_lock_enters_noninteractive_background")
    s.d.pump(1)
    s.d.request("PERF CLEAR")
    s.d.request("PERF START")
    s.d.pump(3)
    props = s.module.pxadb.info_properties(s.d.request("PERF STOP")[-1].payload)
    s.report.setdefault("perf", {})["locked"] = props
    s.check(int(props["raster"]) == 0 and int(props["present"]) == 0, "locked_zero_raster_and_present")
    s.d.shot("idle-locked")
    locked, raw1 = s.read("locked-first")
    kv1 = s.last_kv.copy()
    s.d.pump(2)
    _, raw2 = s.read("locked-second")
    s.check(raw1 == raw2, "locked_durable_save_stable")
    s.check(bool(kv1) and kv1 == s.last_kv, "locked_host_kv_files_byte_identical", first=kv1, second=s.last_kv)
    s.d.request("INPUT TAP 148 120")
    s.d.request("INPUT SWIPE 148 192 148 48 600 8")
    s.d.until(lambda: transition(1), "wake foreground lifecycle", seconds=15)
    s.check(True, "wake_returns_interactive_foreground")
    s.foreground_perf("wake")
    after = s.snapshot("after-wake")
    if s.game == "lantern-grove":
        s.same(before, after, GROVE_STABLE, "grove_wake_keeps_collected_growth")
        s.check(after["produced_ms"] >= locked["produced_ms"], "grove_wake_applies_monotonic_gap")
    elif s.game == "harbor-cafe":
        s.same(locked, after, ("stock", "level", "total_served", "debt"), "cafe_wake_preserves_business")
        if locked["summary"] == after["summary"]:
            s.same(locked, after, ("coins", "stars"), "cafe_wake_keeps_cash_and_stars")
        else:
            # A day can legitimately finish during the short wake PERF window.
            # Do not mistake that ordinary settlement for loss across lock.
            bonus = after["bonus"]
            s.check(after["summary"] == 1 and after["coins"] == min(999999, max(0, locked["coins"]-10)+bonus),
                    "cafe_wake_day_settlement", bonus=bonus)
        s.check(0 <= after["day_ms"]-locked["day_ms"] < 10000, "cafe_wake_has_no_locked_time_catchup")
    else:
        s.check(after["scene"] == locked["scene"] and (after["badges"] & locked["badges"]) == locked["badges"] and
                after["steps"] >= locked["steps"], "elements_wake_recovers_existing_world")


def persistence(s):
    if s.game == "harbor-cafe":
        # Run this timing experiment during an open day with at least 15 seconds
        # remaining, so the day cap cannot hide erroneous offline catch-up.
        state = s.snapshot("cafe-persistence-window")
        if not state["summary"] and state["day_ms"] >= 135000:
            s.resume()
            s.d.pump((150000-state["day_ms"])/1000 + .2)
            state = s.snapshot("cafe-day-closed-before-persistence")
        state = s.cafe_home(state)
        require(not state["summary"] and state["day_ms"] < 135000, "Cafe restart requires an uncapped active day")
    if s.game == "tiny-elements":
        state = s.snapshot("elements-before-persistence")
        if not state["paused"]:
            s.resume()
            s.d.tap(267, 51, settle=.1)
    s.snapshot("before-exit")
    s.exit()
    final, final_raw = s.read("after-exit")
    final_commit = s.report["storage"][s.report["snapshots"][-1]["label"]]
    s.d.pump(3)
    at = len(s.d.logs)
    s.start()
    load_observed = any("Pocket: load completed" in line for line in s.d.logs[at:])
    s.check(load_observed or any(re.fullmatch(r"dropped=[1-9][0-9]*", line) for line in s.d.logs[at:]),
            "restart_load_diagnostic_or_explicit_drop", load_log_observed=load_observed)
    # The comparisons below independently prove restoration from durable bytes.
    restored = s.snapshot("restarted")
    restored_commit = s.report["storage"][s.report["snapshots"][-1]["label"]]
    s.check(restored_commit["generation"] > final_commit["generation"] and
            (s.game != "tiny-elements" or
             restored_commit["envelope"]["generation"] > final_commit["envelope"]["generation"]),
            "restart_commits_new_generation_from_restored_state", before=final_commit, after=restored_commit)
    if s.game == "lantern-grove":
        s.same(final, restored, GROVE_STABLE, "grove_exit_restart_preserves_growth")
        s.check(restored["produced_ms"] >= final["produced_ms"] and
                all(b >= a for a, b in zip(final["pending"], restored["pending"])), "grove_restart_pending_production_nondecreasing")
        s.report["offline_income"] = {"outside_process_seconds": 3, "valid_utc": bool(final["last_wall"]),
                                    "produced_ms_delta": restored["produced_ms"]-final["produced_ms"]}
        if final["last_wall"] and restored["last_wall"] >= final["last_wall"]+3:
            s.check(restored["produced_ms"] >= min(315360000000, final["produced_ms"]+3000), "grove_valid_utc_reopen_income")
    elif s.game == "harbor-cafe":
        # Diagnostics can take eight foreground seconds after a nonzero DROP.
        # Device time independently measures the active interval, excluding the
        # preceding three seconds with no app process.
        launch_logs = s.d.logs[at:]
        timing_markers = ("Pocket: load completed", "Pocket: ready app=", "Heap[launch-ready]")
        timing_basis = next(marker for marker in timing_markers if any(marker in line for line in launch_logs))
        loaded = next(int(line.split("\t", 1)[0]) for line in launch_logs if timing_basis in line)
        paused = next(int(line.split("\t", 1)[0]) for line in launch_logs if "Pocket: menu opened" in line)
        foreground_ms = paused-loaded
        delta = restored["day_ms"]-final["day_ms"]
        s.check(not final["summary"] and not restored["summary"] and 0 <= foreground_ms < 12000 and
                0 <= delta < 12000 and final["day_ms"]+foreground_ms+3000 < 150000 and
                abs(delta-foreground_ms) < 1000, "cafe_restart_excludes_offline_time", active_day_ms=delta,
                device_foreground_ms=foreground_ms, timing_basis=timing_basis, offline_seconds=3)
        s.same(final, restored, tuple(field for field in CAFE_GROWTH if field != "total_failed"),
               "cafe_exit_restart_keeps_inventory_and_growth")
        # Existing customers can run out of patience during that real foreground
        # interval. New arrivals have >=35 seconds and cannot fail within 12.
        expired = sum(c["stage"] != 0 and c["patience"] <= delta for c in final["customers"])
        s.check(restored["total_failed"] == min(60000, final["total_failed"]+expired) and
                restored["failed"] == min(60000, final["failed"]+expired),
                "cafe_restart_retains_failures_and_accounts_for_patience", expired_customers=expired,
                total_failed_before=final["total_failed"], total_failed_after=restored["total_failed"])
    else:
        _, restored_raw = s.read("elements-exact-restored")
        s.check(final_raw == restored_raw, "elements_exit_restart_exact_world_rng_steps_and_ui")
    s.exit()


def run_device(args, report):
    module = transport()
    with (args.output / "serial.jsonl").open("w", buffering=1) as stream:
        with module.pxadb.open_client(args.port, 30) as client:
            info = module.pxadb.info_properties(client.device_info)
            report["hello"], report["device"] = client.device_info, info
            require(info.get("target") == "esp32s3" and info.get("serial", "").lower() == SERIAL, "Unexpected physical board identity")
            d = module.Device(args, client, report, stream)
            selected_started = False
            try:
                require(not any(f.kind == "PKG" and "active=1" in f.payload for f in d.request("PACKAGES")), "Another package is active")
                caps_frames = d.request("INPUT CAPABILITIES")
                report["input_capabilities"] = [f.payload for f in caps_frames]
                caps = {}
                for frame in caps_frames:
                    caps.update(module.pxadb.info_properties(frame.payload))
                require("controller_mask" in caps and int(caps["controller_mask"], 0) == 0, "Expected touch-only controller_mask=0")
                report["controller_checks"] = "unavailable: physical controller_mask=0; all gameplay and menus use touch"
                client.subscribe_logs()
                d.request("INPUT KEY HOME")
                d.request("INPUT SWIPE 148 192 148 48 600 8")
                d.pump(1)
                d.logs.clear()  # Discard LOGSUB history before this selected launch.
                session = Session(d, module)
                selected_started = True
                session.start()
                {"lantern-grove": grove_play, "harbor-cafe": cafe_play, "tiny-elements": elements_play}[args.game](session)
                menu_behavior(session)
                if args.skip_idle_lock:
                    report["idle_lock"] = "skipped by --skip-idle-lock"
                else:
                    idle_lock(session)
                persistence(session)
                session.check(not any(FAULTS.search(line) for line in d.logs), "no_runtime_faults")
            finally:
                # Attempt both cleanup actions even if the connection or CANCEL
                # failed. Do not stop an app that was active before this run.
                try:
                    d.request("INPUT CANCEL")
                except Exception:
                    report["errors"].append({"phase": "cleanup_cancel", "traceback": traceback.format_exc()})
                if selected_started:
                    try:
                        active = any(f.kind == "PKG" and ":"+d.identity+"\t" in f.payload and "active=1" in f.payload
                                     for f in d.request("PACKAGES"))
                        if active:
                            d.request("PACKAGE stop " + d.identity)
                    except Exception:
                        report["errors"].append({"phase": "cleanup_selected_app", "traceback": traceback.format_exc()})


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game", choices=tuple(IDS), required=True)
    parser.add_argument("--port", default="/dev/ttyACM1")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--skip-idle-lock", action="store_true")
    parser.add_argument("--lock-timeout", type=float, default=180)
    args = parser.parse_args()
    args.output = args.output.expanduser().resolve()
    if not args.output.is_relative_to(ARTIFACTS.resolve()) or args.output == ARTIFACTS.resolve():
        parser.error("--output must be a subdirectory of workspace local/artifacts/")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "errors": [], "checks": [], "game": args.game, "port": args.port,
              "output": str(args.output), "skip_idle_lock": args.skip_idle_lock, "lock_timeout": args.lock_timeout,
              "progress_policy": "Preserve existing worlds; normal play adds persistent progress; no save injection or deletion"}
    try:
        require(math.isfinite(args.lock_timeout) and args.lock_timeout > 0, "--lock-timeout must be finite and positive")
        run_device(args, report)
        report["passed"] = not report["errors"]
    except Exception:
        report["errors"].append({"phase": "acceptance", "traceback": traceback.format_exc()})
    finally:
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({"passed": report["passed"], "checks": len(report["checks"]), "errors": report["errors"],
                          "results": str(args.output / "results.json")}, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
