"""Detached DOM staging and independent callback receipts."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import parse_qs, urlsplit


def page(strict=True, attached=False):
    body=''
    for name,color,call in [('stage','#d06020','stage'),('mount','#2468ac','mount'),('remove','#673ab7','unmount'),('finish','#993240','finish')]:
        body+=f'<div><button style="color:{color}" onclick="{call}()">{name.title()}</button></div>'
    body+='<p id=proof style="color:#555555">Ready</p><div id=host></div>'
    if not strict:body+='<p>Unchanged paragraph</p>'*40
    body+='''<script>
let n=0,box=document.createElement('div');const strict=STRICT,proof=document.getElementById('proof'),host=document.getElementById('host');
function check(v){if(!v)throw Error('connection fixture')}
function record(event){let x=new XMLHttpRequest();x.open('GET','/record?event='+event+'&n='+n+'&connected='+(box.parentNode?1:0));x.send()}
function verify(){if(!strict)return;check(box.isConnected===document.contains(box));check(box.getRootNode()===(box.parentNode?document:box));check(box.contains(box));if(box.firstChild)check(box.contains(box.firstChild) && box.firstChild.parentElement===box)}
function stage(){++n;box.textContent='Draft '+n;box.setAttribute('title','Prepared '+n);box.style.color=n%2?'green':'blue';box.classList.toggle('odd');box.dataset.count=n;box.onclick=()=>{proof.textContent='Clicked '+n;record('clicked')};box.innerHTML='<span style="color:#187a42">Staged '+n+'</span>';verify();record('stage')}
function mount(){host.appendChild(box);verify();record('mount')}
function unmount(){box.remove();verify();record('remove')}
function finish(){proof.textContent='Done '+n;record('finish')}
if(strict){check(document.isConnected && document.getRootNode()===document);check(!box.isConnected && box.parentElement===null && !document.contains(box))}
ATTACH
</script>'''.replace('STRICT','true' if strict else 'false').replace('ATTACH','host.appendChild(box);' if attached else '')
    return body


class Fixture:
    def __init__(self,port=0):
        self.events=[];fixture=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def do_GET(self):
                url=urlsplit(self.path);args=parse_qs(url.query)
                if url.path=='/record':
                    fixture.events.append({k:v[0] for k,v in args.items()});data=b'';status=204
                else:
                    data=(page(url.path!='/measure',args.get('attached')==['1']) if url.path in ('/connection','/measure') else '<p>Ready</p>').encode();status=200
                self.send_response(status);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(data)));self.end_headers()
                try:self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):pass
        self.server=ThreadingHTTPServer(('127.0.0.1',port),Handler)
    def start(self):self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
