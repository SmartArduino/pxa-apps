#!/usr/bin/env python3
"""Synthetic image chapters through the actual PXA runtime. No real account.

Use the same fixture package/port as smoke.py. PNG source is 1280x1778;
JPEG and GIF tests cover static native decoding, not animation/comic features.
"""
import argparse, io, json, math, re, tempfile, threading, time
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
    parser.add_argument('--viewer-only',action='store_true',help='Long press, zoom, pan, cached decode and return on four layouts')
    parser.add_argument('--skip-memory',action='store_true',help='Verify descriptor charges and visible release without the historical runtime memory-report hook')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    picture=Image.new('RGBA',(1280,1778),(210,45,55,255))
    ImageDraw.Draw(picture).rectangle((640,0,1279,1777),fill=(30,180,65,255))
    if args.viewer_only:
        # Keep the encoded source above the small cache slot ceiling, matching
        # real illustrated books whose TLS stream must close before decoding.
        picture.paste(Image.effect_noise((384,384),96).convert('RGBA'),(0,0))
        grid=ImageDraw.Draw(picture)
        for x in range(0,1280,80):grid.line((x,0,x,1777),fill='black',width=3)
        for y in range(0,1778,96):grid.line((0,y,1279,y),fill='black',width=3)
    encoded={}
    for fmt in ('PNG','JPEG','GIF'):
        out=io.BytesIO()
        source=picture if fmt=='PNG' else picture.convert('RGB').resize((240,333))
        source.save(out,fmt);encoded[fmt.lower()]=out.getvalue()
    if args.viewer_only:assert 131072<len(encoded['png'])<=524288,len(encoded['png'])
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
                for kind in (('png',) if args.viewer_only else ('png','jpeg','gif')):
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
                        if args.viewer_only:
                            cx,cy=(box[0]+box[2])//2,(box[1]+box[3])//2
                            dp=lambda n:max(1,round(n*dpi/160))
                            # Moving a held image must keep the reading gesture.
                            app.command(f'TOUCH DOWN 0 {cx} {cy}');time.sleep(.08)
                            app.command(f'TOUCH MOVE 0 {cx+dp(12)} {cy}');time.sleep(.65)
                            app.command(f'TOUCH UP 0 {cx+dp(12)} {cy}');time.sleep(.15)
                            assert 'WEREAD PICTURE OPEN' not in app.logs()
                            mark=len(requests);ready=app.logs().count('WEREAD IMAGE READY')
                            app.command(f'TOUCH DOWN 0 {cx} {cy}');time.sleep(.7)
                            app.wait(lambda:'WEREAD PICTURE OPEN' in app.logs())
                            assert 'WEREAD PICTURE OPEN index=0 preview=1' in app.logs(),'Viewer discarded its decoded preview'
                            app.command(f'TOUCH UP 0 {cx} {cy}')
                            app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>ready,30)
                            first=app.shot('viewer-fit');assert len(requests)==mark,'Viewer downloaded a cached image'
                            sw,sh=w-2*inset,h-2*inset;gap=dp(8);foot=min(dp(40),sh//5)
                            control=min(dp(40),sw//5);back=min(dp(64),sw//3);bottom=h-inset-foot//2
                            viewport=(inset+gap,inset+head+gap,w-inset-gap,h-inset-foot-gap)
                            crop=lambda path:Image.open(path).convert('RGB').crop(viewport)
                            app.tap(inset+control+gap+control//2,bottom);zoomed=app.shot('viewer-zoom')
                            assert ImageChops.difference(crop(first),crop(zoomed)).getbbox(),'Zoom did not enlarge the image'
                            x,y=w//2,(viewport[1]+viewport[3])//2
                            app.command(f'TOUCH DOWN 0 {x} {y}')
                            app.command(f'TOUCH MOVE 0 {x+dp(20)} {y+dp(18)}');time.sleep(.12)
                            app.command(f'TOUCH UP 0 {x+dp(20)} {y+dp(18)}');time.sleep(.15)
                            panned=app.shot('viewer-pan')
                            assert ImageChops.difference(crop(zoomed),crop(panned)).getbbox(),'Enlarged image did not pan'
                            app.tap(inset+control//2,bottom);restored=app.shot('viewer-reset')
                            assert not ImageChops.difference(crop(first),crop(restored)).getbbox(),'Zoom reset did not clamp panning'
                            app.tap(w-inset-back//2,bottom)
                            app.wait(lambda:'WEREAD PICTURE CLOSED' in app.logs())
                            app.wait(lambda:app.logs().count('WEREAD IMAGE READY')>=ready+2,30)
                            assert 'WEREAD IMAGE MEMORY HIT index=0' in app.logs(),'Returning from viewer decoded the original image again'
                            returned=Image.open(app.shot('reading-restored')).convert('RGB')
                            assert not ImageChops.difference(shot,returned).getbbox(),'Picture viewer changed the reading position or font'
                            assert len(requests)==mark,'Returning from viewer downloaded the image again'
                            assert 'WEREAD ERROR' not in app.logs() and 'DRAW ERROR' not in app.logs()
                        results.append({'screen':label,'format':kind,'visible_pixels':len(colored),'bounds':box})
                    finally:finish(app)
            if args.viewer_only:
                (args.output/'results.json').write_text(json.dumps({'passed':results},indent=2)+'\n')
                print('Image long press, zoom, pan, cached decode and reading restoration passed on four layouts')
                return
            kind='multiple';state=Path(root)/kind;state.mkdir()
            app=Simulator(args,args.output/kind,state,480,800,240,32)
            try:
                app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(240,100)
                app.wait(lambda:app.logs().count('WEREAD IMAGE READY')==20,timeout=90)
                app.shot('all-images')
                memory=({'image_current_bytes':sum(map(int,re.findall(r'WEREAD IMAGE READY index=\d+ bytes=(\d+)',app.logs())))}
                    if args.skip_memory else app.wait_memory(lambda value:value['image_current_bytes']>0))
                assert memory['image_current_bytes']<=65536*4+20*256,memory
                mark=len(requests)
                # Reading menu -> settings -> final settings page -> images off.
                app.command('KEY BACK');time.sleep(.15);app.tap(360,190)
                app.tap(440,760);app.tap(240,257)
                app.tap(240,760);app.tap(240,105)
                if not args.skip_memory:app.wait_memory(lambda memory:memory['image_current_bytes']==0)
                disabled=Image.open(app.shot('images-disabled')).convert('RGB')
                assert not any((r>150 and g<100 and b<120)or(g>120 and r<100 and b<120) for r,g,b in disabled.getdata())
                time.sleep(.5)
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
