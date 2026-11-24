#!/usr/bin/env python3
"""Master the 16 kHz mono game assets from the original FLAC renders."""

import argparse
import json
from pathlib import Path
import subprocess

import numpy as np


APP = Path(__file__).resolve().parents[1]
RATE = 16000  # The shared PXA music and prepared-sound bus runs at 16 kHz.
VORBIS_QUALITY = 7


def read_master(path, filters):
    # Filter the original before the final band-limited, mono resample.
    filters.append(f"aresample={RATE}:resampler=soxr:precision=28:cutoff=0.95:ochl=mono")
    pcm = subprocess.check_output([
        "ffmpeg", "-v", "error", "-i", str(path), "-af", ",".join(filters), "-f", "f32le", "-"
    ])
    wave = np.frombuffer(pcm, dtype="<f4")
    if not len(wave) or not np.isfinite(wave).all():
        raise ValueError(f"Invalid master: {path}")
    return wave


def level(wave, peak, rms):
    scale = min(peak / np.max(np.abs(wave)), rms / np.sqrt(np.mean(wave**2)))
    return (wave * scale).astype("<f4")


def master_loop(source, output, cue):
    wave = read_master(source, ["highpass=f=40"])
    bar_samples = 4 * 60 * RATE / cue["bpm"]
    size = round(cue["bars"] * bar_samples)
    fade = round(bar_samples)
    start = round(cue["start"] * RATE)
    seam = start + fade
    nearby = np.arange(seam - 160, seam + 161)
    # Prefer a low-amplitude join within 10 ms of the exact bar line.
    join_cost = np.abs(wave[nearby] - wave[nearby-1]) + 0.08 * (
        np.abs(wave[nearby]) + np.abs(wave[nearby-1]))
    start = int(nearby[join_cost.argmin()]) - fade
    part = wave[start:start+size+fade]
    if len(part) != size + fade:
        raise ValueError(f"Master too short for loop: {source}")
    taper = (1 - np.cos(np.linspace(0, np.pi, fade))) / 2
    loop = np.concatenate([part[fade:size], part[size:] * (1-taper) + part[:fade] * taper])
    loop = level(loop, peak=0.76, rms=0.19)
    subprocess.run([
        "ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ac", "1", "-ar", str(RATE), "-i", "-",
        "-c:a", "libvorbis", "-q:a", str(VORBIS_QUALITY), str(output)
    ], input=loop.tobytes(), check=True)


def master_clip(source, output, duration=None):
    voice = duration is None
    filters = ["highpass=f=82", "equalizer=f=2200:t=o:w=1.6:g=1.5"] if voice else ["highpass=f=48"]
    wave = read_master(source, filters)
    hop = RATE // 100
    windows = wave[:len(wave)//hop*hop].reshape(-1, hop)
    rms = np.sqrt(np.mean(windows**2, axis=1))
    active = np.flatnonzero(rms > max(0.0025, rms.max() * (0.035 if voice else 0.072)))
    if not len(active):
        raise ValueError(f"Silent clip master: {source}")
    start = max(0, int(active[0])*hop - 96)
    end = min(len(wave), (int(active[-1])+1)*hop + 480)
    if not voice:
        end = min(len(wave), max(end, start + round(0.18*RATE)), start + round(duration*RATE))
    clip = wave[start:end].copy()
    if not 0.06 < len(clip) / RATE < 2.1:
        raise ValueError(f"Unexpected clip duration: {source}")
    attack = 48 if voice else 16
    release = min(len(clip)//5, 560 if voice else 720)
    clip[:attack] *= np.linspace(0, 1, attack)
    clip[-release:] *= np.linspace(1, 0, release)
    clip = level(clip, peak=0.79 if voice else 0.75, rms=0.25 if voice else 0.23)
    output.write_bytes(np.rint(32767 * clip).astype("<i2").tobytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("masters", type=Path, help="Directory of original FLAC files")
    parser.add_argument("--output", type=Path, default=APP / "assets/audio")
    args = parser.parse_args()
    cues = json.loads((APP / "tools/audio-cues.json").read_text())
    args.output.mkdir(parents=True, exist_ok=True)
    for name, cue in cues["music"].items():
        master_loop(args.masters / f"{name}.flac", args.output / f"{name}.ogg", cue)
    for name, cue in cues["effects"].items():
        master_clip(args.masters / f"{name}.flac", args.output / f"{name}.s16", cue["duration"])
    for name in cues["voices"]["lines"]:
        master_clip(args.masters / f"voice-{name}.flac", args.output / f"voice-{name}.s16")


if __name__ == "__main__":
    main()
