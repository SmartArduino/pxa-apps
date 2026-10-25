#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the signed Pet Garden package in the real simulator or on PXA S3.

Use --simulator EXE --package EXPANDED_DIR --publisher-key DER --output DIR,
or --port SERIAL --output DIR [--skip-idle-lock]. Output must be under the
workspace's ignored local/artifacts/. Device runs require exclusive ownership
of the serial port and no other running package. Never installs, flashes,
clears private storage, or removes any package. Existing pet progress survives.

Uses frozen pocket-game transport helpers, but all assertions concern actual
Pet Garden PGD1 save fields, action logs and rendered minigame objects. An
animated room is deliberately not compared as one static screenshot.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import time
import traceback
import zlib

sys.dont_write_bytecode = True
WORKSPACE = Path(__file__).resolve().parents[4]
HELPERS = Path(__file__).resolve().parents[2] / "common/pocket-game/tests"
sys.path.insert(0, str(HELPERS))
sys.path.insert(0, str(WORKSPACE / "tools/pxadb"))
import simulator as old_sim

IDENTITY = "pxa-pet-garden"
require = old_sim.require


def payload(raw):
    """Decode the on-disk game format independently, including its checksum."""
    require(len(raw) == 128 and raw[:8] == b"PGD1\x01\0\0\0", "Invalid Pet Garden header/size")
    checksum = 2166136261
    for value in raw[:124]:
        checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    require(checksum == struct.unpack_from("<I", raw, 124)[0], "Bad PGD1 checksum")
    require(raw[110:124] == bytes(14), "Nonzero reserved PGD1 bytes")
    names = ("food", "clean", "joy", "energy", "snacks", "stage", "branch", "decor_owned", "decor_on", "selection", "room", "sleeping", "last_game", "mode")
    state = dict(zip(names, raw[8:22]))
    for name, at in (("coins", 22), ("growth", 24), ("rounds", 32), ("best_catch", 34), ("best_rhythm", 36), ("score", 66), ("attempts", 68), ("reward", 70)):
        state[name] = struct.unpack_from("<H", raw, at)[0]
    for name, at in (("active_ms", 38), ("needs_ms", 42), ("harvest_ms", 46), ("game_ms", 54), ("spawn_ms", 58), ("seed", 62)):
        state[name] = struct.unpack_from("<I", raw, at)[0]
    state["pet_x"], state["basket_x"] = struct.unpack_from("<hh", raw, 50)
    state["affinity"] = list(struct.unpack_from("<3H", raw, 26))
    state["beat_index"], state["beat_done"] = raw[72:74]
    state["drops"] = [dict(zip(("x", "y", "kind", "active"), struct.unpack_from("<hhBB", raw, at))) for at in range(74, 104, 6)]
    state["cooldowns"] = list(struct.unpack_from("<3H", raw, 104))
    require(all(0 <= state[k] <= 100 for k in ("food", "clean", "joy", "energy")), "Invalid saved needs")
    require(0 <= state["mode"] <= 6 and state["growth"] <= 999, "Invalid mode or growth")
    return state


def host_kv(raw):
    if len(raw) < 26:
        return None
    magic, version, _, generation, size, crc = struct.unpack_from("<4sHHQII", raw)
    body = raw[24:]
    if magic != b"PXKV" or version != 1 or len(body) != size or zlib.crc32(body) != crc:
        return None
    values, at = {}, 2
    for _ in range(struct.unpack_from("<H", body)[0]):
        require(at < len(body), "Short KV key")
        size = body[at]; at += 1
        key = body[at:at+size].decode(); at += size
        require(at + 2 <= len(body), "Short KV length")
        size = struct.unpack_from("<H", body, at)[0]; at += 2
        require(at + size <= len(body), "Short KV value")
        values[key] = body[at:at+size]; at += size
    require(at == len(body), "Trailing KV data")
    return generation, values


