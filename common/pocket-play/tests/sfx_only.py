#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Verify effects-only audio in final signed Pocket packages using real inputs.

--packages contains APP/package/pxa-APP. Fresh isolated profiles, SDL disk PCM,
actual logged viewport, sample-counted silence/action/pause intervals, normal
menu Exit. Writes only to --output below local/artifacts; builds nothing, never
accesses serial, and does not modify imported transport or production code.
"""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import sys
import tempfile
import time
import traceback
from types import SimpleNamespace

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("sfx_only_music_helpers", HERE / "music_audio.py")
MUSIC = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MUSIC)
TRANSPORT = MUSIC.TRANSPORT
require = MUSIC.require
APPS = ("game-2048", "pocket-chess", "pocket-sokoban", "pet-garden",
        "crisp-arcade", "marble-islands", "lantern-grove", "harbor-cafe", "tiny-elements")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def capture(sim, dev, label, seconds, audible, action=None):
    """Mark before input so a brief effect cannot disappear into a settle delay."""
    frame = 2 * dev["channels"]
    start = sim.pcm.stat().st_size // frame * frame
    size = math.ceil(seconds * dev["rate"]) * frame
    began = time.monotonic()
    action_result = action() if action else None
    sim.wait(lambda: sim.pcm.stat().st_size >= start + size and
             time.monotonic() - began >= seconds, label + " sample-counted PCM", timeout=seconds * 2 + 15)
    with sim.pcm.open("rb") as stream:
        stream.seek(start)
        raw = stream.read(size)
    require(len(raw) == size, "PCM interval is incomplete")
    value = MUSIC.AUDIO.pcm_metrics(raw, dev)
    target = sim.args.output / (sim.name + "-" + label + ".wav")
    MUSIC.wav(target, raw, dev)
    value.update(label=label, offset_bytes=start, bytes=len(raw),
                 requested_seconds=seconds, wall_seconds=round(time.monotonic() - began, 3),
                 sha256=hashlib.sha256(raw).hexdigest(), wav=str(target), action=action_result)
    sim.report.setdefault("pcm_intervals", []).append(value)
    if audible:
        require(value["positive_samples"] > 100 and value["negative_samples"] > 100
                and value["peak"] > 100 and value["rms"] > 2,
                f"{label}: actual action produced no useful signed SFX: {value}")
        require(value["peak"] <= 20000, f"{label}: unexpected clipping/headroom: {value}")
    else:
        require(value["nonzero_samples"] == 0 and value["frames"] >= dev["rate"],
                f"{label}: idle/pause must be exact zero: {value}")
    sim.healthy()
    value["passed"] = True
    return value


def menu(sim, opened):
    after = len(sim.logs())
    if opened:
        sim.touch(276, 18)
    else:
        sim.key("Z")
    sim.log_match(r"Pocket: menu " + ("opened" if opened else "closed"), after=after)


def normal_exit(sim):
    # Menu is already paused. DOWN commits Exit; process may close before UP.
    x = sim.x + int(144.5 * sim.w / sim.cw)
    y = sim.y + int(145.5 * sim.h / sim.ch)
    TRANSPORT.command(sim.sock, f"TOUCH DOWN 0 {x} {y}")
    try:
        TRANSPORT.command(sim.sock, f"TOUCH UP 0 {x} {y}")
    except (OSError, RuntimeError):
        pass
    code = sim.proc.wait(timeout=8)
    require(code == 0 and not sim.forced, f"Abnormal menu exit {code}, forced={sim.forced}")
    sim.healthy()
    sim.report["processes"][-1].update(menu_exit=True, menu_exit_code=code)
    return {"menu_exit_code": code, "forced_shutdown": sim.forced}


def prepare(sim, app):
    # Select stable no-input screens before initial silence sampling.
    if app == "tiny-elements":
        sim.touch(144, 195)  # Fresh illustrated guide -> sandbox.
    elif app == "harbor-cafe":
        sim.log_match(r"harbor event=1 day=1 .*table=0 recipe=0")


def action(sim, app):
    checkpoint = len(sim.logs())
    if app == "game-2048":
        for key in ("LEFT", "UP", "RIGHT", "DOWN"):
            sim.key(key)
        return "Controller LEFT/UP/RIGHT/DOWN moves actual 2048 board"
    if app == "pocket-chess":
        sim.touch(116, 180)
        sim.touch(116, 132)
        return "Touch e2-e4 on actual chess board"
    if app == "pocket-sokoban":
        sim.key("RIGHT")
        sim.log_match(r"Pocket: saved", after=checkpoint)
        return "Controller RIGHT pushes actual level-1 box and saves"
    if app == "pet-garden":
        sim.touch(24, 191)
        sim.log_match(r"pet-garden feed\b", after=checkpoint)
        return "Touch Feed produces real pet-garden feed event"
    if app == "crisp-arcade":
        sim.touch(200, 140)
        sim.log_match(r"crisp launch game=(?:COLOR ROLL|彩条连击)", after=checkpoint)
        for _ in range(5):
            sim.touch(140, 120)
        return "Touch launches upstream COLOR ROLL then fires five real shots"
    if app == "marble-islands":
        sim.key("Z")  # Intro -> race.
        sim.key("X")  # Race -> island selector, no physics bounce cue.
        for _ in range(3):
            sim.touch(210, 65)  # Locked island 2 emits 180Hz failure SFX.
        return "Controller enters race and island selector; touch locked island 2 three times"
    if app == "lantern-grove":
        sim.touch(50, 183)
        sim.log_match(r"lantern-grove collect view=0 .*collected=", after=checkpoint)
        return "Touch Collect commits actual grove collection"
    if app == "harbor-cafe":
        sim.touch(143, 179)
        sim.log_match(r"harbor event=2 day=1 .*table=0 recipe=0", after=checkpoint)
        return "Touch accepts first actual espresso order"
    require(app == "tiny-elements", "Unknown action")
    sim.touch(137, 199)
    sim.log_match(r"Elements: material selected", after=checkpoint)
    return "Touch selects water material in live sandbox"


def run_app(args, sockdir, app, report):
    package = args.packages / app / "package" / ("pxa-" + app)
    require(package.is_dir(), f"Missing final signed package {package}")
    manifest, signature = package / "manifest.pxm", package / "signature.pxs"
    require(manifest.is_file() and signature.is_file(), "Expected signed package")
    require(not any(p.is_file() and p.suffix.lower() == ".wav" for p in package.rglob("*")),
            "WAV must not ship in signed effects-only package")
    aot = package / "artifacts/main.linux-x86_64.aot"
    require(aot.is_file(), "Final signed simulator AOT is missing")
    opts = SimpleNamespace(simulator=args.simulator, publisher_key=args.publisher_key,
                           package=package, output=args.output, timeout=args.timeout,
                           unavailable=False, app=app)
    row = {"passed": False, "package": str(package), "manifest_sha256": sha(manifest),
           "signature_sha256": sha(signature), "simulator_aot_sha256": sha(aot),
           "shipped_wav_count": 0}
    report["apps"][app] = row
    with MUSIC.Simulator(opts, args.output / (app + "-profile"),
                         sockdir / (app + ".sock"), app, report) as sim:
        dev = MUSIC.device(sim)
        prepare(sim, app)
        MUSIC.wait_seconds(sim, 1.2)  # Drain startup audio/SDL queue before fresh idle interval.
        row["idle"] = capture(sim, dev, "idle-before-action", args.idle_seconds, False)
        before = sim.shot("before-action")
        row["sfx"] = capture(sim, dev, "real-action-sfx", 5.0, True, lambda: action(sim, app))
        after = sim.shot("after-action")
        delta = TRANSPORT.difference(before, after)
        require(delta["changed_fraction"] > .0005, f"Action did not change real game view: {delta}")
        row["gameplay_change"] = delta
        menu(sim, True)
        sim.shot("paused")
        MUSIC.wait_seconds(sim, 1.2)
        row["paused"] = capture(sim, dev, "paused-exact-zero", args.idle_seconds, False)
        row["exit"] = normal_exit(sim)
    proc = report["processes"][-1]
    require(proc.get("exit_code") == 0 and proc.get("forced_shutdown") is False
            and proc.get("menu_exit_code") == 0, f"Unclean process: {proc}")
    row["passed"] = True
    print(f"PASS {app}: idle={row['idle']['nonzero_samples']} action_nonzero={row['sfx']['nonzero_samples']} "
          f"positive={row['sfx']['positive_samples']} negative={row['sfx']['negative_samples']} "
          f"paused={row['paused']['nonzero_samples']} exit=0", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--simulator", type=Path, required=True)
    parser.add_argument("--publisher-key", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--apps", nargs="+", choices=APPS, default=list(APPS))
    parser.add_argument("--idle-seconds", type=float, default=4.0)
    parser.add_argument("--timeout", type=float, default=30)
    args = parser.parse_args()
    for name in ("packages", "simulator", "publisher_key", "output"):
        setattr(args, name, getattr(args, name).resolve())
    require(MUSIC.ARTIFACTS in args.output.parents, "--output must be below local/artifacts")
    require(1 <= args.idle_seconds <= 30 and 1 <= args.timeout <= 300, "Invalid duration")
    require(len(set(args.apps)) == len(args.apps), "Do not repeat apps")
    require(not (args.output / "results.json").exists(), "Use a fresh output directory")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "runner_sha256": sha(Path(__file__)),
              "helper_sha256": {str(p): sha(p) for p in (HERE / "music_audio.py", HERE / "audio.py",
                                                         Path(TRANSPORT.__file__))},
              "simulator_sha256": sha(args.simulator), "publisher_key_sha256": sha(args.publisher_key),
              "apps": {}, "processes": [], "screenshots": [], "serial_access": False,
              "limitations": ["Standalone simulator does not inject app background or permission denial.",
                              "Crisp launcher silence and COLOR ROLL SFX tested here; native bridge test covers all four upstream game BGM flags."]}
    code = 0
    try:
        with tempfile.TemporaryDirectory(prefix="sfx-only-") as temp:
            for app in args.apps:
                try:
                    run_app(args, Path(temp), app, report)
                except Exception:
                    report["apps"].setdefault(app, {})["error"] = traceback.format_exc()
                    print(report["apps"][app]["error"], file=sys.stderr, flush=True)
                    code = 1
        report["passed"] = code == 0
    except Exception:
        report["error"] = traceback.format_exc()
        code = 1
    finally:
        report["cleanup"] = {"all_processes_stopped": all("exit_code" in p for p in report["processes"]),
                             "forced_shutdowns": sum(bool(p.get("forced_shutdown")) for p in report["processes"])}
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
