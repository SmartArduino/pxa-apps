#!/usr/bin/env python3
"""Pair signed packages in the real PXA simulator, without changing budgets.

Date.now measures accumulated kernel time at millisecond resolution. Preparation
and restoration run in bounded callbacks outside that timing. These workloads
do not measure device speed, displayed FPS or whole-page load time.
"""
import argparse
import hashlib
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Thread
from urllib.parse import parse_qs, urlsplit

from harness import Browser

CASES = ('selected-single', 'selected-many', 'fragment-front', 'fragment-middle', 'fragment-boundary')


def page(case, token):
    assert case in CASES
    body = '<p id=proof>Profiling</p>'
    if case.startswith('selected-'):
        body += '<select id=s>' + ''.join(
            '<option value=v' + str(i) + '>Chapter ' + str(i) + '</option>'
            for i in range(32)) + '</select>'
    script = 'const token=' + json.dumps(token) + ';let n=0,elapsed=0,sum=0;'
    if case.startswith('selected-'):
        reads = 12 if case == 'selected-many' else 1
        script += '''const s=document.getElementById('s'),options=s.options;
function run(){let t=Date.now();for(let i=0;i<READS;i++)sum+=options[(n*READS+i)%32].selected?1:0;
elapsed+=Date.now()-t;if(++n<32)setTimeout(run,0);else finish()}setTimeout(run,0);
'''.replace('READS', str(reads))
    else:
        count, existing, rounds = (192, 0, 2) if case == 'fragment-boundary' else (64, 128, 32) if case == 'fragment-middle' else (96, 96, 32)
        script += '''const count=COUNT,existing=EXISTING,rounds=ROUNDS;
const f=new DocumentFragment(),host=document.createElement('div'),roots=[];
const source=f.childNodes,destination=host.childNodes,total=Math.max(count,existing);let prepared=0,restored=0;
function check(v){if(!v)throw Error('batch invariant')}
function prepare(){for(let k=0;k<12 && prepared<total;k++,prepared++){
 if(prepared<existing)host.appendChild(document.createTextNode(''));
 if(prepared<count){const node=count===192?document.createTextNode(''):document.createElement('p');
 if(count!==192)node.textContent='Chapter '+prepared;roots.push(node);f.appendChild(node)}
}setTimeout(prepared<total?prepare:run,0)}
function run(){const index=MIDDLE?64:0,before=destination[index]||null,prefix=destination[0];let t=Date.now();host.insertBefore(f,before);
elapsed+=Date.now()-t;check(source===f.childNodes && source.length===0 && destination===host.childNodes);
check(destination.length===existing+count);
for(let i=0;i<count;i++)check(destination[index+i]===roots[i] && roots[i].parentNode===host);
if(index)check(destination[0]===prefix);if(before)check(destination[index+count]===before);sum+=count;restored=0;setTimeout(restore,0)}
function restore(){for(let k=0;k<12 && restored<count;k++,restored++)f.appendChild(roots[restored]);
if(restored<count){setTimeout(restore,0);return}
check(source.length===count && destination.length===existing);
if(++n<rounds)setTimeout(run,0);else finish()}setTimeout(prepare,0);
'''.replace('COUNT', str(count)).replace('EXISTING', str(existing)).replace('ROUNDS', str(rounds)).replace('MIDDLE', 'true' if case == 'fragment-middle' else 'false')
    script += '''function finish(){let x=new XMLHttpRequest();x.open('GET','/result?id='+token+'&ms='+elapsed+'&sum='+sum);x.send();document.getElementById('proof').textContent='Profile complete'}'''
    return (body + '<script>' + script + '</script>').encode()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'before-package', 'after-package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--rounds', type=int, default=2)
    a = p.parse_args()
    if not 1 <= a.rounds <= 20:
        p.error('--rounds must be between 1 and 20')
    packages = {'before': a.before_package, 'after': a.after_package}
    identities = []
    for package in packages.values():
        artifacts = list((package / 'artifacts').glob('main.*.aot'))
        if len(artifacts) != 1:
            p.error('Each package must contain one UI AOT artifact')
        identities.append(tuple(hashlib.sha256(path.read_bytes()).hexdigest() for path in
                                (artifacts[0], package / 'assets/browser-runtime.qjb')))
    if identities[0] != identities[1]:
        p.error('UI AOT and core runtime bytecode must be identical')
    a.output.mkdir(parents=True, exist_ok=True)
    results, rows = {}, []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            url = urlsplit(self.path)
            q = parse_qs(url.query)
            if url.path == '/result':
                results[q['id'][0]] = dict(ms=int(q['ms'][0]), sum=int(q['sum'][0]))
                data = b''
            elif url.path == '/bench':
                data = page(q['case'][0], q['id'][0])
            else:
                data = b'<p>Ready</p>'
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError):
                pass

    # TRACE packages have this same loopback startup URL. Keep one shared server.
    server = ThreadingHTTPServer(('127.0.0.1', 31878), Handler)
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        for run in range(a.rounds):
            order = list(packages) if run % 2 == 0 else list(reversed(packages))
            for label in order:
                output = a.output / (label + '-' + str(run))
                b = Browser(a.simulator, packages[label], a.publisher_key, a.ime_package,
                            output, width=480, height=480, corner=0)
                try:
                    b.wait(lambda: 'BROWSER VIEWPORT' in b.logs())
                    for case in CASES:
                        token = label + '-' + str(run) + '-' + case
                        mark = len(b.logs())
                        b.navigate('http://127.0.0.1:31878/bench?case=' + case + '&id=' + token)
                        b.wait(lambda: token in results or 'BROWSER JS ERROR' in b.logs()[mark:], timeout=90)
                        logs = b.logs()[mark:]
                        assert 'PXA Guest exception:' not in logs and 'BROWSER DRAW ERROR' not in logs, logs[-3000:]
                        row = dict(package=label, round=run, case=case)
                        if token not in results:
                            assert label == 'before' and case == 'selected-many', logs[-3000:]
                            assert 'BROWSER JS ERROR 脚本执行超时' in logs, logs[-3000:]
                            row['timed_out'] = True
                        else:
                            assert 'BROWSER JS ERROR' not in logs, logs[-3000:]
                            expected = {'selected-single': 1, 'selected-many': 12,
                                        'fragment-front': 3072, 'fragment-middle': 2048,
                                        'fragment-boundary': 384}[case]
                            assert results[token]['sum'] == expected, (case, results[token])
                            assert results[token]['ms'] >= 0
                            row.update(results[token], timed_out=False)
                            b.wait(lambda: 'BROWSER JS UPDATE revision=2 ' in b.logs()[mark:])
                        b.shot(case)
                        rows.append(row)
                        (a.output / 'results.json').write_text(json.dumps(
                            dict(ui_aot_sha256=identities[0][0], core_runtime_sha256=identities[0][1],
                                 rows=rows, note=__doc__), indent=2) + '\n')
                        print(json.dumps(row), flush=True)
                    stop = len(b.logs())
                    b.command('KEY HOME')
                    b.wait(lambda: 'PXA RESOURCE BUDGET' in b.logs()[stop:])
                    budget = [s for s in b.logs()[stop:].splitlines() if 'PXA RESOURCE BUDGET' in s][-1]
                    assert 'remaining_internal=0 remaining_external=0' in budget, budget
                finally:
                    b.close()
                    assert not b.forced_shutdown
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == '__main__':
    main()
