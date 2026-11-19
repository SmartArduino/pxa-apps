"""Deterministic MangaDex-shaped HTTP fixture; images are original test panels.

Run: python tests/fixture.py --port 18897
Build only the test variant with --define 'MANGA_API_BASE="http://127.0.0.1:18897"'.
No fixtures or test-server dependency are included in the delivered app.
"""
import argparse
import io
import json
import time
import random
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock
from urllib.parse import urlsplit, parse_qs
from PIL import Image, ImageDraw

BOOK = '11111111-1111-1111-1111-111111111111'
CHAPTER = '22222222-2222-2222-2222-222222222222'
SECOND_BOOK = '33333333-3333-3333-3333-333333333333'
SECOND_CHAPTER = '44444444-4444-4444-4444-444444444444'
def chapter_id(index):return CHAPTER if index==0 else '55555555-5555-5555-5555-'+str(index).zfill(12)
PROGRESSIVE = False
NOISE = False
PANEL_HEIGHT = 640
FIRST_HEIGHT = 0
PANEL_WIDTH = 320
WEBP = False
WEBP_LOSSY = False


def panel(index):
    if NOISE:
        image = Image.frombytes('RGB', (320, 400), random.Random(index).randbytes(320*400*3))
        out = io.BytesIO(); image.save(out, format='PNG'); return out.getvalue()
    image = Image.new('RGB', (PANEL_WIDTH, FIRST_HEIGHT if index==0 and FIRST_HEIGHT else PANEL_HEIGHT), '#f5f0df')
    draw = ImageDraw.Draw(image)
    for i in range(3):
        top = 20 + i * 205
        draw.rectangle((12, top, 308, top + 190), fill=['#ccddc2', '#d8dfeb', '#edcdb9'][index%3], outline='#243b31', width=5)
        draw.ellipse((70+i*20, top+65, 160+i*20, top+155), fill='#fff8e6', outline='#243b31', width=3)
        draw.text((40, top+20), f'PAGE {index+1} / PANEL {i+1}', fill='#20382f', font_size=22)
    out = io.BytesIO(); image.save(out, format='WEBP',lossless=not WEBP_LOSSY,quality=95) if WEBP else image.save(out, format='JPEG',progressive=True) if PROGRESSIVE else image.save(out, format='PNG'); return out.getvalue()


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
        u = urlsplit(self.path);
        with self.server.fault_lock:self.server.counts[u.path]=self.server.counts.get(u.path,0)+1
        q = parse_qs(u.query); root = f'http://127.0.0.1:{self.server.server_port}'
        content_type = 'application/json'; code = 200
        if u.path == '/manga':
            data = [] if q.get('title') == ['missing'] else [dict(id=BOOK, attributes={'title': {'zh': '触摸测试漫画'}, 'tags': [{'attributes': {'name': {'en': 'Long Strip'}}}]})]
            if self.server.two_books and data:data.append(dict(id=SECOND_BOOK,attributes={'title':{'zh':'分页测试漫画'},'tags':[]}))
            body = {'result': 'ok', 'data': data, 'total': len(data)}
        elif u.path in [f'/manga/{BOOK}/feed',f'/manga/{SECOND_BOOK}/feed']:
            ids=[SECOND_CHAPTER] if SECOND_BOOK in u.path else [chapter_id(i) for i in range(self.server.chapter_count)]
            rows=[{'id':ident,'attributes':{'chapter':str(i+1),'title':'出发' if i==0 else '继续','pages':self.server.page_count}} for i,ident in enumerate(ids)]
            if q.get('order[chapter]')==['desc']:rows.reverse()
            start=int(q.get('offset',['0'])[0]);count=min(self.server.toc_batch,int(q.get('limit',['20'])[0]));body={'data':rows[start:start+count],'total':len(rows)}
        elif u.path in [f'/at-home/server/{chapter_id(i)}' for i in range(self.server.chapter_count)]+[f'/at-home/server/{SECOND_CHAPTER}']:
            index=next((i for i in range(self.server.chapter_count) if u.path.endswith(chapter_id(i))),0)
            if index:
                time.sleep(self.server.next_delay_ms/1000)
                if self.server.fail_next:
                    self.send_response(503);self.send_header('Content-Length','0');self.end_headers();return
            body = {'baseUrl': root, 'chapter': {'hash': 'def456' if SECOND_CHAPTER in u.path else 'abc123' if index==0 else 'abc'+format(123+index,'x'), 'dataSaver': [f'{i+1}.png' for i in range(self.server.page_count)]}}
        elif u.path == '/_recover':
            self.server.fail_next=False;body={'ok':True}
        elif u.path == '/_metrics':
            body = self.server.counts
        elif u.path == '/_timings':
            body = self.server.timings
        elif u.path.startswith(('/data-saver/abc123/','/data-saver/def456/','/data-saver/abc')):
            with self.server.fault_lock:self.server.timings.append({'path':u.path,'event':'start','time':time.monotonic()})
            if not u.path.endswith('/1.png'):time.sleep(self.server.delay_ms/1000)
            content_type = 'image/webp' if WEBP else 'image/jpeg' if PROGRESSIVE else 'image/png'; body = b'broken-image' if self.server.broken_first and u.path.endswith('/1.png') else panel(int(u.path.rsplit('/',1)[1].split('.')[0])-1)
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
        if u.path.startswith('/data-saver/abc123/'):time.sleep(self.server.body_delay_ms/1000)
        try: self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError): pass
        if u.path.startswith('/data-saver/abc123/'):
            with self.server.fault_lock:self.server.timings.append({'path':u.path,'event':'end','time':time.monotonic()})


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=18897)
    parser.add_argument('--fail-once', action='store_true', help='Return 503 once per URL to verify transient recovery')
    parser.add_argument('--broken-first',action='store_true')
    parser.add_argument('--progressive',action='store_true')
    parser.add_argument('--pages',type=int,default=3)
    parser.add_argument('--noise',action='store_true',help='Use larger original panels to exercise persistent cache beyond 2 MiB')
    parser.add_argument('--height',type=int,default=640)
    parser.add_argument('--first-height',type=int,default=0)
    parser.add_argument('--width',type=int,default=320)
    parser.add_argument('--webp',action='store_true')
    parser.add_argument('--webp-lossy',action='store_true')
    parser.add_argument('--delay-ms',type=int,default=0)
    parser.add_argument('--body-delay-ms',type=int,default=0,help='Delay the image body after headers, for overlapping transfer tests')
    parser.add_argument('--two-books',action='store_true')
    parser.add_argument('--chapters',type=int,default=1)
    parser.add_argument('--toc-batch',type=int,default=20)
    parser.add_argument('--next-delay-ms',type=int,default=0)
    parser.add_argument('--fail-next',action='store_true')
    args = parser.parse_args()
    PROGRESSIVE = args.progressive
    NOISE = args.noise
    PANEL_HEIGHT = min(2048,max(64,args.height))
    FIRST_HEIGHT = min(2048,max(64,args.first_height)) if args.first_height else 0
    PANEL_WIDTH = min(1024,max(64,args.width))
    WEBP = args.webp or args.webp_lossy
    WEBP_LOSSY = args.webp_lossy
    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    server.fail_once = args.fail_once
    server.fault_lock = Lock()
    server.faulted = set()
    server.counts = {}
    server.timings = []
    server.broken_first = args.broken_first
    server.page_count = min(512,max(1,args.pages))
    server.delay_ms = min(5000,max(0,args.delay_ms))
    server.body_delay_ms = min(5000,max(0,args.body_delay_ms))
    server.two_books = args.two_books
    server.chapter_count=min(128,max(1,args.chapters))
    server.toc_batch=min(20,max(1,args.toc_batch))
    server.next_delay_ms=args.next_delay_ms
    server.fail_next=args.fail_next
    server.serve_forever()
