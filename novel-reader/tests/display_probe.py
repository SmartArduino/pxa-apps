"""Full-system UI screenshots at real pixel sizes/DPI, with an offline restart.

Run after build_playtest.py, installation, and playtest.py have populated --state.
The probe clones this QA state into isolated directories; it never edits it.
"""
import argparse, json, re, shutil, socket, subprocess, time
from PIL import Image, ImageChops
from pathlib import Path

root = next(p for p in Path(__file__).resolve().parents
            if (p / 'tools/simulator.sh').is_file())
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--state', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--binary', type=Path, default=root / 'build/simulator/pai-touch/pxsys_desktop_simulator')
a = p.parse_args()
a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=True)
profiles = [(360, 480, 320, 40, False), (480, 480, 320, 0, True),
            (800, 480, 160, 0, False), (296, 240, 160, 58, False)]
records = []
for width, height, dpi, radius, circle in profiles:
    name = f'{width}x{height}-{dpi}dpi' + ('-circle' if circle else '')
    state = a.output / name / 'state'
    if state.exists():
        shutil.rmtree(state)
    shutil.copytree(a.state, state)
    # No saved shell palette: verify light mode separately from the dark playtest.
    control = Path('/tmp') / f'pxa-reader-{name}.sock'
    log_path = a.output / name / 'run.log'
    args = [str(a.binary), '--width', str(width), '--height', str(height),
            '--density-dpi', str(dpi), '--safe-insets', '8,10,8,10',
            '--locale', 'zh-CN', '--light', '--shape-background', 'matte',
            '--installed-packages-root', str(state / 'packages'),
            '--product-runner', str(a.binary.parent / 'pxsys_product_simulator'),
            '--publisher-key', str(root / 'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'),
            '--state-root', str(state), '--pxadb-control-socket', str(control),
            '--launch', 'pxa-novel-reader-qa']
    args += ['--round'] if circle else ['--corner-radius', str(radius)]
    def logs():
        return re.sub(r'\x1b\[[0-9;]*m', '', log_path.read_text(errors='replace'))
    def wait(predicate, timeout=12):
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            if predicate(logs()):
                return
            time.sleep(.05)
        raise AssertionError(logs()[-2000:])
    def send(command):
        with socket.socket(socket.AF_UNIX) as s:
            s.settimeout(15)
            s.connect(str(control))
            s.sendall((command + '\n').encode())
            result = b''
            while b'\n' not in result:
                result += s.recv(4096)
            header, body = result.split(b'\n', 1)
            if header.startswith(b'PNG '):
                size = int(header[4:])
                while len(body) < size:
                    body += s.recv(min(65536, size - len(body)))
                return body
            assert header == b'OK', result
    def tap(action, arg=None, native=False):
        text = logs()
        hits = re.findall(r'READER-HIT action=(\d+) arg=(-?\d+) x=(\d+) y=(\d+)',
                          text[text.rfind('READER-UI '):])
        hit = next(h for h in hits if int(h[0]) == action and
                   (arg is None or int(h[1]) == arg))
        send(f'TAP {hit[2]} {hit[3]}')
        if not native:
            wait(lambda s: 'READER-UI ' in s[len(text):])
        time.sleep(.3)
    def capture(label):
        (a.output / name / (label + '.png')).write_bytes(send('SCREENSHOT'))
    with log_path.open('w') as log:
        app = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
        try:
            wait(lambda s: 'READER-HIT action=4 arg=0 ' in s)
            capture('shelf')
            tap(4, 1)  # Previously completed cache, no server required.
            wait(lambda s: 'READER-PAGE offset=' in s)
            capture('offline-reading')
            assert 'READER-DOWNLOAD' not in logs(), 'Offline cache fetched again'
            tap(7); tap(0); tap(1); capture('search')
            tap(29, 0); time.sleep(.5); capture('search-query')
            tap(30, native=True); time.sleep(.5); capture('system-ime')
            before = Image.open(a.output / name / 'search.png').convert('RGB')
            after = Image.open(a.output / name / 'system-ime.png').convert('RGB')
            pixels=ImageChops.difference(before, after).tobytes()
            changed = sum(any(pixels[i:i+3]) for i in range(0,len(pixels),3))
            assert changed > width * height // 15, 'System keyboard did not become visible'
            records.append({'profile': name, 'offline_cache': True,
                            'native_ime': 'screenshot', 'pixel_size': [width, height]})
        finally:
            app.terminate()
            app.wait(timeout=10)
            control.unlink(missing_ok=True)
(a.output / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
print('Reader full-system display/offline probes passed:', len(records), 'profiles')
