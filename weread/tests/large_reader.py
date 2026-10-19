#!/usr/bin/env python3
"""Large serializer/directory regression on the real PXA UI, synthetic data only.

8548 aliases, 2538 directory rows, 115612 body bytes and a 950096-byte page.
All sizes describe this generated fixture, not a copied book or account response.
"""
import argparse, base64, hashlib, io, json, tempfile, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit
from PIL import Image, ImageChops
from smoke import Simulator


def fixture(chapter=1):
    aliases = [f'a{i}' for i in range(8548)]
    book = {'bookId': '1', 'title': 'Large synthetic book', 'author': 'Fixture', 'lastChapterIdx': 2538}
    chapters = [{'chapterUid': i, 'chapterIdx': i, 'title': f'Chapter {i:04d} '+ 'title '*8}
                for i in range(1, 2539)]
    params = [{'cUid': i, 'param': f'fixture-{i}'} for i in range(1, 2539)]
    reader = {'bookInfo': book, 'curChapter': chapters[chapter-1], 'contentText': '',
              'chapterIndexes': chapters, 'readerUrlParams': params, 'chapterInfoCount': 2538,
              'curChapterUrlParam': {'param': f'fixture-{chapter}'}, 'chapterSections': [1],
              'curChapterSection': 0, 'prevChapterUrlParam': {}, 'nextChapterUrlParam': {}}
    state = json.dumps({'state': {'reader': reader, 'unused': 'PADDING'}}, separators=(',', ':'))
    script = '<script>window.__NUXT__=(function('+','.join(aliases)+'){return '+state+'}('+','.join(['0']*8548)+'));</script>'
    script = script.replace('PADDING', 'x'*(781098-len(script.encode())+7))
    assert len(script.encode()) == 781098
    body = '<p>'+('Readable paragraph. '*6000)[:115605]+'</p>'
    assert len(body.encode()) == 115612
    wire = '<div id="readerContentRenderContainer">'+body+'</div>'+script
    wire += '<!--'+ 'z'*(950096-len(wire.encode())-7)+'-->'
    assert len(wire.encode()) == 950096
    return book, wire.encode()


