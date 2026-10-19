#!/usr/bin/env python3
"""Synthetic image chapters through the actual PXA runtime. No real account.

Use the same fixture package/port as smoke.py. PNG source is 1280x1778;
JPEG and GIF tests cover static native decoding, not animation/comic features.
"""
import argparse, io, json, math, tempfile, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from PIL import Image, ImageDraw, ImageChops
from smoke import Simulator


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--port',type=int,default=18934)
    parser.add_argument('--extended-only',action='store_true')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    picture=Image.new('RGBA',(1280,1778),(210,45,55,255))
    ImageDraw.Draw(picture).rectangle((640,0,1279,1777),fill=(30,180,65,255))
    encoded={}
    for fmt in ('PNG','JPEG','GIF'):
        out=io.BytesIO()
        source=picture if fmt=='PNG' else picture.convert('RGB').resize((240,333))
        source.save(out,fmt);encoded[fmt.lower()]=out.getvalue()
    qr=io.BytesIO();Image.new('RGB',(80,80),'white').save(qr,'PNG')
    requests=[];kind='png';blocked=threading.Event();release=threading.Event();retry_count=0
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def reply(self,value,mime='application/json',cookies=False):
            data=value if isinstance(value,bytes) else (value.encode() if isinstance(value,str) else json.dumps(value).encode())
            self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)))
            if cookies:
                self.send_header('Set-Cookie','wr_vid=1; Path=/; HttpOnly')
                self.send_header('Set-Cookie','wr_skey=fixture-only; Path=/; HttpOnly')
            self.end_headers()
            try:self.wfile.write(data)
            except (BrokenPipeError,ConnectionResetError):pass
        def do_GET(self):
            nonlocal retry_count
            path=urlsplit(self.path).path
            if path.startswith('/image/'):
                requests.append({'path':self.path,'cookie':self.headers.get('Cookie',''),'referer':self.headers.get('Referer','')})
                assert not self.headers.get('Cookie'),'Account cookies leaked to image origin'
                if kind in ('redirect','redirect-loop','redirect-reject') and path!='/image/final':
                    self.send_response(302)
                    self.send_header('Location',('/image/final?sig=a&amp;key=b' if kind=='redirect' else
                        '/image/redirect-loop' if kind=='redirect-loop' else 'https://127.0.0.1/private'))
                    self.send_header('Set-Cookie','cdn_cookie=must-not-be-forwarded; Path=/')
                    self.send_header('Content-Length','0');self.end_headers();return
                if kind=='retry':
                    retry_count+=1
                    if retry_count==1:self.send_error(503);return
                if kind=='cancel':
                    data=encoded['png'];self.send_response(200)
                    self.send_header('Content-Length',str(len(data)));self.end_headers()
                    self.wfile.write(data[:128]);self.wfile.flush();blocked.set();release.wait(15)
                    try:self.wfile.write(data[128:])
                    except (BrokenPipeError,ConnectionResetError):pass
                    return
                self.reply(encoded.get(kind,encoded['png']),'image/'+(kind if kind in encoded else 'png'));return
            if path.endswith('/getuid'):self.reply({'uid':'synthetic-uid'})
            elif path.endswith('/qrcode'):
                import base64
                self.reply({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(qr.getvalue()).decode()})
            elif path.endswith('/shelfloadmore'):
                self.reply({'books':[{'bookId':'1','title':'图片与正文测试','author':'合成数据'}],
                    'bookReaderUrls':[{'bId':'1','param':'image'}],'totalCount':1,'kkIdx':1})
            elif path.endswith('/reader'):
                image=f"<img src='http://127.0.0.1:{args.port}/image/{kind}' data-ratio='1.389'"
                body=image+(" style='width:3.5em'>" if kind=='png' else '>')
                if kind=='multiple':body=''.join(image+f" style='width:3em' alt='Image {i}'>" for i in range(20))
                # A long following paragraph makes the next page local and
                # image-free, so cancellation cannot be mistaken for refetch.
                body+='<p>'+('ABCDEFGHIJKLMNOPQRSTUVWXYZ. '*60)+'</p>'
                reader={'bookInfo':{'bookId':'1','title':'图片与正文测试'},
                    'curChapter':{'title':'图片测试','chapterUid':1,'chapterIdx':0},
                    'curChapterUrlParam':{'param':'image'},'contentText':'','chapterSections':[1],
                    'chapterInfoCount':1,'chapterIndexes':[{'title':'图片测试','chapterUid':1}],
                    'readerUrlParams':[{'cUid':1,'param':'image'}]}
                self.reply('<div id="readerContentRenderContainer">'+body+'</div><script>window.__NUXT__='+
                    '(function(a){return '+json.dumps({'state':{'reader':reader}})+'}(0));</script>','text/html')
            else:self.reply({})
        def do_POST(self):
            self.rfile.read(int(self.headers.get('Content-Length',0)))
            path=urlsplit(self.path).path
            if path.endswith('/getlogininfo'):self.reply({'vid':1,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):self.reply({'vid':1,'accessToken':'fixture-only','refreshToken':'fixture-only'},cookies=True)
            else:self.reply({})
    server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start()
    results=[]
    def finish(app):
        try:
            if app.process.poll() is None:
                app.command('KEY HOME');app.process.wait(timeout=10)
        finally:app.close()
        assert app.process.returncode==0,('Simulator exit',app.process.returncode,app.logs()[-2000:])
        assert 'remaining_internal=0 remaining_external=0' in app.logs(),app.logs()[-2000:]
    try:
        with tempfile.TemporaryDirectory(prefix='weread-images-') as root:
            layouts=(
                ('compact',296,240,160,58,False),('hidpi',480,480,280,56,False),
                ('round',240,240,160,0,True),('portrait',480,800,240,32,False))
            for label,w,h,dpi,corner,circular in (() if args.extended_only else layouts):
                for kind in ('png','jpeg','gif'):
                    case=label+'-'+kind;state=Path(root)/case;state.mkdir()
                    app=Simulator(args,args.output/case,state,w,h,dpi,corner,circular)
                    try:
                        app.wait(lambda:'WEREAD SHELF READY' in app.logs())
                        inset=math.ceil((min(w,h)/2 if circular else corner)*(1-2**-.5))
                        head=min(round(44*dpi/160),(h-2*inset)//5)
                        app.tap(w//2,inset+head+15)
                        app.wait(lambda:'WEREAD IMAGE READY index=0' in app.logs(),timeout=30)
                        shot=Image.open(app.shot('decoded')).convert('RGB')
                        colored=[(x,y) for y in range(h) for x in range(w)
                            if (lambda p:(p[0]>150 and p[1]<100 and p[2]<120) or
                                (p[1]>120 and p[0]<100 and p[2]<120))(shot.getpixel((x,y)))]
                        assert len(colored)>400,'Decoded image pixels are not visible'
                        box=(min(x for x,y in colored),min(y for x,y in colored),
                             max(x for x,y in colored)+1,max(y for x,y in colored)+1)
                        assert abs((box[3]-box[1])/(box[2]-box[0])-1.389)<.1,(case,box)
                        assert all(inset<=x<w-inset and inset<=y<h-inset for x,y in colored)
                        assert 'WEREAD ERROR' not in app.logs() and 'DRAW ERROR' not in app.logs()
                        results.append({'screen':label,'format':kind,'visible_pixels':len(colored),'bounds':box})
                    finally:finish(app)
            kind='multiple';state=Path(root)/kind;state.mkdir()
            app=Simulator(args,args.output/kind,state,480,800,240,32)
            try:
                app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(240,100)
                app.wait(lambda:app.logs().count('WEREAD IMAGE READY')==20,timeout=90)
                app.shot('all-images')
                memory=app.wait_memory(lambda value:value['image_current_bytes']>0)
                assert memory['image_current_bytes']<=65536*4+20*256,memory
                mark=len(requests)
                # Reading menu -> settings -> final settings page -> images off.
                app.command('KEY BACK');time.sleep(.15);app.tap(360,190)
                app.tap(440,760);app.tap(240,257)
                app.tap(240,760);app.tap(240,105)
                app.wait_memory(lambda memory:memory['image_current_bytes']==0)
                app.shot('images-disabled');time.sleep(.5)
                assert len(requests)==mark,'Disabled images were fetched again'
                results.append({'multiple_visible_images':20,'aggregate_image_charge':memory['image_current_bytes'],
                    'decoded_pixel_budget':65536,'disable_releases_images':True})
            finally:finish(app)
            for kind in (() if args.extended_only else ('retry','cancel','redirect','redirect-loop','redirect-reject')):
                state=Path(root)/kind;state.mkdir();release.clear();blocked.clear()
                mark=len(requests)
                app=Simulator(args,args.output/kind,state,296,240,160,58)
                try:
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(148,70)
                    if kind=='retry':
                        app.wait(lambda:'WEREAD IMAGE ERROR index=0' in app.logs());app.shot('failure')
                        app.tap(148,110)
                        app.wait(lambda:'WEREAD IMAGE READY index=0' in app.logs());app.shot('retried')
                        assert retry_count==2
                        results.append({'image_retry':True})
                    elif kind=='cancel':
                        app.wait(blocked.is_set);app.tap(220,120)
                        before=Image.open(app.shot('before-cancel-release')).convert('RGB')
                        release.set();time.sleep(.4)
                        after=Image.open(app.shot('cancelled')).convert('RGB')
                        assert ImageChops.difference(before,after).getbbox() is None,'Stale pixels after cancelling image'
                        assert 'WEREAD IMAGE READY' not in app.logs(),'Stale image completion after changing page'
                        assert 'WEREAD ERROR' not in app.logs()
                        results.append({'cancel_while_downloading':True,'stale_pixels_rejected':True})
                    elif kind=='redirect':
                        app.wait(lambda:'WEREAD IMAGE READY index=0' in app.logs());app.shot('redirected')
                        assert len(requests)==mark+2 and requests[-1]['path']=='/image/final?sig=a&amp;key=b'
                        results.append({'cdn_redirect':True,'raw_signed_query_preserved':True,'cdn_cookie_not_forwarded':True})
                    else:
                        app.wait(lambda:'WEREAD IMAGE ERROR index=0' in app.logs());app.shot('redirect-rejected')
                        assert len(requests)==mark+(4 if kind=='redirect-loop' else 1)
                        results.append({kind:True})
                finally:release.set();finish(app)
        assert requests and all(not request['cookie'] for request in requests)
        (args.output/'results.json').write_text(json.dumps({'passed':results,'image_requests':requests},indent=2)+'\n')
    finally:release.set();server.shutdown();server.server_close()
    print('Native visible PNG/JPEG/GIF, four layouts, retry/cancel and teardown passed')

if __name__=='__main__':main()
