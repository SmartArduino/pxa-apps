"""Inline-only field handlers, ordered wire receipts and no-op controls."""
from collections import Counter
from html import escape
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import parse_qs, urlsplit


def receipt(name, extra=''):
    return (extra + "if(event.target.id!=='q'||event.currentTarget!==this)throw Error('event identity');"
            "var p=document.getElementById('proof'),seq=+(p.getAttribute('data-seq')||0)+1;p.setAttribute('data-seq',seq);"
            "var x=new XMLHttpRequest();x.open('GET','/record?case="+name+
            "&seq='+seq+'&event='+event.type+'&target='+event.target.id+'&current='+this.id+"
            "'&value='+encodeURIComponent(event.target.value)+'&checked='+event.target.checked);x.send();"
            "document.getElementById('proof').textContent='Field events PASS';")


def page(name):
    attrs={}
    if name in ('input-only', 'standalone', 'noop-valid'):
        attrs={'oninput':receipt(name), 'onchange':receipt(name)}
    elif name=='change-only':attrs={'onchange':receipt(name)}
    elif name=='replace':
        attrs={'oninput':"this.setAttribute('onchange',"+repr(receipt(name)+"return false;")+")", 'onchange':'bad )'}
    elif name=='remove':attrs={'oninput':receipt(name)+"this.removeAttribute('onchange')", 'onchange':'bad )'}
    elif name=='noop-invalid':attrs={'oninput':'bad )', 'onchange':'bad )'}
    elif name=='disables':attrs={'oninput':receipt(name)+"this.disabled=true", 'onchange':receipt(name)}
    elif name in ('radio', 'checkbox'):
        code=receipt(name,"if(this.getAttribute('value')!==null)throw Error('implicit value');")
        attrs={'oninput':code,'onchange':code}
    attributes=''.join(' '+k+'="'+escape(v,quote=True)+'"' for k,v in attrs.items())
    control=('<textarea id=q aria-label=Edit'+attributes+'>old</textarea>' if name=='change-only' else
             '<input id=q aria-label=Edit'+(' type='+name+(' checked' if name=='radio' else '') if name in ('radio','checkbox') else ' value=same')+attributes+'>')
    if name=='ancestor':
        # Bubbling handlers intentionally live only on the form.
        code=receipt(name)
        control='<form id=parent oninput="'+escape(code,quote=True)+'" onchange="'+escape(code,quote=True)+'">'+control+'</form>'
    elif name!='standalone':control='<form>'+control+'</form>'
    body=control+'<p id=proof style="color:#187a42">Ready</p>'
    if name in ('noop-valid','radio'):body+='<script>void 0</script>'
    else:assert '<script' not in body
    return body


class Fixture:
    def __init__(self):
        self.requests=Counter();self.events=[];fixture=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def do_GET(self):
                url=urlsplit(self.path);fixture.requests[url.path]+=1
                if url.path=='/record':
                    item={k:v[0] for k,v in parse_qs(url.query,keep_blank_values=True).items()}
                    fixture.events.append(item);data=b'';status=204
                else:data=page(url.path.strip('/')).encode();status=200
                self.send_response(status);self.send_header('Content-Type','text/html; charset=utf-8')
                self.send_header('Content-Length',str(len(data)));self.end_headers()
                try:self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):pass
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    def start(self):
        self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
