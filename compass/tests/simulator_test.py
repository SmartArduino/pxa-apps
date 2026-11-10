#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the signed app through real PXA sensor, storage and input services.

The isolated test provider supplies explicit synthetic vectors. These tests are
not a substitute for the physical board's rotation and calibration acceptance.
"""
import argparse
import json
import math
import os
import random
import re
import sys
import time
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'common/pocket-studio/tests'))
from simulator import Session, save_blob

p = argparse.ArgumentParser()
p.add_argument('--workspace', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--packages', type=Path)
p.add_argument('--smoke', action='store_true')
args = p.parse_args()
root, out = args.workspace.resolve(), args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
a = SimpleNamespace(
    output=out,
    packages=(args.packages or out.parent / 'packages/simulator').resolve(),
    publisher_key=root / 'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der',
    simulator=out.parent / 'sensor-simulator/pxsys_product_simulator',
)
source = out / 'sensors.txt'
os.environ['COMPASS_TEST_SENSORS'] = str(source)
os.environ['PXA_SIMULATOR_PERF'] = '0'


def sensor(m=(0, 24000, -38000), g=(0, 0, 9806650)):
    temp = source.with_suffix('.tmp')
    temp.write_text(' '.join(str(round(v)) for v in (*m, *g)))
    temp.replace(source)


def wait(s, phrase, seconds=8):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if phrase in s.logs():
            return
        assert s.proc.poll() is None
        time.sleep(.05)
    raise AssertionError(s.logs()[-1600:])


def reading(s, quality, heading=None, settle=1.5):
    # Diagnostics run every 2 s. Only use a newly emitted reading AFTER the
    # filter settling interval, never a log line left over from the prior pose.
    time.sleep(settle)
    offset = len(s.logs())
    end = time.monotonic() + 4
    while time.monotonic() < end:
        values = re.findall(r'quality=(\d+) heading=([\d.]+)', s.logs()[offset:])
        if values:
            q, actual = int(values[-1][0]), float(values[-1][1])
            assert q == quality, (q, quality, s.logs()[-1200:])
            if heading is not None:
                assert abs((actual - heading + 180) % 360 - 180) < 2, (actual, heading)
            return actual
        assert s.proc.poll() is None
        time.sleep(.05)
    raise AssertionError('no fresh diagnostic reading')


def rotate(v, q):
    x, y, z, w = q
    u = (x, y, z)
    def cross(a, b):
        return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
    t = cross(u, v)
    b = cross(u, t)
    return tuple(v[i] + 2*w*t[i] + 2*b[i] for i in range(3))


def bearing(h):
    return (65000 - 24000*math.sin(math.radians(h)),
            -23000 + 24000*math.cos(math.radians(h)), -21000)


report = {'source': 'synthetic vectors via real PXA Host', 'headings': {}}
for w, h in [(480, 480)] if not args.smoke else [(480, 480), (320, 240), (240, 284)]:
    sensor()
    with Session(a, 'compass', w, h) as s:
        wait(s, 'Compass connected')
        time.sleep(.4)
        s.shot('01-calibration-needed')
        print('canvas', s.cw, s.ch, flush=True)
        s.tap(s.cw/2, s.ch-22)
        s.shot('02-calibration-guide')
        if args.smoke:
            s.tap(s.cw/4, s.ch-24)
            s.tap(s.cw*5/6, s.ch-22)
            s.shot('03-settings')
            s.tap(s.cw/2, min(146, s.ch-104)+43)
            s.shot('04-guide')
            continue
        s.tap(s.cw*3/4, s.ch-24)
        wait(s, 'Compass: calibration started')
        rng = random.Random(314159)
        # Continue beyond the 256-reading bounded ring to test rolling fits.
        for i in range(300):
            q = [rng.gauss(0, 1) for _ in range(4)]
            n = math.sqrt(sum(v*v for v in q))
            q = [v/n for v in q]
            m = rotate((0, 24000, -38000), q)
            g = rotate((0, 0, 9806650), q)
            sensor(tuple(m[j] + (65000, -23000, 17000)[j] for j in range(3)), g)
            time.sleep(.125)
        s.shot('03-calibration-complete')
        s.tap(s.cw*3/4, s.ch-24)
        wait(s, 'Compass: calibration saved')
        time.sleep(.7)
        saved = save_blob(s.state, 'compass')
        assert saved[:4] == b'CMP2' and saved[8] == 1
        for heading in [0, 90, 180, 270, 359, 1, 42]:
            sensor(bearing(heading))
            report['headings'][str(heading)] = reading(s, 5, heading)
            if heading in (0, 90, 42):
                s.shot('heading-' + str(heading))
        s.tap(s.cw/6, s.ch-22)
        frozen = reading(s, 6, 42, .1)
        s.shot('04-held')
        sensor(bearing(90))
        assert reading(s, 6, 42, .1) == frozen
        s.shot('05-held-after-motion')
        s.tap(s.cw/6, s.ch-22)
        reading(s, 5, 90)
        source.unlink()
        reading(s, 0, settle=1.7)
        s.shot('06-disconnected')
        sensor(bearing(90))
        reading(s, 5, 90)
        s.shot('07-reconnected')
        sensor((365000, -23000, 17000))
        reading(s, 4, settle=.2)
        s.shot('08-interference')
        sensor(bearing(90), (0, 9806650, 0))
        reading(s, 5, settle=.2)  # Upright uses the rear-facing reference.
        # Only an ambiguous inverted transition loses a horizontal reference.
        sensor(bearing(90), (0, 8032936, -5624779))
        reading(s, 3, settle=.2)
        s.shot('09-vertical')
        sensor(bearing(42))
        reading(s, 5, 42)
        s.tap(s.cw*5/6, s.ch-22)
        s.tap(s.cw/2, min(146, s.ch-104)+12)
        s.shot('10-clear-confirm')
        s.pointer('DOWN', s.cw*3/4, s.ch-24)
        s.pointer('CANCEL', s.cw*3/4, s.ch-24)
        assert save_blob(s.state, 'compass') == saved
        s.tap(s.cw/4, s.ch-24)
        assert save_blob(s.state, 'compass') == saved
        s.tap(s.cw/2, s.ch-24)
        s.shot('11-restored-home')
        report['calibration_hold_faults_cancel'] = True
    if not args.smoke:
        with Session(a, 'compass', w, h) as s:
            wait(s, 'Compass: preferences restored')
            reading(s, 5, 42)
            assert save_blob(s.state, 'compass') == saved
            s.shot('12-restart')
            report['restart'] = True
(out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
