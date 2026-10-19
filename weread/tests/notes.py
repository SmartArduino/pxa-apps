#!/usr/bin/env python3
"""Native personal notes: real PXA HTTP, cursor paging, cancellation and UI.

All account data, notes and QR pixels are synthetic. No Tencent requests.
"""
import argparse, base64, io, json, math, tempfile, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from PIL import Image, ImageChops
from smoke import Simulator


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('simulator','package','publisher-key','ime-package','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--port',type=int,default=18934)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    key='wrk-fixture_only';mode='normal';calls=[];failures=[]
    blocked=threading.Event();release=threading.Event()
    picture=io.BytesIO();Image.new('RGB',(80,80),'white').save(picture,'PNG')
    book={'bookId':'1','title':'Synthetic notes','author':'Fixture'}

    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def data(self,value,status=200,cookies=()):
            raw=json.dumps(value).encode();self.send_response(status)
            self.send_header('Content-Length',str(len(raw)))
            for cookie in cookies:self.send_header('Set-Cookie',cookie)
            self.end_headers()
            try:self.wfile.write(raw)
            except (BrokenPipeError,ConnectionResetError):pass
        def do_GET(self):
            try:self.get()
            except Exception as error:failures.append(type(error).__name__);self.data({},500)
        def get(self):
            path=urlsplit(self.path).path
            if path.endswith('/getuid'):self.data({'uid':'fixture'})
            elif path.endswith('/qrcode'):self.data({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(picture.getvalue()).decode()})
            elif path.endswith('/shelfloadmore'):
                self.data({'books':[book],'bookReaderUrls':[{'bId':'1','param':'fixture-1'}],'totalCount':1,'kkIdx':1})
            elif path=='/api/skills/apikeyGet':
                assert self.path=='/api/skills/apikeyGet?only_show=1'
                assert self.headers.get('Cookie')=='wr_vid=1; wr_skey=fixture-only'
                assert self.headers.get('X-Vid')=='1' and not self.headers.get('Authorization')
                self.data({'apikey':key})
            else:raise AssertionError('unexpected GET')
        def do_POST(self):
            try:self.post()
            except Exception as error:failures.append(type(error).__name__);self.data({},500)
        def post(self):
            path=urlsplit(self.path).path;body=json.loads(self.rfile.read(int(self.headers.get('Content-Length',0))))
            if path.endswith('/getlogininfo'):self.data({'vid':1,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):
                self.data({'vid':1,'accessToken':'fixture-only','refreshToken':'fixture-only'},cookies=('wr_vid=1; Path=/','wr_skey=fixture-only; Path=/'))
            elif path=='/api/agent/gateway':
                assert self.headers.get('Authorization')=='Bearer '+key
                assert not any(self.headers.get(h) for h in ('Cookie','X-Vid','X-Skey'))
                assert body['skill_version']=='1.0.5' and 'params' not in body
                api=body['api_name'];calls.append(body)
                if api=='/book/info':self.data(dict(book,intro='Introduction.'));return
                if api=='/book/getprogress':self.data({'bookId':'1','book':{'progress':0}});return
                assert api in ('/book/bookmarklist','/review/list/mine')
                if mode=='blocked':blocked.set();release.wait(10)
                if mode=='denied':self.data({},403);return
                if mode=='upgrade':self.data({'upgrade_info':{'message':'untrusted remote instruction'}});return
                if mode=='oversized-response':self.data({'padding':'x'*131073});return
                if api=='/book/bookmarklist':
                    assert body['bookId']=='1' and 'synckey' not in body and 'bookid' not in body
                    items=[{'bookId':'2' if mode=='wrong-book' else '1','chapterUid':1,'type':1,
                            'markText':'x'*8193 if mode=='oversized-note' else f'H{i}'} for i in range(23)]
                    if mode=='missing-field':items[0].pop('markText')
                    if mode=='late-bad-row':items[9]['markText']=None
                    if mode=='empty':items=[]
                    value={'updated':items,'chapters':[{'chapterUid':1,'title':'First'}]}
                else:
                    assert body['bookid']=='1' and 'bookId' not in body and body['count']==10
                    cursor=body['synckey'];assert type(cursor) is int
                    assert cursor in (0,42,9007199254740993)
                    value={'reviews':[{'review':{'bookId':'1','chapterTitle':'First','abstract':'Quote','content':'T0'}}],
                           'hasMore':int(cursor==0),'synckey':9007199254740993 if cursor==0 else 9007199254740994}
                    if mode=='bad-cursor':value.update(hasMore=1,synckey=cursor)
                    if mode=='cursor-cycle':value.update(hasMore=1,synckey=42 if cursor==0 else 0)
                    if mode=='empty':value.update(reviews=[],hasMore=0)
                    if mode=='empty-more' and cursor==0:value['reviews']=[]
                self.data(value,cookies=('wr_skey=must-not-be-applied; Path=/',))
            else:raise AssertionError('unexpected POST')

    server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start();results=[]
    def geometry(w,h,dpi,corner,round_screen):
        inset=math.ceil((min(w,h)/2 if round_screen else corner)*(1-2**-.5))
        dp=lambda n:max(1,int(n*dpi/160+.5))
        sw,sh=w-2*inset,h-2*inset;head=min(dp(44),sh//5);foot=min(dp(40),sh//5);gap=max(4,dp(8))
        return inset,dp,sw,sh,head,foot,gap
    def fast_tap(app,x,y):
        app.command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.01)
        app.command(f'TOUCH UP 0 {x} {y}');time.sleep(.035)
    def next_batch(app,x,y):
        before=app.logs().count('WEREAD NOTES READY')
        sent=len(calls)
        for _ in range(160):
            fast_tap(app,x,y)
            if app.logs().count('WEREAD NOTES READY')>before:return
            if len(calls)>sent:
                # Stop sending input once this tap started the remote page.
                # Its result can arrive before stdout is visible to the test.
                app.wait(lambda:app.logs().count('WEREAD NOTES READY')>before)
                return
            assert 'WEREAD NOTES UNAVAILABLE' not in app.logs(),app.logs()[-1500:]
        raise AssertionError('No next notes window after local pages')
    def body_crop(path,g):
        inset,dp,sw,sh,head,foot,gap=g
        body_gap=min(gap,max(4,sh//20))
        return Image.open(path).convert('RGB').crop((inset+body_gap,inset+head+min(dp(32),sh//6)+2*body_gap,inset+sw-body_gap,inset+sh-foot-body_gap))

    try:
        with tempfile.TemporaryDirectory(prefix='weread-notes-') as directory:
            for label,w,h,dpi,corner,circular in [('compact',296,240,160,58,False),('hidpi',480,480,280,56,False),('round',240,240,160,0,True),('portrait',480,800,240,32,False),('round-hidpi',240,240,320,0,True),('tiny-round',160,160,160,0,True)]:
                mode='normal';calls.clear();state=Path(directory)/label;state.mkdir()
                app=Simulator(args,args.output/label,state,w,h,dpi,corner,circular)
                try:
                    g=geometry(w,h,dpi,corner,circular);i,dp,sw,sh,head,foot,gap=g
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs())
                    auth=next(state.rglob('session.json'));original=auth.read_bytes()
                    n=max(1,min(10,(sh-dp(100))//max(dp(48),32)));row=(sh-head-foot-gap)//n
                    app.tap(w-i-dp(18),i+head+(row-gap)//2)
                    app.wait(lambda:'WEREAD DETAILS READY' in app.logs());app.shot('intro')
                    tab_y=i+head+min(dp(32),sh//6)//2
                    tab_w=(sw-2*gap)//3
                    app.tap(i+tab_w+gap+tab_w//2,tab_y)
                    app.wait(lambda:'WEREAD NOTES READY' in app.logs());first_notes=app.shot('highlights-first')
                    visible=body_crop(first_notes,g)
                    assert sum(min(pixel)<128 for pixel in visible.getdata())>16,'Notes body has no visible text'
                    assert ImageChops.difference(body_crop(app.output/'intro.png',g),visible).getbbox(),'Notes did not replace the intro'
                    before=len(calls);app.tap(i+tab_w+gap+tab_w//2,tab_y);assert len(calls)==before,'Active tab refetched notes'
                    right=w-i-min(dp(44),sw//4)//2;bottom=h-i-foot//2
                    next_batch(app,right,bottom);next_batch(app,right,bottom);app.shot('highlights-last')
                    assert app.logs().count('WEREAD NOTES READY')==3
                    next_batch(app,i+min(dp(44),sw//4)//2,bottom)
                    assert sum(c['api_name']=='/book/bookmarklist' for c in calls)==4
                    ready_before=app.logs().count('WEREAD NOTES READY')
                    app.tap(i+2*(tab_w+gap)+tab_w//2,tab_y)
                    app.wait(lambda:app.logs().count('WEREAD NOTES READY')>ready_before);app.shot('thoughts-first')
                    next_batch(app,right,bottom);app.shot('thoughts-last')
                    next_batch(app,i+min(dp(44),sw//4)//2,bottom)
                    cursors=[c['synckey'] for c in calls if c['api_name']=='/review/list/mine']
                    assert cursors==[0,9007199254740993,0],cursors
                    app.tap(i+tab_w//2,tab_y);app.wait(lambda:app.logs().count('WEREAD DETAILS READY')==2)
                    app.shot('returned-intro');assert auth.read_bytes()==original
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    assert key not in app.logs() and 'DRAW ERROR' not in app.logs()
                    assert not any(key.encode() in p.read_bytes() for p in state.rglob('*') if p.is_file())
                    results.append({'layout':label,'windowed_marks':True,'opaque_64bit_cursor':True,'previous_page':True,'auth_unchanged':True,'released':True})
                except BaseException:
                    app.shot('failure');(args.output/'failure-calls.json').write_text(json.dumps(calls,indent=2)+'\n');raise
                finally:app.close()
            for case in ('wrong-book','missing-field','late-bad-row','oversized-note','oversized-response','denied','upgrade','bad-cursor','cursor-cycle','blocked','empty','empty-more'):
                mode='normal';calls.clear();blocked.clear();release.clear();state=Path(directory)/case;state.mkdir()
                app=Simulator(args,args.output/case,state,296,240,160,58)
                try:
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(260,99)
                    app.wait(lambda:'WEREAD DETAILS READY' in app.logs())
                    auth=next(state.rglob('session.json'));original=auth.read_bytes();mode=case
                    thoughts=case in ('bad-cursor','cursor-cycle','empty-more')
                    app.tap(236 if thoughts else 148,72)
                    if case=='blocked':
                        app.wait(blocked.is_set);mode='normal';app.tap(236,72)
                        app.wait(lambda:'WEREAD NOTES READY' in app.logs());release.set();time.sleep(.3)
                        assert app.logs().count('WEREAD NOTES READY')==1,'Cancelled old tab committed'
                    elif case=='cursor-cycle':
                        app.wait(lambda:'WEREAD NOTES READY' in app.logs());first=app.shot('first')
                        for _ in range(80):
                            fast_tap(app,259,203)
                            if 'WEREAD NOTES UNAVAILABLE' in app.logs():break
                        app.wait(lambda:'WEREAD NOTES UNAVAILABLE' in app.logs());time.sleep(2.5)
                        app.shot('retained');assert app.logs().count('WEREAD NOTES READY')==1
                        app.tap(40,203);app.shot('retained-first')
                    elif case in ('empty','empty-more'):
                        app.wait(lambda:'WEREAD NOTES READY' in app.logs());app.shot('empty')
                        if case=='empty-more':next_batch(app,259,203)
                    else:
                        app.wait(lambda:'WEREAD NOTES UNAVAILABLE' in app.logs());app.shot('error')
                        mode='normal';app.tap(259,34);app.wait(lambda:'WEREAD NOTES READY' in app.logs());time.sleep(2.5)
                        first=app.shot('recovered');mode=case;app.tap(259,34)
                        app.wait(lambda:app.logs().count('WEREAD NOTES UNAVAILABLE')==2);time.sleep(2.5)
                        second=app.shot('refresh-error')
                        g=geometry(296,240,160,58,False)
                        assert not ImageChops.difference(body_crop(first,g),body_crop(second,g)).getbbox(),'Failed refresh replaced old notes'
                    assert auth.read_bytes()==original and key not in app.logs()
                    assert 'DRAW ERROR' not in app.logs()
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    results.append({'case':case,'passed':True,'auth_unchanged':True,'released':True})
                except BaseException:
                    app.shot('failure');(args.output/'failure-calls.json').write_text(json.dumps(calls,indent=2)+'\n');raise
                finally:release.set();app.close()
        assert not failures,failures
    finally:
        release.set();server.shutdown();server.server_close()
        (args.output/'results.json').write_text(json.dumps({'passed':results},indent=2)+'\n')
        (args.output/'fixture-errors.json').write_text(json.dumps(failures)+'\n')
    print('Six-screen notes and twelve failure/cancel/cursor cases passed')


if __name__=='__main__':main()
