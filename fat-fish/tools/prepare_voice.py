#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Download the pinned local speech models and verify their SHA-256 digests."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from urllib.request import urlopen
import zipfile


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models-dir", type=Path,
                        default=Path.home() / ".local/share/pxa-fat-fish/models")
    parser.add_argument("--tts-model", choices=("float", "int8"), default="float")
    args = parser.parse_args()
    root = args.models_dir.expanduser().resolve()
    manifest = json.loads(Path(__file__).with_name("voice-models.json").read_text())
    for entry in manifest["files"]:
        if entry.get("variant", args.tts_model) != args.tts_model:
            continue
        target = root / entry["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.is_file() or digest(target) != entry["sha256"]:
            partial = target.with_name(target.name + ".partial")
            try:
                with urlopen(entry["url"], timeout=120) as response, partial.open("wb") as output:
                    shutil.copyfileobj(response, output)
                if partial.stat().st_size != entry["bytes"] or digest(partial) != entry["sha256"]:
                    raise ValueError("Model checksum mismatch for " + entry["path"])
                partial.replace(target)
            finally:
                partial.unlink(missing_ok=True)
        print(entry["path"], flush=True)
    archive = root / "vosk-model-small-cn-0.22.zip"
    with zipfile.ZipFile(archive) as source:
        for item in source.infolist():
            if not (root / item.filename).resolve().is_relative_to(root):
                raise ValueError("Unsafe model archive path")
        source.extractall(root)


if __name__ == "__main__":
    main()
