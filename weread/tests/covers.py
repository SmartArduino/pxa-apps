#!/usr/bin/env python3
"""Visible-only synthetic cover tests on the actual AOT/Wasm PXA app."""
import argparse,base64,io,json,math,tempfile,threading,time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from PIL import Image,ImageDraw
from smoke import Simulator,primary_button

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--port',type=int,default=18934);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    picture=Image.new('RGB',(120,180),(205,42,45));ImageDraw.Draw(picture).rectangle((8,135,111,165),fill=(40,190,70))
    encoded=io.BytesIO();picture.save(encoded,'JPEG');cover=encoded.getvalue()
    qr=io.BytesIO();Image.new('RGB',(80,80),'white').save(qr,'PNG')
    requests=[];mode='normal';blocked=threading.Event();release=threading.Event()
    def book(i):return {'bookId':str(i),'title':f'封面测试 {i}','author':'合成作者','cover':f'http://127.0.0.1:{a.port}/covers/{i}.jpg?key=a&amp;b=c'}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def data(self,value,mime='application/json',cookies=False):
            data=value if isinstance(value,bytes) else json.dumps(value).encode()
            self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)))
            if cookies:self.send_header('Set-Cookie','wr_vid=1; Path=/');self.send_header('Set-Cookie','wr_skey=fixture-only; Path=/')
            self.end_headers()
            try:self.wfile.write(data)
            except (BrokenPipeError,ConnectionResetError):pass
        def do_GET(self):
            path=urlsplit(self.path).path
            if path.startswith('/covers/'):
                assert not self.headers.get('Cookie'),'Cover request leaked account cookies'
                requests.append(self.path)
                if mode=='blocked' and path=='/covers/1.jpg':
                    self.send_response(200);self.send_header('Content-Length',str(len(cover)));self.end_headers()
                    self.wfile.write(cover[:128]);self.wfile.flush();blocked.set();release.wait(15)
                    try:self.wfile.write(cover[128:])
                    except (BrokenPipeError,ConnectionResetError):pass
                    return
                if mode=='corrupt':self.data(b'not-an-image','image/jpeg')
                elif mode=='truncated':
                    self.send_response(200);self.send_header('Content-Length',str(len(cover)));self.end_headers()
                    self.wfile.write(cover[:128]);self.wfile.flush();self.close_connection=True
                else:self.data(cover,'image/jpeg')
            elif path.endswith('/getuid'):self.data({'uid':'fixture'})
            elif path.endswith('/qrcode'):self.data({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(qr.getvalue()).decode()})
            elif path.endswith('/shelfloadmore'):self.data({'books':[book(i) for i in range(1,21)],'bookReaderUrls':[],'totalCount':20,'kkIdx':20})
            elif path=='/api/store/search':self.data({'sid':'fixture','results':[{'type':1,'scopeCount':1,'currentCount':1,'books':[{'bookInfo':book(99)}]}]})
            else:self.data({})
        def do_POST(self):
            self.rfile.read(int(self.headers.get('Content-Length',0)));path=urlsplit(self.path).path
            if path.endswith('/getlogininfo'):self.data({'vid':1,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):self.data({'vid':1,'accessToken':'fixture-only','refreshToken':'fixture-only'},cookies=True)
            else:self.data({})
    server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start();results=[]
    def color_pixels(path):
        return sum(r>150 and g<100 and b<120 for r,g,b in Image.open(path).convert('RGB').getdata())
    try:
        with tempfile.TemporaryDirectory(prefix='weread-covers-') as temp:
            for label,w,h,dpi,corner,circular in [('compact',296,240,160,58,False),('hidpi',480,480,280,56,False),('round',240,240,160,0,True),('portrait',480,800,240,32,False)]:
                requests.clear();state=Path(temp)/label;state.mkdir();app=Simulator(a,a.output/label,state,w,h,dpi,corner,circular)
                try:
                    inset=math.ceil((min(w,h)/2 if circular else corner)*(1-2**-.5));safe_h=h-2*inset
                    rows=max(1,min(10,(safe_h-round(100*dpi/160))//max(round(48*dpi/160),32)))
                    app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>=rows)
                    assert len(requests)==rows,'Offscreen covers were prefetched'
                    assert all('?key=a&amp;b=c' in url for url in requests),'JSON URL query was changed'
                    assert color_pixels(app.shot('covers'))>100
                    foot=min(round(32*dpi/160),safe_h//6);button=min(round(44*dpi/160),(w-2*inset)//4)
                    app.tap(w-inset-button//2,h-inset-foot//2)
                    app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>=2*rows)
                    assert len(requests)==2*rows
                    assert color_pixels(app.shot('next-covers'))>100
                    app.tap(inset+button//2,h-inset-foot//2)
                    app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>=3*rows)
                    assert len(requests)==2*rows,'Returning to a page downloaded its covers again'
                    assert app.logs().count('WEREAD IMAGE MEMORY HIT')+app.logs().count('WEREAD IMAGE CACHE HIT')>=rows
                    if label=='compact':assert app.logs().count('WEREAD IMAGE MEMORY HIT')>=rows
                    assert color_pixels(app.shot('cached-covers'))>100
                    if label=='compact':
                        app.tap(213,36);app.tap(120,83);app.command('TEXT 测试');time.sleep(.2)
                        app.tap(*primary_button(app.shot('search-keyboard')))
                        app.wait(lambda:'WEREAD SEARCH READY' in app.logs() and any('/covers/99.jpg' in x for x in requests))
                        app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>=3*rows+1)
                        assert color_pixels(app.shot('search-cover'))>100
                    assert 'WEREAD ERROR' not in app.logs() and 'DRAW ERROR' not in app.logs()
                    if label=='compact':app.command('KEY BACK');time.sleep(.2)
                    head=min(round(44*dpi/160),safe_h//5)
                    app.tap(w-inset-head//2,inset+head//2)
                    # Current production runtimes do not expose the historical
                    # PXA_WEREAD_MEMORY_REPORT hook. Verify the visible result
                    # here and use the Host resource counters at clean exit.
                    assert color_pixels(app.shot('settings'))==0
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs(), 'Host resources retained after exit'
                    results.append({'layout':label,'visible_only':True,'page_change':True,'return_without_network':True,'raw_query_preserved':True,'cookies_not_forwarded':True,'images_released_at_exit':True})
                finally:app.close()
                count=len(requests)
                app=Simulator(a,a.output/(label+'-restart'),state,w,h,dpi,corner,circular)
                try:
                    app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>=rows)
                    assert len(requests)==count,'Restart discarded the disk cache'
                    assert app.logs().count('WEREAD IMAGE CACHE HIT')==rows
                    assert color_pixels(app.shot('persistent-covers'))>100
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                finally:app.close()
                if label=='compact':
                    # A matching URL with a damaged image must be fetched and
                    # atomically repaired, instead of retaining a bad entry.
                    cached=next(path for path in state.rglob('image-cache-v1-*')
                        if path.read_bytes().split(b'\n',2)[1].startswith(f'http://127.0.0.1:{a.port}/covers/1.jpg'.encode()))
                    prefix=cached.read_bytes().split(b'\n',2)
                    cached.write_bytes(prefix[0]+b'\n'+prefix[1]+b'\ninvalid-image')
                    app=Simulator(a,a.output/'corrupt-cache',state,w,h,dpi,corner,circular)
                    try:
                        app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>=rows)
                        assert len(requests)==count+1,'Damaged disk cache was not repaired once'
                        assert color_pixels(app.shot('repaired'))>100
                        app.command('KEY HOME');app.process.wait(timeout=10)
                    finally:app.close()
                    for path in state.rglob('image-cache-v1-*'):path.unlink()
                    (cached.parent/'image-cache-v1.tmp').mkdir()
                    before_fallback=len(requests)
                    app=Simulator(a,a.output/'cache-write-failure',state,w,h,dpi,corner,circular)
                    try:
                        app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>=rows)
                        assert len(requests)==before_fallback+rows,'Unwritable cache caused a duplicate image download'
                        assert 'WEREAD IMAGE ERROR' not in app.logs(),'Cache write failure prevented network display'
                        assert color_pixels(app.shot('network-fallback'))>100
                        app.command('KEY HOME');app.process.wait(timeout=10)
                        assert 'remaining_internal=0 remaining_external=0' in app.logs()
                        results.append({'damaged_cache_repaired':True,'cache_write_failure_network_fallback':True})
                    finally:app.close()
            for mode in ('blocked','corrupt','truncated'):
                requests.clear();blocked.clear();release.clear();state=Path(temp)/mode;state.mkdir();app=Simulator(a,a.output/mode,state,296,240,160,58)
                try:
                    if mode=='blocked':
                        app.wait(blocked.is_set);app.tap(257,201)
                        app.wait(lambda:any('/covers/3.jpg' in x for x in requests));release.set()
                        app.wait(lambda:'WEREAD IMAGE READY' in app.logs())
                        results.append({'blocked_cover_cancelled_for_next_page':True})
                    else:
                        app.wait(lambda:app.logs().count('WEREAD IMAGE ERROR')>=2);time.sleep(.5)
                        assert len(requests)==2;app.shot('fallback')
                        results.append({'corrupt_covers_fallback_no_retry_loop':True})
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs(), 'Host resources retained after exit'
                finally:release.set();app.close()
    finally:server.shutdown();server.server_close()
    (a.output/'results.json').write_text(json.dumps({'passed':results},ensure_ascii=False,indent=2)+'\n')
    print('Four-screen covers, visible-only loading, paging, search, cancel, corruption and release passed')
if __name__=='__main__':main()
