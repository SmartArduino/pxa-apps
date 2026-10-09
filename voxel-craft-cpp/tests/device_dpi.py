"""Check the firmware-to-Guest DPI path and physical touch alignment.

Build tests/build_playtest.py under an isolated pxa-voxel-craft-cpp-ui identity.
Works with a serial port or a simulator unix socket; never changes real saves.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import time

root = next((p for p in Path(__file__).resolve().parents
             if (p / 'tools/pxadb/pxadb.py').is_file()), None)
if root is None:
    raise SystemExit('Run inside a pxa-projects workspace.')
sys.path.insert(0, str(root / 'tools/pxadb'))
import pxadb

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--port', required=True)
p.add_argument('--identity', default='pxa-voxel-craft-cpp-ui5')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--width', type=int, default=480)
p.add_argument('--height', type=int, default=480)
p.add_argument('--dpi', type=int, default=305)
p.add_argument('--safe', type=int, default=12)
p.add_argument('--log', type=Path, help='Simulator stdout log when Guest logs are external')
p.add_argument('--no-screenshots', action='store_true')
a = p.parse_args()
if not a.identity.startswith('pxa-voxel-craft-cpp-ui'):
    p.error('Only isolated test identities are permitted.')
a.output.mkdir(parents=True, exist_ok=True)
scale = min(a.dpi/160, (a.width-2*a.safe)/250, (a.height-2*a.safe)/208)
font = 18 if scale >= 1.65 else 14 if scale >= 1.25 else 10
bh, gap = max(20, int(29*scale)), max(3, int(6*scale))
top = a.safe+max(4, int(7*scale))
if a.width <= a.height:
    top = max(top, a.safe+(a.height-2*a.safe-int(66*scale)-3*bh-2*gap)//2)
first = top+int(66*scale)+bh//2
ui, craft, display, game = {}, {}, {}, {}
report = dict(passed=False, width=a.width, height=a.height, dpi=a.dpi,
              density_q16=round(a.dpi*65536/160), scale=scale, font=font, scenes=[])
log_at = 0
with pxadb.PxaDbClient(a.port, 30) as c, (a.output/'serial.jsonl').open('w', buffering=1) as log:
    def parse(line):
        # Ignore unrelated retained log records from earlier apps.
        if '[PXA app=' in line and f'app={a.identity} ' not in line:
            return
        values = {k: int(v) for k,v in re.findall(r'(\w+)=(-?\d+)', line)}
        if 'VOXEL-STATE ' in line:
            game.update(values)
            ui['screen']=1
        for label,target in [('VOXEL-UI ',ui), ('VOXEL-CRAFT ',craft), ('VOXEL-DISPLAY ',display)]:
            if label not in line:
                continue
            target.update(values)
            for key in ['menu','bag','back','first','pixels']:
                pair = re.search(key+r'=(\d+),(\d+)', line)
                if pair:
                    target[key] = tuple(map(int, pair.groups()))
    def receive(frame):
        log.write(json.dumps(dict(kind=frame.kind,payload=frame.payload))+'\n')
        parse(frame.payload)
    c.device_info = c.hello()
    c.log_callback = receive
    c.raw_callback = lambda b: log.write(json.dumps(dict(kind='raw',payload=b.decode(errors='replace')))+'\n')
    c.subscribe_logs()
    def req(command):
        log.write(json.dumps(dict(command=command))+'\n')
        return c.request(command, timeout=60)
    def pump(seconds):
        global log_at
        end = time.monotonic()+seconds
        while time.monotonic()<end:
            c.pump_logs()
            if a.log:
                text = a.log.read_text(errors='replace')
                for line in text[log_at:].splitlines():
                    parse(line)
                log_at = len(text)
            time.sleep(.02)
    def tap(x,y):
        req(f'INPUT POINTER DOWN {int(x)} {int(y)} 0');req('INPUT SYNC');pump(.18)
        req(f'INPUT POINTER UP {int(x)} {int(y)} 0');req('INPUT SYNC');pump(.8)
    def scene(name, expected):
        pump(.3)
        assert ui.get('screen') == expected, (name,ui,display)
        if not a.no_screenshots:
            cap = pxadb.screenshot_capture(req('SCREENSHOT'))
            pxadb.write_screenshot(cap, a.output/(name+'.png'))
        report['scenes'].append(name)
    # Assumes the display is unlocked, avoiding changes to device idle settings.
    try:
        if not a.log:
            try:
                req('PACKAGE stop '+a.identity)
            except pxadb.PxaDbError as e:
                if str(e) != 'package_not_running':
                    raise
            req('PACKAGE run '+a.identity)
        pump(12)
        if not ui:
            cap = pxadb.screenshot_capture(req('SCREENSHOT'))
            pxadb.write_screenshot(cap,a.output/'startup-pending.png')
            raise RuntimeError('Fixture did not reach title; for DPI-only QA build with --no-audio. See startup-pending.png.')
        assert display.get('density') == report['density_q16'], display
        assert display.get('pixels') == (a.width,a.height), display
        assert ui.get('font') == font, ui
        assert ui.get('button') == max(16,int(30*scale)), ui
        assert display.get('first') == (a.width//2,first), display
        report['observed'] = dict(display=display.copy(), ui=ui.copy())
        scene('title',0)
        tap(a.width//2,first+2*(bh+gap));scene('settings',3)
        st = a.safe+max(4,int(7*scale))
        sh, sg = max(14,int(20*scale)), max(2,gap//2)
        tap(a.width//2,st+int(29*scale)+6*(sh+sg)+gap//2+bh//2);scene('title-return',0)
        tap(a.width//2,first+bh+gap);scene('load-slots',4)
        tap(a.width//2,st+int(43*scale)+3*(bh+gap)+bh//2);scene('title-load-return',0)
        tap(a.width//2,first);pump(4)
        assert game, 'New-game tap must produce actual VOXEL-STATE gameplay logs'
        scene('game',1)
        tap(*ui['menu']);scene('pause',2)
        tap(a.width//2,st+int(42*scale)+bh//2)
        tap(*ui['bag']);scene('inventory',6)
        tap(*craft['back'])
        tap(*ui['menu']);scene('pause-after-inventory',2)
        if not a.log:
            req('INPUT KEY HOME');pump(1)
            req('PACKAGE run '+a.identity);pump(2);scene('foreground',2)
        report['passed'] = True
    finally:
        req('INPUT CANCEL')
        if not a.log:
            try:
                req('PACKAGE stop '+a.identity)
            except pxadb.PxaDbError as e:
                if str(e) != 'package_not_running':
                    raise
        (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,ensure_ascii=False))
