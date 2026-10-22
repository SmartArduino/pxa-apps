#!/usr/bin/env python3
"""Measure live EMU simulation and display completions; never optical panel Hz."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import time


def measure(client, output, seconds=22, motion=True, expected_crc=None, app_id="pxa-master-system", target_hz=None, buttons=None):
    import pxadb
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    report = {"samples": [], "surface": [], "scope": "EMU completed emulation frames and firmware display completions; not optical refresh"}
    report["app_id"] = app_id
    report["expected_rom_crc"] = expected_crc
    report["input"] = {"buttons":buttons,"motion":motion if buttons is None else False}
    prefix={"pxa-game-boy":"GB","pxa-nes":"NES"}.get(app_id,"EMU")
    rom_counters={}
    seen = set()
    collect = False
    cutoff = 0
    window_end = 0
    previous = client.log_callback, client.raw_callback
    with (output / "serial.log").open("w", buffering=1) as log:
        def record(line):
            nonlocal window_end,rom_counters
            log.write(line.rstrip()+"\n")
            line = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", line)
            if "[PXA app=" in line and f"[PXA app={app_id} " not in line:
                return
            if not collect:
                return
            if f"{prefix} ROM " in line:
                rom_counters=dict(re.findall(r"([a-z0-9_]+)=([^\s]+)",line.split(f"{prefix} ROM ",1)[1]))
                return
            stats_label=f"{prefix} STATS " if prefix!="NES" else "NES SCHED "
            if stats_label in line:
                row = dict(re.findall(r"([a-z0-9_]+)=([^\s]+)", line.split(stats_label,1)[1]))
                for sample in report["samples"]:
                    if sample["wall_us"] == row.get("wall_us"):
                        sample.update(row)
                return
            for label, field in [(f"{prefix} PERF ","samples"),(f"{prefix} DISPLAY ","surface")]:
                if label not in line:
                    continue
                row = dict(re.findall(r"([a-z0-9_]+)=([^\s]+)", line.split(label, 1)[1]))
                stamp = int(row.get("wall_us", "0"))
                if label == f"{prefix} PERF ":
                    if prefix=="NES":
                        for key in ("rate_num","rate_den"):
                            if key in rom_counters:row.setdefault(key,rom_counters[key])
                        if "crc" in rom_counters:row.setdefault("rom_crc",rom_counters["crc"])
                    if expected_crc is not None and row.get('rom_crc','').lower()!=expected_crc.lower():
                        raise RuntimeError(f"ROM mismatch: expected {expected_crc}, got {row.get('rom_crc')}")
                    row.update({f"rom_{key}":value for key,value in rom_counters.items()})
                    if "debt" not in row and "debt_frames" in row:row["debt"]=row["debt_frames"]
                    # Firmware input ACK and Guest CLOCK_MONOTONIC may have
                    # different epochs. Drop the first observed window and
                    # use subsequent complete windows in the Guest's domain.
                    if not window_end:
                        window_end=stamp
                        report["warmup_window"]=row
                        return
                    if stamp-int(row.get("elapsed_us", "0")) < window_end or stamp in seen:
                        return
                    seen.add(stamp)
                    assert int(row["sim_x100"]) == int(row["frames"])*100000000//int(row["elapsed_us"])
                    print(row, flush=True)
                report[field].append(row)
        client.log_callback = lambda frame: record(frame.payload)
        client.raw_callback = lambda data: record(data.decode("utf-8", "replace"))
        try:
            report["packages"] = [f.payload for f in client.request("PACKAGES") if f.kind == "PKG"]
            if not any(f":{app_id}\t" in p and "active=1" in p for p in report["packages"]):
                raise RuntimeError("Start an emulator and load a ROM before measuring")
            client.subscribe_logs()
            for f in client.request("INPUT CONTROLLER 0 1 "+str(buttons if buttons is not None else 8 if motion else 0)):
                props = pxadb.info_properties(f.payload)
                if props.get("status") == "rejected":
                    raise RuntimeError("Controller injection rejected; release physical input before measuring")
                cutoff = int(props.get("device_us", cutoff))
            report["input_ack_device_us"]=cutoff
            try:
                report["display_start"] = [f.payload for f in client.request("PERF START display")]
            except pxadb.PxaDbError as error:
                report["display_error"] = str(error)
            collect = True
            began = time.monotonic()
            next_input = 0
            while time.monotonic()-began < seconds:
                client.pump_logs()
                elapsed = time.monotonic()-began
                if elapsed >= next_input:
                    bits = buttons if buttons is not None else (8 | (16 if elapsed % 1.05 < .22 else 0)) if motion else 0
                    client._write_command(client._next_sequence(), f"INPUT CONTROLLER 0 1 {bits}")
                    next_input = elapsed+.12
                time.sleep(.005)
            collect = False
            if "display_start" in report:
                report["display_stop"] = [f.payload for f in client.request("PERF STOP")]
                props = pxadb.info_properties(";".join(report["display_stop"]))
                if int(props.get("window_us", "0")):
                    report["display_fps"] = int(props["completed"])*1000000/int(props["window_us"])
                client.request("PERF CLEAR")
            report["memory"] = [f.payload for f in client.request("MEMORY")]
        finally:
            try:
                client.request("INPUT CANCEL", timeout=5)
                client.unsubscribe_logs(timeout=10)
            finally:
                client.log_callback, client.raw_callback = previous
    report["valid"] = len(report["samples"]) >= 2
    report["sustained"] = len(report["samples"]) >= 2 and all(
        abs(int(r["sim_x100"])-100*(target_hz if target_hz is not None else int(r["rate_num"])/int(r["rate_den"]))) <= 100 and int(r["debt"]) <= 2 for r in report["samples"])
    report["audio_enabled"] = bool(report["samples"]) and all(r.get("sound") == "1" for r in report["samples"])
    (output / "results.json").write_text(json.dumps(report, indent=2)+"\n")
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--buttons", type=lambda s:int(s,0), help="Optional constant controller mask, 0..4095; default moves Right and pulses A")
    p.add_argument("--app-id", required=True)
    p.add_argument("--target-hz", type=float, help="Optional override for older builds without rate metadata")
    p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=2000000)
    p.add_argument("--serial", required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--seconds", type=float, default=22)
    p.add_argument("--rom-crc", help="Require this cartridge CRC32 in every sampled window")
    p.add_argument("--pxadb-dir", type=Path, default=Path(__file__).resolve().parents[5]/"tools/pxadb")
    args = p.parse_args()
    if args.buttons is not None and not 0<=args.buttons<=4095:p.error("--buttons must be 0..4095")
    sys.path.insert(0, str(args.pxadb_dir))
    import pxadb
    os.environ["PXADB_BAUD"] = str(args.baud)
    with pxadb.PxaDbClient(args.port, 15) as client:
        assert pxadb.info_properties(client.hello()).get("serial") == args.serial
        report = measure(client, args.output, args.seconds,expected_crc=args.rom_crc,app_id=args.app_id,target_hz=args.target_hz,buttons=args.buttons)
    print(json.dumps({"sustained": report["sustained"], "display_fps": report.get("display_fps"), "audio_enabled": report["audio_enabled"]}))


if __name__ == "__main__":
    main()
