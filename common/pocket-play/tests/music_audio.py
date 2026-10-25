#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
r"""Record the three original scores in real signed PXA simulator packages.

    python3 local/pxa-apps/common/pocket-play/tests/music_audio.py \
      --packages local/artifacts/pocket-music/BUILD \
      --simulator local/artifacts/doom/20261023-140500/simulator-build/pxsys_product_simulator \
      --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der \
      --output local/artifacts/pocket-music/BUILD/audio

Packages are ROOT/APP/package/pxa-APP. Capture includes two complete loop
lengths, normal pause/resume, touch and controller mute, a muted process restart,
audible re-enable and restart, and gameplay with an unavailable SDL audio device.
All logs, screenshots, Host saves, signed-manifest hashes, S16 PCM, WAV excerpts
and metrics stay in --output under ignored local/artifacts. The existing audio
and Doom transport modules are imported read-only. Pure save decoders are reused
from worlds_device; this runner never opens a serial port or builds a package.

The standalone simulator control interface cannot inject lifecycle background or
permission denial. These two checks are explicitly reported as not exercised;
menu pause and SDL device failure are tested separately and are not substitutes.
"""

import argparse
from array import array
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import traceback
from types import SimpleNamespace
import wave

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
WORKSPACE = HERE.parents[4]
ARTIFACTS = WORKSPACE / "local/artifacts"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


AUDIO = module("pocket_music_audio_helpers", HERE / "audio.py")
TRANSPORT = AUDIO.TRANSPORT
SAVES = module("pocket_music_save_decoders", HERE / "worlds_device.py")
require = TRANSPORT.require
APPS = {"lantern-grove": 38.400, "harbor-cafe": 34.304, "tiny-elements": 36.000}
VIEWPORT = re.compile(r"Pocket: viewport canvas=(\d+)x(\d+) origin=(-?\d+),(-?\d+) "
                      r"size=(\d+)x(\d+) display=(\d+)x(\d+)")
FAULTS = re.compile(r"Guest exception|callback failed|out of bounds memory access|"
                    r"unreachable instruction|PXA main-loop (?:event|wait) failed|"
                    r"Pocket: .*failed|exit flush timed out|wasm trap|stack overflow|"
                    r"input queue overflow|event queue overflow", re.I)


class Simulator(TRANSPORT.Simulator):
    """Preserve the existing transport, adding only a per-child audio driver."""

    def __enter__(self):
        self.state.mkdir(parents=True, exist_ok=True)
        self.log_file = self.log_path.open("wb")
        argv = [str(self.args.simulator), "--package", str(self.args.package),
                "--publisher-key", str(self.args.publisher_key), "--state-root", str(self.state),
                "--width", "320", "--height", "240", "--corner-radius", "0",
                "--safe-insets", "0,0,0,0", "--pxadb-control-socket", str(self.sock)]
        env = dict(os.environ, SDL_VIDEODRIVER="dummy",
                   SDL_AUDIODRIVER="pocket-test-unavailable" if self.args.unavailable else "disk",
                   SDL_DISKAUDIODELAY="32", SDL_DISKAUDIOFILE=str(self.pcm))
        self.report["processes"].append({"name": self.name, "command": argv,
                                         "audio_driver": env["SDL_AUDIODRIVER"],
                                         "log": str(self.log_path)})
        try:
            self.proc = subprocess.Popen(argv, env=env, stdout=self.log_file,
                                         stderr=subprocess.STDOUT, start_new_session=True)
            self.wait(lambda: self.sock.exists(), "control socket")
            self.log_match(TRANSPORT.MEDIA)
            self.log_match(r"Pocket: ready app=pxa-" + self.args.app + r"\b")
            geometry = list(map(int, self.log_match(VIEWPORT).groups()))
            self.cw, self.ch, self.x, self.y, self.w, self.h, dw, dh = geometry
            require(geometry == [288, 224, 6, 0, 308, 240, 320, 240],
                    f"Unexpected actual viewport: {geometry}")
            self.report["processes"][-1]["viewport"] = geometry
            return self
        except BaseException:
            self.close()
            raise

    @property
    def pcm(self):
        return self.args.output / f"{self.name}-audio.s16"

    def touch(self, x, y):
        # Sample the center of a logical pixel, using the actual logged viewport.
        px = self.x + min(self.w - 1, int((x + .5) * self.w / self.cw))
        py = self.y + min(self.h - 1, int((y + .5) * self.h / self.ch))
        self.tap(px, py)

    def healthy(self):
        match = FAULTS.search(self.logs())
        require(not match, f"Runtime fault: {match.group() if match else ''}; {self.log_path}")

    def __exit__(self, exc_type, exc, tb):
        super().__exit__(exc_type, exc, tb)
        if exc_type is None:
            self.healthy()