class Session:
    """Pet assertions shared between the real simulator and real serial backend."""
    def __init__(self, backend, report, device=False):
        self.b, self.report, self.device = backend, report, device
        self.paused = False
        self.snapshot_index = 0

    def pump(self, seconds):
        if self.device:
            self.b.pump(max(0, seconds))
        else:
            time.sleep(max(0, seconds))
            require(self.b.proc.poll() is None, "Simulator exited unexpectedly")
            require(not old_sim.FAULTS.search(self.b.logs()), "Simulator guest fault")

    def logs(self):
        return "\n".join(self.b.logs) if self.device else self.b.logs()

    def check(self, condition, label, **evidence):
        require(condition, label)
        self.report["checks"].append({"check": label, **evidence})

    def tap(self, x, y, settle=.35):
        self.b.tap(x, y, settle=settle)

    def pointer(self, phase, x, y):
        self.b.pointer(phase, x, y)

    def key(self, key, settle=.25):
        require(not self.device, "Controller unavailable on this device")
        self.b.key(key, settle=settle)

    def hold(self, key, down):
        require(not self.device, "Controller unavailable on this device")
        old_sim.command(self.b.sock, ("KEY_DOWN " if down else "KEY_UP ") + key)
        if down:
            self.b.keys.add(key)
        else:
            self.b.keys.discard(key)

    def shot(self, label):
        if not self.device:
            return self.b.shot(label)
        from pxadb import PxaDbRejectedError
        for attempt in range(3):
            try:
                return self.b.shot(label)
            except PxaDbRejectedError as error:
                if str(error) != "screenshot_failed" or attempt == 2:
                    raise
                self.report.setdefault("screenshot_retries", []).append({"label": label, "attempt": attempt+1})
                self.pump(.4)

    def pause(self):
        if not self.paused:
            self.tap(276, 10, settle=.25)
            self.paused = True

    def resume(self):
        if self.paused:
            self.tap(140, 85, settle=.25)
            self.paused = False

    def reset(self):
        self.pause()
        self.tap(140, 115, settle=.35)
        self.paused = False

    def saved(self, label="poll"):
        self.snapshot_index += 1
        tag = f"{getattr(self.b, 'label', 'device')}-{self.snapshot_index:03d}-{label}"
        if self.device:
            from pxadb import PxaDbError
            # Host save generations can rotate between FSSHA256 and FSGET.
            # Restart the entire validated read to obtain a fresh digest.
            for attempt in range(5):
                try:
                    raw = self.b.saved(tag)
                    break
                except PxaDbError as error:
                    if "file transfer checksum mismatch" not in str(error) or attempt == 4:
                        raise
                    self.report.setdefault("storage_read_retries", []).append({"tag": tag, "attempt": attempt+1, "error": str(error)})
                    self.pump(.2)
        else:
            candidates = []
            for path in self.b.state.glob("**/.pxa-kv-*"):
                try:
                    parsed = host_kv(path.read_bytes())
                    if parsed and IDENTITY + ".save.v1" in parsed[1]:
                        candidates.append(parsed)
                except FileNotFoundError:
                    continue  # An asynchronous writer may replace a generation.
            require(candidates, "No durable Pet Garden KV generation yet")
            generation, values = max(candidates, key=lambda row: row[0])
            raw = values[IDENTITY + ".save.v1"]
            self.report.setdefault("storage", {})[tag] = {"generation": generation, "size": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
        state = payload(raw)
        (self.b.args.output / (tag + ".pgd")).write_bytes(raw)
        (self.b.args.output / (tag + ".json")).write_text(json.dumps(state, indent=2) + "\n")
        return state

    def wait_state(self, predicate, label, timeout=12):
        end, latest = time.monotonic() + timeout, None
        while time.monotonic() < end:
            try:
                latest = self.saved(label)
                if predicate(latest):
                    return latest
            except (AssertionError, KeyError):
                # A fresh package has no payload until its first asynchronous save.
                if "Pocket: saved" in self.logs():
                    raise
            self.pump(.15 if not self.device else .3)
        raise TimeoutError(f"{label}; last state={latest}")

    def flush(self, label):
        # Toggling the menu forces a real asynchronous storage request, without
        # inventing a debug transport or changing the game payload.
        self.pause()
        self.pump(.6)
        return self.wait_state(lambda s: True, label)

    def home(self):
        self.reset()
        return self.wait_state(lambda s: s["mode"] == 0, "home")

    def exit(self):
        self.resume()
        if self.device:
            self.b.menu_exit()
        else:
            self.pause()
            self.pointer("DOWN", 140, 145)
            self.b.wait(lambda: self.b.proc.poll() is not None, "menu EXIT")
            require(self.b.proc.returncode == 0, "Menu EXIT failed")
        self.paused = False


def stable_equal(a, b, keys):
    return all(a[k] == b[k] for k in keys)


def care_and_shop(s):
    # Reset is intentionally the app's safe return-home operation, never storage
    # deletion. A device may already have a pet; compare against that baseline.
    baseline = s.home()
    if max(baseline["cooldowns"]):
        s.resume(); s.pump(max(baseline["cooldowns"])/1000+.1)
        baseline = s.flush("existing-cooldowns-finished")
    s.shot("initial-home")
    s.resume()
    s.tap(24, 191)
    fed = s.flush("fed")
    if baseline["food"] <= 85 and baseline["snacks"] > 0:
        s.check(fed["food"] > baseline["food"] and fed["growth"] == min(999, baseline["growth"] + 7), "real_feed_consumes_snack_and_grows", before=baseline["growth"], after=fed["growth"])
    else:
        s.check(fed["growth"] == baseline["growth"], "full_or_empty_feed_safely_refused")
    s.resume()
    s.tap(71, 191)
    washed = s.flush("washed")
    s.check(washed["growth"] == min(999, fed["growth"] + 7) and washed["clean"] >= fed["clean"], "real_wash_gives_growth")
    s.resume()
    s.tap(119, 191)
    petted = s.flush("petted")
    s.check(petted["growth"] == min(999, washed["growth"] + 3), "real_pet_gives_growth")
    s.resume()
    s.tap(212, 191)
    resting = s.flush("resting")
    if resting["energy"] < 100:
        s.check(resting["sleeping"] == 1, "rest_enters_sleep")
        s.resume(); s.pump(1.2)
        rested = s.flush("rested")
        s.check(rested["energy"] > resting["energy"], "foreground_rest_recovers_energy")
    s.resume()
    s.tap(212, 191)  # Wake; reset below also normalizes full-energy nap completion.
    s.home(); s.resume()
    # The host HOME overlay occupies physical y>=220; use the visible HELP
    # control near the growth/needs row, safely above that navigation layer.
    s.tap(269, 32)
    s.wait_state(lambda v: v["mode"] == 5, "visible-touch-help")
    first = s.shot("guide-page-one")
    s.tap(140, 130)
    second = s.shot("guide-page-two")
    # The guide is static, so a cropped comparison is meaningful here.
    ox = 4 if s.device else 16
    s.check(old_sim.difference(first.crop((ox, 30, ox+288, 210)), second.crop((ox, 30, ox+288, 210))) > .02, "guide_has_two_readable_pages")
    s.tap(140, 130)
    s.wait_state(lambda v: v["mode"] == 0, "guide-home")
    # Keep clear of the device host's left-edge navigation capture area.
    s.tap(60, 110)
    garden = s.flush("garden")
    s.check(garden["room"] != baseline["room"], "touch_toggles_room_and_garden")
    s.resume()
    # Rightmost empty scene space moves the pet instead of petting its center.
    s.tap(275, 105)
    moved = s.flush("moved-pet")
    s.check(moved["pet_x"] == 204, "touch_moves_and_persists_pet_position")
    s.resume()
    s.tap(261, 191)
    shop = s.wait_state(lambda v: v["mode"] == 4, "shop")
    available = [i for i, cost in enumerate((6, 10, 14, 18)) if not (shop["decor_owned"] & (1 << i)) and shop["coins"] >= cost]
    item = available[0] if available else 0
    s.tap(130, 60 + item*35)
    bought = s.flush("decoration")
    bit = 1 << item
    if available:
        s.check(bought["decor_owned"] & bit and bought["decor_on"] & bit and bought["coins"] == shop["coins"] - (6, 10, 14, 18)[item], "real_decoration_purchase_debits_coins", item=item)
    else:
        s.check(shop["decor_owned"] & bit and bought["decor_on"] == (shop["decor_on"] ^ bit) and bought["coins"] == shop["coins"], "owned_decoration_toggles_without_charge")
    s.resume()
    s.tap(130, 60 + item*35)
    toggled = s.flush("decoration-toggle")
    s.check(toggled["coins"] == bought["coins"] and toggled["decor_owned"] == bought["decor_owned"] and toggled["decor_on"] == (bought["decor_on"] ^ bit), "decoration_toggle_preserves_inventory")
    s.shot("shop")
    s.home(); s.resume()


def start_game(s, which):
    s.tap(166, 191)
    s.wait_state(lambda v: v["mode"] == 1, "games-menu")
    s.shot("games-menu")
    s.tap(143, 142 if which else 78, settle=0)
    return time.monotonic()


def bloom(s):
    before = s.flush("before-bloom"); s.resume()
    origin = start_game(s, 1)
    # Trigger the press edge around each target using real elapsed time. There
    # is no guest memory mutation or test-only score injection.
    for beat in range(8):
        target = origin + 1.5 + beat*1.2 - (.10 if s.device else .025)
        while time.monotonic() < target:
            s.pump(min(.03, target-time.monotonic()))
        if s.device or beat % 2:
            s.tap(143, 128, settle=0)
        else:
            s.key("Z", settle=0)
    result = s.wait_state(lambda v: v["mode"] == 6 and v["last_game"] == 1, "bloom-result", timeout=7)
    s.check(result["score"] > 0 and result["coins"] == min(9999, before["coins"]+result["reward"]) and result["growth"] == min(999, before["growth"]+10+result["score"]) and result["rounds"] == min(65535, before["rounds"]+1), "bloom_real_timing_hits_and_positive_earnings", score=result["score"], coins=result["coins"]-before["coins"], growth=result["growth"]-before["growth"])
    s.shot("bloom-result")
    s.tap(212, 191)
    s.wait_state(lambda v: v["mode"] == 0, "bloom-home")


def catch_target(image, ox):
    # Real rendered RGB565 berries survive device JPEG color quantization.
    # Exclude score text, basket and ground; pick the lowest visible berry.
    rgb = image.convert("RGB")
    candidates = []
    for y in range(8+46, 8+151, 2):
        for x in range(ox+8, ox+280, 2):
            r, g, b = rgb.getpixel((x, y))
            pink = abs(r-246)+abs(g-142)+abs(b-164) < 65
            gold = abs(r-246)+abs(g-198)+abs(b-98) < 65
            if pink or gold:
                candidates.append((x-ox, y))
    if not candidates:
        return None
    bottom = max(y for _, y in candidates)
    xs = [x for x, y in candidates if y >= bottom-5]
    return sum(xs)//len(xs)


def catch(s):
    before = s.flush("before-catch"); s.resume()
    start_game(s, 0)
    if not s.device:
        s.hold("RIGHT", True); s.pump(.65)
        right = s.flush("held-right")
        s.hold("RIGHT", False)
        s.check(right["basket_x"] > 220, "held_right_moves_basket_continuously", basket_x=right["basket_x"])
        s.resume(); s.hold("LEFT", True); s.pump(.55)
        left = s.flush("held-left"); s.hold("LEFT", False)
        s.check(left["basket_x"] < right["basket_x"]-45, "held_left_moves_basket_continuously", basket_x=left["basket_x"])
        s.resume()
    s.pointer("DOWN", 144, 150)
    deadline = time.monotonic()+20
    finish_at = len(s.logs())
    while time.monotonic() < deadline:
        image = s.shot("catch-tracking")
        x = catch_target(image, 4 if s.device else 16)
        if x is not None:
            s.pointer("MOVE", x, 150)
        if "mode=6" in s.logs()[finish_at:]:
            break
        s.pump(.12)
    s.pointer("UP", 144, 150)
    result = s.wait_state(lambda v: v["mode"] == 6 and v["last_game"] == 0, "catch-result")
    s.check(result["score"] > 0 and result["coins"] == min(9999, before["coins"]+result["reward"]) and result["growth"] == min(999, before["growth"]+10+result["score"]) and result["rounds"] == min(65535, before["rounds"]+1), "catch_real_drag_catches_and_positive_earnings", score=result["score"], coins=result["coins"]-before["coins"], growth=result["growth"]-before["growth"])
    s.check(result["stage"] == (2 if result["growth"] >= 120 else 1 if result["growth"] >= 45 else 0), "real_earned_growth_matches_evolution_stage", growth=result["growth"], stage=result["stage"])
    s.shot("catch-result")
    s.tap(212, 191)
    s.wait_state(lambda v: v["mode"] == 0, "catch-home")


def input_safety(s):
    baseline = s.flush("before-cancel"); s.resume()
    s.pointer("DOWN", 118, 191)
    s.pump(.1)
    s.pointer("CANCEL", 118, 191)
    after = s.flush("cancelled-pet")
    s.check(after["growth"] == baseline["growth"], "touch_cancel_does_not_activate_care")
    s.resume()
    if not s.device:
        s.pointer("DOWN", 71, 191)
        s.key("ENTER"); s.paused = True
        frozen = s.saved("paused-pointer")
        s.key("ENTER"); s.paused = False
        s.pointer("UP", 71, 191)
        ended = s.flush("cancelled-by-menu")
        s.check(ended["growth"] == frozen["growth"], "menu_cancelled_pointer_stays_cancelled_after_resume")
        s.resume()
        # Select Wash by performing it, then let its cooldown finish before the
        # held-A test so a leaked A press would create observable growth.
        s.tap(71, 191); s.pump(5.2)
        base = s.flush("before-held-A")
        s.hold("Z", True); s.paused = False  # A selects menu Continue.
        s.pump(1.1)
        held = s.flush("held-A-resumed")
        s.check(held["growth"] == base["growth"] and held["mode"] == 0, "menu_A_is_suppressed_until_physical_release")
        s.hold("Z", False); s.resume()
        s.key("Z")
        fresh = s.flush("fresh-A")
        s.check(fresh["growth"] == min(999, held["growth"]+7), "fresh_A_press_works_after_release")
        s.resume()
    else:
        s.report["controller_checks"] = "unavailable: physical controller_mask=0; all device menus use touch"
    before = s.flush("pause-baseline")
    s.shot("menu-paused")
    s.pump(1.0)
    # Resume briefly and force a *new* durable save; checking only an unchanged
    # old KV generation would not prove the guest timer stopped while paused.
    s.resume(); s.pump(.2)
    after = s.flush("pause-resumed")
    s.check(0 <= after["active_ms"]-before["active_ms"] < 850 and stable_equal(before, after, ("growth", "coins", "rounds", "decor_owned")), "menu_pause_freezes_foreground_time_and_preserves_progress", elapsed_ms=after["active_ms"]-before["active_ms"])
    s.resume()


PERSISTENT = ("coins", "growth", "stage", "branch", "decor_owned", "decor_on", "room", "pet_x", "rounds", "best_catch", "best_rhythm")


def interrupt(s):
    s.home(); s.resume()
    start_game(s, 0)
    s.pointer("DOWN", 88, 150); s.pointer("MOVE", 88, 150); s.pointer("UP", 88, 150)
    s.pump(2.2)
    saved = s.flush("interrupted")
    s.check(saved["mode"] == 2 and 1000 < saved["game_ms"] < 4500 and saved["basket_x"] == 88 and any(d["active"] for d in saved["drops"]), "interrupted_catch_has_durable_objects_and_timer", game_ms=saved["game_ms"])
    s.shot("interrupted-catch")
    s.exit()
    return s.saved("after-exit")


def restoration(s, previous):
    require("Pocket: load completed" in s.logs(), "Runtime did not report a save load")
    restored = s.flush("restored-catch")
    s.check(stable_equal(previous, restored, PERSISTENT + ("basket_x",)) and restored["mode"] == 2 and previous["game_ms"] <= restored["game_ms"] < previous["game_ms"]+1800, "earned_progress_and_interrupted_game_restore_across_processes", before_ms=previous["game_ms"], after_ms=restored["game_ms"])
    s.shot("restored-catch")
    s.reset()
    reset = s.wait_state(lambda v: v["mode"] == 0, "reset-home")
    s.check(stable_equal(previous, reset, PERSISTENT), "menu_RESET_returns_home_and_retains_earned_growth_coins_inventory")
    s.shot("reset-retains-progress")
    s.exit()
    return s.saved("after-reset-exit")


def device_idle(s):
    """Mirror the frozen helper's lifecycle/PERF protocol with pet assertions."""
    d = s.b
    before = s.saved("before-idle")
    at = len(d.logs)
    began = time.monotonic()
    def transition(value):
        return any("UI lifecycle:" in line and f"foreground={value}" in line and f"display_interactive={value}" in line for line in d.logs[at:])
    deadline = time.monotonic()+d.args.lock_timeout
    keepalive = time.monotonic()+10
    while not transition(0) and time.monotonic() < deadline:
        d.pump(.1)
        if time.monotonic() >= keepalive:
            d.request("INFO"); keepalive += 10
    require(transition(0), "Idle-lock background transition missing")
    foreground_until_lock = time.monotonic()-began
    d.pump(1)
    d.request("PERF CLEAR"); d.request("PERF START"); d.pump(3)
    props = d.module.pxadb.info_properties(d.request("PERF STOP")[-1].payload)
    s.report["locked_perf"] = props
    s.check(int(props["raster"]) == 0 and int(props["present"]) == 0, "idle_lock_zero_raster_and_present")
    s.shot("idle-locked")
    locked = s.saved("idle-locked")
    d.pump(2)
    again = s.saved("idle-locked-again")
    s.check(stable_equal(locked, again, ("active_ms", "game_ms", "growth", "coins")), "locked_foreground_state_remains_frozen")
    at = len(d.logs)
    d.request("PERF CLEAR"); d.request("PERF START")
    d.request("INPUT TAP 148 120"); d.pump(.3)
    d.request("INPUT SWIPE 148 192 148 48 600 8")
    d.until(lambda: transition(1), "unlock foreground transition", seconds=15)
    d.pump(.6)
    resumed = d.module.pxadb.info_properties(d.request("PERF STOP")[-1].payload)
    s.report["resumed_perf"] = resumed
    require(int(resumed["present"]) > 0, "No frame after unlock")
    after = s.flush("after-unlock")
    active_delta = after["active_ms"]-before["active_ms"]
    s.check(stable_equal(before, after, PERSISTENT) and 0 <= active_delta < (foreground_until_lock+2.0)*1000, "idle_unlock_restores_pet_and_excludes_locked_time", active_ms=active_delta, foreground_wall_ms=round(foreground_until_lock*1000))
    s.resume()


def simulator_run(args, report):
    state = Path(tempfile.mkdtemp(prefix="state-", dir=args.output))
    report["state_root"] = str(state)
    with tempfile.TemporaryDirectory(prefix="pet-sim-") as sockets:
        socket_root = Path(sockets)
        with old_sim.Simulator(args, state, socket_root/"one.sock", "game", report) as backend:
            s = Session(backend, report)
            care_and_shop(s); bloom(s); catch(s); input_safety(s)
            previous = interrupt(s)
        # Deliberate wall time outside a guest process must not enter game_ms.
        time.sleep(2.0)
        with old_sim.Simulator(args, state, socket_root/"two.sock", "reload", report) as backend:
            final = restoration(Session(backend, report), previous)
        with old_sim.Simulator(args, state, socket_root/"three.sock", "home-reload", report) as backend:
            s = Session(backend, report)
            restored = s.flush("restored-home")
            s.check(restored["mode"] == 0 and stable_equal(final, restored, PERSISTENT), "home_progress_persists_after_reset_and_second_relaunch")
            s.exit()


def device_start(d, report):
    """Resolve this app's first-run media prompt before gameplay touches."""
    at = len(d.logs)
    d.request("PACKAGE run "+IDENTITY)
    # Drain the short launch burst before a nine-package response can consume
    # serial bandwidth and drop the guest's audio/initialization diagnostics.
    d.pump(.75)
    # Query the authoritative package state; launch logs can be dropped in
    # different batches. Subsequent audio, screenshots and decoded PGD1 state
    # verify guest initialization instead of relying on one diagnostic line.
    d.until(lambda: any(frame.kind == "PKG" and IDENTITY in frame.payload and "active=1" in frame.payload
                        for frame in d.request("PACKAGES")), "Pet active package")
    d.pump(.25)
    lines = d.logs[at:]
    ready_lines = [line for line in lines if "Pocket: ready app="+IDENTITY in line]
    if ready_lines:
        require(any("canvas=288x224 origin=4,8 scale=1" in line for line in ready_lines), "Unexpected device touch geometry")
    else:
        Session(d, report, device=True).shot("startup-ready-with-log-drop")
        report.setdefault("startup_log_drops", []).append({"package_active": True, "screenshot_verified": True})
    prompted = any("Runtime permission prompt queued:" in line and
                   ":"+IDENTITY+" permission=" in line for line in d.logs[at:])
    if prompted:
        Session(d, report, device=True).shot("audio-permission")
        # Confirmed on the real 296x240 permission sheet: Allow is at 200,183.
        d.tap(196, 175, settle=.6)
    d.until(lambda: any("Pocket: audio ready 16000 Hz mono" in line for line in d.logs[at:]),
            "Pet audio ready after permission", seconds=8)
    report.setdefault("audio_starts", []).append({"permission_prompt": prompted, "ready": True})


def device_run(args, report):
    import device as old_device
    old_device.IDS["pet-garden"] = IDENTITY
    args.game = "pet-garden"
    with (args.output/"serial.jsonl").open("w", buffering=1) as stream:
        with old_device.pxadb.open_client(args.port, 30) as client:
            info = old_device.pxadb.info_properties(client.device_info)
            report["hello"] = client.device_info
            report["device"] = info
            require(info.get("target") == "esp32s3", "Expected ESP32-S3")
            d = old_device.Device(args, client, report, stream)
            d.module = old_device
            try:
                require(not any(f.kind == "PKG" and "active=1" in f.payload for f in d.request("PACKAGES")), "Another package is active")
                client.subscribe_logs()
                report["input"] = [f.payload for f in d.request("INPUT CAPABILITIES")]
                d.request("INPUT KEY HOME")
                d.request("INPUT SWIPE 148 192 148 48 600 8"); d.pump(.5)
                d.logs.clear()
                device_start(d, report)
                s = Session(d, report, device=True)
                report["device_from"] = args.device_from
                if args.device_from == "all":
                    care_and_shop(s); bloom(s)
                else:
                    s.home(); s.resume()
                if args.device_from != "lifecycle":
                    catch(s); input_safety(s)
                previous = interrupt(s)
                d.pump(2); device_start(d, report)
                final = restoration(Session(d, report, device=True), previous)
                device_start(d, report)
                s = Session(d, report, device=True)
                current = s.flush("final-reload")
                s.check(current["mode"] == 0 and stable_equal(current, final, PERSISTENT), "home_progress_persists_after_reset_and_second_relaunch")
                s.resume()
                if not args.skip_idle_lock:
                    device_idle(s)
                s.exit()
                require(not any(old_device.FAULTS.search(line) for line in d.logs), "Device guest fault")
            finally:
                d.request("INPUT CANCEL")
                active = any(f.kind == "PKG" and ":"+IDENTITY+"\t" in f.payload and "active=1" in f.payload for f in d.request("PACKAGES"))
                if active:
                    d.request("PACKAGE stop "+IDENTITY)
                    d.until(lambda: any(f.kind == "PKG" and ":"+IDENTITY+"\t" in f.payload and "active=0" in f.payload for f in d.request("PACKAGES")), "Pet stopped after cleanup", seconds=10)
                report["cleanup"] = {"app_stopped": True, "input_cancelled": True}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", help="Serial device; omit to test the simulator")
    p.add_argument("--simulator", type=Path)
    p.add_argument("--package", type=Path)
    p.add_argument("--publisher-key", type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--lock-timeout", type=float, default=180)
    p.add_argument("--skip-idle-lock", action="store_true")
    p.add_argument("--device-from", choices=("all", "catch", "lifecycle"), default="all", help="Resume remaining device coverage without repeating completed care and purchases")
    args = p.parse_args()
    args.output = args.output.resolve()
    require(args.output.is_relative_to(WORKSPACE/"local/artifacts"), "Output must be inside ignored local/artifacts")
    if not args.port:
        for name in ("simulator", "package", "publisher_key"):
            value = getattr(args, name)
            if value is None:
                p.error("--"+name.replace("_", "-")+" is required in simulator mode")
            setattr(args, name, value.resolve())
            require(getattr(args, name).exists(), f"Missing {name}: {value}")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "game": "pet-garden", "backend": "device" if args.port else "simulator", "checks": [], "processes": []}
    try:
        (device_run if args.port else simulator_run)(args, report)
        report["passed"] = True
    except BaseException:
        report["error"] = traceback.format_exc()
    (args.output/"results.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
