#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
r"""Verify real signed Pocket Play PCM using the product simulator and Pillow.

From the workspace root::

    python3 local/pxa-apps/common/pocket-play/tests/audio.py \
      --simulator local/artifacts/doom/20261023-140500/simulator-build/pxsys_product_simulator \
      --packages local/artifacts/pocket-play/20261024-015231 \
      --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der \
      --output local/artifacts/pocket-play/20261024-015231/audio

--packages contains APP/package/pxa-APP. The default selection is pet-garden,
crisp-arcade and marble-islands; --apps also supports lantern-grove, harbor-cafe
and tiny-elements. Use --apps crisp-arcade to verify one finalized package. Each
app runs with fresh Host state retained in --output, SDL dummy video, and SDL
disk audio. Only short control sockets use a temporary directory outside the
artifacts tree, to stay within the UNIX socket path limit. Actual touch actions
trigger tones; a later, sample-counted interval must be silent while the real
pause menu is open. No builds, device access, or shared state are used. Host
saves, logs, screenshots, S16 PCM, WAVs, and JSON stay in --output.
"""

import argparse
from array import array
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import sys
import tempfile
import time
import traceback
from types import SimpleNamespace
import wave

sys.dont_write_bytecode = True
WORKSPACE = Path(__file__).resolve().parents[5]
ORIGINAL_APPS = ("pet-garden", "crisp-arcade", "marble-islands")
APPS = ORIGINAL_APPS + ("lantern-grove", "harbor-cafe", "tiny-elements")
SPEC = importlib.util.spec_from_file_location(
    "pocket_audio_doom_simulator", WORKSPACE / "local/pxa-apps/doom/tests/simulator.py")
TRANSPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TRANSPORT)
require = TRANSPORT.require
FORMAT = re.compile(r"format=(S16LE|S16BE) channels=(\d+) freq=(\d+)")
FAULTS = re.compile(
    r"Guest exception|callback failed|out of bounds memory access|unreachable instruction|"
    r"PXA main-loop (?:event|wait) failed|Pocket: .*failed|exit flush timed out|"
    r"Pocket: (?:audio unavailable|optional audio denied|optional audio unsupported)", re.I)


def pcm_metrics(raw, device):
    require(len(raw) % (2 * device["channels"]) == 0, "Incomplete S16 PCM frame")
    samples = array("h", raw)
    if (device["format"] == "S16LE") != (sys.byteorder == "little"):
        samples.byteswap()
    count = len(samples)
    return {
        "frames": count // device["channels"], "samples": count,
        "seconds": count / (device["rate"] * device["channels"]),
        "nonzero_samples": sum(value != 0 for value in samples),
        "positive_samples": sum(value > 0 for value in samples),
        "negative_samples": sum(value < 0 for value in samples),
        "peak": max((abs(value) for value in samples), default=0),
        "rms": math.sqrt(sum(value * value for value in samples) / max(count, 1)),
    }


def touch(sim, x, y):
    # App coordinates are 288x224, centered in the requested 320x240 viewport.
    sim.tap(x + 16, y + 8)


def trigger(sim, app):
    if app == "pet-garden":
        for x, event in ((24, "feed"), (71, "wash"), (119, "pet")):
            after = len(sim.logs())
            touch(sim, x, 191)
            sim.log_match(r"pet-garden " + event + r"\b", after=after)
        return "touch feed/wash/pet: 660/880/520 Hz"
    if app == "crisp-arcade":
        touch(sim, 200, 140)
        sim.log_match(r"crisp launch game=COLOR ROLL")
        for _ in range(8):
            touch(sim, 140, 120)
        return "touch launch COLOR ROLL and fire eight shots"
    if app == "lantern-grove":
        after = len(sim.logs())
        touch(sim, 50, 183)
        sim.log_match(r"lantern-grove collect view=0 .*collected=", after=after)
        return "touch collect the grove: 660 Hz"
    if app == "harbor-cafe":
        sim.log_match(r"harbor event=1 day=1 .*table=0 recipe=0")
        after = len(sim.logs())
        touch(sim, 143, 179)
        sim.log_match(r"harbor event=2 day=1 .*table=0 recipe=0", after=after)
        return "touch accept the first customer's order: 660 Hz"
    if app == "tiny-elements":
        touch(sim, 144, 195)  # Dismiss the fresh-save illustrated guide.
        after = len(sim.logs())
        touch(sim, 137, 199)
        sim.log_match(r"Elements: material selected", after=after)
        return "touch select water: 506 Hz"
    require(app == "marble-islands", f"No audio trigger defined for {app}")
    # INTRO -> RACE -> ISLANDS. Island 2 is locked in the fresh progress state.
    touch(sim, 140, 120)
    touch(sim, 40, 204)
    for _ in range(3):
        touch(sim, 210, 65)
    return "touch islands then locked island 2 three times: 180 Hz"


