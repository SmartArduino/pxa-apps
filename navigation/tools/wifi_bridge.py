#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Forward a Linux host's real Wi-Fi geolocation to PXA; never fabricate a fix."""
import argparse
import hashlib
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import os
from pathlib import Path
import re
import secrets
import subprocess
import threading
import time
import urllib.error
import urllib.request

API = 'https://api.beacondb.net/v1/geolocate'
USER_AGENT = 'PXA-Navigation/0.4.0 (+https://github.com/SmartArduino/pxa-apps)'

def fields(line):
    result = []; part = []; escaped = False
    for char in line:
        if escaped: part.append(char); escaped = False
        elif char == '\\': escaped = True
        elif char == ':': result.append(''.join(part)); part = []
        else: part.append(char)
    if escaped: part.append('\\')
    return result + [''.join(part)]

def access_points(output):
    result = []; seen = set()
    for line in output.splitlines():
        row = fields(line)
        if len(row) != 2: continue
        mac, ssid = row[0].lower(), row[1]
        if not re.fullmatch(r'(?:[0-9a-f]{2}:){5}[0-9a-f]{2}', mac): continue
        if int(mac[:2], 16) & 3 or not ssid or ssid.endswith('_nomap') or mac in seen: continue
        if len(ssid.encode('utf8')) > 32: continue
        seen.add(mac); result.append({'macAddress': mac, 'ssid': ssid})
        if len(result) == 10: break
    return result

def scan(interface):
    command = ['nmcli', '--terse', '--escape', 'yes', '--fields', 'BSSID,SSID', 'device', 'wifi', 'list', '--rescan', 'yes']
    if interface: command += ['ifname', interface]
    completed = subprocess.run(command, capture_output=True, text=True, timeout=20, check=True)
    points = access_points(completed.stdout)
    if len(points) < 2: raise ValueError('insufficient_access_points')
    return points

def locate(points):
    body = {'considerIp': False, 'fallbacks': {'ipf': False, 'lacf': False}, 'wifiAccessPoints': points}
    request = urllib.request.Request(API, data=json.dumps(body).encode(), headers={'Content-Type': 'application/json', 'User-Agent': USER_AGENT})
    with urllib.request.urlopen(request, timeout=15) as response:
        raw = response.read(65537)
        if len(raw) > 65536: raise ValueError('response_limit')
        data = json.loads(raw)
    if not isinstance(data, dict): raise ValueError('invalid_location')
    location = data.get('location', {})
    if not isinstance(location, dict): raise ValueError('invalid_location')
    lat, lng, accuracy = location.get('lat'), location.get('lng'), data.get('accuracy')
    for value in (lat, lng, accuracy):
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value): raise ValueError('invalid_location')
    if not -85 <= lat <= 85 or not -180 <= lng <= 180 or not 0 < accuracy <= 1000000 or data.get('fallback'):
        raise ValueError('invalid_location')
    return {'lat': lat, 'lng': lng, 'accuracy': accuracy, 'datum': 'wgs84', 'provider': 'host-wifi'}

class Latest:
    def __init__(self, clock=time.monotonic):
        self.clock = clock; self.lock = threading.Lock(); self.value = None; self.observed = 0
        self.session = secrets.token_hex(8); self.sequence = 0; self.error = 'location_unavailable'
    def publish(self, value, observed):
        with self.lock:
            self.sequence += 1; self.value = dict(value, sample_id=f'{self.session}:{self.sequence}'); self.observed = observed; self.error = 'ok'
    def clear(self, reason='location_unavailable'):
        with self.lock: self.value = None; self.error = reason
    def failure(self):
        with self.lock: return 'stale_location' if self.error == 'ok' else self.error
    def get(self):
        with self.lock:
            if self.value is None: return None
            age = max(0, int((self.clock() - self.observed) * 1000))
            if age > 30000: return None
            return dict(self.value, age_ms=age)

def refresh(latest, interface):
    observed = latest.clock()
    try: latest.publish(locate(scan(interface)), observed); return 'ok'
    except urllib.error.HTTPError as error:
        code = error.code; error.close(); status = 'no_coverage' if code == 404 else 'service_unavailable'; latest.clear(status); return status
    except (ValueError, OSError, subprocess.SubprocessError, urllib.error.URLError):
        latest.clear(); return 'location_unavailable'

def handler(latest, token):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_): pass
        def do_GET(self):
            if not hmac.compare_digest(self.path.encode('utf8'), ('/fix/' + token).encode('utf8')):
                self.send_error(404); return
            value = latest.get(); body = json.dumps(value or {'error': latest.failure()}).encode()
            self.send_response(200 if value else 503)
            self.send_header('Content-Type', 'application/json'); self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)
    return Handler

def private_file(path, value):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, 'w') as stream:
        os.fchmod(stream.fileno(), 0o600); stream.write(value)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--interface'); parser.add_argument('--bind', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8765); parser.add_argument('--interval', type=float, default=15)
    parser.add_argument('--token-file', type=Path); parser.add_argument('--endpoint-file', type=Path)
    parser.add_argument('--once', action='store_true', help='Print one real fix; return nonzero when no location is available')
    args = parser.parse_args()
    if args.interval < 10: parser.error('--interval must be at least 10 seconds')
    latest = Latest()
    if args.once:
        status = refresh(latest, args.interface); value = latest.get()
        print(json.dumps(value or {'error': status})); return 0 if value else 2
    if not args.token_file or not args.endpoint_file: parser.error('serve mode requires --token-file and --endpoint-file')
    token = secrets.token_hex(16); private_file(args.token_file, token + '\n')
    address = f'[{args.bind}]' if ':' in args.bind else args.bind
    private_file(args.endpoint_file, f'http://{address}:{args.port}/fix/{token}\n')
    server = ThreadingHTTPServer((args.bind, args.port), handler(latest, token))
    stopped = threading.Event()
    def worker():
        while not stopped.is_set():
            status = refresh(latest, args.interface)
            print(f'Wi-Fi position: {status}', flush=True)
            stopped.wait(args.interval)
    thread = threading.Thread(target=worker, daemon=True); thread.start()
    print(f'Endpoint saved to {args.endpoint_file}; press Ctrl-C to stop', flush=True)
    try: server.serve_forever()
    except KeyboardInterrupt: pass
    finally: stopped.set(); server.server_close()
    return 0
if __name__ == '__main__': raise SystemExit(main())
