#!/usr/bin/env python3
"""Alternate native O3 baseline/current workloads on one Linux CPU."""
import argparse
import json
import os
from pathlib import Path
import resource
import statistics
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-root', required=True, type=Path, help='Directory containing both baseline app sources')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--samples', type=int, default=7)
    parser.add_argument('--iterations', type=int, default=100)
    parser.add_argument('--cpu', type=int)
    args = parser.parse_args()
    if args.samples < 1 or args.iterations < 1:
        parser.error('samples and iterations must be positive')
    allowed = os.sched_getaffinity(0)
    cpu = min(allowed) if args.cpu is None else args.cpu
    if cpu not in allowed:
        parser.error('cpu is outside the allowed CPU set')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    app = Path(__file__).resolve().parents[1]
    roots = {'before': args.baseline_root.resolve(), 'after': app.parent}
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    for name, root in roots.items():
        command = [sys.executable, str(app / 'tests/benchmark.py'), '--source-root', str(root),
                   '--output', str(output / name), '--build-only']
        if args.sdk:
            command += ['--sdk', str(args.sdk.resolve())]
        subprocess.run(command, check=True)
    results = {'iterations': args.iterations, 'pinned_cpu': cpu, 'samples': [], 'summary': []}
    for engine in ('full', 'lite'):
        for workload in range(4):
            failed_before = False
            for sample in range(args.samples):
                for name in (('before', 'after') if sample % 2 == 0 else ('after', 'before')):
                    if name == 'before' and failed_before:
                        continue
                    command = [str(output / name / engine),
                               str(roots[name] / 'pinyin-ime/assets/dict_pinyin.dat'),
                               str(workload), str(args.iterations)]
                    run = subprocess.run(command, capture_output=True, text=True,
                                         preexec_fn=lambda: os.sched_setaffinity(0, {cpu}))
                    value = {'variant': name, 'engine': engine, 'workload': workload, 'sample': sample}
                    if run.returncode:
                        if name == 'after':
                            raise RuntimeError(run.stderr or f'Current provider failed: {run.returncode}')
                        value.update(failed=True, returncode=run.returncode)
                        failed_before = True
                    else:
                        value.update(json.loads(run.stdout.splitlines()[-1]))
                    results['samples'].append(value)
            common = [v for v in results['samples'] if v['engine'] == engine and v['workload'] == workload and not v.get('failed')]
            if len({v['reply_hash'] for v in common}) != 1:
                raise RuntimeError(f'{engine} workload {workload}: replies differ; review semantics before comparing performance')
            summary = {'engine': engine, 'workload': workload, 'baseline_failed': failed_before}
            for name in roots:
                values = [v['cpu_us_per_cycle'] for v in common if v['variant'] == name]
                if values:
                    summary[name] = statistics.median(values)
            results['summary'].append(summary)
            print(json.dumps(summary), flush=True)
            (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
