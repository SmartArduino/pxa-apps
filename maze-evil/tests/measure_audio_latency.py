#!/usr/bin/env python3
"""Measure Maze Evil software audio latency with PXA_AUDIO_LATENCY_TRACE.

Uses the device clock in AudioTrace logs, not serial command round-trip time.
UART submission is not speaker onset. Does not record a microphone or modify
volume/settings/save data. Requires an inactive installed game and trace build.
"""
import argparse
import json
from pathlib import Path
import re
import signal
import sys
import time


def main():
    def interrupted(_signum, _frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupted)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--serial', default='a4:cb:8f:d5:d4:f0')
    args = parser.parse_args()
    pxadb_dir = next(p / 'tools/pxadb' for p in Path(__file__).resolve().parents
                     if (p / 'tools/pxadb/pxadb.py').exists())
    sys.path.insert(0, str(pxadb_dir))
    import pxadb

    args.output.mkdir(parents=True, exist_ok=True)
    report = dict(scope=__doc__, rows=[], logs=[], phases=[], passed=False)
    started = monitoring = False
    with pxadb.PxaDbClient(args.port, timeout=15) as client:
        client.log_callback = lambda frame: report['logs'].append(frame.payload)
        client.raw_callback = lambda raw: report['logs'].append(raw.decode(errors='replace'))

        def request(command):
            frames = client.request(command, timeout=40)
            report['rows'].append(dict(command=command,
                frames=[dict(kind=f.kind, payload=f.payload) for f in frames]))
            if any('status=rejected' in f.payload for f in frames):
                raise RuntimeError('Input rejected: '+command)
            return frames

        def wait(seconds):
            deadline = time.monotonic()+seconds
            while time.monotonic() < deadline:
                client.pump_logs()
                time.sleep(min(.02, max(0, deadline-time.monotonic())))

        def tap(hold=.06, x=220, y=120):
            request(f'INPUT POINTER DOWN {x} {y} 0')
            wait(hold)
            request(f'INPUT POINTER UP {x} {y} 0')
            request('INPUT SYNC')

        try:
            report['device'] = client.hello()
            if pxadb.info_properties(report['device']).get('serial') != args.serial:
                raise RuntimeError('Unexpected device')
            packages = request('PACKAGES')
            report['packages'] = [dict(kind=f.kind, payload=f.payload) for f in packages]
            if any('active=1' in f.payload for f in packages):
                raise RuntimeError('Stop the foreground application first')
            client.subscribe_logs()
            request('INPUT KEY HOME'); wait(.5)
            request('PACKAGE run pxa-maze-evil'); started = True; wait(1.5)
            tap(.06, 148, 120); wait(1)
            # Restart to keep seed/health/enemy state comparable across phases.
            for name, interval, shots, hold in (
                    ('isolated', 1.2, 6, .06),
                    ('rapid', .60, 8, .06),
                    ('long-tap', 1.2, 4, .20)):
                if report['phases']:
                    request('PACKAGE stop pxa-maze-evil'); started = False; wait(.4)
                    request('PACKAGE run pxa-maze-evil'); started = True; wait(1)
                    tap(.06, 148, 120); wait(.7)
                begin = len(report['logs'])
                barrier = request('INPUT SYNC')
                phase_us = int(pxadb.info_properties(barrier[-1].payload)['device_us'])
                request('MEMORY START')
                monitoring = True
                for _ in range(shots):
                    start = time.monotonic()
                    tap(hold)
                    wait(.06)
                    # Recover the immediate command burst from the device's
                    # bounded history; the analyser deduplicates these logs.
                    request('LOGSUB')
                    wait(max(0, interval-(time.monotonic()-start)))
                wait(1.5)
                memory = request('MEMORY STOP')
                monitoring = False
                report['phases'].append(dict(name=name, device_us=phase_us, interval=interval,
                    shots=shots, hold=hold, log_begin=begin, log_end=len(report['logs']),
                    memory=[dict(kind=f.kind, payload=f.payload) for f in memory]))
                (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
                print(name, 'completed', flush=True)
            if not any(re.search(r'AudioTrace.*(?:tone|effect) us=', line) for line in report['logs']):
                raise RuntimeError('Trace firmware required; no audio timing samples')
            report['passed'] = True
        finally:
            try:
                request('INPUT CANCEL')
                if monitoring:
                    request('MEMORY STOP')
                if started:
                    request('PACKAGE stop pxa-maze-evil')
                wait(.5)
                client.unsubscribe_logs()
            finally:
                (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
