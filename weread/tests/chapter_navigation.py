#!/usr/bin/env python3
"""Real PXA chapter jumps: reuse, session recovery, identity and retained state."""
import argparse,base64,io,json,tempfile,threading,time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit,parse_qs
from PIL import Image
from smoke import Simulator

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for field in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+field,type=Path,required=True)
    p.add_argument('--port',type=int,default=18934);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    image=io.BytesIO();Image.new('RGB',(80,80),'white').save(image,'PNG');qr=image.getvalue()
    image=io.BytesIO();Image.new('RGB',(120,160),'red').save(image,'JPEG');jpeg=image.getvalue()
    mode='healthy';calls=[];renewals=[];errors=[];blocked=threading.Event();release=threading.Event()
    def html(uid,book='1',denied=False):
        total=2 if mode=='catalog-expired' else 1000
        chapters=[{'chapterUid':i,'chapterIdx':i,'title':f'Chapter {i:04}'} for i in range(1,total+1)]
        if mode in ('healthy','locked'):chapters[1]['isLock']=True
        reader={'bookInfo':{'bookId':book,'title':'Synthetic chapter book','paid':1,'lastChapterIdx':3 if total==2 else total},
            'curChapter':{'chapterUid':uid,'chapterIdx':uid,'title':f'Chapter {uid:04}'},
            'curChapterUrlParam':{'param':f'fixture-{uid}'},'curChapterSection':0,'chapterSections':[1],
            'prevChapterUrlParam':{'param':f'fixture-{uid-1}'} if uid>1 else {},
            'nextChapterUrlParam':{'param':f'fixture-{uid+1}'},'contentText':f'<p>Chapter {uid}. '+('Readable paragraph. '*200)+'</p>',
            'chapterIndexes':chapters,'readerUrlParams':[{'cUid':i,'param':f'fixture-{i}'} for i in range(1,total+1)],
            'chapterInfoCount':total,'errorInfo':{'code':-505} if denied else {},'isCurChapterNeedToPay':denied}
        if mode=='locked':reader['errorInfo']={}
        if mode=='image-blocked' and uid==1:reader['contentText']=f'<img src="http://127.0.0.1:{a.port}/slow-image" style="width:3em" data-ratio="1.333">'+reader['contentText']
        return '<script>window.__NUXT__=(function(a){return '+json.dumps({'state':{'reader':reader,'unused':'x'*160000}})+'}(0));</script>'
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def reply(self,value,status=200,cookies=(),location=None):
            data=value.encode() if isinstance(value,str) else json.dumps(value).encode()
            self.send_response(status);self.send_header('Content-Length',str(len(data)))
            if location:self.send_header('Location',location)
            for cookie in cookies:self.send_header('Set-Cookie',cookie)
            self.end_headers()
            try:self.wfile.write(data)
            except (BrokenPipeError,ConnectionResetError):pass
        def expired(self):
            return 'wr_skey=renewed-only' not in self.headers.get('Cookie','')
        def do_GET(self):
            path=urlsplit(self.path).path
            if path.endswith('/getuid'):self.reply({'uid':'fixture'})
            elif path.endswith('/qrcode'):self.reply({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(qr).decode()})
            elif path.endswith('/shelfloadmore'):
                self.reply({'books':[{'bookId':'1','title':'Synthetic chapter book'}],'bookReaderUrls':[{'bId':'1','param':'fixture-1'}],'totalCount':1,'kkIdx':1})
            elif path=='/slow-image':
                assert not self.headers.get('Cookie')
                self.send_response(200);self.send_header('Content-Length',str(len(jpeg)));self.end_headers()
                self.wfile.write(jpeg[:128]);self.wfile.flush();blocked.set();release.wait(12)
                try:self.wfile.write(jpeg[128:])
                except (BrokenPipeError,ConnectionResetError):pass
            elif path.endswith('/reader'):
                assert not self.headers.get('Authorization')
                uid=int(parse_qs(urlsplit(self.path).query)['bc'][0].split('-')[1]);calls.append(('reader',uid))
                if uid==1:self.reply(html(uid));return
                if mode in ('expired','rejected','http-auth','redirect') and self.expired():
                    if mode=='http-auth':self.reply({},403)
                    elif mode=='redirect':self.reply({},302,location='/wrwebsimplenjlogic/login')
                    else:self.reply({'sessionTimeout':1})
                elif mode=='rate-limit':self.reply({},429)
                elif mode=='malformed':self.reply('<html>unrecognized response</html>')
                elif mode=='wrong-chapter':self.reply(html(3))
                elif mode=='wrong-book':self.reply(html(uid,'different'))
                elif mode in ('denied','locked'):self.reply(html(uid,denied=True))
                elif mode=='blocked':blocked.set();release.wait(12);self.reply(html(uid))
                else:self.reply(html(uid))
            elif path.endswith('/catalogloadmore'):
                calls.append(('catalog',0));assert not self.headers.get('Authorization')
                if self.expired():self.reply({'data':{'errcode':-2012}})
                else:self.reply({'succ':1,'chapterInfos':[{'chapterUid':3,'chapterIdx':3,'title':'Chapter 3'}],
                    'readerUrlParmas':[{'cUid':3,'param':'fixture-3'}]})
            else:errors.append(path);self.reply({},404)
        def do_POST(self):
            path=urlsplit(self.path).path;self.rfile.read(int(self.headers.get('Content-Length',0)))
            if path.endswith('/getlogininfo'):self.reply({'vid':1,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):self.reply({'vid':1,'accessToken':'fixture-only','refreshToken':'fixture-only'},cookies=('wr_vid=1; Path=/','wr_skey=fixture-only; Path=/'))
            elif path=='/web/login/renewal':
                assert not self.headers.get('Authorization');renewals.append(mode)
                self.reply({'succ':0 if mode=='rejected' else 1},cookies=('wr_skey=renewed-only; Path=/',))
            else:errors.append(path);self.reply({},404)
    server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start();results=[]
    def contents(app):app.tap(148,120);app.tap(76,116)
    def check_lock(app,expected,name):
        shot=Image.open(app.shot(name)).convert('RGB')
        dark=sum(max(shot.getpixel((x,y)))<100 for y in range(134,158) for x in range(252,273))
        assert (dark>20 if expected else dark<5),(name,dark)
    try:
        with tempfile.TemporaryDirectory(prefix='chapter-navigation-',dir=a.output) as temp:
            for case in ('healthy','locked','expired','http-auth','redirect','rejected','rate-limit','malformed','wrong-chapter','wrong-book','denied','blocked','catalog-expired','image-blocked'):
                mode=case;calls.clear();renewals.clear();blocked.clear();release.clear()
                state=Path(temp)/case;state.mkdir();app=Simulator(a,a.output/case,state,296,240,160,58)
                try:
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(120,83)
                    app.wait(lambda:'WEREAD READER READY' in app.logs());auth=next(state.rglob('session.json'));original=auth.read_bytes()
                    if case=='image-blocked':app.wait(blocked.is_set)
                    contents(app)
                    if case in ('healthy','locked','denied'):check_lock(app,case!='denied','directory-initial')
                    if case=='catalog-expired':
                        app.tap(148,203);app.wait(lambda:'WEREAD CATALOG READY' in app.logs())
                        assert calls.count(('catalog',0))==2 and len(renewals)==1
                    else:
                        if case=='healthy':
                            app.tap(148,80);app.wait(lambda:'WEREAD CURRENT CHAPTER LOCAL' in app.logs())
                            assert calls==[('reader',1)];contents(app)
                        app.tap(148,140)
                        if case in ('healthy','expired','http-auth','redirect','image-blocked'):
                            app.wait(lambda:app.logs().count('WEREAD READER READY')==2)
                            assert 'WEREAD CATALOG REUSED' in app.logs()
                            assert calls.count(('reader',2))==(1 if case in ('healthy','image-blocked') else 2)
                            assert len(renewals)==(0 if case in ('healthy','image-blocked') else 1)
                            if case=='healthy':
                                contents(app);check_lock(app,False,'directory-unlocked');app.command('KEY BACK')
                            if case=='image-blocked':
                                release.set();time.sleep(.2)
                                assert 'WEREAD IMAGE READY' not in app.logs(),'Old chapter image completed after jump'
                        elif case=='blocked':
                            app.wait(blocked.is_set);app.command('KEY BACK');release.set();time.sleep(.3)
                            assert app.logs().count('WEREAD READER READY')==1
                        else:
                            marker=('WEREAD READER AUTHORIZATION REQUIRED' if case=='locked' else
                                'WEREAD READER CONTENT UNAVAILABLE' if case=='denied' else
                                'WEREAD CHAPTER TARGET MISMATCH' if case.startswith('wrong-') else
                                'WEREAD READER HYDRATION ERROR' if case=='malformed' else 'WEREAD READING FAILED')
                            app.wait(lambda:marker in app.logs())
                            assert app.logs().count('WEREAD READER READY')==1 and 'WEREAD ERROR' not in app.logs()
                            if case=='rejected':assert auth.read_bytes()==original and len(renewals)==1
                            if case=='rate-limit':assert calls.count(('reader',2))==1 and not renewals
                            if case in ('denied','locked'):
                                app.command('KEY BACK');time.sleep(.1);check_lock(app,True,'directory-locked')
                            app.tap(148,80);app.wait(lambda:'WEREAD CURRENT CHAPTER LOCAL' in app.logs())
                            assert calls.count(('reader',1))==1,'Failure discarded the old chapter'
                    app.shot('result');app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    assert 'DRAW ERROR' not in app.logs() and 'Guest exception:' not in app.logs()
                    assert not errors,errors
                    results.append({'case':case,'passed':True})
                finally:release.set();app.close()
    finally:release.set();server.shutdown();server.server_close()
    (a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Chapter jumps, catalog reuse, transactional renewal, failures and cancellation passed')

if __name__=='__main__':main()
