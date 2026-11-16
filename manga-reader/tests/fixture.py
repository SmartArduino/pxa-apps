"""Deterministic MangaDex-shaped HTTP fixture; images are original test panels.

Run: python tests/fixture.py --port 18897
Build only the test variant with --define 'MANGA_API_BASE="http://127.0.0.1:18897"'.
No fixtures or test-server dependency are included in the delivered app.
"""
import argparse
import io
import json
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock
from urllib.parse import urlsplit, parse_qs
from PIL import Image, ImageDraw

BOOK = '11111111-1111-1111-1111-111111111111'
CHAPTER = '22222222-2222-2222-2222-222222222222'


def panel(index):
    image = Image.new('RGB', (400, 800), '#f5f0df')
    draw = ImageDraw.Draw(image)
    for i in range(3):
        top = 20 + i * 255
        draw.rectangle((20, top, 380, top + 230), fill=['#ccddc2', '#d8dfeb', '#edcdb9'][index], outline='#243b31', width=5)
        draw.ellipse((70+i*20, top+65, 160+i*20, top+155), fill='#fff8e6', outline='#243b31', width=3)
        draw.text((40, top+20), f'PAGE {index+1} / PANEL {i+1}', fill='#20382f', font_size=22)
    out = io.BytesIO(); image.save(out, format='PNG'); return out.getvalue()


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if getattr(self.server, 'fail_once', False):
            with self.server.fault_lock:
                fail = self.path not in self.server.faulted
                self.server.faulted.add(self.path)
            if fail:
                self.send_response(503)
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
        u = urlsplit(self.path); q = parse_qs(u.query); root = f'http://127.0.0.1:{self.server.server_port}'
        content_type = 'application/json'; code = 200
        if u.path == '/manga':
            data = [] if q.get('title') == ['missing'] else [dict(id=BOOK, attributes={'title': {'zh': '触摸测试漫画'}})]
            body = {'result': 'ok', 'data': data, 'total': len(data)}
        elif u.path == f'/manga/{BOOK}/feed':
            body = {'data': [{'id': CHAPTER, 'attributes': {'chapter': '1', 'title': '出发', 'pages': 3}}], 'total': 1}
        elif u.path == f'/at-home/server/{CHAPTER}':
            body = {'baseUrl': root, 'chapter': {'hash': 'abc123', 'dataSaver': ['1.png', '2.png', '3.png']}}
        elif u.path.startswith('/data-saver/abc123/'):
            content_type = 'image/png'; body = panel(int(u.path[-5])-1)
        elif u.path == '/catalog.json':
            body = {'books': [{'title': '自定义漫画', 'chapters': '/toc.json'}]}
        elif u.path == '/toc.json':
            body = {'chapters': [{'title': '第一话', 'pages': '/pages.json'}]}
        elif u.path == '/pages.json':
            body = {'pages': ['/data-saver/abc123/1.png', '/data-saver/abc123/2.png']}
        elif u.path == '/slow':
            time.sleep(4); body = {'books': []}
        else:
            code = 404; body = {'error': 'not found'}
        if not isinstance(body, bytes): body = json.dumps(body, ensure_ascii=False).encode()
        self.send_response(code); self.send_header('Content-Type', content_type); self.send_header('Content-Length', str(len(body))); self.end_headers()
        try: self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError): pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=18897)
    parser.add_argument('--fail-once', action='store_true', help='Return 503 once per URL to verify transient recovery')
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    server.fail_once = args.fail_once
    server.fault_lock = Lock()
    server.faulted = set()
    server.serve_forever()
