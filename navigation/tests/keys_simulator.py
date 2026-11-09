#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise Gen4 navigation with Up, Down, Confirm and the real Host IME.

Requires a fixture package built with PXA_NAV_TEST_ORIGIN=http://127.0.0.1:18787
and the isolated runner made by build_gen4_simulator.py. All settings are fake.
"""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import threading
import time
from urllib.parse import urlparse, parse_qs
import zlib
from PIL import Image


class Simulator:
    def __init__(self, args, settings=None, preserve_state=False):
        self.args = args
        self.output = args.output.resolve()
        self.output.mkdir(parents=True, exist_ok=True)
        self.state = self.output / 'state'
        identity = hashlib.sha256(args.publisher_key.read_bytes()).hexdigest() + '-pxa-navigation'
        storage = self.state / 'app-data' / identity
        storage.mkdir(parents=True, exist_ok=True)
        values = settings if settings is not None else {
            'amap.key': b'0'*32, 'search.city': b'', 'location.source': struct.pack('<I', 0)}
        body = struct.pack('<H', len(values))
        for key, value in sorted(values.items()):
            body += bytes([len(key)]) + key.encode() + struct.pack('<H', len(value)) + value
        if not preserve_state:
            (storage / '.pxa-kv-a').write_bytes(b'PXKV' + struct.pack('<HHQII', 1, 0, 1, len(body), zlib.crc32(body)) + body)
        self.socket = f'/tmp/pxa-navigation-keys-{os.getpid()}.sock'
        self.log_path = self.output / 'simulator.log'
        self.log = self.log_path.open('w')
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
                   PXA_SIM_NAV_MODE='none', PXA_SIMULATOR_PERF='0')
        self.process = subprocess.Popen([
            str(args.runner.resolve()), '--package', str(args.package.resolve()),
            '--publisher-key', str(args.publisher_key.resolve()), '--state-root', str(self.state),
            '--pxadb-control-socket', self.socket, '--width', str(args.width), '--height', str(args.height),
            '--density-dpi', str(args.density_dpi), '--locale', 'zh-CN'], env=env, stdout=self.log, stderr=subprocess.STDOUT)

    def wait(self, predicate, seconds=10):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            assert self.process.poll() is None, self.logs()[-2000:]
            if predicate():
                return
            time.sleep(.05)
        raise AssertionError(('timed out', self.focus(), self.logs()[-2000:]))

    def logs(self):
        return self.log_path.read_text(errors='replace')

    def focus(self):
        values = re.findall(r'^STUDY_FOCUS (.*)$', self.logs(), re.M)
        return values[-1] if values else ''

    def command(self, value):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(8)
            connection.connect(self.socket)
            connection.sendall((value+'\n').encode())
            with connection.makefile('rb') as stream:
                header = stream.readline()
                if header.startswith(b'PNG '):
                    size = int(header.split()[1]); data = stream.read(size)
                    assert len(data) == size
                    return data
                assert header.strip() == b'OK', (value, header)

    def key(self, value):
        assert value in ('UP', 'DOWN', 'ENTER'), 'No fourth Back key or pointer input'
        self.command('KEY_DOWN '+value)
        time.sleep(.035)
        if self.process.poll() is None:
            try:
                self.command('KEY_UP '+value)
            except (FileNotFoundError, ConnectionRefusedError,
                    ConnectionResetError, BrokenPipeError):
                assert self.process.wait(timeout=2) == 0
        time.sleep(.16)

    def choose(self, label, activate=True):
        for _ in range(100):
            if self.focus() == label:
                if activate:
                    self.key('ENTER')
                return
            self.key('DOWN')
        raise AssertionError(('unreachable', label, self.focus()))

    def shot(self, name):
        time.sleep(.3)  # Let native focus-follow scrolling finish.
        (self.output / (name+'.png')).write_bytes(self.command('SCREENSHOT AFTER_PRESENT'))

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
        self.process.wait(timeout=5)
        self.log.close()
        Path(self.socket).unlink(missing_ok=True)


def exercise(s, calls):
    s.wait(lambda: Path(s.socket).exists() and 'Navigation: ready' in s.logs())
    s.wait(lambda: bool(s.focus()))
    # The drawing surface must not be an unnamed stop ahead of real controls.
    assert s.focus() == '模拟导航', s.focus()
    s.shot('home-focus')
    s.key('UP'); assert s.focus() == '退出地图'
    s.key('DOWN'); assert s.focus() == '模拟导航'
    s.choose('模拟导航'); s.choose('行程路书'); s.shot('roadbook')
    s.choose('下一段'); s.choose('上一段'); s.choose('返回')
    s.choose('结束导航'); s.choose('放大地图'); s.choose('缩小地图')
    s.choose('路线全览'); s.choose('搜索地点')
    s.choose('[文本输入]', False)
    s.command('TEXT 114.050000,22.540000'); time.sleep(.2)
    s.choose('搜索'); time.sleep(.3)
    s.choose('114.050000,22.540000')
    s.choose('选择起点'); s.choose('[文本输入]', False)
    s.command('TEXT 114.050000,22.540000'); time.sleep(.2); s.choose('搜索')
    s.choose('114.050000,22.540000')
    s.choose('选择目的地'); s.choose('[文本输入]', False)
    s.command('TEXT test'); time.sleep(.2); s.choose('搜索')
    s.wait(lambda: any(path == '/v3/place/text' for path, _ in calls)); time.sleep(.4)
    for i in range(1, 6):
        s.choose('地点 '+str(i), False)
        s.shot('result-focus-'+str(i))
    s.key('ENTER')
    s.choose('步行'); s.choose('骑行'); s.choose('驾车'); s.choose('步行')
    s.choose('查看路线')
    s.wait(lambda: any(path.startswith('/v5/direction/') for path, _ in calls)); time.sleep(.5)
    request = next(query for path, query in calls if path.startswith('/v5/direction/'))
    assert request['destination'] == ['114.054040,22.542000'], request
    s.shot('route-preview'); s.choose('开始导航'); s.choose('结束导航')
    s.choose('偏好设置'); s.choose('搜索城市'); s.choose('[文本输入]')
    s.wait(lambda: s.focus().startswith('[键盘]')); s.shot('system-ime')
    # Enter actual letters through the native keyboard using the same 3 keys.
    s.choose('[键盘]h'); s.choose('[键盘]i'); s.shot('system-ime-typed')
    s.choose('[键盘]完成'); s.choose('定位来源'); s.choose('网络转发')
    s.choose('免费 Wi-Fi'); s.choose('地图服务'); s.choose('保存设置')
    s.wait(lambda: s.focus() != '保存设置'); s.choose('返回')
    s.choose('退出地图')
    assert s.process.wait(timeout=5) == 0
    assert not any(value in s.logs() for value in ('Exception:', 'PXA task failed', 'map frame rejected'))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('package', 'runner', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--publisher-key', type=Path, default=Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'))
    p.add_argument('--width', type=int, default=240); p.add_argument('--height', type=int, default=284)
    p.add_argument('--density-dpi', type=int, default=130)
    a = p.parse_args()
    assert '/local/artifacts/' in str(a.output.resolve())
    assert b'http://127.0.0.1:18787' in (a.package/'artifacts/main.linux-x86_64.aot').read_bytes()
    calls = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            url = urlparse(self.path); query = parse_qs(url.query)
            calls.append((url.path, query))
            kind = 'application/json'
            if url.path == '/v3/place/text':
                body = {'status': '1', 'pois': [{'name': f'地点 {i+1}', 'location': f'{114.054+i*.00001:.6f},22.542000',
                         'address': '三键列表滚动测试'} for i in range(5)]}
            elif url.path.startswith('/v5/direction/'):
                body = {'status': '1', 'route': {'paths': [{'distance': '634', 'cost': {'duration': '480'}, 'steps': [
                    {'instruction': '沿测试道路直行', 'road': '测试道路', 'distance': '634',
                     'polyline': '114.050000,22.540000;114.054040,22.542000'}]}]}}
            elif url.path == '/v3/staticmap':
                size = tuple(map(int, query['size'][0].split('*')))
                encoded = io.BytesIO(); Image.new('RGB', size, '#e6eee8').save(encoded, 'PNG')
                body = encoded.getvalue(); kind = 'image/png'
            else:
                self.send_error(404); return
            if not isinstance(body, bytes):
                body = json.dumps(body, ensure_ascii=False).encode()
            self.send_response(200); self.send_header('Content-Type', kind)
            self.send_header('Content-Length', str(len(body))); self.end_headers()
            self.wfile.write(body)

    server = ThreadingHTTPServer(('127.0.0.1', 18787), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    sim = Simulator(a)
    try:
        exercise(sim, calls)
        (sim.output/'result.json').write_text(json.dumps({'result': 'passed', 'viewport': [a.width, a.height],
            'density_dpi': a.density_dpi, 'input': ['UP', 'DOWN', 'ENTER'], 'native_ime': True}, indent=2))
        print('PASS Gen4 three-key focus, result scrolling, route, zoom, roadbook, Host IME, settings and exit')
    finally:
        sim.close(); server.shutdown(); server.server_close()


if __name__ == '__main__':
    main()
