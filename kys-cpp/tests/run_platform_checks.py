#!/usr/bin/env python3
"""Check the installed app's resource screen and platform backends over PXADB."""
import argparse
import json
import os
from pathlib import Path
import sys
import time

WORKSPACE = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(WORKSPACE / "tools/pxadb"))
import pxadb


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True, help="Explicit serial path or unix: PXADB endpoint")
    parser.add_argument("--baud", type=int, default=2000000)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--media", help="Verified PXADB media directory for a volume-local installation")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(WORKSPACE / "local/artifacts"):
        parser.error("Evidence must be under the workspace local/artifacts directory")
    output.mkdir(parents=True, exist_ok=True)
    os.environ["PXADB_BAUD"] = str(args.baud)
    seen = []
    report = {"endpoint": args.endpoint, "baud": args.baud, "gameplay_validated": False, "passed": False}
    started = time.monotonic()
    with (output / "pxadb.jsonl").open("w", buffering=1) as log:
        def record(kind, payload):
            log.write(json.dumps({"seconds": time.monotonic()-started, "kind": kind, "payload": payload}, ensure_ascii=False)+"\n")
            seen.append(payload)

        with pxadb.PxaDbClient(args.endpoint, timeout=15) as client:
            client.log_callback = lambda f: record(f.kind, f.payload)
            client.raw_callback = lambda b: record("RAW", b.decode("utf-8", "replace"))

            def request(command):
                record("COMMAND", command)
                result = client.request(command, timeout=30)
                for frame in result:
                    if not command.startswith("SCREENSHOT") or frame.kind not in ("DATA",):
                        record(frame.kind, frame.payload)
                return result

            def pump(seconds):
                end = time.monotonic()+seconds
                while time.monotonic()<end:
                    client.pump_logs()
                    time.sleep(.02)

            report["hello"] = client.hello()
            client.subscribe_logs()
            memory_command = "MEMORY" if "memory" in pxadb.device_capabilities(client.device_info) else "INFO"
            report["before"] = [{"kind": f.kind, "payload": f.payload} for f in request(memory_command)]
            identities = [f.payload.split("\t", 1)[0] for f in request("PACKAGES") if f.kind == "PKG"]
            identity = next(i for i in identities if i.endswith(":pxa-kys-cpp"))
            if args.media:
                media = args.media
                if not media.endswith("/pxa-kys-cpp") or "/PXA/media/" not in media:
                    parser.error("--media must name this app's verified PXA media directory")
            else:
                paths = request("PACKAGE paths " + identity)
                media = next(f.payload.split("\t", 1)[1] for f in paths
                             if f.kind == "PATH" and f.payload.startswith("media\t"))
            fs = pxadb.NormalFsClient(client)
            parts = Path(media).parts
            for length in range(1, len(parts)+1):
                directory = "/".join(parts[:length])
                try:
                    fs.list(directory)
                except pxadb.PxaDbError:
                    fs.mkdir(directory)
            fixture = output / "media-probe.txt"
            fixture.write_text("kys-cpp media probe\n")
            remote_probe = media + "/kys-port-media-probe.txt"
            fs.put(fixture, remote_probe)
            report["media"] = media
            # This app declares only optional audio.playback. The check needs it.
            request("PERMISSIONS pxa-kys-cpp 0 allow")
            request("PACKAGE run pxa-kys-cpp")
            launch_mark = len(seen)
            deadline = time.monotonic()+90
            while not any("KYS resource check: incomplete" in line for line in seen):
                failures = [line for line in seen[launch_mark:] if any(marker in line for marker in
                    ("Package launch failed at", "instantiate failed:", "PXA Guest exception"))]
                if failures:
                    report["error"] = failures
                    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n")
                    raise RuntimeError("App launch failed; inspect report.json and pxadb.jsonl")
                if time.monotonic() >= deadline:
                    raise RuntimeError("App did not reach its resource screen; inspect pxadb.jsonl")
                pump(.25)
            shot = pxadb.screenshot_capture(request("SCREENSHOT"))
            pxadb.write_screenshot(shot, output / "before.png")
            report["screen"] = shot.metadata
            request("INPUT CONTROLLER 0 1 0")
            pump(.15)
            request("INPUT CONTROLLER 0 1 16")  # PXA_CONTROLLER_A
            pump(.15)
            request("INPUT CONTROLLER 0 1 0")
            pump(10)
            shot = pxadb.screenshot_capture(request("SCREENSHOT"))
            pxadb.write_screenshot(shot, output / "after.png")
            report["after"] = [{"kind": f.kind, "payload": f.payload} for f in request(memory_command)]
            # The first touch after controller use reveals the overlay and is
            # consumed by the shared frontend; the second reaches SDL mouse.
            request("INPUT TAP 160 130")
            pump(.3)
            request("INPUT TAP 160 130")
            pump(.3)
            report["touch_injected"] = any("KYS input event type=1026" in line for line in seen)
            request("INPUT CONTROLLER 0 1 32")  # PXA_CONTROLLER_B
            pump(.15)
            request("INPUT CONTROLLER 0 1 0")
            pump(3)
            markers = ["cifa=PASS", "sqlite-persist-utf8=PASS", "zip-roundtrip=PASS",
                       "gbk-big5=PASS", "geometry=PASS", "pcm-submit=PASS", "sd-read-at=PASS",
                       "KYS PLATFORM CHECKS PASS"]
            report["checks"] = {m: any(m in line for line in seen) for m in markers}
            report["back_exited"] = any("PXA SDL game returned status=0" in line for line in seen)
            report["passed"] = all(report["checks"].values()) and report["touch_injected"] and report["back_exited"]
            fs.remove(remote_probe)
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n")
    print(json.dumps({"platform_checks_passed": report["passed"], "gameplay_validated": False}, ensure_ascii=False))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
