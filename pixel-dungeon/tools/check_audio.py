#!/usr/bin/env python3
"""Check shared compact audio, full decoding, and optional built package copies."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

APP = Path(__file__).resolve().parents[1]


def check(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", action="append", type=Path, default=[],
                        help="unpacked package directory; repeat for each target")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    roots = [APP, APP.parent / "pixel-dungeon-cpp", *args.package]
    music = sorted((APP / "assets/music").glob("*.ogg"))
    effects = sorted((APP / "assets/sfx").glob("*.pcm"))
    check(len(music) == 6 and len(effects) == 23, "missing music or sound effect")
    rows = []
    for original in [*music, *effects]:
        relative = original.relative_to(APP)
        content = original.read_bytes()
        for root in roots:
            check(not list(root.glob("assets-*/music")),
                  f"unexpected target-specific music override: {root}")
            copy = root / relative
            check(copy.is_file() and copy.read_bytes() == content,
                  f"audio differs or is missing: {copy}")
        row = {"path": str(relative), "bytes": len(content),
               "sha256": hashlib.sha256(content).hexdigest()}
        if original.suffix == ".ogg":
            info = json.loads(subprocess.check_output([
                "ffprobe", "-v", "error", "-select_streams", "a:0",
                "-show_entries", "stream=codec_name,channels,duration", "-of", "json",
                str(original)]))["streams"][0]
            check(info["codec_name"] == "opus" and info["channels"] == 2,
                  f"expected stereo Opus: {original}")
            # Decode to the Host's 16 kHz mono format, including the entire tail.
            # One track at a time; this tool is never part of the Guest runtime.
            pcm = subprocess.check_output([
                "ffmpeg", "-nostdin", "-v", "error", "-xerror", "-i", str(original),
                "-map", "0:a:0", "-ac", "1", "-ar", "16000", "-f", "s16le", "-"])
            check(any(pcm), f"silent music: {original}")
            decoded_seconds = len(pcm) / 32000
            check(abs(decoded_seconds - float(info["duration"])) < .02,
                  f"truncated music: {original}")
            row.update(codec="opus", channels=2, decoded_seconds=decoded_seconds,
                       decoded_sha256=hashlib.sha256(pcm).hexdigest())
        rows.append(row)
    report = {"passed": True, "copies_checked": len(roots),
              "package_count": len(args.package), "music_bytes": sum(p.stat().st_size for p in music),
              "effect_bytes": sum(p.stat().st_size for p in effects), "files": rows}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Shared audio passed: 6 complete Opus tracks, 23 PCM effects, {len(roots)} copies")


if __name__ == "__main__":
    main()
