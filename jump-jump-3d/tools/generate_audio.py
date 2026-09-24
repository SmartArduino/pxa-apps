#!/usr/bin/env python3
"""Generate the Jump Jump 3D audio bank from the original game's sounds.

The WeChat mini game ships its sounds as `res/*.mp3`. PXA boards expose only a
Guest PCM stream (`pxa_audio_write_pcm`) and no registered asset sink, so the
sounds are decoded here into 8 kHz mono IMA ADPCM nibbles packed into 32-bit
words, and emitted as a C header the Guest mixes itself.

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
import argparse
import hashlib
import pathlib
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

APP_DIR = pathlib.Path(__file__).resolve().parent.parent
OUTPUT = APP_DIR / "jump3d_audio_data.h"

DEFAULT_RATE = 11025
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

IMA_INDEX_TABLE = (-1, -1, -1, -1, 2, 4, 6, 8)
IMA_STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41,
    45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190,
    209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724,
    796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
    2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132,
    7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767,
)


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


def encode_ima(samples: list[int]) -> list[int]:
    nibbles: list[int] = []
    predictor = 0
    index = 0
    for sample in samples:
        step = IMA_STEP_TABLE[index]
        difference = sample - predictor
        nibble = 0
        if difference < 0:
            nibble = 8
            difference = -difference
        delta = step >> 3
        if difference >= step:
            nibble |= 4
            difference -= step
            delta += step
        if difference >= step >> 1:
            nibble |= 2
            difference -= step >> 1
            delta += step >> 1
        if difference >= step >> 2:
            nibble |= 1
            delta += step >> 2
        if nibble & 8:
            delta = -delta
        predictor = max(-32768, min(32767, predictor + delta))
        index = max(0, min(88, index + IMA_INDEX_TABLE[nibble & 7]))
        nibbles.append(nibble)
    return nibbles


def pack(nibbles: list[int]) -> list[int]:
    words = []
    for start in range(0, len(nibbles), 8):
        word = 0
        for offset, nibble in enumerate(nibbles[start:start + 8]):
            word |= (nibble & 0xF) << (offset * 4)
        words.append(word)
    return words


def build_header(source: pathlib.Path | None, cache: pathlib.Path,
                 rate: int) -> str:
    lines = [
        "/* Generated by tools/generate_audio.py - do not edit by hand.",
        " *",
        " * Mono IMA ADPCM, eight 4-bit nibbles per 32-bit word, decoded and",
        " * interpolated to the 16 kHz session rate by jump3d_audio.c.",
        " * Source: the original WeChat Jump Jump mini game audio (res/<name>.mp3),",
        " */",
        "#ifndef JUMP3D_AUDIO_DATA_H",
        "#define JUMP3D_AUDIO_DATA_H",
        "",
        "#include <stdint.h>",
        "",
        "/* The j3_audio_clip_t type lives in jump3d_audio.h. */",
        "",
    ]
    entries = []
    total_bytes = 0
    for name, symbol, looping in SOUNDS:
        path = fetch(name, cache, source)
        samples = decode(path, rate)
        samples, gain = normalise(samples)
        peak, rms = level(samples)
        words = pack(encode_ima(samples))
        print(f"  {name:12s} {len(samples) / rate:5.2f} s gain={gain:4.2f} "
              f"peak={peak:4.2f} rms={rms:5.3f}", file=sys.stderr)
        total_bytes += len(words) * 4
        lines.append(f"/* {name}: {len(samples) / rate:.2f} s, "
                     f"{len(words) * 4} bytes */")
        lines.append(f"static const uint32_t j3_audio_{symbol}_words"
                     f"[{max(1, len(words))}u] = {{")
        for start in range(0, len(words), 8):
            chunk = words[start:start + 8]
            lines.append("    " + ",".join(f"0x{word:08x}u" for word in chunk) + ",")
        lines.append("};")
        lines.append("")
        entries.append((symbol, len(samples), looping))
    lines.append("/* X-macro form of the bank, used to pin the Guest clip enum. */")
    lines.append("#define J3_AUDIO_BANK(X) \\")
    for index, (symbol, samples, looping) in enumerate(entries):
        lines.append(f"    X({symbol.upper()}, {index}u, {samples}u, {1 if looping else 0}u) \\")
    lines.append("")
    lines.append("static const j3_audio_clip_t j3_audio_bank[] = {")
    for symbol, samples, looping in entries:
        lines.append(f"    {{j3_audio_{symbol}_words, {samples}u, {1 if looping else 0}u}},")
    lines.append("};")
    lines.append("")
    lines.append(f"#define J3_AUDIO_CLIP_COUNT {len(entries)}u")
    lines.append(f"#define J3_AUDIO_CLIP_RATE_HZ {rate}u")
    lines.append("")
    lines.append("#endif")
    lines.append("")
    header = "\n".join(lines)
    print(f"audio bank: {total_bytes / 1024:.0f} KiB of ADPCM", file=sys.stderr)
    return header


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
    content = build_header(arguments.source, arguments.cache, arguments.rate)
    if arguments.check:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != content:
            print(f"stale: {OUTPUT}", file=sys.stderr)
            return 1
        print(f"up to date: {OUTPUT}")
        return 0
    OUTPUT.write_text(content, encoding="utf-8")
    digest = hashlib.sha256(content.encode("utf-8")).hexdigest()[:16]
    print(f"wrote {OUTPUT} ({len(content)} bytes, sha256:{digest})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