def device(sim):
    sim.log_match(r"Pocket: audio ready 16000 Hz mono")
    fmt, channels, rate = sim.log_match(AUDIO.FORMAT).groups()
    result = {"format": fmt, "channels": int(channels), "rate": int(rate)}
    require(1 <= result["channels"] <= 8 and 8000 <= result["rate"] <= 192000,
            f"Invalid SDL audio device {result}")
    sim.wait(lambda: sim.pcm.exists() and sim.pcm.stat().st_size > 0, "PCM capture")
    return result


def wait_seconds(sim, seconds):
    deadline = time.monotonic() + seconds
    sim.wait(lambda: time.monotonic() >= deadline, f"{seconds:.3f}s elapsed", timeout=seconds + 15)


def wav(path, raw, dev):
    if dev["format"] == "S16BE":
        samples = array("h", raw)
        samples.byteswap()
        raw = samples.tobytes()
    with wave.open(str(path), "wb") as stream:
        stream.setnchannels(dev["channels"])
        stream.setsampwidth(2)
        stream.setframerate(dev["rate"])
        stream.writeframes(raw)


def interval(sim, dev, label, seconds, audible, settle=1.0, continuous=False):
    # Let the Host queue and SDL's buffered disk writer settle before delimiting
    # a fresh sample-counted interval. Capture progress, not a sleep, proves size.
    wait_seconds(sim, settle)
    frame_bytes = dev["channels"] * 2
    start = sim.pcm.stat().st_size // frame_bytes * frame_bytes
    size = math.ceil(seconds * dev["rate"]) * frame_bytes
    began = time.monotonic()
    result = {"app": sim.args.app, "process": sim.name, "label": label, "passed": False,
              "device": dev, "offset_bytes": start, "requested_seconds": seconds,
              "capture_progress": []}
    sim.report.setdefault("pcm_intervals", []).append(result)
    def captured():
        elapsed = time.monotonic() - began
        available = max(0, sim.pcm.stat().st_size - start)
        progress = result["capture_progress"]
        complete = available >= size and elapsed >= seconds
        if not progress or elapsed - progress[-1]["wall_seconds"] >= 1 or complete:
            perf = re.findall(r"Pocket: perf frames=(\d+) period_ms=(\d+) mean_draw_us=(\d+)", sim.logs())
            progress.append({"wall_seconds": round(elapsed, 3),
                             "pcm_seconds": round(available / (dev["rate"] * frame_bytes), 6),
                             "last_perf": list(map(int, perf[-1])) if perf else None})
        return complete
    sim.wait(captured, f"{label} PCM interval", timeout=seconds * 2 + 20)
    result["wall_seconds"] = round(time.monotonic() - began, 3)
    analysis_began = time.monotonic()
    with sim.pcm.open("rb") as stream:
        stream.seek(start)
        raw = stream.read(size)
    metrics = AUDIO.pcm_metrics(raw, dev)
    target = sim.args.output / f"{sim.name}-{label}.wav"
    wav(target, raw, dev)
    result.update(metrics, wav=str(target))
    longest = run = 0
    for value in array("h", raw):
        run = 0 if value else run + 1
        longest = max(longest, run)
    result["longest_exact_zero_seconds"] = longest / (dev["channels"] * dev["rate"])
    if audible:
        require(metrics["peak"] > 200 and metrics["positive_samples"] > 100 and
                metrics["negative_samples"] > 100 and metrics["rms"] > 20,
                f"{label}: music is inaudible: {metrics}")
        require(metrics["peak"] <= 20000, f"{label}: excessive peak {metrics['peak']}")
        # Windows allow the scores' intentional rests. Sustained one-second
        # silence would indicate a missing phrase or stalled audio delivery.
        window = dev["rate"] * frame_bytes
        windows = [AUDIO.pcm_metrics(raw[p:p+window], dev)
                   for p in range(0, len(raw) - window + 1, window)]
        result["one_second_rms"] = [round(value["rms"], 3) for value in windows]
        result["active_windows"] = sum(value["rms"] > 5 for value in windows)
        if continuous:
            require(result["active_windows"] == len(windows),
                    f"{label}: unexpected silent one-second windows: {result['one_second_rms']}")
    else:
        require(metrics["nonzero_samples"] == 0, f"{label}: expected exact silence: {metrics}")
    result["analysis_seconds"] = round(time.monotonic() - analysis_began, 3)
    sim.healthy()
    result["passed"] = True
    return result


