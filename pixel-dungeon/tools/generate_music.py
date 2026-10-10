#!/usr/bin/env python3
"""Generate shared, full-length Opus music directly from the upstream originals."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

APP = Path(__file__).resolve().parents[1]
TRACKS = ("theme", "sewers", "prison", "caves", "city", "halls")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--spd-assets", default=os.environ.get("PXA_SPD_ASSETS"))
    parser.add_argument("--output-root", type=Path, default=APP.parent,
                        help="parent of pixel-dungeon and pixel-dungeon-cpp")
    args = parser.parse_args()
    if not args.spd_assets:
        parser.error("pass --spd-assets /path/to/shattered-pixel-dungeon")
    source = Path(args.spd_assets) / "core/src/main/assets/music"
    # Validate every input before replacing anything. Never recompress the
    # already lossy Opus files shipped in assets/music.
    for track in TRACKS:
        original = source / f"{track}_1.ogg"
        if not original.is_file():
            parser.error(f"missing original music: {original}")
        info = json.loads(subprocess.check_output([
            "ffprobe", "-v", "error", "-select_streams", "a:0",
            "-show_entries", "stream=codec_name", "-of", "json", str(original)]))
        if not info["streams"] or info["streams"][0]["codec_name"] != "vorbis":
            parser.error(f"use the upstream Vorbis original, not an Opus copy: {original}")
    with tempfile.TemporaryDirectory(prefix="pixel-dungeon-music-") as temporary:
        output = Path(temporary)
        for track in TRACKS:
            filename = f"{track}_1.ogg"
            subprocess.run([
                "ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-y",
                "-i", str(source / filename), "-map", "0:a:0", "-vn", "-sn", "-dn",
                "-c:a", "libopus", "-b:a", "24k", "-vbr", "on", "-ac", "2",
                str(output / filename)], check=True)
        for app in ("pixel-dungeon", "pixel-dungeon-cpp"):
            destination = args.output_root / app / "assets/music"
            destination.mkdir(parents=True, exist_ok=True)
            for track in TRACKS:
                filename = f"{track}_1.ogg"
                shutil.copyfile(output / filename, destination / filename)
    print("Both games: six full-length stereo Opus tracks, 24 kbps VBR, all targets")


if __name__ == "__main__":
    main()
