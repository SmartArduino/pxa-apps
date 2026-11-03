#!/usr/bin/env python3
"""Restore user-supplied game resources without adding them to source control."""
import argparse
import hashlib
from pathlib import Path
import zipfile

EXPECTED = {
    "jinyong.bundle": (54975709, "adb7971b6d2314d181b67b2d16b4eb41838b53ab61dc6504220ce198e8aabe0f"),
    "resources.pxi": (64, "3558c37994d0de336f000e42c78cd1a230a39a83498942ad18039f57af427c40"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    destination = Path(__file__).resolve().parent / "assets"
    with zipfile.ZipFile(args.archive) as archive:
        # Validate every input before changing any existing resource.
        contents = {}
        for name, (size, digest) in EXPECTED.items():
            raw = archive.read("output/pxa-jinyong/assets/" + name)
            if len(raw) != size or hashlib.sha256(raw).hexdigest() != digest:
                raise ValueError("Resource checksum mismatch: " + name)
            contents[name] = raw
        destination.mkdir(exist_ok=True)
        for name, raw in contents.items():
            temporary = destination / (name + ".tmp")
            temporary.write_bytes(raw)
            temporary.replace(destination / name)
    print("Restored and verified Jin Yong resources.")


if __name__ == "__main__":
    main()