def run_app(args, temporary, app, report):
    package = args.packages / app / "package" / ("pxa-" + app)
    require(package.is_dir(), f"Missing expanded signed package: {package}")
    child = SimpleNamespace(**vars(args), package=package, capture_audio=True)
    result = {"package": str(package), "manifest_sha256": hashlib.sha256(
        (package / "manifest.pxm").read_bytes()).hexdigest()}
    report["apps"][app] = result
    pcm_path = args.output / f"{app}-audio.s16"
    state_root = Path(tempfile.mkdtemp(prefix=f"{app}-state-", dir=args.output))
    result["state_root"] = str(state_root)
    with TRANSPORT.Simulator(child, state_root, temporary / (app + ".sock"), app, report) as sim:
        sim.log_match(r"Pocket: ready app=pxa-" + app + r"\b")
        require("canvas=288x224 origin=16,8 scale=1" in sim.logs(), "Unexpected canvas geometry")
        sim.log_match(r"Pocket: audio ready 16000 Hz mono")
        match = sim.log_match(FORMAT)
        audio_format, channels, rate = match.groups()
        device = {"format": audio_format, "channels": int(channels), "rate": int(rate)}
        require(1 <= device["channels"] <= 8 and 8000 <= device["rate"] <= 192000,
                f"Invalid SDL device format: {device}")
        result["device"] = device
        result["host_mix"] = {"rate": 16000, "channels": 1}
        sim.shot("ready")
        result["trigger"] = trigger(sim, app)
        sim.shot("tones-triggered")
        # Let the final short tone reach SDL before clearing the runtime queue.
        time.sleep(.5)
        after = len(sim.logs())
        touch(sim, 276, 10)
        sim.log_match(r"Pocket: menu opened", after=after)
        sim.shot("paused")
        # Drain the Host queue and SDL stdio buffering before marking a fresh
        # interval. Wait for capture progress, not just elapsed wall-clock time:
        # SDL's disk callback may run at a different rate from real playback.
        time.sleep(1.2)
        frame_bytes = device["channels"] * 2
        require(pcm_path.is_file(), "SDL disk output is missing")
        silence_start = pcm_path.stat().st_size
        silence_start -= silence_start % frame_bytes
        silence_bytes = math.ceil(args.pause_seconds * device["rate"]) * frame_bytes
        sim.wait(lambda: pcm_path.stat().st_size >= silence_start + silence_bytes,
                 "at least the requested paused PCM interval")
        with pcm_path.open("rb") as stream:
            stream.seek(silence_start)
            silent = stream.read(silence_bytes)
        result["pause"] = {"offset_bytes": silence_start,
                           "requested_seconds": args.pause_seconds, **pcm_metrics(silent, device)}
        require(result["pause"]["seconds"] >= .5, "Pause capture is shorter than 0.5 seconds")
        require(result["pause"]["nonzero_samples"] == 0,
                f"Paused audio did not settle to silence: {result['pause']}")
        require(not FAULTS.search(sim.logs()), "Pocket runtime/audio fault; inspect simulator log")
        # The unchanged transport exits normally through PXA HOME and records
        # exit_code/forced_shutdown. Paused PCM was checked before deactivation.
    raw = pcm_path.read_bytes()
    result["audio"] = {"path": str(pcm_path), **pcm_metrics(raw, device)}
    sound = result["audio"]
    require(sound["nonzero_samples"] >= 128 and sound["positive_samples"] >= 64
            and sound["negative_samples"] >= 64 and sound["peak"] > 64 and sound["rms"] > 1,
            f"No real signed tone output: {sound}")
    # Source mixer clamps at 20000; allow modest SDL resampling overshoot while
    # requiring substantial headroom below S16 saturation.
    require(sound["peak"] <= 22000, f"Audio exceeds bounded mixer peak: {sound}")
    require(not FAULTS.search(sim.logs()), "Pocket runtime/audio fault during clean shutdown")
    wav_path = args.output / f"{app}-audio.wav"
    little = array("h", raw)
    if device["format"] == "S16BE":
        little.byteswap()
    with wave.open(str(wav_path), "wb") as wav:
        wav.setparams((device["channels"], 2, device["rate"], 0, "NONE", "not compressed"))
        wav.writeframes(little.tobytes())
    result.update(wav=str(wav_path), passed=True)
    print(f"{app}: PCM nonzero={sound['nonzero_samples']} peak={sound['peak']} "
          f"RMS={sound['rms']:.2f}; paused {result['pause']['seconds']:.3f}s silence; exit=0", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--simulator", type=Path, required=True, help="Actual PXA product simulator executable")
    parser.add_argument("--packages", type=Path, required=True, help="Artifact root containing APP/package/pxa-APP")
    parser.add_argument("--publisher-key", type=Path, required=True, help="Signed packages' publisher public DER key")
    parser.add_argument("--output", type=Path, required=True, help="Output directory below local/artifacts/")
    parser.add_argument("--apps", choices=APPS, nargs="+", default=list(ORIGINAL_APPS), help="Finalized apps to run (default: Pet Garden, Crisp Arcade, Marble Islands)")
    parser.add_argument("--pause-seconds", type=float, default=.75, help="Fresh silent PCM duration, at least 0.5 seconds (default: 0.75)")
    parser.add_argument("--timeout", type=float, default=30, help="Maximum seconds for each startup/capture wait (default: 30)")
    args = parser.parse_args()
    for name in ("simulator", "packages", "publisher_key", "output"):
        setattr(args, name, getattr(args, name).expanduser().resolve())
    require(args.output.is_relative_to(WORKSPACE / "local/artifacts"), "--output must be under ignored local/artifacts/")
    require(args.simulator.is_file() and args.publisher_key.is_file(), "Missing simulator or publisher key")
    require(args.packages.is_dir(), "Missing --packages artifact root")
    require(math.isfinite(args.pause_seconds) and .5 <= args.pause_seconds <= 10,
            "--pause-seconds must be between 0.5 and 10")
    require(math.isfinite(args.timeout) and 1 <= args.timeout <= 300, "--timeout must be between 1 and 300")
    require(len(set(args.apps)) == len(args.apps), "Do not repeat --apps entries")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "apps": {}, "processes": [], "screenshots": []}
    try:
        with tempfile.TemporaryDirectory(prefix="pocket-audio-") as temporary:
            for app in args.apps:
                run_app(args, Path(temporary), app, report)
        report["passed"] = True
        return 0
    except Exception:
        report["error"] = traceback.format_exc()
        print(report["error"], file=sys.stderr)
        return 1
    finally:
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
