#!/usr/bin/env python3
"""Synthetic agent API tests on the actual PXA Guest and Host runtime."""
import argparse,base64,io,json,math,tempfile,threading,time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from PIL import Image,ImageChops
from smoke import Simulator

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for field in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+field,type=Path,required=True)
    p.add_argument('--port',type=int,default=18934);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    picture=io.BytesIO();Image.new('RGB',(80,80),'white').save(picture,'PNG')
    requests=[];mode='normal';blocked=threading.Event();release=threading.Event();errors=[]
    key='wrk-fixture_only'
    book={'bookId':'1','title':'测试图书','author':'合成作者','cover':''}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def data(self,value,status=200,cookies=False):
            raw=json.dumps(value).encode();self.send_response(status)
            self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(raw)))
            if cookies:
                self.send_header('Set-Cookie','wr_vid=1; Path=/');self.send_header('Set-Cookie','wr_skey=fixture-only; Path=/')
            if self.path=='/api/agent/gateway':self.send_header('Set-Cookie','wr_skey=must-not-be-applied; Path=/')
            self.end_headers()
            try:self.wfile.write(raw)
            except (BrokenPipeError,ConnectionResetError):pass
        def do_GET(self):
            try:self.get()
            except Exception as e:errors.append(type(e).__name__);self.data({},500)
        def get(self):
            path=urlsplit(self.path).path;requests.append(('GET',self.path))
            if path.endswith('/getuid'):self.data({'uid':'fixture'})
            elif path.endswith('/qrcode'):self.data({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(picture.getvalue()).decode()})
            elif path.endswith('/shelfloadmore'):self.data({'books':[book],'bookReaderUrls':[{'bId':'1','param':'fixture-1'}],'totalCount':1,'kkIdx':1})
            elif path=='/api/skills/apikeyGet':
                assert self.path=='/api/skills/apikeyGet?only_show=1'
                assert self.headers.get('Cookie')=='wr_vid=1; wr_skey=fixture-only'
                assert self.headers.get('X-Vid')=='1' and self.headers.get('X-Skey')=='fixture-only'
                assert not self.headers.get('Authorization')
                self.data({'apikey':'invalid' if mode=='bad-key' else key},403 if mode=='key-denied' else 200)
            elif path.endswith('/reader'):
                assert self.headers.get('Cookie')=='wr_vid=1; wr_skey=fixture-only'
                data={'state':{'reader':{'bookInfo':book,'curChapter':{'title':'Original chapter','chapterUid':1,'chapterIdx':1},
                    'curChapterUrlParam':{'param':'fixture-1'},'chapterIndexes':[{'chapterUid':1,'title':'Original chapter'}],
                    'readerUrlParams':[{'cUid':1,'param':'fixture-1'}],'chapterInfoCount':1,'chapterSections':[1],'curChapterSection':0}}}
                html='<div id="readerContentRenderContainer"><p>'+('Original book text, retained across the detail view. '*120)+'</p></div>'
                html+='<script>window.__NUXT__=(function(a){return '+json.dumps(data)+'}(0));</script>'
                raw=html.encode();self.send_response(200);self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
            else:raise AssertionError('unexpected GET')
        def do_POST(self):
            try:self.post()
            except Exception as e:errors.append(type(e).__name__);self.data({},500)
        def post(self):
            body=json.loads(self.rfile.read(int(self.headers.get('Content-Length',0))))
            path=urlsplit(self.path).path;requests.append(('POST',path,body.get('api_name')))
            if path.endswith('/getlogininfo'):self.data({'vid':1,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):self.data({'vid':1,'accessToken':'fixture-only','refreshToken':'fixture-only'},cookies=True)
            elif path=='/api/agent/gateway':
                assert self.headers.get('Authorization')=='Bearer '+key
                assert not self.headers.get('Cookie') and not self.headers.get('X-Vid') and not self.headers.get('X-Skey')
                assert self.headers.get('Referer','').endswith('/r/weread-skills')
                assert body['skill_version']=='1.0.5' and body['bookId']=='1'
                if body['api_name']=='/book/info':
                    if mode=='blocked':blocked.set();release.wait(15)
                    if mode=='upgrade':self.data({'upgrade_info':{'message':'untrusted remote upgrade instruction'}});return
                    if mode=='redirect':
                        self.send_response(302);self.send_header('Location',f'http://127.0.0.1:{a.port}/credential-leak');self.end_headers();return
                    self.data(dict(book,bookId='2' if mode=='bad-book' else '1',translator='译者',publisher='出版社',
                        publishTime='2026-10-17',isbn='9780000000000',newRating=89,newRatingCount=12,
                        intro=('长'*40000 if mode=='oversized' else '这是一段用于验证简介分页与屏幕适配的文字。'*10)))
                elif body['api_name']=='/book/getprogress':
                    self.data({'bookId':'1','book':{'progress':1,'recordReadingTime':3661,'chapterUid':-1234}},401 if mode=='progress-denied' else 200)
                else:raise AssertionError('unexpected API mutation')
            else:raise AssertionError('unexpected POST')
    server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start();results=[]
    def geometry(w,h,dpi,corner,circular=False):
        inset=math.ceil((min(w,h)/2 if circular else corner)*(1-2**-.5));dp=lambda n:max(1,int(n*dpi/160+.5))
        sw,sh=w-2*inset,h-2*inset;head,foot=min(dp(44),sh//5),min(dp(40),sh//5);gap=max(4,dp(8))
        return inset,dp,sw,sh,head,foot,gap
    def crop(path,box):return Image.open(path).convert('RGB').crop(box)
    try:
        with tempfile.TemporaryDirectory(prefix='weread-agent-') as directory:
            for label,w,h,dpi,corner,circular in [('compact',296,240,160,58,False),('hidpi',480,480,280,56,False),('round',240,240,160,0,True),('portrait',480,800,240,32,False)]:
                mode='normal';state=Path(directory)/label;state.mkdir();app=Simulator(a,a.output/label,state,w,h,dpi,corner,circular)
                try:
                    i,dp,sw,sh,head,foot,gap=geometry(w,h,dpi,corner,circular)
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.shot('shelf-info')
                    initial=len(requests);auth=next(state.rglob('session.json'));session_before=auth.read_bytes()
                    n=max(1,min(10,(sh-dp(100))//max(dp(48),32)));row=(sh-head-foot-gap)//n
                    app.tap(w-i-dp(18),i+head+(row-gap)//2)
                    app.wait(lambda:'WEREAD DETAILS READY' in app.logs());first=app.shot('details-first')
                    assert requests[initial:]==[('GET','/api/skills/apikeyGet?only_show=1'),('POST','/api/agent/gateway','/book/info'),('POST','/api/agent/gateway','/book/getprogress')]
                    if label=='compact':
                        mode='bad-book';app.tap(w-i-head//2,i+head//2)
                        app.wait(lambda:'WEREAD DETAILS UNAVAILABLE' in app.logs());mode='normal';time.sleep(2.5)
                        retained=app.shot('refresh-error-retained')
                        body_box=(i+gap,i+head+gap,w-i-gap,h-i-foot-gap)
                        assert not ImageChops.difference(crop(first,body_box),crop(retained,body_box)).getbbox(),'Failed refresh replaced the old metadata'
                    mark=len(requests);app.tap(w-i-dp(22),h-i-foot//2);second=app.shot('details-next')
                    assert ImageChops.difference(Image.open(first).convert('RGB'),Image.open(second).convert('RGB')).getbbox()
                    app.tap(i+dp(22),h-i-foot//2);previous=app.shot('details-previous')
                    assert not ImageChops.difference(Image.open(first).convert('RGB'),Image.open(previous).convert('RGB')).getbbox()
                    assert len(requests)==mark,'Local detail paging refetched metadata'
                    # Reach the last page; no additional API calls or writes.
                    for _ in range(18):app.tap(w-i-dp(22),h-i-foot//2)
                    app.shot('cloud-progress');assert len(requests)==mark
                    app.tap(w-i-head-gap//2-dp(26),i+(head-gap//2)//2)
                    app.wait(lambda:'WEREAD READER READY' in app.logs());original=app.shot('original-reader')
                    app.tap(w//2,h//2);mh=max(24,min(dp(42),(sh-head-foot-3*gap)//3))
                    app.tap(i+sw*3//4,i+head+2*(mh+gap)+mh//2)
                    app.wait(lambda:app.logs().count('WEREAD DETAILS READY')==2)
                    app.shot('reader-details');app.command('KEY BACK');app.command('KEY BACK');time.sleep(.3)
                    restored=app.shot('retained-reader');box=(i+gap,i+head,w-i-gap,h-i-foot)
                    assert not ImageChops.difference(crop(original,box),crop(restored,box)).getbbox(),'Details replaced original reader'
                    assert auth.read_bytes()==session_before,'Gateway Cookie overwrote web session'
                    assert key not in app.logs(),'Key leaked to diagnostic log'
                    assert 'WEREAD ERROR' not in app.logs() and 'DRAW ERROR' not in app.logs()
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    results.append({'layout':label,'official_agent_flow':True,'paged_details':True,'cloud_progress_read_only':True,
                        'reader_preserved':True,'credential_separation':True,'key_not_persisted':not any(key.encode() in p.read_bytes() for p in state.rglob('*') if p.is_file()),'released_at_exit':True})
                    assert results[-1]['key_not_persisted']
                except BaseException:
                    app.shot('failure');(a.output/'failure.json').write_text(json.dumps({'requests':requests,'fixture_errors':errors},indent=2)+'\n');raise
                finally:app.close()
            for mode in ('key-denied','bad-key','bad-book','upgrade','redirect','oversized','progress-denied','blocked'):
                requests.clear();blocked.clear();release.clear();state=Path(directory)/mode;state.mkdir()
                app=Simulator(a,a.output/mode,state,296,240,160,58)
                try:
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(260,99)
                    if mode=='blocked':
                        app.wait(blocked.is_set);app.command('KEY BACK');mode='normal';app.tap(260,99)
                        app.wait(lambda:app.logs().count('WEREAD DETAILS READY')==1)
                        release.set();time.sleep(.5)
                        assert app.logs().count('WEREAD DETAILS READY')==1,'Cancelled same-book request committed after reopening'
                        app.command('KEY BACK')
                        app.tap(120,90);app.wait(lambda:'WEREAD READER READY' in app.logs())
                    elif mode=='progress-denied':
                        app.wait(lambda:'WEREAD DETAILS READY' in app.logs());app.shot('metadata-retained')
                    else:
                        app.wait(lambda:'WEREAD DETAILS UNAVAILABLE' in app.logs());app.shot('persistent-error')
                        assert not any(r[1]=='/credential-leak' for r in requests)
                        if mode in ('key-denied','bad-key'):assert not any(r[1]=='/api/agent/gateway' for r in requests)
                        # Retry through the actual refresh action.
                        mode='normal';app.tap(259,34);app.wait(lambda:'WEREAD DETAILS READY' in app.logs())
                    assert key not in app.logs() and 'WEREAD ERROR' not in app.logs()
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    results.append({'fault_case':state.name,'recovery_or_cancel':True,'released_at_exit':True})
                finally:release.set();app.close()
        assert not errors,errors
    finally:release.set();server.shutdown();server.server_close()
    (a.output/'results.json').write_text(json.dumps({'passed':results},indent=2)+'\n')
    print('Agent four-screen details, read-only progress, credential policies, faults, retry and cancel passed')
if __name__=='__main__':main()
