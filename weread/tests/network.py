#!/usr/bin/env python3
"""Transport failures before/after headers, retry and cancellation in PXA.

The local fixture only uses synthetic credentials. Closing a TCP connection
without an HTTP reply exercises the transport error path, unlike HTTP 500.
"""
import argparse
import base64
import io
import json
import socket
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

from PIL import Image
from smoke import Simulator


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--port', type=int, default=18934)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    picture = io.BytesIO()
    Image.new('RGB', (80, 80), 'white').save(picture, 'PNG')
    mode = 'healthy'
    reached = threading.Event()
    release = threading.Event()
    calls = []
    failures = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def data(self, value, cookies=()):
            raw = json.dumps(value).encode()
            self.send_response(200)
            self.send_header('Content-Length', str(len(raw)))
            for cookie in cookies:
                self.send_header('Set-Cookie', cookie)
            self.end_headers()
            try:
                self.wfile.write(raw)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_GET(self):
            path = urlsplit(self.path).path
            calls.append(path)
            if path.endswith('/getuid'):
                self.data({'uid': 'fixture'})
            elif path.endswith('/qrcode'):
                self.data({'succ': 1, 'data': 'data:image/png;base64,' +
                           base64.b64encode(picture.getvalue()).decode()})
            elif path.endswith('/shelfloadmore'):
                assert 'wr_skey=fixture-only' in self.headers.get('Cookie', '')
                reached.set()
                if mode == 'close-before-headers':
                    self.close_connection = True
                    self.connection.shutdown(socket.SHUT_RDWR)
                elif mode == 'truncated-body':
                    self.send_response(200)
                    self.send_header('Content-Length', '2048')
                    self.end_headers()
                    self.wfile.write(b'{"books":[')
                    self.wfile.flush()
                    self.close_connection = True
                elif mode == 'cancel-before-headers':
                    release.wait(10)
                else:
                    self.data({'books': [], 'bookReaderUrls': [],
                               'totalCount': 0, 'kkIdx': 1})
            else:
                failures.append('unexpected GET')
                self.data({})

        def do_POST(self):
            path = urlsplit(self.path).path
            self.rfile.read(int(self.headers.get('Content-Length', 0)))
            if path.endswith('/getlogininfo'):
                self.data({'vid': 1, 'skey': 'fixture-only', 'code': 'fixture-only'})
            elif path.endswith('/weblogin'):
                self.data({'vid': 1, 'accessToken': 'fixture-only',
                           'refreshToken': 'fixture-only'},
                          ('wr_vid=1; Path=/', 'wr_skey=fixture-only; Path=/'))
            else:
                failures.append('unexpected POST')
                self.data({})

    def start_server():
        server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
        server.daemon_threads = True
        threading.Thread(target=server.serve_forever, daemon=True).start()
        return server

    server = start_server()
    refusing = None
    results = []
    try:
        with tempfile.TemporaryDirectory(prefix='weread-network-') as directory:
            for case in ('close-before-headers', 'truncated-body',
                         'cancel-before-headers', 'connection-refused'):
                state = Path(directory) / case
                state.mkdir()
                mode = 'healthy'
                app = Simulator(args, args.output / (case + '-seed'), state,
                                296, 240, 160, 58)
                try:
                    app.wait(lambda: 'WEREAD SHELF READY' in app.logs())
                    app.command('KEY HOME')
                    app.process.wait(timeout=10)
                finally:
                    app.close()
                auth = next(state.rglob('session.json'))
                original = auth.read_bytes()
                mode = case
                reached.clear()
                release.clear()
                calls.clear()
                if case == 'connection-refused':
                    server.shutdown()
                    server.server_close()
                    # Keep the port reserved without listening: the actual
                    # connect() fails before any HTTP handler can run.
                    refusing = socket.socket()
                    refusing.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                    refusing.bind(('127.0.0.1', args.port))
                app = Simulator(args, args.output / case, state, 296, 240, 160, 58)
                try:
                    if case != 'connection-refused':
                        app.wait(reached.is_set)
                    if case == 'cancel-before-headers':
                        app.command('KEY HOME')
                        app.process.wait(timeout=10)
                        release.set()
                    else:
                        app.wait(lambda: 'WEREAD ERROR' in app.logs(), timeout=5)
                        assert 'WEREAD SHELF READY' not in app.logs()
                        app.shot('error')
                        mode = 'healthy'
                        if refusing is not None:
                            assert not calls, 'Refused connection reached an HTTP handler'
                            refusing.close()
                            refusing = None
                            server = start_server()
                        # The shelf footer is an explicit retry even when empty.
                        app.tap(148, 204)
                        app.wait(lambda: 'WEREAD SHELF READY' in app.logs())
                        app.shot('retry')
                        app.command('KEY HOME')
                        app.process.wait(timeout=10)
                    assert auth.read_bytes() == original, 'Transport failure changed authentication'
                    assert all(path.endswith('/shelfloadmore') for path in calls), calls
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    results.append({'case': case, 'passed': True,
                                    'authentication_unchanged': True,
                                    'resources_released': True})
                finally:
                    release.set()
                    app.close()
        assert not failures, failures
    finally:
        release.set()
        if refusing is not None:
            refusing.close()
        server.shutdown()
        server.server_close()
        (args.output / 'results.json').write_text(
            json.dumps({'passed': results}, indent=2) + '\n')
    print('Four transport failure/cancellation/retry cases passed')


if __name__ == '__main__':
    main()
