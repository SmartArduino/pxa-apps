"""DocumentFragment lists, controls and script activation with HTTP receipts."""
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from threading import Thread
from urllib.parse import parse_qs,urlsplit


def page(case):
    body='<div><button style="color:#d06020" onclick="stage()">Stage</button></div><div><button style="color:#702080" onclick="mount()">Mount</button></div>'
    if case=='list':body+='<div><button style="color:#673ab7" onclick="clearRows()">Clear</button></div>'
    if case in ('select','radio'):body+='<div><button style="color:#993240" onclick="document.getElementById(\'form\').submit()">Submit</button></div>'
    if case=='select':body+='<form id=form action=/verify-select><select id=s name=chapter><option value=a>Chapter A</select></form>'
    elif case=='radio':body+='<form id=form method=post action=/verify-radio><input type=radio name=q value=old checked></form>'
    else:body+='<div id=host><span id=anchor>End</span></div>'
    body+='<p id=proof style="color:#555555">Ready</p>'
    body+='''<script>
const mode='CASE',api=typeof document.createDocumentFragment==='function';let fragment=null,source=null,marker=null;
function check(v){if(!v)throw Error('batch fixture')}
function record(event,value){let x=new XMLHttpRequest();x.open('GET','/record?case='+mode+'&event='+event+'&value='+encodeURIComponent(String(value)));x.send()}
function stage(){check(api);fragment=document.createDocumentFragment();source=fragment.childNodes;
 if(mode==='list'){for(const value of ['A','B','C']){let p=document.createElement('p');p.id='chapter-'+value;p.textContent='Chapter '+value;if(value==='B')p.style.color='#187a42';p.onclick=()=>{document.getElementById('proof').textContent='Clicked '+value;record('clicked',value)};fragment.appendChild(p)}check(fragment.getElementById('chapter-B')===fragment.querySelectorAll('p')[1]);marker=fragment.getElementById('chapter-B')}
 else if(mode==='select'){fragment.appendChild(new Option('Chapter B','b',false,true));let group=document.createElement('optgroup');group.disabled=true;group.appendChild(new Option('Disabled','skip'));fragment.appendChild(group)}
 else if(mode==='radio'){for(const value of ['one','two']){let box=document.createElement('div'),r=document.createElement('input');r.type='radio';r.name='q';r.value=value;r.checked=true;box.appendChild(r);fragment.appendChild(box)}}
 else {for(const value of ['one','two']){let s=document.createElement('script');s.src='/'+value+'.js';fragment.appendChild(s)}let p=document.createElement('p');p.id='all-present';fragment.appendChild(p)}
 check(!fragment.isConnected);record('stage',source.length)
}
function mount(){let target=mode==='select'?document.getElementById('s'):mode==='radio'?document.getElementById('form'):document.getElementById('host');target.insertBefore(fragment,target.firstChild);check(source.length===0 && !fragment.isConnected);
 if(mode==='list')check(document.getElementById('chapter-B')===marker && marker.isConnected);
 if(mode==='select'){check(target.value==='b');target.onchange=function(){record('choice',this.value)}}
 if(mode==='radio'){let r=target.querySelectorAll('input');check(!r[0].checked && r[1].checked && !r[2].checked)}
 record('mount',source.length)
}
function clearRows(){const host=document.getElementById('host');for(const node of host.childNodes.slice())if(node.id!=='anchor')node.remove();record('clear',host.childNodes.length)}
record('ready',api?1:0);
</script>'''.replace('CASE',case)
    return body


class Fixture:
    def __init__(self):
        self.events=[];self.scripts=[];self.submissions=[];f=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def request_page(self,post=False):
                url=urlsplit(self.path);args=parse_qs(url.query);wire=self.rfile.read(int(self.headers.get('Content-Length','0'))) if post else b'';kind='text/html; charset=utf-8'
                if url.path=='/record':f.events.append({k:v[0] for k,v in args.items()});body='';status=204
                elif url.path in ('/one.js','/two.js'):
                    f.scripts.append(url.path);body="if(!document.getElementById('all-present') || source.length)throw Error('partial batch');document.getElementById('proof').textContent"+('="Script one";' if url.path=='/one.js' else '+=" two";record("loaded",document.getElementById("proof").textContent);');status=200;kind='application/javascript'
                elif url.path.startswith('/verify-'):
                    valid=(url.path=='/verify-select' and not post and url.query=='chapter=a') or (url.path=='/verify-radio' and post and wire==b'q=two');f.submissions.append(dict(path=self.path,post=post,body=wire.decode(),valid=valid));body='<p>Batch form PASS</p>' if valid else 'FAIL';status=200 if valid else 422
                else:body=page(url.path.strip('/'));status=200
                data=body.encode();self.send_response(status);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(data)));self.end_headers()
                try:self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):pass
            def do_GET(self):self.request_page()
            def do_POST(self):self.request_page(True)
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    def start(self):self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
