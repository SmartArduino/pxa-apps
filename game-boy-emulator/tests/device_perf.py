#!/usr/bin/env python3
"""Measure live GB simulation and display completions; never optical panel Hz."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import time


def measure(client, output, seconds=22, motion=True, expected_crc=None,
            input_mode="controller", touch_points=None, completion_counter="display"):
    import pxadb
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    if input_mode not in ("controller", "touch", "none"):
        raise ValueError("Unknown input mode")
    if completion_counter not in ("display", "game"):
        raise ValueError("Unknown completion counter")
    if input_mode == "touch" and (not touch_points or
            any(name not in touch_points for name in (("right", "a") if motion else ("neutral",)))):
        raise ValueError("Touch mode needs right/a points for play or a neutral point for stationary scenes")
    report = {"samples": [], "surface": [], "input_mode": input_mode,
              "completion_counter": completion_counter,
              "scope": "GB completed emulation frames and firmware "+completion_counter+" completions; not optical refresh"}
    if input_mode == "touch":
        report["touch_points"] = touch_points
    seen = set()
    collect = False
    cutoff = 0
    window_end = 0
    previous = client.log_callback, client.raw_callback
    with (output / "serial.log").open("w", buffering=1) as log:
        def record(line):
            nonlocal window_end
            log.write(line.rstrip()+"\n")
            line = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", line)
            if not collect:
                return
            if "GB STATS " in line:
                row = dict(re.findall(r"([a-z0-9_]+)=([^\s]+)", line.split("GB STATS ", 1)[1]))
                for sample in report["samples"]:
                    if sample["wall_us"] == row.get("wall_us"):
                        sample.update(row)
                return
            for label, field in [("GB PERF ", "samples"), ("GB DISPLAY ", "surface")]:
                if label not in line:
                    continue
                row = dict(re.findall(r"([a-z0-9_]+)=([^\s]+)", line.split(label, 1)[1]))
                stamp = int(row.get("wall_us", "0"))
                if label == "GB PERF ":
                    if expected_crc is not None and row.get('rom_crc','').lower()!=expected_crc.lower():
                        raise RuntimeError(f"ROM mismatch: expected {expected_crc}, got {row.get('rom_crc')}")
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
            if not any(":pxa-game-boy\t" in p and "active=1" in p for p in report["packages"]):
                raise RuntimeError("Start Game Boy and load a ROM before measuring")
            client.subscribe_logs()
            initial = None
            if input_mode == "controller":
                initial = "INPUT CONTROLLER 0 1 "+("8" if motion else "0")
            elif input_mode == "touch":
                x,y = touch_points["right" if motion else "neutral"]
                initial = f"INPUT POINTER DOWN {x} {y} 0" if motion else f"INPUT TAP {x} {y}"
            for f in client.request(initial) if initial else ():
                props = pxadb.info_properties(f.payload)
                if props.get("status") == "rejected":
                    raise RuntimeError("Input injection rejected; release physical input before measuring")
                cutoff = int(props.get("device_us", cutoff))
            report["input_ack_device_us"]=cutoff
            probe_start = completion_counter+"_start"
            try:
                report[probe_start] = [f.payload for f in client.request(
                    "PERF START display" if completion_counter == "display" else "PERF START")]
            except pxadb.PxaDbError as error:
                report[completion_counter+"_error"] = str(error)
            collect = True
            began = time.monotonic()
            next_input = 0
            touch_a_down = False
            while time.monotonic()-began < seconds:
                client.pump_logs()
                elapsed = time.monotonic()-began
                if elapsed >= next_input:
                    if input_mode == "controller":
                        bits = (8 | (16 if elapsed % 1.05 < .22 else 0)) if motion else 0
                        client._write_command(client._next_sequence(), f"INPUT CONTROLLER 0 1 {bits}")
                    elif input_mode == "touch":
                        if motion:
                            down = elapsed % 1.05 < .22
                            if down != touch_a_down:
                                x,y = touch_points["a"]
                                client.request(f"INPUT POINTER {'DOWN' if down else 'UP'} {x} {y} 1")
                                touch_a_down = down
                        else:
                            x,y = touch_points["neutral"]
                            client.request(f"INPUT TAP {x} {y}")
                    next_input = elapsed+(.12 if motion or input_mode == "controller" else 2)
                time.sleep(.005)
            collect = False
            if probe_start in report:
                measured_us = int((time.monotonic()-began)*1000000)
                probe_stop = completion_counter+"_stop"
                report[probe_stop] = [f.payload for f in client.request("PERF STOP")]
                props = pxadb.info_properties(";".join(report[probe_stop]))
                firmware_us = int(props.get("window_us", "0"))
                window_us = firmware_us or measured_us
                report["completion_window_source"] = "firmware" if firmware_us else "host monotonic; serial command boundary uncertainty"
                report["completion_window_us"] = window_us
                if window_us:
                    report["display_fps" if completion_counter == "display" else "game_completed_fps"] = int(props["completed"])*1000000/window_us
                client.request("PERF CLEAR")
            report["memory"] = [f.payload for f in client.request("MEMORY")]
        finally:
            try:
                client.request("INPUT CANCEL", timeout=5)
                client.unsubscribe_logs(timeout=10)
            finally:
                client.log_callback, client.raw_callback = previous
    report["sustained"] = len(report["samples"]) >= 2 and all(
        abs(int(r["sim_x100"])-5972.75) <= 100 and int(r["debt"]) <= 2 for r in report["samples"])
    report["audio_enabled"] = bool(report["samples"]) and all(r.get("sound") == "1" for r in report["samples"])
    (output / "results.json").write_text(json.dumps(report, indent=2)+"\n")
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=2000000)
    p.add_argument("--serial", required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--seconds", type=float, default=22)
    p.add_argument("--rom-crc", help="Require this cartridge CRC32 in every sampled window")
    p.add_argument("--input-mode", choices=("controller", "touch", "none"), default="controller")
    p.add_argument("--counter", choices=("display", "game"), default="display",
                   help="All LCD transfers or completed new game frame IDs")
    p.add_argument("--stationary", action="store_true", help="Keep the game scene stationary")
    p.add_argument("--touch-right", type=int, nargs=2, metavar=("X", "Y"))
    p.add_argument("--touch-a", type=int, nargs=2, metavar=("X", "Y"))
    p.add_argument("--touch-neutral", type=int, nargs=2, metavar=("X", "Y"))
    p.add_argument("--pxadb-dir", type=Path, default=Path(__file__).resolve().parents[4]/"tools/pxadb")
    args = p.parse_args()
    sys.path.insert(0, str(args.pxadb_dir))
    import pxadb
    os.environ["PXADB_BAUD"] = str(args.baud)
    with pxadb.PxaDbClient(args.port, 15) as client:
        assert pxadb.info_properties(client.hello()).get("serial") == args.serial
        points={name:value for name,value in (("right",args.touch_right),("a",args.touch_a),
                                             ("neutral",args.touch_neutral)) if value is not None}
        report = measure(client, args.output, args.seconds,motion=not args.stationary,
                         expected_crc=args.rom_crc,input_mode=args.input_mode,touch_points=points,
                         completion_counter=args.counter)
    print(json.dumps({"sustained": report["sustained"], "display_fps": report.get("display_fps"),
                      "game_completed_fps":report.get("game_completed_fps"),"audio_enabled": report["audio_enabled"]}))


if __name__ == "__main__":
    main()