def small_fixture():
    book = {'bookId': '1', 'title': 'Small synthetic book', 'author': 'Fixture'}
    reader = {'bookInfo': book, 'curChapter': {'chapterUid': 1, 'title': 'Chapter'},
              'contentText': '', 'chapterIndexes': [{'chapterUid': 1, 'title': 'Chapter'}],
              'readerUrlParams': [{'cUid': 1, 'param': 'fixture-1'}], 'chapterInfoCount': 1,
              'curChapterUrlParam': {'param': 'fixture-1'}, 'chapterSections': [1]}
    wire = '<div id="readerContentRenderContainer"><p>'+('Small chapter text. '*1000)+'</p></div>'
    wire += '<script>window.__NUXT__=(function(a){return '+json.dumps({'state': {'reader': reader}})+'}(0));</script>'
    return book, wire.encode()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for field in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--'+field, type=Path, required=True)
    p.add_argument('--port', type=int, default=19178)
    p.add_argument('--cases', nargs='+', choices=('known', 'unknown', 'fragmented'), default=['known', 'unknown'])
    p.add_argument('--catalog', action='store_true')
    p.add_argument('--small', action='store_true', help='Same small workload for paired baseline memory measurements')
    a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    if a.small and a.catalog: p.error('--small does not have multiple catalog windows')
    book, payload = small_fixture() if a.small else fixture(); calls = []; case = 'known'; errors = []
    picture = io.BytesIO(); Image.new('RGB', (40, 40), 'white').save(picture, 'PNG')
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_): pass
        def reply(self, value, cookies=False):
            raw = json.dumps(value).encode(); self.send_response(200)
            self.send_header('Content-Length', str(len(raw)))
            if cookies:
                self.send_header('Set-Cookie', 'wr_vid=1; Path=/')
                self.send_header('Set-Cookie', 'wr_skey=fixture-only; Path=/')
            self.end_headers(); self.wfile.write(raw)
        def do_GET(self):
            path = urlsplit(self.path).path
            try:
                if path.endswith('/getuid'): self.reply({'uid': 'fixture'})
                elif path.endswith('/qrcode'):
                    self.reply({'succ': 1, 'data': 'data:image/png;base64,'+base64.b64encode(picture.getvalue()).decode()})
                elif path.endswith('/shelfloadmore'):
                    self.reply({'books': [book], 'bookReaderUrls': [{'bId': '1', 'param': 'fixture-1'}], 'totalCount': 1, 'kkIdx': 1})
                elif path.endswith('/reader'):
                    assert 'wr_skey=fixture-only' in self.headers.get('Cookie', '')
                    query = parse_qs(urlsplit(self.path).query)
                    assert query['bc'] == ['fixture-1']
                    calls.append(case); self.send_response(200); self.send_header('Content-Type', 'text/html')
                    if case == 'unknown': self.send_header('Connection', 'close')
                    else: self.send_header('Content-Length', str(len(payload)))
                    self.end_headers()
                    size = 257 if case == 'fragmented' else 4096
                    for i in range(0, len(payload), size):
                        self.wfile.write(payload[i:i+size])
                    if case == 'unknown': self.close_connection = True
                else: raise AssertionError('Unexpected endpoint '+path)
            except (BrokenPipeError, ConnectionResetError): pass
            except Exception as error: errors.append(str(error))
        def do_POST(self):
            path = urlsplit(self.path).path; self.rfile.read(int(self.headers.get('Content-Length', 0)))
            if path.endswith('/getlogininfo'): self.reply({'vid': 1, 'skey': 'fixture-only', 'code': 'fixture-only'})
            elif path.endswith('/weblogin'): self.reply({'vid': 1, 'accessToken': 'fixture-only', 'refreshToken': 'fixture-only'}, True)
            else: errors.append('Unexpected POST'); self.reply({})
    server = ThreadingHTTPServer(('127.0.0.1', a.port), Handler); server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start(); results = []
    try:
        with tempfile.TemporaryDirectory(prefix='weread-large-reader-') as directory:
            for case in a.cases:
                state = Path(directory)/case; state.mkdir()
                app = Simulator(a, a.output/case, state, 296, 240, 160, 58)
                try:
                    app.wait(lambda: 'WEREAD SHELF READY' in app.logs())
                    auth = next(state.rglob('session.json')); original = auth.read_bytes()
                    before = app.wait_memory(lambda x: x.get('linear_current_bytes', 0)>0)
                    start = time.monotonic(); app.tap(120, 83)
                    app.wait(lambda: 'WEREAD READER READY' in app.logs() or 'WEREAD ERROR' in app.logs() or 'HYDRATION ERROR' in app.logs() or 'Guest exception:' in app.logs(), 60)
                    elapsed = time.monotonic()-start
                    assert 'WEREAD READER READY' in app.logs(), app.logs()[-2500:]
                    memory = app.wait_memory(lambda x: x.get('linear_current_bytes', 0)>0)
                    first = app.shot('first'); mark = len(calls); app.tap(260, 120); second = app.shot('second')
                    assert ImageChops.difference(Image.open(first).convert('RGB'), Image.open(second).convert('RGB')).getbbox()
                    assert len(calls) == mark, 'Local page turn performed a network request'
                    if a.catalog and case == a.cases[0]:
                        app.tap(148, 120); app.tap(76, 116)
                        app.shot('catalog-first')
                        # Two rows per compact screen; the 151st page opens
                        # the next 300-row window through the original reader URL.
                        for _ in range(150): app.tap(148, 203)
                        app.wait(lambda: 'WEREAD CATALOG READY' in app.logs(), 60)
                        assert 'offset=300' in app.logs(), app.logs()[-1000:]
                        app.shot('catalog-window-two')
                        count = app.logs().count('WEREAD CATALOG READY')
                        app.tap(60, 203)
                        app.wait(lambda: app.logs().count('WEREAD CATALOG READY') > count, 60)
                        assert 'offset=0' in app.logs()
                        app.shot('catalog-window-one-restored')
                    assert auth.read_bytes() == original and not errors, errors
                    app.command('KEY HOME'); app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    results.append({'case': case, 'ready_seconds': elapsed, 'before': before, 'reading': memory,
                                    'first_png_sha256': hashlib.sha256(first.read_bytes()).hexdigest(),
                                    'fixture_directory_count': 1 if a.small else 2538,
                                    'catalog_windows_roundtrip': a.catalog and case == a.cases[0],
                                    'no_page_turn_network': True, 'released': True})
                except BaseException as error:
                    try: app.shot('failure')
                    except Exception: pass
                    results.append({'case': case, 'failed': str(error), 'logs': app.logs()[-3000:]})
                    raise
                finally:
                    app.close(); (a.output/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    finally: server.shutdown(); server.server_close()
    print(json.dumps(results, indent=2))


if __name__ == '__main__': main()
