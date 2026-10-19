"""Compare interpreter loops in two signed TRACE packages with identical UI AOT.

Run after builds finish. Date.now has millisecond resolution; these small loop
checks do not measure displayed FPS, input latency or hardware memory.
"""
import argparse
import hashlib
import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit, parse_qs

from harness import Browser

parser = argparse.ArgumentParser(description=__doc__)
for name in ('simulator', 'before-package', 'after-package', 'publisher-key', 'ime-package', 'output'):
    parser.add_argument('--' + name, type=Path, required=True)
parser.add_argument('--rounds', type=int, default=5)
args = parser.parse_args()
if not 1 <= args.rounds <= 100:
    parser.error('--rounds must be between 1 and 100')
packages = {'before': args.before_package, 'after': args.after_package}
ui_hashes = []
for package in packages.values():
    artifacts = list((package/'artifacts').glob('main.*.aot'))
    if len(artifacts) != 1:
        parser.error('Each package must contain exactly one UI AOT artifact')
    ui_hashes.append(hashlib.sha256(artifacts[0].read_bytes()).hexdigest())
if len(set(ui_hashes)) != 1:
    parser.error('UI AOT must be identical for a controlled comparison')
results = {}
programs = {
    'sparse-splice': ('var a=new Array(100000);a[8]="x";',
                      'a.splice(1,1);s=a[7]==="x"?1:0', 1, 1),
    'integer': ('', 's=(s+(i&7))|0', 4000, sum(i & 7 for i in range(4000))),
    'coercion': ('var o={valueOf(){return 7}};', 's+=(+o)+1', 250, 2000),
}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        p = urlsplit(self.path)
        args = parse_qs(p.query)
        if p.path == '/bench':
            token, kind = args['id'][0], args['kind'][0]
            setup, expression, count, _ = programs[kind]
            body = (f'<p id=a>Running</p><script>{setup}'
                    f'var t=Date.now(),s=0;for(var i=0;i<{count};i=i+1){{{expression}}}'
                    f"var elapsed=Date.now()-t;fetch('/result?id={token}&ms='+elapsed+'&sum='+s);"
                    "document.getElementById('a').textContent='Done';</script>").encode()
        elif p.path == '/result':
            results[args['id'][0]] = {'ms': int(args['ms'][0]), 'sum': int(args['sum'][0])}
            body = b'{}'
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
out = args.output
out.mkdir(parents=True, exist_ok=True)
rows = []
try:
    for round in range(args.rounds):
        for pkg in list(packages) if round % 2 == 0 else list(reversed(packages)):
            browser = Browser(args.simulator, packages[pkg], args.publisher_key,
                              args.ime_package, out/f'{pkg}-{round}')
            try:
                browser.wait(lambda: 'BROWSER METRICS' in browser.logs())
                for kind in programs:
                    key = f'{pkg}-{round}-{kind}'
                    mark = len(browser.logs())
                    browser.navigate(f'http://127.0.0.1:{server.server_port}/bench?id={key}&kind={kind}')
                    browser.wait(lambda: key in results or 'Guest exception' in browser.logs()[mark:]
                                 or 'BROWSER JS ERROR' in browser.logs()[mark:])
                    assert key in results, browser.logs()[-2500:]
                    assert results[key]['sum'] == programs[kind][3]
                    rows.append({'package': pkg, 'round': round, 'kind': kind, **results[key]})
                    print(json.dumps(rows[-1]), flush=True)
            finally:
                browser.close()
                assert not browser.forced_shutdown
finally:
    server.shutdown()
    (out/'result.json').write_text(json.dumps(rows, indent=2))