def menu(sim, opened):
    after = len(sim.logs())
    if opened:
        sim.touch(276, 10)
    else:
        sim.key("Z")  # Controller A activates the selected Continue row.
    sim.log_match(r"Pocket: menu " + ("opened" if opened else "closed"), after=after)


def help_page(sim, app):
    if app == "lantern-grove":
        sim.key("TAB")  # SELECT
    elif app == "harbor-cafe":
        sim.key("Q")  # L
    # Fresh Tiny and every save in this scenario already have HELP open.
    sim.shot("help")


def controller_toggle(sim, app):
    sim.key("S" if app == "tiny-elements" else "Z")  # Y / A


def touch_toggle(sim):
    sim.touch(210, 195)  # All help footers; crucially below Tiny's logical y=182.


def storage(state):
    candidates = []
    for name in (".pxa-kv-a", ".pxa-kv-b"):
        for pattern in ("app-data/*/" + name, "private-files/*/.pxa-storage/" + name):
            for path in state.glob(pattern):
                try:
                    generation, values = SAVES.host_values(path.read_bytes())
                    candidates.append((generation, values, path))
                except (OSError, AssertionError, ValueError):
                    continue
    return max(candidates, key=lambda value: value[0]) if candidates else None


def sound_save(sim, app, enabled):
    def current():
        snapshot = storage(sim.state)
        if not snapshot:
            return None
        raw, envelope = SAVES.committed_world(snapshot[1], "pxa-" + app + ".save.v1")
        decoded = SAVES.DECODERS[app](raw)
        actual = not decoded["muted"] if app == "tiny-elements" else bool(decoded["sound"])
        if actual != enabled:
            return None
        return {"enabled": actual, "host_generation": snapshot[0], "path": str(snapshot[2]),
                "payload_sha256": hashlib.sha256(raw).hexdigest(), "envelope": envelope,
                "view": decoded["ui"] if app == "tiny-elements" else decoded["view"]}
    return sim.wait(current, "durable sound " + ("on" if enabled else "off"))


def playable(sim, app):
    sim.key("X")  # Controller B dismisses HELP in all three games.
    before = sim.shot("gameplay-before")
    checkpoint = len(sim.logs())
    if app == "lantern-grove":
        sim.touch(48, 183)
        sim.log_match(r"lantern-grove collect\b", after=checkpoint)
    elif app == "harbor-cafe":
        sim.touch(160, 200)  # Normal Supplies page, visible even without a guest.
    else:
        sim.touch(137, 199)  # Water material in the real world toolbar.
        sim.log_match(r"Elements: material selected", after=checkpoint)
    after = sim.shot("gameplay-after")
    changed = TRANSPORT.difference(before, after)
    require(changed["changed_fraction"] > .002, f"Unavailable audio blocked gameplay: {changed}")
    sim.healthy()
    return changed


