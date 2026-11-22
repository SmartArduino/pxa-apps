#!/usr/bin/env python3
"""Check two-contact zoom/cancel on the foreground 480x480 Mosaico map."""
from pathlib import Path
import argparse
import datetime
import importlib.util
import json
import re
import sys
import time

p = argparse.ArgumentParser(__doc__)
p.add_argument('--pxadb', required=True, type=Path)
p.add_argument('--port', required=True)
p.add_argument('--baud', type=int, default=2000000)
p.add_argument('--output', type=Path)
a = p.parse_args()
out = a.output or Path(__file__).resolve().parents[3] / 'artifacts/unciv' / datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
out.mkdir(parents=True, exist_ok=False)
sys.path.insert(0, str(a.pxadb.resolve().parent))
spec = importlib.util.spec_from_file_location('unciv_pinch_pxadb', a.pxadb)
pxadb = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = pxadb
spec.loader.exec_module(pxadb)
logs = []
with pxadb.open_client(a.port, 20, baudrate=a.baud) as client:
    client.log_callback = lambda f: logs.append(f'[{f.kind}] {f.payload}')
    info = '\n'.join(f.payload for f in client.request('INFO') if f.kind == 'OK')
    fields = pxadb.info_properties(info)
    if fields.get('board') != 'esp-mosaico' or (fields.get('width'), fields.get('height')) != ('480', '480'):
        raise SystemExit('This workload requires the 480x480 ESP-Mosaico.')
    capabilities = client.request('INPUT CAPABILITIES')
    if not any('max_pointers=' in f.payload and int(re.search(r'max_pointers=(\d+)', f.payload)[1]) >= 2 for f in capabilities):
        raise SystemExit('Two debug pointers are required.')
    def require_foreground():
        if not any(f.kind == 'PKG' and f.payload.split('\t')[0].endswith(':pxa-unciv') and 'active=1' in f.payload for f in client.request('PACKAGES')):
            raise RuntimeError('Open the Unciv map before running this workload.')
    def command(value):
        require_foreground()
        frames = client.request(value)
        if any('status=rejected' in f.payload for f in frames):
            raise RuntimeError('Pointer injection rejected: ' + value)
        time.sleep(.18)
    def shot(name):
        time.sleep(.5)
        require_foreground()
        capture = pxadb.screenshot_capture(client.request('SCREENSHOT', timeout=60))
        pxadb.write_screenshot(capture, out / (name + '.png'))
    client.subscribe_logs()
    require_foreground()
    client.request('INPUT CANCEL')
    shot('before')
    logs.clear()
    try:
        for value in ['DOWN 180 200 0', 'DOWN 300 200 1', 'MOVE 120 200 0', 'MOVE 360 200 1', 'UP 360 200 1', 'MOVE 110 200 0', 'UP 110 200 0']:
            command('INPUT POINTER ' + value)
        shot('spread')
        for value in ['DOWN 120 200 0', 'DOWN 360 200 1', 'MOVE 190 200 0', 'MOVE 290 200 1', 'UP 190 200 0', 'UP 290 200 1']:
            command('INPUT POINTER ' + value)
        shot('contract')
        for value in ['DOWN 180 200 0', 'DOWN 300 200 1', 'MOVE 165 200 0', 'MOVE 315 200 1']:
            command('INPUT POINTER ' + value)
        command('INPUT CANCEL')
        shot('cancelled')
        command('INPUT TAP 435 67')
        shot('restored')
    finally:
        client.request('INPUT CANCEL')
        (out / 'device.log').write_text('\n'.join(logs))
text = '\n'.join(logs)
sizes = [int(n) for n in re.findall(r'Unciv: pinch size=(\d+)', text)]
if sizes[-3:] != [64, 32, 40]:
    raise SystemExit('Expected spread/contract/cancel zoom sizes 64, 32, 40; inspect raw evidence.')
if re.search(r'Unciv: event=(?:move|attack|found|improve|turn)\b', text):
    raise SystemExit('Unexpected game action during zoom; inspect raw evidence.')
result = dict(passed=True, zoom_sizes=sizes[-3:], info=info,
              note='Device-timed two-pointer injection. Inspect original frames; not a mechanical finger test.')
(out / 'result.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
