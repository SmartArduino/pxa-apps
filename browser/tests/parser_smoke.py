#!/usr/bin/env python3
"""Exercise parser limits in the real PXA interpreter, including recovery.

Native sanitizers cannot model WAMR's separate operand stack. Each generated
parser case must raise one ordinary JS exception. Recursive runtime cases must
catch the expected stack error; supported cases must succeed. All must execute
the next script and render.
No website code or credentials are used. Each case uses a fresh simulator so
the complete result also identifies independent faults.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import threading

from harness import Browser
from parser_cases import cases, supported_cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--case', action='append', help='Run only these named cases (repeatable).')
    parser.add_argument('--runtime', action='store_true',
                        help='Also check recursive callbacks and VM semantics in the real interpreter.')
    args = parser.parse_args()
    sources = cases()
    expected_errors = dict.fromkeys(sources, 1)
    supported = supported_cases()
    if args.runtime:
        from runtime_cases import recursive_cases, semantic_cases, checked_recursion
        recursive = recursive_cases()
        sources.update({name: checked_recursion(source, proxy='proxy' in name)
                        for name, source in recursive.items()})
        expected_errors.update(dict.fromkeys(recursive, 0))
        supported.update(semantic_cases())
    sources.update(supported)
    expected_errors.update(dict.fromkeys(supported, 0))
    if args.case:
        unknown=set(args.case)-sources.keys()
        if unknown:parser.error('unknown cases: '+', '.join(sorted(unknown)))
        sources={name:sources[name] for name in dict.fromkeys(args.case)}

    recovered = set()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            if self.path.startswith('/recovered/') and self.path[11:] in sources:
                recovered.add(self.path[11:])
                self.send_response(204)
                self.end_headers()
                return
            source = sources.get(self.path[1:])
            if source is None:
                self.send_error(404)
                return
            body = ('<p id=a>Safe</p><script>' + source + ';</script>'
                    '<script>document.getElementById("a").textContent="Recovered";'
                    'fetch("/recovered/' + self.path[1:] + '");</script>').encode()
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    results = []
    try:
        for name in sources:
            browser = Browser(args.simulator, args.package, args.publisher_key,
                              args.ime_package, args.output / name)
            try:
                browser.wait(lambda: 'BROWSER METRICS' in browser.logs())
                browser.navigate(f'http://127.0.0.1:{server.server_port}/{name}')
                browser.wait(lambda: 'BROWSER JS READY' in browser.logs() or
                             'Guest exception' in browser.logs())
                assert 'Guest exception' not in browser.logs(), 'Guest trap'
                assert re.search(r'BROWSER JS READY[^\n]* errors=' + str(expected_errors[name]) + r'\b', browser.logs())
                browser.wait(lambda: 'BROWSER JS UPDATE revision=1' in browser.logs())
                browser.wait(lambda: name in recovered)
                browser.shot('recovered')
                result = {'case': name, 'passed': True}
            except Exception as error:
                result = {'case': name, 'passed': False, 'reason': str(error)[-500:]}
            finally:
                browser.close()
                if browser.forced_shutdown or 'Guest exception' in browser.logs():
                    result = {'case': name, 'passed': False, 'reason': 'Guest trap or blocked shutdown'}
            results.append(result)
            print(json.dumps(result), flush=True)
    finally:
        server.shutdown()
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output / 'result.json').write_text(json.dumps(results, indent=2))
    assert all(result['passed'] for result in results), results


if __name__ == '__main__':
    main()
