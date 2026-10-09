#!/usr/bin/env python3
"""Generate the Jump Jump 3D audio bank from the original game's sounds.

The Guest sends playback commands to the Host mixer. This tool writes native
16 kHz mono signed PCM effects (.s16), streamed Vorbis background music (.ogg),
and a small metadata header; no ADPCM decoder or PCM pump runs in the Guest.

Sources (fetched into a cache directory, or copied from a local checkout of the
reference mini game):

    scale_intro  charge start swell      scale_loop  charge sustain loop
    success      landing                  perfect     (unused, kept for parity)
    combo1..8    consecutive centre hits  pop         new block appears
    fall/fall_2  miss / tip over          start       restart
    sing         music box melody         store       convenience store jingle
    water        manhole splash           icon        background music loop

Usage:
    python3 tools/generate_audio.py                    # writes jump3d_audio_data.h
    python3 tools/generate_audio.py --check            # verifies it is current
    python3 tools/generate_audio.py --source <dir>     # use a local res/ copy
"""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

APP_DIR = pathlib.Path(__file__).resolve().parent.parent
OUTPUT = APP_DIR / "jump3d_audio_data.h"

DEFAULT_RATE = 16000
SILENCE_DB = -45.0
TAIL_MS = 120
RAW_URL = ("https://raw.githubusercontent.com/yaoshanliang/weapp-jump/"
           "master/res/{name}.mp3")
DEFAULT_CACHE = pathlib.Path(tempfile.gettempdir()) / "pxa-jump3d-audio-cache"

# Loudness targets. The original mp3s are mixed for a phone audio stack and
# differ by up to 26 dB between clips (the charge sustain is almost inaudible
# under the charge swell), so each clip is levelled to a common RMS with a peak
# ceiling instead of being played back at its authored level.
TARGET_RMS = 0.16
PEAK_CEILING = 0.55
MIN_GAIN = 0.5
MAX_GAIN = 8.0
# Everything below this is inaudible on the small panel speaker and only eats
# headroom, so the decode runs through a high pass.
HIGH_PASS_HZ = 110

# name, symbol, looping
SOUNDS = [
    ("scale_intro", "scale_intro", False),
    ("scale_loop", "scale_loop", True),
    ("success", "success", False),
    ("pop", "pop", False),
    ("combo1", "combo1", False),
    ("combo2", "combo2", False),
    ("combo3", "combo3", False),
    ("combo4", "combo4", False),
    ("combo5", "combo5", False),
    ("combo6", "combo6", False),
    ("combo7", "combo7", False),
    ("combo8", "combo8", False),
    ("fall", "fall", False),
    ("fall_2", "fall_2", False),
    ("start", "start", False),
    ("sing", "sing", False),
    ("store", "store", False),
    ("water", "water", False),
    ("icon", "icon", True),
]


def fetch(name: str, cache: pathlib.Path, source: pathlib.Path | None) -> pathlib.Path:
    path = cache / f"{name}.mp3"
    if source is not None:
        candidate = source / f"{name}.mp3"
        if not candidate.is_file():
            raise SystemExit(f"missing source audio: {candidate}")
        return candidate
    if path.is_file() and path.stat().st_size > 0:
        return path
    cache.mkdir(parents=True, exist_ok=True)
    url = RAW_URL.format(name=name)
    try:
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read()
    except (urllib.error.URLError, TimeoutError) as error:
        raise SystemExit(
            f"cannot download {url} ({error}); pass --source <res dir> or "
            f"place {name}.mp3 in {cache}"
        ) from error
    path.write_bytes(data)
    return path


