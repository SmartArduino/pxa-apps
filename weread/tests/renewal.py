#!/usr/bin/env python3
"""Transactional session renewal through the actual PXA HTTP/runtime path."""
import argparse,base64,io,json,tempfile,threading,time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from PIL import Image
from smoke import Simulator

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for field in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+field,type=Path,required=True)
    p.add_argument('--port',type=int,default=18934);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    picture=io.BytesIO();Image.new('RGB',(80,80),'white').save(picture,'PNG')
    mode='seed';calls=[];errors=[];blocked=threading.Event();release=threading.Event()
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def data(self,data,status=200,cookies=()):
            raw=data if isinstance(data,bytes) else json.dumps(data).encode()
            self.send_response(status);self.send_header('Content-Length',str(len(raw)))
            for cookie in cookies:self.send_header('Set-Cookie',cookie)
            self.end_headers()
            try:self.wfile.write(raw)
            except (BrokenPipeError,ConnectionResetError):pass
        def do_GET(self):
            path=urlsplit(self.path).path;calls.append(('GET',path))
            if path.endswith('/getuid'):self.data({'uid':'fixture'})
            elif path.endswith('/qrcode'):self.data({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(picture.getvalue()).decode()})
            elif path.endswith('/shelfloadmore'):
                if mode=='seed' or (mode=='success' and 'wr_skey=renewed-only' in self.headers.get('Cookie','')):
                    self.data({'books':[],'bookReaderUrls':[],'totalCount':0,'kkIdx':1})
                else:self.data({'data':{'errcode':-2012}})
            else:errors.append('unexpected GET');self.data({},404)
        def do_POST(self):
            try:self.post()
            except Exception as e:errors.append(type(e).__name__);self.data({},500)
        def post(self):
            path=urlsplit(self.path).path;calls.append(('POST',path));body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            if path.endswith('/getlogininfo'):self.data({'vid':1,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):self.data({'vid':1,'accessToken':'fixture-only','refreshToken':'fixture-only'},cookies=('wr_vid=1; Path=/','wr_skey=fixture-only; Path=/'))
            elif path=='/web/login/renewal':
                assert body=={'rq':'%2Fweb%2Fbook%2Fread','ql':False}
                assert self.headers.get('Cookie')=='wr_vid=1; wr_skey=fixture-only'
                assert not self.headers.get('Authorization')
                if mode=='cancel':blocked.set();release.wait(10)
                cookies=() if mode=='no-cookies' else ('wr_skey=renewed-only; Path=/',)
                if mode=='wrong-account':cookies+=('wr_vid=2; Path=/',)
                if mode=='malformed':self.data(b'{',cookies=cookies)
                elif mode=='oversized':self.data(b'{"succ":1,"padding":"'+b'x'*5000+b'"}',cookies=cookies)
                elif mode=='http-error':self.data({'succ':1},500,cookies)
                elif mode=='rejected':self.data({'succ':0},cookies=cookies)
                else:self.data({'succ':1},cookies=cookies)
            else:errors.append('unexpected POST');self.data({},404)
    server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start();results=[]
    try:
        with tempfile.TemporaryDirectory(prefix='weread-renewal-') as directory:
            for case in ('success','rejected','http-error','malformed','oversized','wrong-account','no-cookies','still-expired','cancel'):
                mode='seed';state=Path(directory)/case;state.mkdir()
                app=Simulator(a,a.output/(case+'-seed'),state,296,240,160,58)
                try:
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.command('KEY HOME');app.process.wait(timeout=10)
                finally:app.close()
                auth=next(state.rglob('session.json'));original=auth.read_bytes();calls.clear();blocked.clear();release.clear();mode=case
                app=Simulator(a,a.output/case,state,296,240,160,58)
                try:
                    if case=='cancel':
                        app.wait(blocked.is_set);app.tap(259,34);release.set();time.sleep(2.5)
                        assert 'WEREAD SESSION RENEWED' not in app.logs()
                    elif case=='success':app.wait(lambda:'WEREAD SHELF READY' in app.logs());time.sleep(2.5)
                    else:app.wait(lambda:'WEREAD LOGIN EXPIRED' in app.logs());time.sleep(2.5)
                    app.shot('result');assert calls.count(('POST','/web/login/renewal'))==1
                    assert calls.count(('GET','/wrwebsimplenjlogic/api/shelfloadmore'))==(2 if case in ('success','no-cookies','still-expired') else 1)
                    assert not any(path.endswith(('/getuid','/weblogin')) for _,path in calls),'Renewal triggered login mutation'
                    if case in ('success','still-expired'):
                        current=json.loads(auth.read_text());assert current['vid']=='1' and 'wr_skey=renewed-only' in current['cookies']
                    else:assert auth.read_bytes()==original,'Unconfirmed renewal changed saved credentials'
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    results.append({'case':case,'single_attempt':True,'transactional_auth':True,'released_at_exit':True})
                finally:release.set();app.close()
        assert not errors,errors
    finally:release.set();server.shutdown();server.server_close()
    (a.output/'results.json').write_text(json.dumps({'passed':results},indent=2)+'\n')
    print('Nine transactional renewal/retry/cancel cases passed')
if __name__=='__main__':main()
