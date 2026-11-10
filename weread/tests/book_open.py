#!/usr/bin/env python3
"""Real PXA opening: gzip bounds, local reopen, book identity and cancellation."""
import argparse,base64,gzip,io,json,subprocess,tempfile,threading,time,zlib
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit,parse_qs
from PIL import Image,ImageChops
from smoke import Simulator
from large_reader import fixture

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for field in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+field,type=Path,required=True)
    p.add_argument('--cases',nargs='+',choices=('identity','gzip','unknown','concatenated','large-gzip','large-unknown','corrupt','truncated','bomb','unsupported','blocked','markup','fraction'),
        default=['identity','gzip','unknown','concatenated','large-gzip','large-unknown','corrupt','truncated','bomb','unsupported','blocked'])
    p.add_argument('--port',type=int,default=18934);p.add_argument('--decoder-test',type=Path)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    book,plain=fixture();packed=gzip.compress(plain,mtime=0);limit=1302528
    body_start=plain.index(b'<p>');body_end=plain.index(b'</p>',body_start)+4
    markup=('<p>'+"<span class='w' data-wco='123'>文</span>"*8000+'</p>').encode()
    markup_payload=plain[:body_start]+markup+plain[body_end:plain.rfind(b'<!--')]
    fractional_payload=plain.replace(b'}(0,',b'}(.15,',1)
    assert len(markup)>196608 and fractional_payload!=plain
    payloads={'markup':markup_payload,'fraction':fractional_payload}
    if a.decoder_test:
        native=a.output/'native';native.mkdir();expected=native/'plain';expected.write_bytes(plain)
        fixed=zlib.compressobj(wbits=31,strategy=zlib.Z_FIXED)
        named=io.BytesIO()
        with gzip.GzipFile(fileobj=named,mode='wb',filename='chapter.html',mtime=0) as f:f.write(plain)
        crc=bytearray(packed);crc[-8]^=1;size=bytearray(packed);size[-4]^=1
        cases={'dynamic':(packed,True),'fixed':(fixed.compress(plain)+fixed.flush(),True),
            'stored':(gzip.compress(plain,compresslevel=0,mtime=0),True),'named':(named.getvalue(),True),
            'concatenated':(gzip.compress(plain[:12345])+gzip.compress(plain[12345:]),True),
            'bad-crc':(crc,False),'bad-size':(size,False),'truncated':(packed[:-1],False),
            'garbage':(packed+b'junk',False),'bad-header':(b'bad header',False),'empty':(b'',False),
            'too-many-members':(gzip.compress(b'')*17,False),'bomb':(gzip.compress(b'x'*(limit+1)),False)}
        for label,(data,valid) in cases.items():
            path=native/label;path.write_bytes(data)
            subprocess.run([str(a.decoder_test.resolve()),str(path),str(expected) if valid else '-',str(limit)],check=True,stdout=subprocess.DEVNULL)
    image=io.BytesIO();Image.new('RGB',(40,40),'white').save(image,'PNG');qr=image.getvalue()
    mode='identity';account=1;calls=[];errors=[];blocked=threading.Event();release=threading.Event()
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def reply(self,value,cookies=False):
            data=json.dumps(value).encode();self.send_response(200);self.send_header('Content-Length',str(len(data)))
            if cookies:
                self.send_header('Set-Cookie',f'wr_vid={account}; Path=/');self.send_header('Set-Cookie','wr_skey=fixture-only; Path=/')
            self.end_headers();self.wfile.write(data)
        def do_GET(self):
            path=urlsplit(self.path).path
            if path.endswith('/getuid'):self.reply({'uid':'fixture'})
            elif path.endswith('/qrcode'):self.reply({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(qr).decode()})
            elif path.endswith('/shelfloadmore'):
                self.reply({'books':[book,dict(book,bookId='2',title='Second book')],
                    'bookReaderUrls':[{'bId':str(i),'param':f'fixture-{i}'} for i in (1,2)],'totalCount':2,'kkIdx':1})
            elif path.endswith('/reader'):
                encoding=self.headers.get('Accept-Encoding');assert encoding in ('gzip','identity')
                assert 'wr_skey=fixture-only' in self.headers.get('Cookie','') and not self.headers.get('Authorization')
                assert f'wr_vid={account}' in self.headers.get('Cookie','')
                uid=int(parse_qs(urlsplit(self.path).query)['bc'][0].split('-')[1]);calls.append(uid)
                data=payloads.get(mode,plain)
                if uid!=1:data=data.replace(b'"bookId":"1"',b'"bookId":"2"',1)
                zipped=mode!='identity' and encoding=='gzip'
                if zipped:
                    if mode=='concatenated':data=gzip.compress(data[:12345])+gzip.compress(data[12345:])
                    else:data=gzip.compress(data,compresslevel=0 if mode.startswith('large-') else 9,mtime=0)
                if uid==2:
                    if mode=='corrupt':data=data[:-8]+bytes([data[-8]^1])+data[-7:]
                    elif mode=='bomb':data=gzip.compress(b'x'*(limit+1),mtime=0)
                    elif mode=='truncated':data=data[:-1]
                self.send_response(200)
                if mode not in ('unknown','large-unknown'):self.send_header('Content-Length',str(len(data)))
                if zipped:self.send_header('Content-Encoding','br' if uid==2 and mode=='unsupported' else 'gzip')
                self.end_headers()
                try:
                    if uid==2 and mode=='blocked':
                        self.wfile.write(data[:20]);self.wfile.flush();blocked.set();release.wait(15);self.wfile.write(data[20:])
                    else:
                        for at in range(0,len(data),137):self.wfile.write(data[at:at+137])
                except (BrokenPipeError,ConnectionResetError):pass
            else:errors.append(path);self.reply({})
        def do_POST(self):
            path=urlsplit(self.path).path;self.rfile.read(int(self.headers.get('Content-Length',0)))
            if path.endswith('/getlogininfo'):self.reply({'vid':account,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):self.reply({'vid':account,'accessToken':'fixture-only','refreshToken':'fixture-only'},True)
            else:errors.append(path);self.reply({})
    server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start();results=[]
    def home(app):app.tap(148,120);app.tap(76,162)
    def body(path):return Image.open(path).convert('RGB').crop((25,49,271,199))
    try:
        with tempfile.TemporaryDirectory(prefix='book-open-',dir=a.output) as tmp:
            for mode in a.cases:
                account=1;calls.clear();blocked.clear();release.clear();state=Path(tmp)/mode;state.mkdir()
                app=Simulator(a,a.output/mode,state,296,240,160,58)
                try:
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(120,83)
                    app.wait(lambda:'WEREAD READER READY' in app.logs(),60)
                    fetches=2 if mode.startswith('large-') else 1
                    if mode.startswith('large-'):assert 'WEREAD READER IDENTITY FALLBACK' in app.logs()
                    elif mode!='identity':assert 'gzip=1' in app.logs()
                    app.tap(260,120);original=app.shot('read-second-page');home(app)
                    app.tap(120,83);app.wait(lambda:'WEREAD READER MEMORY HIT' in app.logs())
                    restored=app.shot('reopened');assert calls==[1]*fetches,'Reopen downloaded the book again'
                    assert not ImageChops.difference(body(original),body(restored)).getbbox(),'Reopen lost the current page'
                    home(app);app.tap(120,140)
                    if mode=='blocked':
                        app.wait(blocked.is_set);app.command('KEY BACK');release.set();app.process.wait(timeout=10)
                        assert app.logs().count('WEREAD READER READY')==1
                    else:
                        success=mode in ('identity','gzip','unknown','concatenated','large-gzip','large-unknown','markup','fraction')
                        app.wait(lambda:app.logs().count('WEREAD READER READY')==2 if success else 'WEREAD READING FAILED' in app.logs(),60)
                        assert calls==[1]*fetches+[2]*fetches,'A different book used the previous book cache'
                        if not success:
                            app.tap(120,83);app.wait(lambda:app.logs().count('WEREAD READER READY')==2,60)
                            assert calls==[1,2,1],'Failed new-book load lost the previous reading entry'
                            time.sleep(2.5) # Let the prior failure toast expire before pixel comparison.
                            restored=app.shot('restored-after-failure')
                            assert not ImageChops.difference(body(original),body(restored)).getbbox(),'Failed new-book load lost the previous position'
                        if mode=='identity':
                            # A warm process changing accounts must not expose
                            # the previous account's retained chapter.
                            home(app);app.tap(260,34);app.tap(260,203);account=2;app.tap(80,132)
                            app.wait(lambda:app.logs().count('WEREAD SHELF READY')==2)
                            app.tap(120,83);app.wait(lambda:app.logs().count('WEREAD READER READY')==3,60)
                            assert calls==[1,2,1] and app.logs().count('WEREAD READER MEMORY HIT')==1
                        app.shot('result');app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    assert 'Guest exception:' not in app.logs() and 'DRAW ERROR' not in app.logs() and 'WEREAD ERROR' not in app.logs()
                    assert not errors,errors
                    results.append({'case':mode,'passed':True,'plain_bytes':len(payloads.get(mode,plain)),'gzip_bytes':len(gzip.compress(payloads.get(mode,plain),mtime=0))})
                finally:
                    release.set();app.close()
                    (a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    finally:release.set();server.shutdown();server.server_close()
    (a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Book gzip, limits, local reopening, identity and cancellation passed')

if __name__=='__main__':main()