def decode(path: pathlib.Path, rate: int) -> list[int]:
    """Decodes to mono int16 at `rate`, trimmed of leading/trailing silence."""
    decoded = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", str(path), "-ac", "1",
         "-ar", str(rate), "-af", f"highpass=f={HIGH_PASS_HZ}",
         "-f", "s16le", "-"],
        check=True, capture_output=True).stdout
    samples = [
        int.from_bytes(decoded[index:index + 2], "little", signed=True)
        for index in range(0, len(decoded) - 1, 2)
    ]
    window = rate // 50  # 20 ms
    threshold = 10.0 ** (SILENCE_DB / 20.0) * 32768.0
    last = 0
    first = 0
    for start in range(0, len(samples) - window, window):
        chunk = samples[start:start + window]
        energy = sum(value * value for value in chunk) / len(chunk)
        if energy ** 0.5 > threshold:
            last = start + window
            if first == 0:
                first = start
    if last == 0:
        return samples
    keep = last + (TAIL_MS * rate) // 1000
    if keep > len(samples):
        keep = len(samples)
    start = max(0, first - (10 * rate) // 1000)
    return samples[start:keep]


def level(samples: list[int]) -> tuple[float, float]:
    peak = max((abs(value) for value in samples), default=0) / 32768.0
    total = sum(value * value for value in samples)
    rms = (total / len(samples)) ** 0.5 / 32768.0 if samples else 0.0
    return peak, rms


def normalise(samples: list[int]) -> tuple[list[int], float]:
    """Brings a clip to the common RMS while keeping its peak under the
    ceiling; returns the scaled samples and the gain that was applied."""
    peak, rms = level(samples)
    if rms <= 0.0 or peak <= 0.0:
        return samples, 1.0
    gain = TARGET_RMS / rms
    gain = min(gain, PEAK_CEILING / peak)
    gain = max(MIN_GAIN, min(MAX_GAIN, gain))
    scaled = []
    for value in samples:
        result = int(value * gain)
        if result > 32767:
            result = 32767
        elif result < -32768:
            result = -32768
        scaled.append(result)
    return scaled, gain


def build_assets(source: pathlib.Path | None, cache: pathlib.Path, rate: int) -> dict[pathlib.Path, bytes]:
    """Resident 16-bit effects and streamed Vorbis music, all at native 16 kHz."""
    import struct
    output = {}
    entries = []
    for name, symbol, looping in SOUNDS:
        samples, gain = normalise(decode(fetch(name, cache, source), rate))
        # Remove steps at the wrap point of the charge loop; one-shots are
        # faded by the Host mixer. Keep the original duration and pitch.
        if looping:
            fade = min(64, len(samples)//2)
            for i in range(fade):
                samples[i] = samples[i] * i // fade
                samples[-1-i] = samples[-1-i] * i // fade
        pcm = struct.pack("<" + "h" * len(samples), *samples)
        if name == "icon":
            encoded = subprocess.run(["ffmpeg", "-v", "error", "-f", "s16le",
                "-ar", str(rate), "-ac", "1", "-i", "pipe:0", "-c:a", "libvorbis",
                "-q:a", "3", "-f", "ogg", "pipe:1"], input=pcm, capture_output=True, check=True).stdout
            path = "assets/audio/icon.ogg"
            output[APP_DIR / path] = encoded
        else:
            path = f"assets/audio/{name}.s16"
            output[APP_DIR / path] = pcm
        entries.append((symbol, len(samples), looping, path))
        print(f"{name:12s} {len(samples)/rate:5.2f} s {len(output[APP_DIR/path]):7d} bytes gain={gain:.2f}", file=sys.stderr)
    lines = ["/* Generated by tools/generate_audio.py. Host owns PCM playback. */",
        "#ifndef JUMP3D_AUDIO_DATA_H", "#define JUMP3D_AUDIO_DATA_H", "",
        "#define J3_AUDIO_BANK(X) \\"]
    for i, (symbol, count, loop, path) in enumerate(entries):
        lines.append(f"    X({symbol.upper()}, {i}u, {count}u, {int(loop)}u) \\")
    lines += ["", "static const j3_audio_clip_t j3_audio_bank[] = {"]
    for symbol, count, loop, path in entries:
        lines.append(f'    {{"{path}", {count}u, {int(loop)}u}},')
    lines += ["};", "", f"#define J3_AUDIO_CLIP_COUNT {len(entries)}u",
        f"#define J3_AUDIO_CLIP_RATE_HZ {rate}u", "", "#endif", ""]
    output[OUTPUT] = "\n".join(lines).encode()
    return output

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="exit non-zero when the generated header is stale")
    parser.add_argument("--source", type=pathlib.Path,
                        help="directory holding the original res/*.mp3 files")
    parser.add_argument("--cache", type=pathlib.Path, default=DEFAULT_CACHE,
                        help=f"download cache (default {DEFAULT_CACHE})")
    parser.add_argument("--rate", type=int, default=DEFAULT_RATE,
                        help=f"source rate in Hz (default {DEFAULT_RATE}; the "
                             "mixer interpolates to the 16 kHz session rate)")
    arguments = parser.parse_args()
    if shutil.which("ffmpeg") is None:
        raise SystemExit("ffmpeg is required to decode the source mp3 files")
    if arguments.rate != 16000:
        raise SystemExit("Host prepared sounds require exactly 16000 Hz")
    assets = build_assets(arguments.source, arguments.cache, arguments.rate)
    if arguments.check:
        # Vorbis serial numbers are random; compare decoded music, not Ogg bytes.
        for path, content in assets.items():
            if path.suffix == ".ogg":
                if not path.is_file(): return 1
                decode_ogg = lambda data: subprocess.run(["ffmpeg", "-v", "error", "-i", "pipe:0",
                    "-f", "s16le", "pipe:1"], input=data, capture_output=True, check=True).stdout
                equal = decode_ogg(path.read_bytes()) == decode_ogg(content)
            else:
                equal = path.is_file() and path.read_bytes() == content
            if not equal:
                print(f"stale: {path}", file=sys.stderr)
                return 1
        return 0
    for path, content in assets.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    print(f"wrote {len(assets)} Host audio assets/metadata")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
