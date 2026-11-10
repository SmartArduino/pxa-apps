#!/usr/bin/env python3
"""Account-bound shelf startup, offline covers and cancellable revalidation.

Uses the actual PXA Guest, private filesystem and network/decoder services.
All books, images and credentials are synthetic. Fixture port must match build.
"""
import argparse,base64,io,json,tempfile,threading,time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from PIL import Image
from smoke import Simulator

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--port',type=int,default=18934)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    image=io.BytesIO();Image.new('RGB',(80,120),(210,40,50)).save(image,'JPEG');picture=image.getvalue()
    image=io.BytesIO();Image.new('RGB',(80,80),'white').save(image,'PNG');qr=image.getvalue()
    mode='normal';release=threading.Event();reached=threading.Event();covers=[];results=[]
    def book(i):return {'bookId':str(i),'title':f'本地书架 {i}','author':'合成作者','cover':f'http://127.0.0.1:{a.port}/covers/{i}.jpg'}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def reply(self,value,cookies=False):
            data=value if isinstance(value,bytes) else json.dumps(value).encode()
            self.send_response(200);self.send_header('Content-Length',str(len(data)))
            if cookies:
                self.send_header('Set-Cookie','wr_vid=1; Path=/');self.send_header('Set-Cookie','wr_skey=fixture-only; Path=/')
            self.end_headers()
            try:self.wfile.write(data)
            except (BrokenPipeError,ConnectionResetError):pass
        def do_GET(self):
            path=urlsplit(self.path).path
            if path.startswith('/covers/'):
                assert not self.headers.get('Cookie');covers.append(path);self.reply(picture)
            elif path.endswith('/shelfloadmore'):
                current=mode;reached.set()
                if current=='blocked':release.wait(12)
                if current=='offline':self.close_connection=True;return
                if current=='expired':self.reply({'sessionTimeout':1});return
                first=51 if current=='changed' else 1
                self.reply({'books':[book(i) for i in range(first,first+20)],'totalCount':20,'kkIdx':20})
            elif path.endswith('/getuid'):self.reply({'uid':'fixture'})
            elif path.endswith('/qrcode'):self.reply({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(qr).decode()})
            else:self.reply({})
        def do_POST(self):
            self.rfile.read(int(self.headers.get('Content-Length',0)));path=urlsplit(self.path).path
            if path.endswith('/getlogininfo'):self.reply({} if mode=='logout-hold' else {'vid':1,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):self.reply({'vid':1,'accessToken':'fixture-only','refreshToken':'fixture-only'},True)
            else:self.reply({'succ':0})
    server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start()
    def launch(label,state):return Simulator(a,a.output/label,state,296,240,160,58)
    def finish(app):
        try:
            app.command('KEY HOME');app.process.wait(timeout=10)
            assert 'remaining_internal=0 remaining_external=0' in app.logs()
            assert 'DRAW ERROR' not in app.logs() and 'Guest exception:' not in app.logs()
        finally:app.close()
    def visible(path):return sum(r>150 and g<100 and b<120 for r,g,b in Image.open(path).convert('RGB').getdata())
    try:
        with tempfile.TemporaryDirectory(prefix='weread-startup-',dir=a.output) as temp:
            state=Path(temp);app=launch('seed',state)
            try:
                app.wait(lambda:'WEREAD SHELF CACHE SAVED' in app.logs() and app.logs().count('WEREAD IMAGE READY')>=2)
            finally:finish(app)
            snapshot=next(state.rglob('shelf-v1.json'));original=snapshot.read_bytes()
            auth=next(state.rglob('session.json'));original_auth=auth.read_bytes()
            mode='blocked';reached.clear();release.clear();before=len(covers);app=launch('slow-sync',state)
            try:
                app.wait(lambda:reached.is_set() and app.logs().count('WEREAD IMAGE READY')>=2)
                assert 'WEREAD SHELF CACHE READY' in app.logs() and 'WEREAD SHELF READY' not in app.logs()
                assert len(covers)==before,'Cached cover required a network request'
                assert visible(app.shot('before-network-completion'))>100
                turn_start=time.monotonic();app.tap(257,201)
                app.wait(lambda:'WEREAD SHELF SYNC INTERRUPTED' in app.logs() and len(covers)==before+2)
                assert time.monotonic()-turn_start<2,'Paging waited for the blocked shelf request'
                release.set();time.sleep(.3)
                assert 'WEREAD SHELF READY' not in app.logs(),'Cancelled sync published a stale result'
                assert snapshot.read_bytes()==original and auth.read_bytes()==original_auth
                results.append({'cached_shelf_before_http_completion':True,'cached_covers_without_network':True,'paging_cancels_slow_sync':True})
            finally:release.set();finish(app)
            mode='normal';before=len(covers);app=launch('stable-sync',state)
            try:
                app.wait(lambda:'WEREAD SHELF CACHE SAVED' in app.logs() and app.logs().count('WEREAD IMAGE READY')>=2)
                time.sleep(.25)
                assert app.logs().count('WEREAD IMAGE READY')==2,'Unchanged sync decoded covers again'
                assert len(covers)==before
                results.append({'stable_refresh_preserves_decoded_covers':True})
            finally:finish(app)
            mode='offline';before=len(covers);app=launch('offline',state)
            try:
                app.wait(lambda:'WEREAD SHELF CACHE RETAINED' in app.logs() and app.logs().count('WEREAD IMAGE READY')>=2)
                assert visible(app.shot('offline-books'))>100 and len(covers)==before
                assert 'WEREAD ERROR' not in app.logs() and auth.read_bytes()==original_auth
                results.append({'offline_shelf_and_covers':True})
            finally:finish(app)
            for case in ('wrong-account','corrupt'):
                if case=='wrong-account':
                    data=json.loads(original);data['account']='different-account';snapshot.write_text(json.dumps(data))
                else:snapshot.write_text('broken snapshot')
                mode='blocked';reached.clear();release.clear();app=launch(case,state)
                try:
                    app.wait(reached.is_set);time.sleep(.2)
                    assert 'WEREAD SHELF CACHE READY' not in app.logs() and 'WEREAD IMAGE READY' not in app.logs()
                    release.set();app.wait(lambda:'WEREAD SHELF CACHE SAVED' in app.logs())
                    results.append({case+'_ignored':True})
                finally:release.set();finish(app)
            mode='expired';app=launch('expired',state)
            try:
                app.wait(lambda:'WEREAD LOGIN EXPIRED' in app.logs())
                assert 'WEREAD SHELF CACHE READY' in app.logs() and auth.read_bytes()==original_auth
                app.shot('login-expired');results.append({'expired_session_requires_login':True})
            finally:finish(app)
            mode='changed';before=len(covers);app=launch('changed',state)
            try:
                app.wait(lambda:'WEREAD SHELF CACHE SAVED' in app.logs() and '/covers/52.jpg' in covers[before:])
                data=json.loads(snapshot.read_text());assert data['data']['books'][0]['bookId']=='51'
                app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>=2)
                assert visible(app.shot('updated-shelf'))>100
                results.append({'changed_remote_shelf_published':True})
            finally:finish(app)
            app=launch('remember-off',state)
            try:
                app.wait(lambda:'WEREAD SHELF CACHE SAVED' in app.logs())
                app.tap(258,36);app.tap(257,201);app.tap(120,80)
                app.wait(lambda:not snapshot.exists() and not auth.exists())
                results.append({'remember_off_removes_snapshot_and_session':True})
            finally:finish(app)
            auth.write_bytes(original_auth);snapshot.write_bytes(original)
            prefs=next(state.rglob('state.json'));saved=json.loads(prefs.read_text())
            saved['preferences']['remember']=True;prefs.write_text(json.dumps(saved))
            mode='normal';app=launch('logout',state)
            try:
                app.wait(lambda:'WEREAD SHELF CACHE SAVED' in app.logs())
                app.tap(258,36);app.tap(257,201);mode='logout-hold';app.tap(80,130)
                app.wait(lambda:'WEREAD QR READY' in app.logs() and not snapshot.exists() and not auth.exists())
                results.append({'logout_removes_snapshot_and_session':True})
            finally:finish(app)
    finally:release.set();server.shutdown();server.server_close()
    (a.output/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
    print('Shelf startup, offline display, account isolation, expiry and cancellable refresh passed')

if __name__=='__main__':main()
