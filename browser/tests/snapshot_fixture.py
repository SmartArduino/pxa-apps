"""Bounded polling during incremental rendering, with server-side receipts."""
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from threading import Thread
import time
from urllib.parse import parse_qs,urlsplit

CASES=('noop','detached','changed','script-noop','script-change')


def page(case):
    assert case in CASES
    body='<button style="color:#d06020" onclick="start()">Start</button><p id=head style="color:#2468ac">Head 0</p>'
    body+='<p>Stable paragraph</p>'*40+'<p id=tail>Tail 0</p>'
    body+='''<script>
let serial=0,n=0,ghost=document.createElement('div');const mode='CASE';
function generation(){document.getElementById('head').textContent='Head '+(++serial);document.getElementById('tail').textContent='Tail '+serial}
function poll(){let x=new XMLHttpRequest();x.open('GET','/poll?case='+mode+'&n='+n);x.onload=()=>{++n;
 if(mode==='changed' && n===2)generation();
 if(mode==='detached'){ghost.textContent='Staged '+n;ghost.dataset.count=n}
 if(n<48)poll();else{let done=new XMLHttpRequest();done.open('GET','/done?case='+mode+'&n='+n);done.send()}
};x.send()}
function start(){generation();if(mode.indexOf('script-')===0){const script=document.createElement('script');script.src='/helper.js?case='+mode;script.onload=poll;document.body.appendChild(script)}else poll()}
</script>'''.replace('CASE',case)
    return body


class Fixture:
    def __init__(self):
        self.polls={c:[] for c in CASES};self.done={};self.helpers=[];f=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def do_GET(self):
                url=urlsplit(self.path);args=parse_qs(url.query);case=args.get('case',[''])[0];kind='text/html; charset=utf-8'
                if url.path=='/poll':
                    f.polls[case].append(int(args['n'][0]));time.sleep(.04);body='ok'
                elif url.path=='/done':f.done[case]=int(args['n'][0]);body='ok'
                elif url.path=='/helper.js':
                    f.helpers.append(case);time.sleep(.12);body='generation()' if case=='script-change' else 'window.helperLoaded=true';kind='application/javascript'
                else:body=page(url.path.strip('/'))
                data=body.encode();self.send_response(200);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(data)));self.end_headers()
                try:self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):pass
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    def start(self):self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
