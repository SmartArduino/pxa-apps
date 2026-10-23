#!/usr/bin/env python3
"""Measure live EMU simulation and display completions; never optical panel Hz."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import time


def simulation_target(app_id, row, override=None):
    if override is not None:
        return override
    if int(row.get("rate_num", 0)) > 0 and int(row.get("rate_den", 0)) > 0:
        return int(row["rate_num"]) / int(row["rate_den"])
    if app_id == "pxa-game-boy":
        return 4194304 / 70224
    return None


def display_target_met(report, minimum_fps):
    """Require both game submissions and LCD completions above the threshold.

    System overlays may add LCD completions. Never use those alone as evidence
    that the game is faster. This is independent of full-speed CPU emulation.
    """
    rows = report.get("samples", [])
    return (len(rows) >= 2 and report.get("display_fps", 0) > minimum_fps and
            all(w["fps"] > minimum_fps for w in report.get("display_windows", []) if w["complete"]) and
            all(int(row.get("submit_x100", 0)) > minimum_fps * 100 for row in rows))


def measure(client, output, seconds=22, motion=True, expected_crc=None, app_id="pxa-master-system", target_hz=None, buttons=None, display_window_seconds=0):
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
            next_display = display_window_seconds
            def finish_display_window():
                report["display_stop"] = [f.payload for f in client.request("PERF STOP")]
                props = pxadb.info_properties(";".join(report["display_stop"]))
                window_us = int(props.get("window_us", "0"))
                if window_us:
                    completed = int(props["completed"])
                    report.setdefault("display_windows", []).append({
                        "completed": completed, "window_us": window_us,
                        "fps": completed * 1000000 / window_us,
                        "complete": not display_window_seconds or window_us >= display_window_seconds * 800000})
                client.request("PERF CLEAR")
            while time.monotonic()-began < seconds:
                client.pump_logs()
                elapsed = time.monotonic()-began
                if elapsed >= next_input:
                    bits = buttons if buttons is not None else (8 | (16 if elapsed % 1.05 < .22 else 0)) if motion else 0
                    client._write_command(client._next_sequence(), f"INPUT CONTROLLER 0 1 {bits}")
                    next_input = elapsed+.12
                if display_window_seconds and "display_start" in report and elapsed >= next_display:
                    finish_display_window()
                    client.request("PERF START display")
                    next_display = time.monotonic()-began+display_window_seconds
                time.sleep(.005)
            collect = False
            if "display_start" in report:
                finish_display_window()
                windows=report.get("display_windows", [])
                if windows:
                    report["display_fps"] = sum(w["completed"] for w in windows)*1000000/sum(w["window_us"] for w in windows)
            report["memory"] = [f.payload for f in client.request("MEMORY")]
        finally:
            try:
                client.request("INPUT CANCEL", timeout=5)
                client.unsubscribe_logs(timeout=10)
            finally:
                client.log_callback, client.raw_callback = previous
    report["valid"] = len(report["samples"]) >= 2
    report["sustained"] = len(report["samples"]) >= 2 and all(
        (target := simulation_target(app_id, r, target_hz)) is not None and
        abs(int(r["sim_x100"])-100*target) <= 100 and int(r["debt"]) <= 2
        for r in report["samples"])
    report["audio_enabled"] = bool(report["samples"]) and all(r.get("sound") == "1" for r in report["samples"])
    (output / "results.json").write_text(json.dumps(report, indent=2)+"\n")
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--buttons", type=lambda s:int(s,0), help="Optional constant controller mask, 0..4095; default moves Right and pulses A")
    p.add_argument("--app-id", required=True)
    p.add_argument("--target-hz", type=float, help="Optional override for older builds without rate metadata")
    p.add_argument("--minimum-display-fps", type=float, help="Require game submissions in every complete window and LCD completions strictly above this FPS; independent of full-speed emulation")
    p.add_argument("--display-window-seconds", type=float, default=0, help="Optionally split LCD counters into short windows to expose slower scenes; 0 records one window")
    p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=2000000)
    p.add_argument("--serial", required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--seconds", type=float, default=22)
    p.add_argument("--rom-crc", help="Require this cartridge CRC32 in every sampled window")
    p.add_argument("--pxadb-dir", type=Path, default=Path(__file__).resolve().parents[5]/"tools/pxadb")
    args = p.parse_args()
    if args.buttons is not None and not 0<=args.buttons<=4095:p.error("--buttons must be 0..4095")
    if args.display_window_seconds and args.display_window_seconds<2:p.error("--display-window-seconds must be zero or at least 2")
    sys.path.insert(0, str(args.pxadb_dir))
    import pxadb
    os.environ["PXADB_BAUD"] = str(args.baud)
    with pxadb.PxaDbClient(args.port, 15) as client:
        assert pxadb.info_properties(client.hello()).get("serial") == args.serial
        report = measure(client, args.output, args.seconds,expected_crc=args.rom_crc,app_id=args.app_id,target_hz=args.target_hz,buttons=args.buttons,display_window_seconds=args.display_window_seconds)
    summary={"sustained": report["sustained"], "display_fps": report.get("display_fps"), "audio_enabled": report["audio_enabled"]}
    if args.minimum_display_fps is not None:
        summary["display_target_met"]=display_target_met(report,args.minimum_display_fps)
    print(json.dumps(summary))
    if args.minimum_display_fps is not None and not summary["display_target_met"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
