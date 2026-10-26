#!/usr/bin/env python3
"""Run imported games in the real headless PXA product simulator."""
import argparse
import concurrent.futures
import io
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time
from PIL import Image

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--simulator', type=Path, required=True)
parser.add_argument('--packages', type=Path, required=True)
parser.add_argument('--publisher-key', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--apps')
parser.add_argument('--workers', type=int, default=3)
parser.add_argument('--width', type=int, default=320)
parser.add_argument('--height', type=int, default=240)
parser.add_argument('--wait-packages', type=float, default=0, help='Wait up to this many seconds for each package')
parser.add_argument('--require-aot', action='store_true')
parser.add_argument('--resume', action='store_true', help='Reuse passing results for the same artifact and screen size')
parser.add_argument('--store-media', action='store_true', help='Hide Host performance counters and navigation before capture')
args = parser.parse_args()
games = json.loads((repo/'common/retro-games/games.json').read_text())['games']
if args.apps:
    selected = set(args.apps.split(','))
    assert selected <= {game['directory'] for game in games}
    games = [game for game in games if game['directory'] in selected]
args.output.mkdir(parents=True, exist_ok=True)

def command(path, text):
    with socket.socket(socket.AF_UNIX) as client:
        client.settimeout(20)
        client.connect(str(path))
        client.sendall((text+'\n').encode())
        parts = []
        while part := client.recv(65536):
            parts.append(part)
    result = b''.join(parts)
    if result.startswith(b'ERR'):
        raise RuntimeError(result.decode())
    return result

def run(game):
    name = game['directory']
    out = args.output/name
    out.mkdir(exist_ok=True)
    package = args.packages/name/('pxa-'+name)
    result = {'app': name, 'passed': False, 'input': 'SDL keyboard controller injection; not physical buttons'}
    try:
        end = time.monotonic()+args.wait_packages
        artifact = package/'artifacts/main.linux-x86_64.aot'
        while not ((package/'signature.pxs').exists() and (artifact.exists() or
                   (not args.require_aot and (package/'artifacts/main.wasm').exists()))):
            if time.monotonic()>=end:
                raise FileNotFoundError('Package is not ready: '+str(package))
            time.sleep(1)
        if not artifact.exists():
            artifact = package/'artifacts/main.wasm'
        result['artifact_sha256'] = hashlib.sha256(artifact.read_bytes()).hexdigest()
        result['screen_size'] = [args.width, args.height]
        result['navigation_mode'] = 'gesture-no-bar' if args.store_media else 'default'
        result['performance_overlay'] = False
        if args.resume and (out/'result.json').exists():
            old = json.loads((out/'result.json').read_text())
            if (old.get('passed') and old.get('artifact_sha256') == result['artifact_sha256'] and
                    old.get('screen_size') == result['screen_size'] and
                    old.get('navigation_mode') == result['navigation_mode']):
                print(name, 'PASS (unchanged artifact)', flush=True)
                return old
        with tempfile.TemporaryDirectory(prefix='pxa-retro-') as tmp, (out/'runtime.log').open('wb') as log:
            path = Path(tmp)/'control.sock'
            (Path(tmp)/'state').mkdir()
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', PXA_SIMULATOR_PERF='0')
            if args.store_media:
                env['PXA_SIM_NAV_MODE'] = 'gesture-no-bar'
            proc = subprocess.Popen([str(args.simulator.resolve()), '--package', str(package.resolve()),
                '--publisher-key', str(args.publisher_key.resolve()), '--state-root', str(Path(tmp)/'state'),
                '--width', str(args.width), '--height', str(args.height), '--corner-radius', '0',
                '--pxadb-control-socket', str(path)], env=env,
                stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                end = time.monotonic()+30
                while not path.exists():
                    if proc.poll() is not None:
                        raise RuntimeError('Simulator exited during startup')
                    if time.monotonic()>end:
                        raise TimeoutError('Control socket startup')
                    time.sleep(.05)
                time.sleep(2)
                def shot(label):
                    header, png = command(path, 'SCREENSHOT AFTER_PRESENT').split(b'\n', 1)
                    assert header == f'PNG {len(png)}'.encode()
                    image = Image.open(io.BytesIO(png)).convert('RGB')
                    assert image.size == (args.width, args.height)
                    assert len(image.getcolors(args.width*args.height) or [])>2, 'Blank framebuffer'
                    (out/(label+'.png')).write_bytes(png)
                    return image.tobytes()
                before = shot('menu')
                def key(name, hold=.12):
                    command(path, 'KEY_DOWN '+name);time.sleep(hold)
                    command(path, 'KEY_UP '+name);time.sleep(.4)
                key('Z')
                time.sleep(1)
                after = shot('playing')
                if game['family'] in ('puzzles', 'arduboy', 'saf'):
                    assert before != after, 'A did not leave the initial game menu'
                key('RIGHT', .2)
                shot('input')
                key('ENTER')
                shot('paused')
                key('ENTER')
                shot('resumed')
            finally:
                if proc.poll() is None:
                    try:
                        command(path, 'KEY HOME');proc.wait(timeout=10)
                    except (OSError, RuntimeError, subprocess.TimeoutExpired):
                        if proc.poll() is None:os.killpg(proc.pid, signal.SIGTERM)
                try:proc.wait(timeout=5)
                except subprocess.TimeoutExpired:os.killpg(proc.pid, signal.SIGKILL);proc.wait()
            assert proc.returncode == 0, f'Exit {proc.returncode}'
        text = (out/'runtime.log').read_text(errors='replace')
        for error in ('Guest exception', 'callback failed', 'failed at', 'surface create failed', 'CORE SAFETY STOP'):
            assert error not in text, error
        assert 'ARDU MEM' not in text and 'SAF MEM' not in text, 'Per-frame tracing enabled in release'
        result['passed'] = True
    except Exception as error:
        result['error'] = str(error)
    (out/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(name, 'PASS' if result['passed'] else 'FAIL '+result.get('error', ''), flush=True)
    return result

with concurrent.futures.ThreadPoolExecutor(max(1, args.workers)) as pool:
    results = list(pool.map(run, games))
(args.output/'result.json').write_text(json.dumps(results, indent=2)+'\n')
assert all(row['passed'] for row in results), 'Inspect failing runtime.log files in '+str(args.output)
