#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Preserve and physically exercise one installed Pocket Play game on pai-touch.

    python3 local/pxa-apps/common/pocket-play/tests/play_device.py \
        --port /dev/ttyACM1 --game pet-garden \
        --output local/artifacts/pocket-play/20261024-120000/pet-garden

Run sequentially for pet-garden, crisp-arcade and marble-islands. Requires the
existing PXADB/Pillow dependencies and installed signed apps. No builds, installs,
reset, Restart/Retry, save injection or private-storage writes. Normal gameplay
and its checkpoints remain in the save. Both KV banks are backed up BEFORE launch.
An existing active package or output directory is refused. All evidence goes to
--output, which must be a fresh directory under workspace local/artifacts.

Bounded/unsafe saved states produce diagnostics and a nonzero result, never a
passing gameplay claim. JPEG images support manual review; gameplay assertions
use validated durable binary saves and fresh game logs. --help is offline.
"""

import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import sys
import time
import traceback

sys.dont_write_bytecode = True
WORKSPACE = Path(__file__).resolve().parents[5]
ARTIFACTS = WORKSPACE / "local/artifacts"
GAMES = ("pet-garden", "crisp-arcade", "marble-islands")


def require(value, message):
    if not value:
        raise AssertionError(message)


class BoundedState(RuntimeError):
    """A real state prevents this bounded exercise from establishing acceptance."""


def load_transport():
    # Proven file-relative transport import; no connection is opened on import.
    path = Path(__file__).with_name("display_device.py")
    spec = importlib.util.spec_from_file_location("pocket_play_display_transport", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def decode_pet(raw, decoder):
    decoder.checked_payload(raw, b"PGD1", 128)
    require(not any(raw[5:8]) and not any(raw[110:124]), "PGD1 reserved bytes")
    names = ("food", "clean", "joy", "energy", "snacks", "stage", "branch",
             "decor_owned", "decor_on", "selection", "room", "sleeping", "last_game", "mode")
    s = dict(zip(names, raw[8:22]))
    s.update(zip(("coins", "growth", "affinity0", "affinity1", "affinity2", "rounds",
                  "best_catch", "best_rhythm"), struct.unpack_from("<8H", raw, 22)))
    s.update(zip(("active_ms", "needs_ms", "harvest_ms"), struct.unpack_from("<3I", raw, 38)))
    s["pet_x"], s["basket_x"] = struct.unpack_from("<2h", raw, 50)
    s.update(zip(("game_ms", "spawn_ms", "seed"), struct.unpack_from("<3I", raw, 54)))
    s.update(zip(("score", "attempts", "reward", "beat_index", "beat_done"),
                 struct.unpack_from("<3H2B", raw, 66)))
    s["drops"] = [list(struct.unpack_from("<hhBB", raw, 74 + i*6)) for i in range(5)]
    s["cooldown"] = list(struct.unpack_from("<3H", raw, 104))
    require(all(s[k] <= 100 for k in ("food", "clean", "joy", "energy")) and
            s["snacks"] <= 20 and s["coins"] <= 9999 and s["growth"] <= 999, "PGD1 balances")
    require(s["stage"] == (2 if s["growth"] >= 120 else 1 if s["growth"] >= 45 else 0) and
            (s["branch"] == 0 if s["stage"] < 2 else 1 <= s["branch"] <= 3), "PGD1 evolution")
    require(s["decor_owned"] <= 15 and s["decor_on"] <= 15 and
            not s["decor_on"] & ~s["decor_owned"] and s["selection"] <= 5 and s["mode"] <= 6 and
            all(s[k] <= 1 for k in ("room", "sleeping", "last_game")), "PGD1 UI/decor")
    require(84 <= s["pet_x"] <= 204 and 20 <= s["basket_x"] <= 268 and s["seed"] and
            s["needs_ms"] < 12000 and s["harvest_ms"] < 30000 and s["game_ms"] <= 15000 and
            s["spawn_ms"] < 900 and s["score"] <= 40 and s["attempts"] <= 20 and
            s["reward"] <= 42 and s["beat_index"] <= 8 and s["beat_done"] <= 1, "PGD1 run bounds")
    require(s["best_catch"] <= 40 and s["best_rhythm"] <= 8 and
            all(s[f"affinity{i}"] <= s["growth"] and s["cooldown"][i] <= 5000 for i in range(3)),
            "PGD1 affinity/bests/cooldowns")
    require(all(0 <= x <= 288 and -160 <= y <= 800 and kind <= 1 and active <= 1
                for x, y, kind, active in s["drops"]), "PGD1 drops")
    require((s["mode"] != 1 or s["selection"] <= 1) and
            (s["mode"] != 4 or s["selection"] <= 3) and
            (s["mode"] != 2 or s["last_game"] == 0) and
            (s["mode"] != 3 or (s["last_game"] == 1 and s["game_ms"] <= 11200 and
                                 s["score"] <= 8 and s["attempts"] <= 8)), "PGD1 mode consistency")
    return s


MARBLE_ROWS = (
    ("       ", " ....G ", " .C... ", " ...C. ", " .C... ", " S.... ", "       "),
    ("       ", "   .CG ", "   .   ", " .C.   ", " .     ", " S     ", "       "),
    ("       ", " .C..G ", " . . . ", " ..C.. ", " . . . ", " S.C.. ", "       "),
    ("       ", " ...CG ", " .     ", " .T.C. ", "     . ", " SC... ", "       "),
    ("       ", " ...CG ", " .O.O. ", " ..C.. ", " .O.O. ", " SC... ", "       "),
    ("       ", " .C..G ", " . # . ", " CT.C. ", " . # . ", " S.C.. ", "       "),
    ("       ", "   .CG ", "   T   ", " .C.C. ", " T   T ", " S.C.. ", "       "),
    ("       ", " .C..G ", " T # T ", " ..OC. ", " . # . ", " SC.C. ", "       "),
)
PAR_MS = (26000, 18000, 29000, 30000, 26000, 34000, 35000, 37000)


def decode_marble(raw, decoder):
    decoder.checked_payload(raw, b"MISL", 88)
    s = dict(zip(("unlocked", "level", "phase"), raw[5:8]))
    s["medals"], s["best_ms"] = list(raw[8:16]), list(struct.unpack_from("<8I", raw, 16))
    s["elapsed_ms"], s["coins"], s["fall_ms"] = struct.unpack_from("<IHH", raw, 48)
    s.update(zip(("x", "y", "vx", "vy"), (n/65536 for n in struct.unpack_from("<4i", raw, 56))))
    require(1 <= s["unlocked"] <= 8 and s["level"] < s["unlocked"] and s["phase"] <= 2,
            "MISL unlocked level/phase")
    require(not any(raw[72:84]) and s["elapsed_ms"] <= 3600000 and s["fall_ms"] <= 1000,
            "MISL reserved/timers")
    for i, (medal, best) in enumerate(zip(s["medals"], s["best_ms"])):
        require(medal <= 3 and (medal == 0) == (best == 0) and best <= 3600000 and
                (not best or medal == (3 if best <= PAR_MS[i] else 2 if best <= PAR_MS[i]*3//2 else 1)) and
                (i < s["unlocked"] or not (medal or best)) and
                (i+1 >= s["unlocked"] or medal > 0), "MISL medal/best consistency")
    rows = MARBLE_ROWS[s["level"]]
    require(s["coins"] < 1 << sum(row.count("C") for row in rows) and
            -.1 <= s["x"] <= 7.1 and -.1 <= s["y"] <= 7.1 and
            abs(s["vx"]) <= 2.2 and abs(s["vy"]) <= 2.2, "MISL run bounds")
    if s["phase"] != 1:
        require(0 <= s["x"] < 7 and 0 <= s["y"] < 7 and
                rows[int(s["y"])][int(s["x"])] != " " and not s["fall_ms"], "MISL unsupported run")
        for y, row in enumerate(rows):
            for x, cell in enumerate(row):
                if cell == "#":
                    nx, ny = min(x+1, max(x, s["x"])), min(y+1, max(y, s["y"]))
                    require((s["x"]-nx)**2 + (s["y"]-ny)**2 >= (.18-.003)**2, "MISL wall overlap")
                if cell == "O":
                    require((s["x"]-x-.5)**2 + (s["y"]-y-.5)**2 >= (.18+.268)**2, "MISL bumper overlap")
    if s["phase"] == 2:
        require(s["coins"].bit_count() == sum(row.count("C") for row in rows) and
                s["medals"][s["level"]] and rows[int(s["y"])][int(s["x"])] == "G" and
                s["vx"] == s["vy"] == 0, "MISL win consistency")
    return s


def decode_crisp(raw, decoder):
    decoder.checked_payload(raw, b"CRSP", 48)
    require(raw[5] < 4 and not any(raw[6:8]) and not any(raw[40:44]), "CRSP header/reserved")
    s = {"selected": raw[5], "best": list(struct.unpack_from("<4I", raw, 8)),
         "plays": list(struct.unpack_from("<4I", raw, 24))}
    require(all(n <= 99999999 for n in s["best"] + s["plays"]), "CRSP score/play range")
    return s


DECODERS = {"pet-garden": decode_pet, "crisp-arcade": decode_crisp, "marble-islands": decode_marble}


def progress_kept(game, before, after):
    if game == "pet-garden":
        require(all(after[k] >= before[k] for k in
                    ("growth", "stage", "rounds", "best_catch", "best_rhythm", "affinity0", "affinity1", "affinity2")),
                "Pet progress regressed")
        require(after["decor_owned"] & before["decor_owned"] == before["decor_owned"] and
                (not before["branch"] or before["branch"] == after["branch"]), "Pet evolution/decor lost")
    elif game == "crisp-arcade":
        require(all(b >= a for field in ("best", "plays") for a, b in zip(before[field], after[field])),
                "Crisp high score or play count regressed")
    else:
        require(after["unlocked"] >= before["unlocked"] and
                all(b >= a for a, b in zip(before["medals"], after["medals"])) and
                all(not a or 0 < b <= a for a, b in zip(before["best_ms"], after["best_ms"])),
                "Marble unlock, medal or best time regressed")


def reload_semantics(game, before, after):
    progress_kept(game, before, after)
    if game == "crisp-arcade":
        require(before == after, "Crisp durable selection/highscores/plays changed on reload")
    elif game == "pet-garden":
        delta = after["active_ms"] - before["active_ms"]
        require(0 <= delta < 10000, "Pet unexpected reload foreground time")
        require(before["mode"] not in (2, 3) and not before["sleeping"],
                "Pet reload comparison requires settled awake care/result state")
        ticks = (before["needs_ms"] + delta)//12000
        harvest = (before["harvest_ms"] + delta)//30000
        excluded = {"active_ms", "needs_ms", "harvest_ms", "cooldown", "snacks", "food", "clean", "joy", "energy"}
        require(all(before[k] == after[k] for k in before if k not in excluded), "Pet settled reload state changed")
        require(all(after[k] == max(0, before[k]-ticks) for k in ("food", "clean", "joy", "energy")) and
                after["snacks"] == min(20, before["snacks"]+harvest) and
                after["needs_ms"] == (before["needs_ms"]+delta)%12000 and
                after["harvest_ms"] == (before["harvest_ms"]+delta)%30000 and
                after["cooldown"] == [max(0, n-delta) for n in before["cooldown"]],
                "Pet reload does not match foreground-only care timers/balances")
    else:
        require(all(before[k] == after[k] for k in ("unlocked", "medals", "best_ms", "level", "phase", "coins", "fall_ms")),
                "Marble reload lost run/progress")
        require(0 <= after["elapsed_ms"]-before["elapsed_ms"] < 10000 and
                math.hypot(after["x"]-before["x"], after["y"]-before["y"]) < .035,
                "Marble braked run did not restore position/timer")


def device_class(transport):
    class PlayDevice(transport.Device):
        def __init__(self, args, client, report, stream):
            super().__init__(args, client, report, stream)
            self.app, self.identity = args.game, "pxa-"+args.game
            self.case = report
            self.paused = False
            self.latest = None
            self.original_values = None
            (args.output / self.app).mkdir()

        def check(self, condition, label, **details):
            self.case["checks"].append({"check": label, "passed": bool(condition), **details})
            require(condition, label)

        def pointer(self, phase, x, y):
            px, py = self.point(x, y)
            self.case["touches"].append({"phase": phase, "logical": [x, y], "physical": [px, py]})
            self.request(f"INPUT POINTER {phase} {px} {py} 1")
            self.request("INPUT SYNC")

        def accepted_tap(self, x, y, settle=.15):
            # PXADB processed/SYNC acknowledgements do not prove Guest delivery.
            # Require each edge from this single gesture; never retry a care action.
            px, py = self.point(x, y)
            at = len(self.logs)
            accepted = f"PxaHost: pointer accepted id=1 phase={{}} x={px} y={py}"
            try:
                self.pointer("DOWN", x, y)
                self.until(lambda: self.has(accepted.format(0), at),
                           "Pet touch DOWN was not accepted by Host", seconds=2)
                self.pump(.08)
            finally:
                self.pointer("UP", x, y)
            self.until(lambda: self.has(accepted.format(2), at),
                       "Pet touch UP was not accepted by Host", seconds=2)
            self.case["diagnostics"].append({"pet_touch_host_accepted": True,
                                              "logical": [x, y], "physical": [px, py]})
            self.pump(settle)

        def hold(self, x, y, seconds, label=None):
            self.pointer("DOWN", x, y)
            try:
                self.pump(seconds)
                if label:
                    self.shot(label)
            finally:
                self.pointer("UP", x, y)

        def pause(self):
            if not self.paused:
                at = len(self.logs)
                self.tap(276, 18, settle=.1)
                self.until(lambda: self.has("Pocket: menu opened", at), "No fresh pause diagnostic", seconds=5)
                self.paused = True

        def resume(self):
            if self.paused:
                at = len(self.logs)
                self.tap(140, 85, settle=.1)  # Continue, never row 1 Restart.
                self.until(lambda: self.has("Pocket: menu closed", at), "No fresh Continue diagnostic", seconds=5)
                self.paused = False

        def start(self, label, require_load=False):
            require(not self.active(), "A package is already active")
            at = len(self.logs)
            self.owned = True
            self.request("PACKAGE run " + self.identity)
            allowed = False
            deadline = time.monotonic()+18
            while time.monotonic() < deadline:
                if not allowed and any("Runtime permission prompt queued:" in line and
                                       ":"+self.identity+" permission=" in line for line in self.logs[at:]):
                    self.shot(label+"-permission")
                    self.request("INPUT TAP 200 183")
                    self.request("INPUT SYNC")
                    allowed = True
                ready = self.has("Pocket: ready app="+self.identity, at)
                completed = any("Launch completion: " in line and ":"+self.identity+" " in line and
                                "status=0" in line for line in self.logs[at:])
                if (ready or completed) and self.selected_active():
                    if not self.geometry_if_present(at):
                        self.derive_geometry()  # Expected mapping; the fresh menu effect verifies touch.
                    self.shot(label+"-ready")  # Actual nonblank game capture, before the menu overlay.
                    self.paused = False
                    self.pause()  # Freeze before storage-transfer time can advance play.
                    rejected = ("save-rejected", "rejected corrupt", "rejected invalid")
                    require(not any(token in line for token in rejected for line in self.logs[at:]), "Game rejected its existing save")
                    require(self.selected_active(), "Selected package stopped during startup/menu verification")
                    self.case["launches"].append({"phase": label, "ready_log": ready,
                        "host_launch_completed": completed, "load_log": self.has("Pocket: load completed", at),
                        "require_load": require_load, "permission_allowed": allowed,
                        "selected_package_active": True, "nonblank_game_capture": True,
                        "menu_touch_verified": True, "save_comparison_verified": False})
                    self.case["viewports"][-1].update(touch_verified=True, touch_check="fresh_pause_menu_log")
                    self.shot(label+"-paused")
                    return
                self.pump(.05)
            raise BoundedState("Startup lacks selected active package plus fresh ready/Host completion; no automatic retry")

        def geometry_if_present(self, at):
            if self.viewport is not None:
                return True
            return self.geometry(at)

        def snapshot(self, label):
            fs = transport.pxadb.NormalFsClient(self.client)
            frames = self.request(f"PACKAGE paths {transport.PUBLISHER}:{self.identity}")
            roots = [f.payload.split("\t", 1)[1] for f in frames
                     if f.kind == "PATH" and f.payload.startswith("private\t")]
            require(len(roots) == 1, "No unique selected-package private path")
            root = roots[0]
            entries = fs.list(root)
            storage = any(kind == "D" and name == ".pxa-storage" for kind, _, name in entries)
            banks, candidates = [], []
            if storage:
                for kind, _, name in fs.list(root+"/.pxa-storage"):
                    if kind != "F" or name not in (".pxa-kv-a", ".pxa-kv-b"):
                        continue
                    path = self.args.output / self.app / (label+"-"+name.removeprefix(".pxa-")+".bin")
                    for attempt in range(3):
                        try:
                            fs.get(root+"/.pxa-storage/"+name, path)
                            break
                        except transport.pxadb.PxaDbError as error:
                            if "file transfer checksum mismatch" not in str(error) or attempt == 2:
                                raise
                            self.case["transport_retries"].append({"label": label, "attempt": attempt+1, "error": str(error)})
                            self.pump(.2)
                    raw = path.read_bytes()
                    row = {"bank": name, "path": str(path.relative_to(self.args.output)), **digest(raw)}
                    try:
                        generation, values = self.decoder.host_values(raw)
                        require(all(len(v) <= 960 for v in values.values()), "Host value exceeds storage limit")
                        row.update(valid=True, generation=generation)
                        candidates.append((generation, values))
                    except (AssertionError, UnicodeError, struct.error) as error:
                        row.update(valid=False, error=str(error))
                    banks.append(row)
            require(not banks or candidates, "No valid existing KV bank; refusing to launch over corrupt storage")
            generation, values = max(candidates, key=lambda item: item[0]) if candidates else (0, {})
            require(all(v == values for g, v in candidates if g == generation), "Conflicting equal-generation KV banks")
            evidence = {"label": label, "generation": generation, "banks": banks,
                        "values": {k: digest(v) for k, v in values.items()}}
            self.case["storage"].append(evidence)
            key = self.identity+".save.v1"
            payload, state, envelope = None, None, {"format": "absent"}
            if key in values:
                try:
                    payload, envelope = self.decoder.committed_world(values, key)
                    state = DECODERS[self.app](payload, self.decoder)
                except (AssertionError, UnicodeError, struct.error) as error:
                    evidence["decode_error"] = str(error)
                    raise
            if self.original_values is not None:
                require(all(values.get(k) == v for k, v in self.original_values.items()
                            if k != key and not k.startswith(key+".b")), "Unrelated KV/preference value changed")
            result = {"generation": generation, "values": values, "payload": payload, "state": state,
                      "bank_hashes": {row["bank"]: row["sha256"] for row in banks}}
            evidence.update(payload=digest(payload) if payload is not None else None, envelope=envelope, state=state)
            self.latest = result
            return result

        def checkpoint(self, label):
            previous_generation = self.latest["generation"] if self.latest else 0
            was_running = not self.paused
            self.pause()
            previous = None
            for attempt in range(6):
                result = self.snapshot(label+f"-{attempt+1}")
                if (result["state"] is not None and result["generation"] >= previous_generation+int(was_running) and
                        previous is not None and result["values"] == previous["values"] and
                        result["bank_hashes"] == previous["bank_hashes"]):
                    return result
                previous = result
                self.pump(.35)
            raise BoundedState("Paused durable banks failed to stabilize: "+label)

        def normal_exit(self, label):
            self.pause()
            self.exit(label)  # Proven touch row 2 Exit and selected-package inactive checks.
            self.paused = False

        def exercise_pet(self, before):
            s = before["state"]
            if s["mode"] == 6:
                self.resume(); self.accepted_tap(213, 185)
                before = self.checkpoint("pet-home"); s = before["state"]
            if s["mode"] not in (0, 1, 2, 3):
                raise BoundedState(f"Pet mode={s['mode']} has no safe touch path to care; save retained")
            if s["mode"] == 0 and not s["sleeping"] and s["snacks"] and s["food"] <= 85 and not s["cooldown"][0]:
                # Stay inside the Feed button, away from panel edge hit targets.
                self.resume(); self.accepted_tap(38, 185)
                self.shot("pet-fed-live")
                after = self.checkpoint("pet-fed"); a = after["state"]
                elapsed = a["active_ms"]-s["active_ms"]
                ticks, harvest = (s["needs_ms"]+elapsed)//12000, (s["harvest_ms"]+elapsed)//30000
                self.check(a["growth"] == min(999, s["growth"]+7) and
                           a["affinity0"] == min(999, s["affinity0"]+7) and
                           max(0, min(100, s["food"]+28)-ticks) <= a["food"] <= min(100, max(0, s["food"]-ticks)+28) and
                           min(20, s["snacks"]+harvest)-1 <= a["snacks"] <= min(20, s["snacks"]-1+harvest) and
                           a["coins"] == s["coins"],
                           "pet_normal_feed_advances_care_and_balances", elapsed_ms=elapsed)
                s = a
            else:
                self.case["diagnostics"].append({"pet_feed_omitted": "Existing mode/sleep/food/snacks/cooldown is not feedable"})
            self.resume()
            if s["mode"] == 0:
                self.accepted_tap(164, 185); self.accepted_tap(130, 80, settle=.1)
            elif s["mode"] == 1:
                self.accepted_tap(130, 80, settle=.1)
            playing = self.checkpoint("pet-playing")["state"]
            self.check(playing["mode"] in (2, 3), "pet_normal_minigame_entered")
            self.resume()
            if playing["mode"] == 2:
                self.pointer("DOWN", 144, 140); self.pointer("MOVE", 240, 140)
                self.pump(.15); self.pointer("UP", 240, 140)
                moved = self.checkpoint("pet-dragged")["state"]
                self.check(abs(moved["basket_x"]-240) <= 1, "pet_touch_drag_moves_basket")
                self.resume()
            else:
                self.accepted_tap(144, 110, settle=.1)
            self.shot("pet-minigame-live")
            self.pump(16)
            after = self.checkpoint("pet-result"); a = after["state"]
            reward = 2+a["score"]//2 if playing["mode"] == 2 else 2+a["score"]
            elapsed = a["active_ms"]-playing["active_ms"]
            harvest = (playing["harvest_ms"]+elapsed)//30000
            self.check(a["mode"] == 6 and a["rounds"] == min(65535, playing["rounds"]+1) and
                       a["growth"] == min(999, playing["growth"]+10+a["score"]) and
                       a["coins"] == min(9999, playing["coins"]+reward) and a["reward"] == reward and
                       a["snacks"] == min(20, playing["snacks"]+2+harvest),
                       "pet_normal_play_settles_round_growth_reward_and_balances", score=a["score"], reward=reward)
            progress_kept(self.app, s, after["state"])
            if not any(after["state"][k] > s[k] for k in
                       ("food", "growth", "affinity0", "affinity1", "rounds", "coins", "best_catch", "best_rhythm")):
                raise BoundedState("Pet reward/progress fields saturated; interaction alone cannot prove durable advancement")
            self.shot("pet-play-complete")
            return after

        def exercise_crisp(self, before):
            s = before["state"]
            self.resume()
            at = len(self.logs)
            self.tap(210, 143, settle=.1)  # Normal Color Roll launcher card, index 3.
            self.until(lambda: self.has("crisp launch game=", at), "Crisp card did not launch", seconds=5)
            self.shot("crisp-before-shot")
            self.tap(144, 125, settle=.1)  # Actual Color Roll shoot input, once only.
            deadline = time.monotonic()+4
            while time.monotonic() < deadline and not self.has("crisp score game=", at) and not self.has("crisp game-over", at):
                self.pump(.05)
            after = self.checkpoint("crisp-scored"); a = after["state"]
            import re
            scores = [int(m.group(1)) for line in self.logs[at:]
                      if (m := re.search(r"crisp score game=.* score=(\d+) best=", line))]
            if not any(n > 0 for n in scores):
                raise BoundedState("Single normal Color Roll shot produced no positive score diagnostic; no retry sent")
            self.check(a["selected"] == 3 and a["plays"][3] == min(99999999, s["plays"][3]+1) and
                       all(a["plays"][i] == s["plays"][i] for i in range(3)), "crisp_normal_launch_count", observed_scores=scores)
            progress_kept(self.app, s, a)
            self.check(a["best"][3] >= max(scores), "crisp_real_shot_score_saved_without_lowering_highscores")
            self.case["diagnostics"].append({"crisp_existing_highscore_exceeded": a["best"][3] > s["best"][3],
                                              "play_counter_saturated": s["plays"][3] == 99999999})
            self.shot("crisp-after-shot-paused")
            if a["plays"][3] == s["plays"][3] and a["best"][3] == s["best"][3]:
                raise BoundedState("Crisp play count saturated and existing highscore not exceeded; no durable advancement claim")
            return after

        def exercise_marble(self, before):
            s = before["state"]
            if s["phase"] != 0:
                raise BoundedState(f"Marble phase={s['phase']}; cannot establish drag/brake without replacing this run")
            self.check(s["level"] < s["unlocked"], "marble_selected_existing_unlocked_level", level=s["level"]+1,
                       selection_policy="Resume existing unlocked run; retain its gems/time")
            self.resume()
            if not self.case["prelaunch_had_save"]:
                self.tap(144, 180, settle=.1)  # Dismiss instructions; retain init's existing unlocked run.
            self.hold(253, 170, .45)
            stopped = self.checkpoint("marble-before-drag")["state"]
            if stopped["phase"] != 0:
                raise BoundedState("Existing Marble velocity reached a fall before braking; no Retry sent")
            # Steer a short distance toward this tile's center, keeping the saved run.
            dx, dy = math.floor(stopped["x"])+.5-stopped["x"], math.floor(stopped["y"])+.5-stopped["y"]
            if max(abs(dx), abs(dy)) < .05:
                dx, dy = .1, 0
            mx, my = ((12 if dx >= 0 else -12), 0) if abs(dx) >= abs(dy) else (0, (12 if dy >= 0 else -12))
            self.resume()
            self.pointer("DOWN", 110, 130); self.pointer("MOVE", 110+mx, 130+my)
            self.pump(.3); self.pointer("UP", 110+mx, 130+my)
            moved = self.checkpoint("marble-dragged")["state"]
            distance = math.hypot(moved["x"]-stopped["x"], moved["y"]-stopped["y"])
            self.check(moved["phase"] == 0 and moved["level"] == s["level"] and distance > .005 and
                       moved["elapsed_ms"] > stopped["elapsed_ms"], "marble_drag_advances_real_run", distance=distance)
            self.resume(); self.hold(253, 170, .55, "marble-brake-held")
            after = self.checkpoint("marble-braked"); a = after["state"]
            v0, v1 = math.hypot(moved["vx"], moved["vy"]), math.hypot(a["vx"], a["vy"])
            self.check(a["phase"] == 0 and a["level"] == s["level"] and v0 > .005 and v1 < v0*.5 and v1 < .04,
                       "marble_touch_brake_reduces_velocity", before_speed=v0, after_speed=v1)
            self.check(s["elapsed_ms"] <= stopped["elapsed_ms"] < moved["elapsed_ms"] <= a["elapsed_ms"] and
                       all(new["coins"] & old["coins"] == old["coins"] for old, new in
                           ((s, stopped), (stopped, moved), (moved, a))),
                       "marble_movement_and_brake_retain_run_time_and_gems")
            progress_kept(self.app, s, a)
            return after

        def exercise(self):
            backup = self.snapshot("prelaunch-backup")
            self.original_values = backup["values"].copy()
            self.case["prelaunch_had_save"] = backup["state"] is not None
            # A falling save auto-retries in the app after 650 ms. Preserve it offline.
            if self.app == "marble-islands" and backup["state"]:
                saved = backup["state"]
                if saved["phase"] == 1:
                    raise BoundedState("Prelaunch Marble save is FALLING; launching would auto-retry and replace it")
                if saved["phase"] == 0:
                    edge = min(saved["x"] % 1, 1-saved["x"] % 1, saved["y"] % 1, 1-saved["y"] % 1)
                    tile = MARBLE_ROWS[saved["level"]][int(saved["y"])][int(saved["x"])]
                    if math.hypot(saved["vx"], saved["vy"]) > .04 or edge < .08 or tile in "T#O":
                        raise BoundedState("Prelaunch Marble run is moving/near an edge or obstacle; refusing possible automatic retry before pause")
            if self.app == "pet-garden" and backup["state"] and backup["state"]["active_ms"] > 0xffffffff-60000:
                raise BoundedState("Pet active_ms is saturated/near saturation; cannot prove foreground timer preservation")
            self.viewport = None
            self.start("initial", require_load=self.case["prelaunch_had_save"])
            before = self.checkpoint("initial-state")
            if backup["state"] is not None:
                old, new = backup["state"], before["state"]
                if self.app != "pet-garden" or (old["mode"] not in (2, 3) and not old["sleeping"]):
                    reload_semantics(self.app, old, new)
                else:
                    progress_kept(self.app, old, new)
                    require(0 <= new["active_ms"]-old["active_ms"] < 10000, "Pet initial foreground time changed")
                    require(new["mode"] == old["mode"] and new["game_ms"] >= old["game_ms"],
                            "Pet transient startup changed existing mode/run; cannot prove preservation")
                self.check(True, "initial_launch_preserves_original_save_semantics",
                           before=digest(backup["payload"]), after=digest(before["payload"]))
            self.case["launches"][-1]["save_comparison_verified"] = True
            self.check(True, "prelaunch_banks_backed_up_before_package_run", had_save=self.case["prelaunch_had_save"])
            gameplay_error = None
            try:
                after = {"pet-garden": self.exercise_pet, "crisp-arcade": self.exercise_crisp,
                         "marble-islands": self.exercise_marble}[self.app](before)
                self.case["gameplay_passed"] = True
            except BoundedState as error:
                gameplay_error = str(error)
                self.case["diagnostics"].append({"bounded_gameplay": gameplay_error})
                after = self.checkpoint("bounded-gameplay")
            self.pump(1)
            held = self.checkpoint("paused-held")
            self.check(after["payload"] == held["payload"] and after["values"] == held["values"] and
                       after["bank_hashes"] == held["bank_hashes"], "pause_holds_payload_values_and_kv_bank_bytes")
            self.normal_exit("normal-exit")
            final = self.snapshot("after-normal-exit")
            self.check(final["payload"] == held["payload"] and final["values"] == held["values"],
                       "normal_exit_preserves_paused_snapshot_bytes")
            # Only settled states have a meaningful bounded reload comparison.
            state = final["state"]
            if (self.app == "pet-garden" and (state["mode"] in (2, 3) or state["sleeping"])) or (
                    self.app == "marble-islands" and state["phase"] == 1):
                raise BoundedState(gameplay_error or "Saved transient state cannot be reloaded without advancing/replacing it")
            self.viewport = None
            self.start("reload", require_load=True)
            restored = self.checkpoint("reloaded")
            reload_semantics(self.app, final["state"], restored["state"])
            self.case["launches"][-1]["save_comparison_verified"] = True
            self.check(True, "normal_reload_preserves_semantics", payload_byte_identical=final["payload"] == restored["payload"],
                       before=digest(final["payload"]), after=digest(restored["payload"]))
            self.normal_exit("final-normal-exit")
            ended = self.snapshot("final-stopped")
            self.check(ended["payload"] == restored["payload"] and ended["values"] == restored["values"],
                       "final_normal_exit_keeps_reloaded_bytes")
            if backup["state"] is not None:
                progress_kept(self.app, backup["state"], ended["state"])
            self.owned = False
            if gameplay_error:
                raise BoundedState(gameplay_error)
    return PlayDevice


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", default="/dev/ttyACM1", help="explicit PXADB port (default: /dev/ttyACM1)")
    parser.add_argument("--game", choices=GAMES, required=True)
    parser.add_argument("--output", required=True, type=Path, help="fresh directory below local/artifacts")
    args = parser.parse_args(argv)
    args.output = args.output.expanduser().resolve()
    if ARTIFACTS.resolve() not in args.output.parents:
        parser.error("--output must be below workspace local/artifacts")
    if args.output.exists():
        parser.error("--output already exists; choose a fresh directory")
    args.output.mkdir(parents=True, exist_ok=False)
    report = {"schema": "pocket-play-device-v1", "game": args.game, "port": args.port,
              "passed": False, "gameplay_passed": False, "errors": [], "diagnostics": [], "checks": [],
              "launches": [], "viewports": [], "screenshots": [], "touches": [], "storage": [],
              "transport_retries": [], "ended_without_active_app": False,
              "policy": "Back up before launch; preserve existing saves; normal touch only; no Restart/Retry/reset/save writes",
              "screenshot_review": "JPEG evidence retained for human inspection"}
    device = None
    try:
        transport = load_transport()
        with (args.output / "serial.jsonl").open("w", buffering=1) as stream:
            with transport.pxadb.open_client(args.port, 30) as client:
                info = transport.pxadb.info_properties(client.device_info)
                report["device"] = info
                require(info.get("target") == "esp32s3" and info.get("serial", "").lower() == transport.SERIAL,
                        "Unexpected physical board identity")
                device = device_class(transport)(args, client, report, stream)
                try:
                    require(not device.active(), "Another package is active; refused to touch or stop it")
                    client.subscribe_logs()
                    device.logs.clear(); device.drops.clear(); device.faults.clear()
                    device.request("INPUT KEY HOME")
                    device.request("INPUT SWIPE 148 192 148 48 600 8")
                    device.pump(.8)
                    report["input_capabilities"] = [f.payload for f in device.request("INPUT CAPABILITIES")]
                    device.exercise()
                    device.healthy()
                    report["passed"] = report["gameplay_passed"] = True
                finally:
                    # On healthy failures prefer the same normal saving Exit. Stop
                    # only this runner's selected package if the normal path fails.
                    if device.owned:
                        try:
                            if device.selected_active():
                                device.request("INPUT CANCEL")
                                device.normal_exit("cleanup-normal-exit")
                        except Exception:
                            report["errors"].append({"phase": "cleanup_normal_exit", "error": traceback.format_exc()})
                            device.stop()
                    frames = client.request("PACKAGES", timeout=30)
                    report["ended_without_active_app"] = not any(f.kind == "PKG" and "active=1" in f.payload for f in frames)
                    report["log_drops"], report["faults"] = device.drops, device.faults
    except BaseException as error:
        report["errors"].append({"phase": "acceptance", "type": type(error).__name__, "error": traceback.format_exc()})
        if isinstance(error, BoundedState):
            report["diagnostics"].append({"inconclusive": str(error)})
        report["passed"] = False
    finally:
        report["passed"] = bool(report["passed"] and not report["errors"] and report["ended_without_active_app"])
        (args.output / "results.json").write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n")
    print(json.dumps({"passed": report["passed"], "gameplay_passed": report["gameplay_passed"],
                      "report": str(args.output / "results.json"), "errors": report["errors"]}, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
