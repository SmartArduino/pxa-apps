"""Original-tag CSS, nested click owners and image handlers with HTTP receipts."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import parse_qs, urlsplit
from image_fixtures import fixtures

STYLE = '''<style>
p.alpha,div.outer{color:#187a42}p.beta,a.nav{color:#993240}
h2,span.inner{color:#702080}li,button.toggle{color:#d06020}
button.pick,p.plain{color:#754324}span.disabled{color:#8a541a}
#proof{color:#555555}
</style>'''
SCRIPT = '''<script>
let seq=0,stop=false;const proof=document.getElementById('proof');
function send(kind,event){let x=new XMLHttpRequest();x.open('GET','/record?kind='+kind+'&seq='+(++seq)+'&target='+(event?event.target.id:'')+'&owner='+(event?event.currentTarget.id:''));x.send();proof.textContent='Chosen '+kind+' '+seq}
function inner(event){send('inner',event);if(stop)event.stopPropagation()}
send('ready');
</script>'''


def page(case):
    if case == 'blocks':
        body = '''<p id=alpha class=alpha onclick="send('alpha',event)">Alpha</p>
<p id=beta class=beta onclick="send('beta',event)">Beta</p>
<h2 id=heading onclick="send('heading',event)">Heading</h2>
<ul><li id=entry onclick="send('entry',event)">Entry</li></ul>
<button id=pick class=pick onclick="send('pick',event)">Pick</button>'''
    elif case == 'nested':
        body = '''<div id=outer class=outer onclick="send('outer',event)">Outer <span id=inner class=inner onclick="inner(event)">Inner</span> Tail <a id=nav class=nav href=/verify onclick="event.preventDefault();send('link',event)">Next</a> End</div>
<p id=plain class=plain>Outside</p>
<button id=toggle class=toggle onclick="stop=true;send('toggle',event)">Stop bubbling</button>
<button disabled><span id=blocked class=disabled onclick="send('blocked',event)">Blocked</span></button>'''
    elif case == 'image':
        body = '''<div id=outer class=outer onclick="send('outer',event)">Before<img id=pic src=/pic.png width=48 height=48 onclick="send('image',event)">After</div><p id=plain class=plain>Outside</p>'''
    else:
        raise AssertionError(case)
    return STYLE + body + '<p id=proof>Ready</p>' + SCRIPT


class Fixture:
    def __init__(self):
        self.events = []
        self.navigations = 0
        self.images = 0
        f = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_GET(self):
                u = urlsplit(self.path)
                mime = 'text/html; charset=utf-8'
                if u.path == '/record':
                    f.events.append({k: v[0] for k, v in parse_qs(u.query, keep_blank_values=True).items()})
                    body, status = b'', 204
                elif u.path == '/pic.png':
                    f.images += 1
                    body, status, mime = fixtures()['icon.png'], 200, 'image/png'
                elif u.path == '/verify':
                    f.navigations += 1
                    body, status = b'<p>Unexpected navigation</p>', 200
                else:
                    body, status = page(u.path.strip('/')).encode(), 200
                self.send_response(status)
                self.send_header('Content-Type', mime)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                try:
                    self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError):
                    pass

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)

    def start(self):
        self.thread = Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        return self

    def close(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
