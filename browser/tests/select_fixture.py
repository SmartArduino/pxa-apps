"""Select menus with independent HTTP wire receipts, including delayed JS."""
from html import escape
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from threading import Event, Thread
import time
from urllib.parse import parse_qs, urlencode, urlsplit


def expected(name):
    if name in ('single-post','single-get'):return [('q','第二 & +'),('go','send')]
    if name in ('multi','script-multi'):return [('tag','b'),('tag','d'),('go','send')]
    if name=='script-duplicate':return [('q','same'),('go','send')]
    if name in ('early','script-dynamic'):return [('q','b'),('go','send')]
    if name=='scroll':return [('q','63'),('go','send')]
    if name=='hidden':return [('q',''),('q','hidden'),('go','send')]
    if name in ('disabled','none'):return [('go','send')]
    raise ValueError(name)


def page(name):
    scripted=name.startswith('script-') or name=='early'
    method='get' if name=='single-get' else 'post';query='stale=1' if method=='get' else 'keep=1'
    multiple=name in ('multi','script-multi');extra=' multiple' if multiple else ' disabled' if name=='disabled' else ' size=2' if name=='none' else ''
    handlers=''
    if scripted:
        code=("this._seq=(this._seq||0)+1;var x=new XMLHttpRequest();x.open('GET','/record?case="+name+
              "&seq='+this._seq+'&event='+event.type+'&index='+this.selectedIndex+'&value='+encodeURIComponent(this.value));x.send();"
              "document.getElementById('proof').textContent='Events PASS';")
        handlers=' oninput="'+escape(code,quote=True)+'" onchange="'+escape(code,quote=True)+'"'
    body=f'<form id=f method={method} action="/verify/{name}?{query}#result">'
    body+='<select id=s aria-label=Choose name='+('tag' if multiple else 'q')+extra+handlers+'>'
    if multiple:body+='<option value=a selected>Alpha<option value=b>Beta<option value=c disabled selected>Disabled<option value=d hidden selected>Hidden'
    elif name=='script-duplicate':body+='<option value=same>First<option value=skip disabled>Disabled<option value=same>Second'
    elif name=='early':body+='<option value=a>Chapter A<option value=b>Chapter B<option value=c>Chapter C'
    elif name in ('scroll','overflow'):
        for i in range(65 if name=='overflow' else 64):body+='<option value='+str(i)+'>Choice '+str(i)
    else:body+='<option value="">Choose<option value=a>First<option value="第二 &amp; +">Second<option value=skip disabled>Disabled'
    body+='</select>'
    if name=='hidden':body+='<select name=q hidden><option selected value=hidden>Hidden</select>'
    if name=='early':body+='<script src=/delay.js></script>'
    body+='<button name=go value=send style="color:#d06020">Submit</button></form><p id=proof style="color:#187a42">Ready</p>'
    if name=='script-dynamic':
        body='<form id=f method=post action="/verify/script-dynamic?keep=1"><button name=go value=send style="color:#d06020">Submit</button></form><p id=proof style="color:#187a42">Ready</p>'
        body+="<script>const f=document.getElementById('f'),s=document.createElement('select');s.name='q';s.id='s';s.add(new Option('Alpha','a'));s.add(new Option('Beta','b'));s.add(new Option('Gamma','c'));s.oninput=s.onchange=function(event){"+code+"};f.insertBefore(s,f.firstChild);</script>"
    return body


class Fixture:
    def __init__(self):
        self.events=[];self.submissions=[];self.delay_started=Event();fixture=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def request_page(self,method):
                url=urlsplit(self.path);wire=self.rfile.read(int(self.headers.get('Content-Length','0'))) if method=='post' else b'';status=200
                if url.path=='/record':
                    fixture.events.append({k:v[0] for k,v in parse_qs(url.query,keep_blank_values=True).items()});status=204;text=''
                elif url.path=='/delay.js':
                    fixture.delay_started.set();time.sleep(4)
                    text="document.getElementById('s').add(new Option('Inserted','new'),0);"
                elif url.path.startswith('/verify/'):
                    name=url.path.rsplit('/',1)[-1];pairs=expected(name);encoded=urlencode(pairs)
                    valid=(method==('get' if name=='single-get' else 'post') and
                        (url.query==encoded if method=='get' else url.query=='keep=1' and wire.decode()==encoded))
                    fixture.submissions.append(dict(case=name,method=method,path=self.path,body=wire.decode(),valid=valid))
                    status=200 if valid else 422;text='<p style="color:#187a42">Select data PASS</p>' if valid else 'Select data FAIL'
                else:text=page(url.path.strip('/'))
                data=text.encode();self.send_response(status)
                self.send_header('Content-Type','text/javascript; charset=utf-8' if url.path=='/delay.js' else 'text/html; charset=utf-8')
                self.send_header('Content-Length',str(len(data)));self.end_headers()
                try:self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):pass
            def do_GET(self):self.request_page('get')
            def do_POST(self):self.request_page('post')
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    def start(self):
        self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
