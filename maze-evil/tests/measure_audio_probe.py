#!/usr/bin/env python3
"""Measure the installed Audio probe; grant its audio permission first.

Requires a 296x240 pai-touch with Audio 0.9 trace firmware. Measures software
sample-clock timing, not speaker onset. Leaves the probe installed and stopped.
"""
import argparse
import json
from pathlib import Path
import signal
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--serial', default='a4:cb:8f:d5:d4:f0')
    args = parser.parse_args()
    pxadb_dir = next(p / 'tools/pxadb' for p in Path(__file__).resolve().parents
                     if (p / 'tools/pxadb/pxadb.py').exists())
    sys.path.insert(0, str(pxadb_dir))
    import pxadb

    def interrupted(_signal, _frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupted)
    args.output.mkdir(parents=True, exist_ok=True)
    report = dict(scope=__doc__, passed=False, logs=[], rows=[], phases=[])
    with pxadb.PxaDbClient(args.port, timeout=15) as client:
        client.log_callback = lambda frame: report['logs'].append(frame.payload)
        client.raw_callback = lambda raw: report['logs'].append(raw.decode(errors='replace'))

        def request(command):
            frames = client.request(command, timeout=40)
            report['rows'].append(dict(command=command,
                frames=[dict(kind=f.kind, payload=f.payload) for f in frames]))
            return frames

        def wait(seconds):
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                client.pump_logs()
                time.sleep(.02)

        def tap(x, y):
            request(f'INPUT POINTER DOWN {x} {y} 0')
            wait(.06)
            frames = request(f'INPUT POINTER UP {x} {y} 0')
            request('INPUT SYNC')
            return int(pxadb.info_properties(frames[-1].payload)['device_us'])

        report['device'] = client.hello()
        if pxadb.info_properties(report['device']).get('serial') != args.serial:
            raise RuntimeError('Unexpected device')
        packages = request('PACKAGES')
        if any('active=1' in f.payload for f in packages):
            raise RuntimeError('Stop the foreground app first')
        client.subscribe_logs()
        started = monitoring = False
        try:
            request('PACKAGE run pxa-audio-latency-probe')
            started = True
            wait(2)
            (args.output / 'ready.jpg').write_bytes(
                pxadb.screenshot_bytes(request('SCREENSHOT JPEG')))
            request('MEMORY START')
            monitoring = True
            for name in ('shotgun', 'alert-shotgun', 'door'):
                begin = len(report['logs'])
                timestamp = tap(90, 110)
                wait(.06)
                request('LOGSUB')
                wait(2.4)
                report['phases'].append(dict(name=name, device_us=timestamp,
                    log_begin=begin, log_end=len(report['logs'])))
            report['memory'] = [dict(kind=f.kind, payload=f.payload)
                                for f in request('MEMORY STOP')]
            monitoring = False
            (args.output / 'finished.jpg').write_bytes(
                pxadb.screenshot_bytes(request('SCREENSHOT JPEG')))
            report['passed'] = any('AudioTrace: effect us=' in line
                                   for line in report['logs'])
            if not report['passed']:
                raise RuntimeError('Trace firmware and granted probe permission required')
        finally:
            try:
                request('INPUT CANCEL')
                if monitoring:
                    request('MEMORY STOP')
                if started:
                    request('PACKAGE stop pxa-audio-latency-probe')
                wait(.5)
                client.unsubscribe_logs()
            finally:
                (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Audio effect probe passed')


if __name__ == '__main__':
    main()
