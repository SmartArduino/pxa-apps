#!/usr/bin/env python3
"""Rebuild the signed dictionary from the pinned, licensed upstream word list."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

REVISION = "037169998150a768ff43863fc44d563222d4b801"
URL = "https://github.com/VanillaNahida/lvgl_pinyin_ime.git"
EXPECTED = "179311c55ab9b912a07ef040e5a95af97e704e2248b7b97735f17a3e05d13b67"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, help="Existing clean checkout at the pinned revision")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1]/"assets/dict_pinyin.dat")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="pxa-ime-dictionary-") as temporary:
        source = args.reference
        if source is None:
            source = Path(temporary)/"reference"
            subprocess.run(["git","clone","--no-checkout",URL,str(source)],check=True)
            subprocess.run(["git","-C",str(source),"checkout","--detach",REVISION],check=True)
        revision = subprocess.check_output(["git","-C",str(source),"rev-parse","HEAD"],text=True).strip()
        if revision != REVISION:
            parser.error(f"reference must be at {REVISION}, got {revision}")
        subprocess.run(["python3",str(source/"tools/gen_dict.py"),"--output",str(args.output.resolve())],check=True)
        digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
        print(f"sha256={digest} bytes={args.output.stat().st_size}")
        if EXPECTED and digest != EXPECTED:
            raise SystemExit("Dictionary differs from the committed model; review the generator/toolchain")

if __name__ == "__main__":
    main()
