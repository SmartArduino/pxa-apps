#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Verify five additional original scores in signed PXA SDL packages.

Run --self-test before a real run. --packages contains APP/package/pxa-APP.
Select --apps pet-garden marble-islands for the first pair. Loop lengths for
2048/chess/sokoban come from --port-music; --loop-ms APP=MILLISECONDS can override
an individual length. Record two complete nominal loops plus one second, allowing
score rests while reporting one-second RMS and maximum silence. Each process
must exit normally through the actual menu Exit row. Failed cleanup never passes.
No mute UI is assumed. This runner builds nothing and never accesses a device.
All PCM, WAV, state, screenshots and reports stay below --output in local/artifacts.
"""

import argparse
from array import array
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import struct
import sys
import tempfile
import time
import traceback
from types import SimpleNamespace

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("extra_audio_music_helpers", HERE / "music_audio.py")
MUSIC = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MUSIC)
TRANSPORT = MUSIC.TRANSPORT
require = MUSIC.require
APPS = ("game-2048", "pocket-chess", "pocket-sokoban", "pet-garden", "marble-islands")
PORT_APPS = APPS[:3]
DEFAULT_PORT_MUSIC = HERE.parents[1] / "pocket-game/music.hpp"


def parse_port_loops(text):
    periods = re.search(r"step_ms\s*\[\s*\]\s*=\s*\{([^}]+)\}", text)
    steps = re.search(r"melody\s*\[\s*3\s*\]\s*\[\s*(\d+)\s*\]", text)
    require(periods is not None and steps is not None, "Cannot derive port score lengths; use --loop-ms")
    values = [int(value.strip()) for value in periods.group(1).split(",") if value.strip()]
    require(len(values) == 3 and min(values) > 0 and int(steps.group(1)) > 0,
            "Invalid port score dimensions")
    return dict(zip(PORT_APPS, (value * int(steps.group(1)) for value in values)))


def loops(args):
    result = {"pet-garden": 42240, "marble-islands": 30720}
    sources = {app: "shared Pocket Play score: 128 steps" for app in result}
    overrides = {}
    for item in args.loop_ms:
        app, separator, value = item.partition("=")
        require(separator and app in APPS and app not in overrides, "Use unique --loop-ms APP=MILLISECONDS")
        milliseconds = int(value)
        require(1000 <= milliseconds <= 300000, "Loop duration must be 1000..300000 ms")
        overrides[app] = milliseconds
    if any(app in PORT_APPS and app not in overrides for app in args.apps):
        raw = args.port_music.read_bytes()
        result.update(parse_port_loops(raw.decode()))
        sources.update({app: {"path": str(args.port_music), "sha256": hashlib.sha256(raw).hexdigest()}
                        for app in PORT_APPS})
    result.update(overrides)
    sources.update({app: "explicit --loop-ms" for app in overrides})
    return {app: {"milliseconds": result[app], "source": sources[app]} for app in args.apps}


def signed_metrics(raw, dev):
    value = MUSIC.AUDIO.pcm_metrics(raw, dev)
    require(value["positive_samples"] > 100 and value["negative_samples"] > 100
            and value["peak"] > 200 and value["rms"] > 20, f"Inaudible score: {value}")
    require(value["peak"] <= 20000, f"Clipping/headroom failure: {value}")
    return value


def rest_metrics(raw, dev):
    """Count silent frames, rather than interleaved samples; accept score rests."""
    samples = array("h", raw)
    if (dev["format"] == "S16LE") != (sys.byteorder == "little"):
        samples.byteswap()
    channels, rate = dev["channels"], dev["rate"]
    require(len(samples) % channels == 0, "Incomplete PCM frame")
    longest = run = 0
    for start in range(0, len(samples), channels):
        run = run + 1 if all(value == 0 for value in samples[start:start + channels]) else 0
        longest = max(longest, run)
    window = rate * channels * 2
    rms = [round(MUSIC.AUDIO.pcm_metrics(raw[start:start + window], dev)["rms"], 3)
           for start in range(0, len(raw) - window + 1, window)]
    longest_windows = current = 0
    for value in rms:
        current = current + 1 if value <= 5 else 0
        longest_windows = max(longest_windows, current)
    return {"max_exact_silent_gap_seconds": round(longest / rate, 6),
            "one_second_rms": rms, "quiet_one_second_windows": sum(value <= 5 for value in rms),
            "max_consecutive_quiet_one_second_windows": longest_windows,
            "window_policy": "Intentional score rests are allowed; per-loop audibility is required."}


def capture(sim, dev, label, seconds, audible, loop_seconds=None):
    value = MUSIC.interval(sim, dev, label, seconds, audible, continuous=False)
    frame_bytes = dev["channels"] * 2
    with sim.pcm.open("rb") as stream:
        stream.seek(value["offset_bytes"])
        raw = stream.read(math.ceil(seconds * dev["rate"]) * frame_bytes)
    value.update(rest_metrics(raw, dev))
    value["pcm_sha256"] = hashlib.sha256(raw).hexdigest()
    if audible:
        signed_metrics(raw, dev)
        if loop_seconds is not None:
            loop_bytes = round(loop_seconds * dev["rate"]) * frame_bytes
            value["per_nominal_loop"] = [signed_metrics(raw[index * loop_bytes:(index + 1) * loop_bytes], dev)
                                         for index in range(2)]
    return value


def normal_exit(sim, opened=False):
    if not opened:
        MUSIC.menu(sim, True)
    sim.shot("exit-menu")
    # DOWN activates Exit immediately. The Host can close its control socket
    # before an UP acknowledgement; verify that exact process exits 0 either way.
    x = sim.x + int((144.5) * sim.w / sim.cw)
    y = sim.y + int((145.5) * sim.h / sim.ch)
    TRANSPORT.command(sim.sock, f"TOUCH DOWN 0 {x} {y}")
    try:
        TRANSPORT.command(sim.sock, f"TOUCH UP 0 {x} {y}")
    except (OSError, RuntimeError):
        pass  # The exit code below must prove a normal shutdown.
    code = sim.proc.wait(timeout=8)
    require(code == 0 and not sim.forced, f"Menu Exit was abnormal: code={code}, forced={sim.forced}")
    sim.healthy()
    entry = sim.report["processes"][-1]
    entry.update(menu_exit=True, menu_exit_code=code)
    return {"method": "actual menu Exit touch", "exit_code": code, "forced_shutdown": sim.forced}


def require_clean_process(entry):
    require(entry.get("menu_exit") is True and entry.get("menu_exit_code") == 0
            and entry.get("exit_code") == 0 and entry.get("forced_shutdown") is False,
            f"Process did not finish via normal menu Exit: {entry}")


def playable(sim, app):
    if app == "marble-islands":
        sim.key("Z")  # Fresh tutorial -> live race.
    before = sim.shot("gameplay-before")
    after_log = len(sim.logs())
    if app == "pet-garden":
        sim.touch(24, 191)
        sim.log_match(r"pet-garden feed\b", after=after_log)
        action = "Touch Feed, observe real feed event and changed game frame"
    elif app == "marble-islands":
        sim.key("RIGHT", hold=.6)
        sim.key("X")  # B opens the real island selector after race input.
        action = "Dismiss tutorial, steer live race, open island selector"
    elif app == "game-2048":
        for key in ("LEFT", "UP", "RIGHT", "DOWN"):
            sim.key(key)
        action = "Four real board movement actions"
    elif app == "pocket-chess":
        sim.touch(116, 180)  # e2
        sim.touch(116, 132)  # e4
        action = "Play e2-e4 through real touch chess board"
    else:
        sim.key("RIGHT")  # Fresh level 1 has a legal box push immediately right.
        sim.log_match(r"Pocket: saved", after=after_log)
        action = "Controller RIGHT pushes a real box and persists the changed board"
    after = sim.shot("gameplay-after")
    changed = TRANSPORT.difference(before, after)
    require(changed["changed_fraction"] > .0005, f"Audio unavailable blocked gameplay: {changed}")
    sim.healthy()
    return {"action": action, **changed}


def run_app(args, temporary, app, report):
    package = args.packages / app / "package" / ("pxa-" + app)
    require(package.is_dir(), f"Missing signed package: {package}")
    output = args.output / app
    output.mkdir(parents=True, exist_ok=True)
    require(not (output / "music.log").exists(), f"Use a fresh --output: {output}")
    state = Path(tempfile.mkdtemp(prefix="music-state-", dir=output))
    child = SimpleNamespace(**dict(vars(args), package=package, output=output,
                                   app=app, unavailable=False, capture_audio=True))
    result = {"passed": False, "package": str(package), "state_root": str(state),
              "package_hashes": {name: hashlib.sha256((package / name).read_bytes()).hexdigest()
                                 for name in ("manifest.pxm", "signature.pxs", "artifacts/main.linux-x86_64.aot")},
              "loop": report["loops"][app], "intervals": {},
              "mute_ui": "Absent in these five games; no invented mute test."}
    report["apps"][app] = result
    print(f"RUN {app}: signed SDL two loops + 1s; loop={result['loop']['milliseconds']}ms", flush=True)
    process_start = len(report["processes"])
    with MUSIC.Simulator(child, state, temporary / "music.sock", "music", report) as sim:
        dev = MUSIC.device(sim)
        result["device"] = dev
        sim.shot("ready")
        duration = result["loop"]["milliseconds"] / 1000
        cuts = result["intervals"]
        cuts["two_loops"] = capture(sim, dev, "two-loops", duration * 2 + 1, True, duration)
        MUSIC.menu(sim, True)
        sim.shot("paused")
        cuts["menu_silent"] = capture(sim, dev, "menu-silent", args.check_seconds, False)
        MUSIC.menu(sim, False)
        cuts["continue_resumed"] = capture(sim, dev, "continue-resumed", args.check_seconds, True)
        result["normal_exit"] = normal_exit(sim)
    require_clean_process(report["processes"][-1])
    unavailable_state = Path(tempfile.mkdtemp(prefix="unavailable-state-", dir=output))
    unavailable = SimpleNamespace(**dict(vars(child), unavailable=True))
    with MUSIC.Simulator(unavailable, unavailable_state, temporary / "unavailable.sock",
                         "unavailable", report) as sim:
        sim.log_match(r"Pocket: optional audio unsupported|Pocket: audio unavailable")
        require("Pocket: audio ready" not in sim.logs(), "Unavailable SDL driver unexpectedly opened")
        result["unavailable_gameplay"] = playable(sim, app)
        MUSIC.menu(sim, True)
        sim.shot("unavailable-menu")
        result["unavailable_normal_exit"] = normal_exit(sim, opened=True)
    require_clean_process(report["processes"][-1])
    result["processes"] = report["processes"][process_start:]
    result["passed"] = True
    sound = cuts["two_loops"]
    print(f"PASS {app}: {sound['seconds']:.3f}s PCM, peak={sound['peak']}, "
          f"maxgap={sound['max_exact_silent_gap_seconds']:.6f}s, pause/continue, "
          "two menu Exits 0, unavailable audio playable", flush=True)


def fixture():
    dev = {"format": "S16LE", "channels": 1, "rate": 1000}
    # Two active phrases separated by 1.2 seconds of deliberate score rest.
    values = [1000, -1000] * 1000 + [0] * 1200 + [1200, -1200] * 1000
    raw = struct.pack("<" + "h" * len(values), *values)
    signed_metrics(raw, dev)
    rest = rest_metrics(raw, dev)
    require(rest["max_exact_silent_gap_seconds"] == 1.2 and rest["quiet_one_second_windows"] == 1,
            "Fixture must report, and permit, intentional score rest")
    stereo = struct.pack(">" + "h" * (len(values) * 2), *(sample for value in values for sample in (value, value)))
    big = {"format": "S16BE", "channels": 2, "rate": 1000}
    signed_metrics(stereo, big)
    require(rest_metrics(stereo, big)["max_exact_silent_gap_seconds"] == 1.2,
            "Stereo big-endian silence must count complete frames")
    rejected = 0
    for bad in ([0] * 1000, [1000] * 1000, [32767, -32768] * 500):
        try:
            signed_metrics(struct.pack("<" + "h" * len(bad), *bad), dev)
        except AssertionError:
            rejected += 1
    require(rejected == 3, "Fixtures must reject silence, one-sided PCM and clipping")
    good = {"menu_exit": True, "menu_exit_code": 0, "exit_code": 0, "forced_shutdown": False}
    require_clean_process(good)
    rejected = 0
    for change in ({"menu_exit": False}, {"exit_code": 1}, {"forced_shutdown": True}):
        try:
            require_clean_process(dict(good, **change))
        except AssertionError:
            rejected += 1
    require(rejected == 3, "Normal-exit fixture must reject HOME-only/failed/forced processes")
    parsed = parse_port_loops("constexpr uint8_t melody[3][64]={}; constexpr uint16_t step_ms[]={225,360,280};")
    require(parsed == dict(zip(PORT_APPS, (14400, 23040, 17920))), "Port duration derivation")
    require(parse_port_loops("melody[3][128]={}; step_ms[]={100,200,300};")["game-2048"] == 12800,
            "Loop derivation must follow source dimensions")
    print("PASS offline fixtures: PCM audibility, polarity, clipping, score rest, stereo/endian, normal Exit, dynamic loops")


def markdown(report):
    lines = ["# Five additional signed SDL music checks", "", f"Passed: {report['passed']}", "",
             "| App | Loop ms | PCM seconds | Peak | Max silent gap s | Quiet 1s windows | Menu Exit / unavailable Exit | Passed |",
             "| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |"]
    for app, result in report["apps"].items():
        sound = result.get("intervals", {}).get("two_loops", {})
        exits = "/".join(str(result.get(key, {}).get("exit_code", "missing"))
                         for key in ("normal_exit", "unavailable_normal_exit"))
        lines.append(f"| {app} | {result.get('loop', {}).get('milliseconds', '')} | {sound.get('seconds', '')} | "
                     f"{sound.get('peak', '')} | {sound.get('max_exact_silent_gap_seconds', '')} | "
                     f"{sound.get('quiet_one_second_windows', '')} | {exits} | {result['passed']} |")
    lines.extend(["", "Each passing app has signed package hashes, two complete nominal loop lengths plus 1 second of continuous SDL PCM, positive and negative samples, peak <= 20000, exact-silent paused PCM, audible Continue, and playable input when SDL audio cannot open. Intentional score rests are reported rather than rejected. Both normal and unavailable-audio sessions must finish through the actual menu Exit with process exit code 0 and forced_shutdown=false.",
                  "", "No mute UI exists in this selection. No SDK build or serial/device access is performed. Simulator lifecycle background and permission denial are outside this runner's controls.", ""])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    for name in ("packages", "simulator", "publisher-key", "output"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--apps", nargs="+", choices=APPS, default=list(APPS))
    parser.add_argument("--port-music", type=Path, default=DEFAULT_PORT_MUSIC)
    parser.add_argument("--loop-ms", action="append", default=[], metavar="APP=MILLISECONDS")
    parser.add_argument("--check-seconds", type=float, default=3.0)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    if args.self_test:
        fixture()
        return 0
    for name in ("packages", "simulator", "publisher_key", "output"):
        require(getattr(args, name) is not None, "Missing --" + name.replace("_", "-"))
        setattr(args, name, getattr(args, name).resolve())
    args.port_music = args.port_music.resolve()
    require(args.output.is_relative_to(MUSIC.ARTIFACTS.resolve()), "Output must be inside ignored local/artifacts")
    require(len(args.apps) == len(set(args.apps)), "Do not repeat --apps")
    require(math.isfinite(args.check_seconds) and 2 <= args.check_seconds <= 10, "Check interval must be 2..10 seconds")
    require(math.isfinite(args.timeout) and 2 <= args.timeout <= 300, "Timeout must be 2..300 seconds")
    args.output.mkdir(parents=True, exist_ok=True)
    require(not (args.output / "results.json").exists(), "Use fresh output to retain earlier evidence")
    report = {"passed": False, "apps": {}, "processes": [], "screenshots": [], "loops": loops(args)}
    began = time.monotonic()
    try:
        with tempfile.TemporaryDirectory(prefix="extra-audio-") as temporary:
            for app in args.apps:
                try:
                    run_app(args, Path(temporary), app, report)
                except Exception:
                    report["apps"].setdefault(app, {"passed": False})["error"] = traceback.format_exc()
                    print(report["apps"][app]["error"], file=sys.stderr, flush=True)
                finally:
                    (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        report["passed"] = len(report["apps"]) == len(args.apps) and all(value["passed"] for value in report["apps"].values())
    except Exception:
        report["error"] = traceback.format_exc()
        print(report["error"], file=sys.stderr)
    finally:
        report["elapsed_seconds"] = round(time.monotonic() - began, 3)
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        (args.output / "report.md").write_text(markdown(report))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
