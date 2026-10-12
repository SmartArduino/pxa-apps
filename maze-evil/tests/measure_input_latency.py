#!/usr/bin/env python3
"""Repeated Maze Evil input/LCD-completion benchmark on 296x240 pai-touch.

Requires an installed, inactive pxa-maze-evil. Does not change settings or save
data. Injected sample timestamps exclude physical sensor and USB transit time;
Host delivery/presentation counters do not prove which game update used input.
"""
import argparse
import json
from pathlib import Path
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
    parser.add_argument('--repeat', type=int, default=3)
    parser.add_argument('--modes', nargs='+', choices=['standing', 'looking', 'walking', 'firing', 'multitouch'],
                        default=['standing', 'looking'])
    parser.add_argument('--restart-each-repeat', action='store_true')
    parser.add_argument('--pxadb-dir', type=Path)
    args = parser.parse_args()
    if args.repeat < 1:
        parser.error('--repeat must be positive')
    if args.pxadb_dir is None:
        args.pxadb_dir = next((p / 'tools/pxadb' for p in Path(__file__).resolve().parents
                              if (p / 'tools/pxadb/pxadb.py').is_file()), None)
    if args.pxadb_dir is None:
        parser.error('Specify --pxadb-dir pointing to pxa-projects/tools/pxadb')
    sys.path.insert(0, str(args.pxadb_dir))
    import pxadb

    args.output.mkdir(parents=True, exist_ok=True)
    report = dict(phases=[], startup_memory=[], rows=[], passed=False, latency_scope=__doc__,
                  configuration=dict(modes=args.modes, repeat=args.repeat,
                    restart_each_repeat=args.restart_each_repeat, seed='0x9e3779b9',
                    display=[296,240], render=[148,120], frame_period_ms=33))
    running = recording = monitoring = False
    started = time.monotonic()
    with pxadb.PxaDbClient(args.port, timeout=15) as client:
        def request(command):
            frames = client.request(command, timeout=40)
            report['rows'].append(dict(command=command, seconds=time.monotonic()-started,
                frames=[dict(kind=f.kind, payload=f.payload) for f in frames]))
            if command.startswith('INPUT ') and any('status=rejected' in f.payload for f in frames):
                raise RuntimeError('Input adapter rejected '+command)
            return frames

        def input_metrics():
            result = {}
            for frame in request('PERF INPUT'):
                if frame.kind == 'DATA':
                    result.update({k: int(v) for k, v in pxadb.info_properties(frame.payload).items()})
            for stage in ('guest', 'present', 'visible'):
                count = result[stage + '_count']
                result[stage + '_mean_us'] = result[stage + '_total_us']/count if count else None
            return result

        def shot(name):
            for attempt in range(6):
                try:
                    capture = pxadb.screenshot_capture(request('SCREENSHOT JPEG'))
                    break
                except pxadb.PxaDbError as error:
                    if attempt == 5 or (str(error) != 'screenshot_rate_limited' and
                                        not pxadb.screenshot_retryable(error)):
                        raise
                    time.sleep(.25)
            if capture.metadata.get('source') != 'surface':
                raise RuntimeError('Game surface is obscured: '+str(capture.metadata))
            pxadb.write_screenshot(capture, args.output/(name+'.jpg'))
            report[name] = capture.metadata

        def wait_until(deadline):
            time.sleep(max(0, deadline-time.monotonic()))

        try:
            client.device_info = client.hello()
            report['hello'] = client.device_info
            if pxadb.info_properties(client.device_info).get('serial') != args.serial:
                raise RuntimeError('Unexpected device serial')
            packages = request('PACKAGES')
            if any('active=1' in f.payload for f in packages):
                raise RuntimeError('Stop the active app first; benchmark does not replace it')
            if not any(':pxa-maze-evil\t' in f.payload for f in packages):
                raise RuntimeError('Install pxa-maze-evil first')
            caps = pxadb.info_properties(request('INPUT CAPABILITIES')[-1].payload)
            report['input_capabilities'] = caps
            if (caps.get('width'), caps.get('height')) != ('296', '240'):
                raise RuntimeError('This gesture requires the 296x240 pai-touch profile')
            if 'multitouch' in args.modes and int(caps.get('max_pointers', '0')) < 2:
                raise RuntimeError('Independent dual-contact injection is required')
            request('INPUT KEY HOME')
            time.sleep(.5) # Let the system's first Home draw finish after cold boot.
            request('INPUT SWIPE 148 216 148 45 400 8')
            request('INPUT KEY HOME')
            request('MEMORY START'); monitoring = True
            request('PACKAGE run pxa-maze-evil')
            running = True
            time.sleep(2)
            report['startup_memory'].append([dict(kind=f.kind, payload=f.payload)
                                             for f in request('MEMORY STOP')])
            monitoring = False
            shot('title')
            request('INPUT POINTER DOWN 148 120 0')
            time.sleep(.1)
            request('INPUT POINTER UP 148 120 0')
            request('INPUT SYNC')
            time.sleep(.8)
            for repeat in range(args.repeat):
                if repeat and args.restart_each_repeat:
                    request('PACKAGE stop pxa-maze-evil'); running = False
                    time.sleep(.5)
                    request('MEMORY START'); monitoring = True
                    request('PACKAGE run pxa-maze-evil'); running = True
                    time.sleep(1)
                    report['startup_memory'].append([dict(kind=f.kind, payload=f.payload)
                                                     for f in request('MEMORY STOP')])
                    monitoring = False
                    request('INPUT POINTER DOWN 148 120 0'); time.sleep(.1)
                    request('INPUT POINTER UP 148 120 0'); request('INPUT SYNC')
                    time.sleep(.8)
                for mode in args.modes:
                    request('PERF CLEAR')
                    request('PERF START'); recording = True
                    input_metrics()
                    request('MEMORY START'); monitoring = True
                    begin = time.monotonic()
                    if mode == 'looking':
                        request('INPUT POINTER DOWN 225 115 0')
                        time.sleep(.07)
                        for index in range(48):
                            wait_until(begin+.12+(index+1)*.065)
                            x = 225 + (12 if index % 2 == 0 else -12)
                            request(f'INPUT POINTER MOVE {x} 115 0')
                        request(f'INPUT POINTER UP {x} 115 0')
                        request('INPUT SYNC')
                    elif mode == 'walking':
                        request('INPUT POINTER DOWN 75 150 0'); time.sleep(.07)
                        request('INPUT POINTER MOVE 75 110 0')
                        wait_until(begin+1.5)
                        request('INPUT POINTER UP 75 110 0'); request('INPUT SYNC')
                    elif mode == 'firing':
                        for index in range(4):
                            wait_until(begin+.1+index*.65)
                            request('INPUT POINTER DOWN 220 120 0'); time.sleep(.1)
                            request('INPUT POINTER UP 220 120 0'); request('INPUT SYNC')
                    elif mode == 'multitouch':
                        request('INPUT POINTER DOWN 75 150 0'); time.sleep(.07)
                        request('INPUT POINTER MOVE 75 110 0')
                        request('INPUT POINTER DOWN 225 115 1')
                        for index in range(32):
                            wait_until(begin+.25+(index+1)*.065)
                            request(f'INPUT POINTER MOVE {75 + index % 4} 110 0')
                            x = 225 + (12 if index % 2 == 0 else -12)
                            request(f'INPUT POINTER MOVE {x} 115 1')
                        request(f'INPUT POINTER CANCEL {x} 115 1')
                        # The left finger remains held while the right ID is
                        # reused for a tap. Cancelling must not leave it owned.
                        request('INPUT POINTER DOWN 220 120 1'); time.sleep(.1)
                        request('INPUT POINTER UP 220 120 1')
                        request('INPUT POINTER UP 78 110 0'); request('INPUT SYNC')
                    wait_until(begin+4)
                    metrics = input_metrics()
                    probe = pxadb.info_properties(request('PERF STOP')[-1].payload)
                    recording = False
                    memory = request('MEMORY STOP'); monitoring = False
                    values = []
                    for offset in range(0, int(probe['present']), 16):
                        props = pxadb.info_properties(request(f'PERF READ present {offset}')[-1].payload)
                        values.extend(int(v) for v in props['values'].split(',') if v)
                    if not values or len(values) != int(probe['present']):
                        raise RuntimeError('Missing LCD-completion samples')
                    if any(int(probe[k]) for k in ('raster_overflow', 'present_overflow', 'surface_changed')):
                        raise RuntimeError('Invalid probe window: '+str(probe))
                    phase = dict(repeat=repeat, mode=mode, input=metrics, probe=probe,
                        present_us=values, fps=1e6*len(values)/sum(values),
                        memory=[dict(kind=f.kind, payload=f.payload) for f in memory])
                    report['phases'].append(phase)
                    print(json.dumps({k: phase[k] for k in ('repeat','mode','input','fps')}), flush=True)
                    (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
                    request('PERF CLEAR')
                    if mode in ('walking', 'firing', 'multitouch'):
                        shot(f'repeat-{repeat}-{mode}')
            shot('playing')
            report['passed'] = True
        finally:
            try:
                request('INPUT CANCEL')
                if monitoring:
                    request('MEMORY STOP')
                if recording:
                    request('PERF STOP')
                request('PERF CLEAR')
                if running:
                    request('PACKAGE stop pxa-maze-evil')
            finally:
                (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