def run_app(args, temporary, app, report):
    package = args.packages / app / "package" / ("pxa-" + app)
    require(package.is_dir(), f"Missing expanded signed package: {package}")
    output = args.output / app
    output.mkdir(parents=True, exist_ok=True)
    state = Path(tempfile.mkdtemp(prefix="host-state-", dir=output))
    child = SimpleNamespace(**dict(vars(args), package=package, output=output,
                                   app=app, unavailable=False, capture_audio=True))
    result = {"passed": False, "package": str(package), "state_root": str(state),
              "manifest_sha256": hashlib.sha256((package / "manifest.pxm").read_bytes()).hexdigest(),
              "loop_seconds": APPS[app], "intervals": {}}
    report["apps"][app] = result
    cuts = result["intervals"]
    print(f"RUN {app}: signed package, two full loop lengths and audio controls", flush=True)
    with Simulator(child, state, temporary / "music.sock", "music", report) as sim:
        dev = device(sim)
        result["device"] = dev
        sim.shot("ready")
        cuts["two_loops"] = interval(sim, dev, "two-loops", APPS[app] * 2 + 1,
                                     True, continuous=True)
        menu(sim, True)
        sim.shot("menu-paused")
        cuts["menu_silent"] = interval(sim, dev, "menu-silent", args.check_seconds, False)
        menu(sim, False)
        cuts["menu_resume"] = interval(sim, dev, "menu-resume", args.check_seconds, True)
        help_page(sim, app)
        touch_toggle(sim)
        sim.shot("touch-muted")
        cuts["touch_mute"] = interval(sim, dev, "touch-mute", args.check_seconds, False)
        result["touch_mute_save"] = sound_save(sim, app, False)
        controller_toggle(sim, app)
        cuts["controller_enable"] = interval(sim, dev, "controller-enable", args.check_seconds, True)
        controller_toggle(sim, app)
        cuts["controller_mute"] = interval(sim, dev, "controller-mute", args.check_seconds, False)
        result["muted_save"] = sound_save(sim, app, False)
    with Simulator(child, state, temporary / "muted.sock", "muted-reload", report) as sim:
        dev = device(sim)
        sim.log_match(r"Pocket: load completed")
        sim.shot("restored-muted")
        cuts["muted_reload"] = interval(sim, dev, "muted-reload", args.check_seconds, False)
        result["muted_reload_save"] = sound_save(sim, app, False)
        touch_toggle(sim)
        sim.shot("touch-enabled")
        cuts["touch_enable"] = interval(sim, dev, "touch-enable", args.check_seconds, True)
        result["enabled_save"] = sound_save(sim, app, True)
    with Simulator(child, state, temporary / "enabled.sock", "enabled-reload", report) as sim:
        dev = device(sim)
        sim.log_match(r"Pocket: load completed")
        cuts["enabled_reload"] = interval(sim, dev, "enabled-reload", args.check_seconds, True)
        result["enabled_reload_save"] = sound_save(sim, app, True)
    unavailable = SimpleNamespace(**dict(vars(child), unavailable=True))
    with Simulator(unavailable, state, temporary / "unavailable.sock", "unavailable", report) as sim:
        sim.log_match(r"Pocket: optional audio unsupported|Pocket: audio unavailable")
        require("Pocket: audio ready" not in sim.logs(), "Unavailable device unexpectedly opened")
        result["unavailable_audio_gameplay"] = playable(sim, app)
        menu(sim, True)
        sim.shot("unavailable-menu-still-works")
        sim.log_match(r"Pocket: saved")
    for path in output.glob("*-audio.s16"):
        require(path.stat().st_size % 2 == 0, f"Truncated S16 stream: {path}")
    result["passed"] = True
    print(f"PASS {app}: loop PCM, pause/resume, touch/controller mute, durable reload, unavailable audio", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--simulator", type=Path, required=True)
    parser.add_argument("--publisher-key", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--apps", nargs="+", choices=tuple(APPS), default=list(APPS))
    parser.add_argument("--check-seconds", type=float, default=2.0)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    for name in ("packages", "simulator", "publisher_key", "output"):
        setattr(args, name, getattr(args, name).resolve())
    require(args.output.is_relative_to(ARTIFACTS.resolve()), "--output must be under ignored local/artifacts")
    require(len(set(args.apps)) == len(args.apps), "Do not repeat --apps")
    require(math.isfinite(args.check_seconds) and 1 <= args.check_seconds <= 10,
            "--check-seconds must be between 1 and 10")
    require(math.isfinite(args.timeout) and 2 <= args.timeout <= 300,
            "--timeout must be between 2 and 300")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "apps": {}, "processes": [], "screenshots": [],
              "limitations": {"background": "Not exercised: standalone simulator has no focus/lifecycle control command; use physical idle-lock evidence.",
                              "permission_denial": "Not exercised: standalone simulator auto-grants declared permission; SDL device unavailability is tested separately."}}
    began = time.monotonic()
    try:
        with tempfile.TemporaryDirectory(prefix="pocket-music-") as temporary:
            for app in args.apps:
                try:
                    run_app(args, Path(temporary), app, report)
                except Exception:
                    report["apps"].setdefault(app, {"passed": False})["error"] = traceback.format_exc()
                    print(report["apps"][app]["error"], file=sys.stderr, flush=True)
        report["passed"] = all(value["passed"] for value in report["apps"].values())
    except Exception:
        report["error"] = traceback.format_exc()
        print(report["error"], file=sys.stderr)
    finally:
        report["elapsed_seconds"] = round(time.monotonic() - began, 3)
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
