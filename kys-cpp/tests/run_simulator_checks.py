#!/usr/bin/env python3
"""Run the real PXA product simulator and require every kys-cpp platform check."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

WORKSPACE = Path(__file__).resolve().parents[4]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--simulator", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--publisher-key", type=Path, required=True)
    parser.add_argument("--publisher-root", help="Root key ID when testing a rotated publisher")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(WORKSPACE / "local/artifacts"):
        parser.error("Evidence must be under workspace local/artifacts")
    out.mkdir(parents=True, exist_ok=True)
    publisher = bytes.fromhex(args.publisher_root) if args.publisher_root else hashlib.sha256(args.publisher_key.read_bytes()).digest()
    app = b"pxa-kys-cpp"
    media_key = hashlib.sha256(b"PXA-APPKEY-1" + publisher + len(app).to_bytes(2, "little") + app).hexdigest()
    media = out / "state/media" / media_key
    media.mkdir(parents=True, exist_ok=True)
    (media / "kys-port-media-probe.txt").write_text("kys-cpp media probe\n")
    sequence = out / "sequence.json"
    sequence.write_text(json.dumps([{"key": "Z", "wait": 3, "label": "platform-checks"}]))
    driver = Path(__file__).resolve().parents[2] / "tests/sdl-games/simulator.py"
    run = subprocess.run([sys.executable, str(driver), "--simulator", str(args.simulator),
        "--package", str(args.package), "--publisher-key", str(args.publisher_key),
        "--output", str(out), "--sequence", str(sequence), "--duration", "3"],
        capture_output=True, text=True)
    (out / "driver.log").write_text(run.stdout + run.stderr)
    log = (out / "runtime.log").read_text(errors="replace")
    markers = ["cifa=PASS", "sqlite-persist-utf8=PASS", "zip-roundtrip=PASS",
               "gbk-big5=PASS", "geometry=PASS", "pcm-submit=PASS", "sd-read-at=PASS",
               "KYS PLATFORM CHECKS PASS"]
    report_path = out / "result.json"
    report = json.loads(report_path.read_text())
    report["purpose"] = "kys-cpp platform checks"
    report["gameplay_validated"] = False
    report["checks"] = {m: m in log for m in markers}
    report["passed"] = run.returncode == 0 and all(report["checks"].values())
    report_path.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({"platform_checks_passed": report["passed"], "gameplay_validated": False}))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
