"""Bounded textarea forms with independently checked wire data."""
from collections import Counter
from html import escape
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from threading import Thread
from urllib.parse import urlencode, urlsplit

INITIAL = 'Initial & <b>\nSecond'
EDITED = '编辑 A & <b>\n第二行 + %'
PASS = '<title>Textarea PASS</title><p style="color:#187a42">多行表单通过 / Textarea PASS</p>'


def expected(name):
    if name == 'plain-limit': return [('q', 'x'*512), ('m', ''), ('go', 'send')]
    if name == 'switch': return [('m', EDITED), ('p', 'private'), ('go', 'send')]
    value = {'initial': INITIAL, 'post-edit': EDITED, 'get-edit': EDITED,
             'script-preset': 'Script\nPreset', 'script-edit': EDITED+'\nHandled',
             'boundary': 'x'*510+'ab'}[name]
    proof = 'edited' if name == 'script-edit' else 'script' if name.startswith('script-') else 'ok'
    return [('m',value), ('r','Read\nOnly'), ('h','Hidden\nValue'), ('proof',proof), ('go','send')]


def page(name):
    method = 'get' if name == 'get-edit' else 'post'
    query = 'stale=1' if method == 'get' else 'kept=1'
    opening = f'<title>多行输入</title><form id=f method={method} action="/verify/{name}?{query}#result">'
    if name == 'plain-limit':
        return opening+'<input name=q placeholder=Plain><textarea name=m></textarea><button name=go value=send>Send</button></form>'
    if name == 'switch':
        return opening+'<textarea name=m aria-label=Multiline></textarea><input type=password name=p placeholder=Password><button name=go value=send>Send</button></form>'
    if name == 'overflow':
        return opening+'<textarea hidden name=m>'+'x'*513+'</textarea><button name=go value=send>Send</button></form>'
    initial = INITIAL if name in ('initial','script-preset') else ''
    body = opening+'<label>Message<textarea id=m name=m>\n'+escape(initial)+'</textarea></label>'
    body += '''<textarea name=r readonly style="color:#187a42">Read
Only</textarea><textarea hidden name=h>Hidden
Value</textarea><textarea name=skip disabled>Skip</textarea>
<input id=proof type=hidden name=proof value=ok><button name=go value=send style="color:#d06020">Send</button></form><p id=status></p>'''
    if name.startswith('script-'):
        body += '<script>const t=document.getElementById("m"),f=document.getElementById("f"),proof=document.getElementById("proof");'
        if name == 'script-preset':
            body += 't.value="Script\\nPreset";proof.value="script";f.onsubmit=()=>{if(t.defaultValue!=='+json.dumps(INITIAL)+' )throw Error("default value")};'
        else:
            body += 't.oninput=()=>{if(t.defaultValue!=="" || t.value!=='+json.dumps(EDITED)+')throw Error("input value");t.value+="\\nHandled"};'
            body += 't.onchange=()=>{proof.value="edited";document.getElementById("status").textContent="Events PASS"};'
        body += '</script>'
    return body


class Fixture:
    def __init__(self):
        self.requests=Counter();self.submissions=[];fixture=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_): pass
            def request_page(self,method):
                url=urlsplit(self.path);fixture.requests[self.path]+=1
                body=self.rfile.read(int(self.headers.get('Content-Length','0'))) if method=='post' else b''
                if url.path.startswith('/verify/'):
                    name=url.path.rsplit('/',1)[-1]
                    pairs=expected(name) if name!='overflow' else []
                    wire=urlencode([(key,value.replace('\n','\r\n')) for key,value in pairs])
                    valid=(name!='overflow' and method==('get' if name=='get-edit' else 'post') and
                           (url.query==wire if method=='get' else url.query=='kept=1' and body.decode()==wire and
                            self.headers.get('Content-Type','').split(';')[0]=='application/x-www-form-urlencoded'))
                    fixture.submissions.append(dict(case=name,method=method,path=self.path,body=body.decode(),valid=valid))
                    text,status=(PASS,200) if valid else ('Textarea data FAIL',422)
                else: text,status=page(url.path.strip('/')),200
                data=text.encode();self.send_response(status)
                self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(data)))
                self.end_headers()
                try:self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):pass
            def do_GET(self):self.request_page('get')
            def do_POST(self):self.request_page('post')
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    def start(self):
        self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
