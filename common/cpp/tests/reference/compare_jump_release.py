#!/usr/bin/env python3
"""Compare real release diagnostics, including workload and original variability."""
import argparse
import json
import statistics
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("before", type=Path)
parser.add_argument("reference", type=Path)
parser.add_argument("after", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
reports = [json.loads(path.read_text()) for path in (args.before, args.reference, args.after)]
assert all(report["passed"] for report in reports)
identities = [{key: value for key, value in
               (field.split("=", 1) for field in report["info"][0].split(";") if "=" in field)
               if key in ("target", "serial", "version")} for report in reports]
assert identities[0] == identities[1] == identities[2], "use the same device and Host"
assert max(row["up"] for row in reports[0]["runs"]) < min(row["down"] for row in reports[1]["runs"])
assert max(row["up"] for row in reports[1]["runs"]) < min(row["down"] for row in reports[2]["runs"]), "device timestamps reset or runs overlap"
holds = [{row["requested_hold_seconds"] for row in report["runs"]} for report in reports]
assert holds[0] == holds[1] == holds[2]
for report in reports:
    for row in report["runs"]:
        # All measurements are of the first jump towards x=2,z=0. Check the
        # resulting distance as well as charge; no screenshot latency enters it.
        expected_x = row["vx_q6"] * (2 * row["vy_q6"] / 1e6 / 72)
        assert abs(row["land_x_q6"] - expected_x) < 40
        assert row["land_z_q6"] == 0
        assert abs(row["device_hold_us"] - row["requested_hold_seconds"]*1e6) < 25000

rows = []
for hold in sorted(holds[0]):
    row = {"requested_hold_seconds": hold}
    for name, report in zip(("old_cpp", "original_c", "fixed_cpp"), reports):
        samples = [sample for sample in report["runs"] if sample["requested_hold_seconds"] == hold]
        if name != "old_cpp":
            assert len(samples) >= 3, "repeat the reference and fixed game at least three times"
        row[name] = {key: statistics.median(sample[key] for sample in samples) for key in
                     ("device_hold_us", "charge_us", "land_x_q6", "ticks", "moves", "draws", "max_gap_us")}
        row[name]["charge_range_us"] = [min(s["charge_us"] for s in samples), max(s["charge_us"] for s in samples)]
    difference = row["fixed_cpp"]["charge_us"]-row["original_c"]["charge_us"]
    # C's per-callback rounding has observable jitter. This is a median guard,
    # not a claim that every physical hold or each individual sample is equal.
    tolerance = max(20005, .10*row["original_c"]["charge_us"])
    assert abs(difference) <= tolerance, (hold, "charge still diverges", difference, tolerance)
    row.update(fixed_minus_C_charge_us=difference, median_tolerance_us=tolerance)
    rows.append(row)
result = {"passed": True, "host": identities[0],
          "runs": dict(zip(("old_cpp", "original_c", "fixed_cpp"), (len(r["runs"]) for r in reports))),
          "comparison": "median within one 20 ms step or 10%; original rounding jitter remains",
          "rows": rows}
args.output.write_text(json.dumps(result, indent=2)+"\n")
print(json.dumps(result, indent=2))
