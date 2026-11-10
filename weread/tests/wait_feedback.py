#!/usr/bin/env python3
"""Hold actual requests to verify visible, static waiting feedback and recovery."""
import argparse
import base64
import io
import json
import math
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

from PIL import Image, ImageChops
from smoke import Simulator


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--port', type=int, default=18934)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    qr = io.BytesIO()
    Image.new('RGB', (80, 80), 'white').save(qr, 'PNG')
    entered = threading.Event()
    release = threading.Event()
    target = 1
    fail = False
    calls = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def reply(self, value, status=200, cookies=False):
            data = value.encode() if isinstance(value, str) else json.dumps(value).encode()
            self.send_response(status)
            self.send_header('Content-Length', str(len(data)))
            if cookies:
                self.send_header('Set-Cookie', 'wr_vid=1; Path=/')
                self.send_header('Set-Cookie', 'wr_skey=fixture-only; Path=/')
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_GET(self):
            url = urlsplit(self.path)
            if url.path.endswith('/getuid'):
                self.reply({'uid': 'fixture'})
            elif url.path.endswith('/qrcode'):
                self.reply({'succ': 1, 'data': 'data:image/png;base64,' + base64.b64encode(qr.getvalue()).decode()})
            elif url.path.endswith('/shelfloadmore'):
                self.reply({'books': [{'bookId': '1', 'title': '等待提示测试'}],
                            'bookReaderUrls': [{'bId': '1', 'param': 'fixture-1'}], 'totalCount': 1, 'kkIdx': 1})
            elif url.path.endswith('/reader'):
                uid = int(parse_qs(url.query)['bc'][0].split('-')[1])
                calls.append(uid)
                if uid == target:
                    rejected = fail
                    entered.set()
                    assert release.wait(12), 'Test did not release its held request'
                    if rejected:
                        self.reply({}, 503)
                        return
                reader = {
                    'bookInfo': {'bookId': '1', 'title': '等待提示测试', 'lastChapterIdx': 2},
                    'curChapter': {'chapterUid': uid, 'chapterIdx': uid, 'title': f'Chapter {uid}'},
                    'curChapterUrlParam': {'param': f'fixture-{uid}'},
                    'prevChapterUrlParam': {'param': 'fixture-1'} if uid == 2 else {},
                    'contentText': '<p>' + (f'Chapter {uid} readable paragraph. ' * 80) + '</p>',
                    'chapterIndexes': [{'chapterUid': i, 'chapterIdx': i, 'title': f'Chapter {i}'} for i in (1, 2)],
                    'readerUrlParams': [{'cUid': i, 'param': f'fixture-{i}'} for i in (1, 2)],
                    'chapterInfoCount': 2, 'chapterSections': [1], 'curChapterSection': 0,
                }
                self.reply('<script>window.__NUXT__=(function(a){return ' +
                           json.dumps({'state': {'reader': reader}}) + '}(0));</script>')
            else:
                self.reply({}, 404)

        def do_POST(self):
            self.rfile.read(int(self.headers.get('Content-Length', 0)))
            if self.path.split('?')[0].endswith('/getlogininfo'):
                self.reply({'vid': 1, 'skey': 'fixture-only', 'code': 'fixture-only'})
            else:
                self.reply({'vid': 1, 'accessToken': 'fixture-only', 'refreshToken': 'fixture-only'}, cookies=True)

    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    results = []
    try:
        with tempfile.TemporaryDirectory(prefix='weread-wait-', dir=args.output) as temporary:
            layouts = (('compact', 296, 240, 160, 58, False), ('hidpi', 480, 480, 280, 56, False),
                       ('round', 240, 240, 160, 0, True), ('portrait', 480, 800, 240, 32, False))
            for name, width, height, dpi, corner, circular in layouts:
                calls.clear()
                target = 1
                fail = False
                entered.clear()
                release.clear()
                state = Path(temporary) / name
                state.mkdir()
                app = Simulator(args, args.output / name, state, width, height, dpi, corner, circular)
                inset = math.ceil((min(width, height) / 2 if circular else corner) * (1 - 2**-.5))
                dp = lambda n: max(1, round(n * dpi / 160))
                safe_width, safe_height = width - 2 * inset, height - 2 * inset
                head, foot, gap = min(dp(44), safe_height // 5), min(dp(40), safe_height // 5), max(4, dp(8))
                rows = max(1, min(10, (safe_height - dp(100)) // max(dp(48), 32)))
                row_height = (safe_height - head - foot - gap) // rows
                changes = []

                def waiting(label, baseline, footer=False):
                    app.wait(entered.is_set)
                    first = app.shot(label)
                    original = Image.open(baseline).convert('RGB')
                    picture = Image.open(first).convert('RGB')
                    bounds = ImageChops.difference(original, picture).getbbox()
                    if footer:
                        top = height - inset - min(dp(24), safe_height // 7) - dp(2)
                        assert bounds and bounds[1] >= top, (name, label, bounds)
                        body = (0, 0, width, top)
                    else:
                        assert bounds and bounds[3] <= inset + head + dp(2), (name, label, bounds)
                        body = (0, inset + head + dp(2), width, height)
                    assert not ImageChops.difference(original.crop(body), picture.crop(body)).getbbox(), 'Waiting changed page content'
                    latest = app.logs().rsplit('WEREAD WAIT PRESENTED', 1)[-1].splitlines()[0]
                    assert 'partial=1 regions=1' in latest, ('Waiting submitted a full Canvas update', latest)
                    submissions = app.logs().count('WEREAD WAIT PRESENTED')
                    time.sleep(.5)
                    later = app.shot(label + '-held')
                    assert not ImageChops.difference(picture, Image.open(later).convert('RGB')).getbbox()
                    assert app.logs().count('WEREAD WAIT PRESENTED') == submissions, 'Unchanged waiting text was submitted again'
                    changes.append({'phase': label, 'pixel_bounds': bounds, 'changed_bounds_fraction':
                                    (bounds[2] - bounds[0]) * (bounds[3] - bounds[1]) / (width * height)})

                def contents(chapter):
                    app.tap(width // 2, height // 2)
                    menu_height = max(24, min(dp(42), (safe_height - head - foot - 3 * gap) // 3))
                    app.tap(inset + (safe_width - gap) // 4,
                            inset + head + menu_height + gap + menu_height // 2)
                    if rows == 1 and chapter == 2:
                        app.tap(width // 2, height - inset - foot // 2)

                def select(chapter):
                    app.tap(width // 2, inset + head + (chapter - 1 if rows > 1 else 0) * row_height + row_height // 2)

                try:
                    app.wait(lambda: 'WEREAD SHELF READY' in app.logs())
                    before = app.shot('before-opening')
                    app.tap(width // 2, inset + head + row_height // 2)
                    waiting('opening', before)
                    release.set()
                    app.wait(lambda: app.logs().count('WEREAD READER READY') == 1)
                    app.shot('opened')

                    target = 2
                    entered.clear()
                    release.clear()
                    contents(2)
                    before = app.shot('before-jumping')
                    select(2)
                    waiting('jumping', before)
                    select(2)
                    assert calls.count(2) == 1, 'Repeated taps started another jump'
                    release.set()
                    app.wait(lambda: app.logs().count('WEREAD READER READY') == 2)
                    readable = app.shot('jumped')

                    target = 1
                    entered.clear()
                    release.clear()
                    contents(1)
                    if rows == 1:
                        app.tap(inset + safe_width // 6, height - inset - foot // 2)
                    before = app.shot('before-cancelling')
                    select(1)
                    waiting('cancelling', before)
                    app.tap(inset + safe_width * 5 // 6, height - inset - foot // 2)
                    release.set()
                    time.sleep(.3)
                    assert app.logs().count('WEREAD READER READY') == 2
                    cancelled = app.shot('cancelled')
                    assert not ImageChops.difference(Image.open(readable), Image.open(cancelled)).getbbox()

                    fail = True
                    entered.clear()
                    release.clear()
                    contents(1)
                    if rows == 1:
                        app.tap(inset + safe_width // 6, height - inset - foot // 2)
                    before = app.shot('before-failing')
                    select(1)
                    waiting('failing', before)
                    release.set()
                    app.wait(lambda: 'WEREAD READING FAILED' in app.logs())
                    time.sleep(2.5)
                    failed = app.shot('failed')
                    assert not ImageChops.difference(Image.open(before), Image.open(failed)).getbbox()
                    cleared = app.logs().rsplit('WEREAD WAIT CLEARED', 1)[-1].splitlines()[0]
                    assert 'partial=1 regions=1' in cleared, ('Removing the hint redrew the full Canvas', cleared)
                    app.tap(inset + safe_width * 5 // 6, height - inset - foot // 2)
                    assert app.logs().count('WEREAD READER READY') == 2
                    fail = False
                    entered.clear()
                    release.clear()
                    before = app.shot('before-previous-reading')
                    app.tap(inset + dp(12), height // 2)
                    waiting('previous-reading', before, footer=True)
                    release.set()
                    app.wait(lambda: app.logs().count('WEREAD READER READY') == 3)
                    app.command('KEY HOME')
                    app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    assert 'DRAW ERROR' not in app.logs() and 'Guest exception:' not in app.logs()
                    results.append({'layout': name, 'opening': True, 'jump': True, 'cancel': True, 'failure': True,
                                    'status_changes': changes})
                finally:
                    release.set()
                    app.close()
    finally:
        release.set()
        server.shutdown()
        server.server_close()
    (args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('Local waiting updates, unchanged page content, duplicate suppression and recovery passed on four layouts')


if __name__ == '__main__':
    main()
