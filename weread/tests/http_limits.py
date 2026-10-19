#!/usr/bin/env python3
"""HTTP admission limits and retry in the real Guest/runtime; synthetic data only.

Use --trace only with WEREAD_HTTP_TRACE. The diagnostic can inspect a larger
Content-Length, but must reject it before constructing the response body.
"""
import argparse,base64,io,json,tempfile,threading
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from PIL import Image
from smoke import Simulator


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for field in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+field,type=Path,required=True)
    p.add_argument('--port',type=int,default=18934);p.add_argument('--trace',action='store_true')
    p.add_argument('--reader-budget',type=int,default=786432)
    p.add_argument('--bounded-allocation',action='store_true',help='Allow a controlled resource_limit before an unknown-length body exceeds its transport cap')
    p.add_argument('--selection',action='store_true',help='Replay historical WEREAD_STREAM_READER packages; current code uses bounded response bytes')
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    mode='healthy';failures=[];calls=[];picture=io.BytesIO()
    Image.new('RGB',(80,80),'white').save(picture,'PNG')
    book={'bookId':'1','title':'HTTP limits','author':'Synthetic'}
    reader={'bookInfo':book,'curChapter':{'title':'Chapter','chapterUid':1,'chapterIdx':1},
        'contentText':'<p>'+'Readable bounded chapter. '*200+'</p>',
        'curChapterUrlParam':{'param':'fixture-1'},'prevChapterUrlParam':{},'nextChapterUrlParam':{},
        'curChapterSection':0,'chapterSections':[1],'chapterInfoCount':1,
        'chapterIndexes':[{'title':'Chapter','chapterUid':1,'chapterIdx':1}],
        'readerUrlParams':[{'cUid':1,'param':'fixture-1'}]}
    html='<script>window.__NUXT__=(function(a){return '+json.dumps({'state':{'reader':reader}})+'}(0));</script>'
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def data(self,value,cookies=()):
            raw=json.dumps(value).encode();self.send_response(200)
            self.send_header('Content-Length',str(len(raw)))
            for cookie in cookies:self.send_header('Set-Cookie',cookie)
            self.end_headers();self.wfile.write(raw)
        def do_GET(self):
            path=urlsplit(self.path).path
            try:
                if path.endswith('/getuid'):self.data({'uid':'fixture'})
                elif path.endswith('/qrcode'):self.data({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(picture.getvalue()).decode()})
                elif path.endswith('/shelfloadmore'):
                    self.data({'books':[book],'bookReaderUrls':[{'bId':'1','param':'fixture-1'}],'totalCount':1,'kkIdx':1})
                elif path.endswith('/reader'):
                    calls.append(mode);self.send_response(200)
                    self.send_header('Content-Type','x'*5000 if mode=='headers' else 'text/html')
                    raw=html.encode()
                    if mode in ('large-page','fragmented-page','unknown-page'):
                        raw=b'<style>'+b'x'*950000+b'</style>'+raw
                    elif mode=='body-limit':
                        raw=b'<div id="readerContentRenderContainer">'+b'x'*196609+b'</div>'+raw
                    elif mode=='state-limit':
                        raw=b'<script>window.__NUXT__=(function(a){return {unused:"'+b'x'*786432+b'"}}(0));</script>'
                    elif mode=='duplicate-data':raw+=raw
                    if mode in ('known-body','over-transport'):
                        self.send_header('Content-Length',str(a.reader_budget+1 if mode=='known-body' else 8388609))
                    elif mode in ('unknown-body','unknown-page'):self.send_header('Connection','close')
                    else:self.send_header('Content-Length',str(len(raw)))
                    self.end_headers()
                    if mode=='unknown-body':
                        # No Content-Length: exceed the limit through a streaming body.
                        for _ in range(a.reader_budget//4096+1):self.wfile.write(b'x'*4096)
                        self.close_connection=True
                    elif mode=='fragmented-page':
                        for at in range(0,len(raw),257):self.wfile.write(raw[at:at+257]);self.wfile.flush()
                    elif mode not in ('known-body','over-transport','headers'):
                        self.wfile.write(raw)
                        if mode=='unknown-page':self.close_connection=True
                    else:self.close_connection=True
                else:raise AssertionError('Unexpected GET')
            except (BrokenPipeError,ConnectionResetError):pass
            except Exception as error:failures.append(type(error).__name__)
        def do_POST(self):
            path=urlsplit(self.path).path;self.rfile.read(int(self.headers.get('Content-Length',0)))
            if path.endswith('/getlogininfo'):self.data({'vid':1,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):
                self.data({'vid':1,'accessToken':'fixture-only','refreshToken':'fixture-only'},
                    ('wr_vid=1; Path=/','wr_skey=fixture-only; Path=/'))
            else:failures.append('Unexpected POST');self.data({})
    server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start();results=[]
    try:
        with tempfile.TemporaryDirectory(prefix='weread-http-limits-')as directory:
            cases=('large-page','fragmented-page','unknown-page','body-limit','state-limit','duplicate-data','over-transport','headers') if a.selection else ('known-body','over-transport','headers','unknown-body')
            for case in cases:
                mode='healthy';state=Path(directory)/case;state.mkdir()
                app=Simulator(a,a.output/case,state,296,240,160,58)
                try:
                    app.wait(lambda:'WEREAD SHELF READY'in app.logs())
                    auth=next(state.rglob('session.json'));before=auth.read_bytes()
                    mode=case;app.tap(120,83)
                    success=case in ('large-page','fragmented-page','unknown-page')
                    if success:
                        app.wait(lambda:'WEREAD READER READY'in app.logs(),timeout=45)
                        assert 'WEREAD READER SELECTED'in app.logs()
                        memory=app.wait_memory(lambda x:x.get('linear_current_bytes',0)>0)
                        assert memory['linear_peak_bytes']<524288,memory
                        count=len(calls);app.tap(260,120);assert len(calls)==count,'Local turn refetched content'
                        app.shot('large-page-read')
                    else:
                        code=-15 if case=='duplicate-data'else -16
                        allowed=(code,-9)if a.bounded_allocation and case=='unknown-body'else(code,)
                        app.wait(lambda:any(f'WEREAD ERROR code={value} 'in app.logs()for value in allowed),timeout=45)
                        assert 'WEREAD READER PARSE'not in app.logs()
                        if a.selection and case in ('body-limit','state-limit','duplicate-data'):
                            expected={'body-limit':'limit=3 error=1','state-limit':'limit=2 error=1','duplicate-data':'limit=0 error=2'}[case]
                            assert 'WEREAD READER SELECTION FAILED'in app.logs()and expected in app.logs()
                    if a.trace and not a.selection:
                        if case=='known-body':
                            assert f'body_bytes={a.reader_budget+1} limit={a.reader_budget}'in app.logs()
                            assert f'stage=5 code=-16 bytes=0 limit={a.reader_budget}'in app.logs()
                        elif case in ('over-transport','headers'):
                            assert f'stage=2 code=-16 bytes=0 limit={a.reader_budget}'in app.logs()
                        else:assert 'stage=4 code=-16'in app.logs()
                    assert auth.read_bytes()==before
                    if not success:
                        app.shot('limit-retained-shelf');mode='healthy';app.tap(120,83)
                        app.wait(lambda:'WEREAD READER READY'in app.logs())
                        app.shot('healthy-retry');assert auth.read_bytes()==before
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0'in app.logs()
                    assert 'Guest event failed:'not in app.logs()and 'Guest exception:'not in app.logs()and not failures,failures
                    results.append({'case':case,'trace':a.trace,'selection':a.selection,'large_page_read':success,'bounded_failure':not success,'retry':True,'credentials_unchanged':True,'released':True})
                except BaseException:
                    app.shot('failure');raise
                finally:app.close()
    finally:server.shutdown();server.server_close()
    (a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(results,ensure_ascii=False))

if __name__=='__main__':main()
