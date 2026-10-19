#!/usr/bin/env python3
"""Synthetic, private local bookmarks on the actual PXA AOT/Wasm UI."""
import argparse,base64,io,json,math,shutil,struct,tempfile,threading,time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs,urlsplit
from PIL import Image,ImageChops
from smoke import Simulator

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--port',type=int,default=19178);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    qr=io.BytesIO();Image.new('RGB',(40,40),'white').save(qr,'PNG')
    mapping=struct.unpack('<20992H',(Path(__file__).parents[1]/'assets/weread-cjk.bin').read_bytes())
    reverse={chr(value):chr(0x4e00+i)for i,value in enumerate(mapping)}
    def encoded(text):return ''.join(reverse.get(c,c)for c in text)
    calls=[];mode='normal';blocked=threading.Event();release=threading.Event()
    book={'bookId':'1','title':'书签测试','author':'合成作者'}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def reply(self,value,mime='application/json',cookies=False):
            data=value.encode() if isinstance(value,str) else json.dumps(value).encode()
            self.send_response(200);self.send_header('Content-Length',str(len(data)));self.send_header('Content-Type',mime)
            if cookies:self.send_header('Set-Cookie','wr_vid=1; Path=/');self.send_header('Set-Cookie','wr_skey=fixture-only; Path=/')
            self.end_headers()
            try:
                if mode=='blocked' and mime=='text/html':
                    self.wfile.write(data[:128]);self.wfile.flush();blocked.set();release.wait(10)
                    self.wfile.write(data[128:])
                else:self.wfile.write(data)
            except (BrokenPipeError,ConnectionResetError):pass
        def do_GET(self):
            path=urlsplit(self.path).path;calls.append(path)
            if path.endswith('/getuid'):self.reply({'uid':'fixture'})
            elif path.endswith('/qrcode'):self.reply({'succ':1,'data':'data:image/png;base64,'+base64.b64encode(qr.getvalue()).decode()})
            elif path.endswith('/shelfloadmore'):self.reply({'books':[book],'bookReaderUrls':[{'bId':'1','param':'fixture-1'}],'totalCount':1,'kkIdx':1})
            elif path.endswith('/reader'):
                chapter=int(parse_qs(urlsplit(self.path).query)['bc'][0].rsplit('-',1)[1])
                reader={'bookInfo':book,'curChapter':{'title':f'章节 {chapter}','chapterUid':chapter,'chapterIdx':chapter},
                    'curChapterUrlParam':{'param':f'fixture-{chapter}'},'chapterIndexes':[{'chapterUid':i,'title':f'章节 {i}'} for i in (1,2)],
                    'readerUrlParams':[{'cUid':i,'param':f'fixture-{i}'} for i in (1,2)],'chapterInfoCount':2,
                    'nextChapterUrlParam':{'param':'fixture-2'} if chapter==1 else {},'prevChapterUrlParam':{'param':'fixture-1'} if chapter==2 else {},
                    'chapterSections':[1],'curChapterSection':0}
                if mode=='wrong-book':reader['bookInfo']=dict(book,bookId='2')
                text=encoded(('Updated ' if mode=='updated' else '')+''.join(f'<p>Chapter {chapter}, paragraph {i}: bookmark UTF-8 中文𠳐 test.</p>' for i in range(80)))
                self.reply('<div id="readerContentRenderContainer">'+text+'</div><script>window.__NUXT__=(function(a){return '+json.dumps({'state':{'reader':reader}})+'}(0));</script>','text/html')
            else:self.reply({})
        def do_POST(self):
            self.rfile.read(int(self.headers.get('Content-Length',0)));path=urlsplit(self.path).path;calls.append(path)
            if path.endswith('/getlogininfo'):self.reply({'vid':1,'skey':'fixture-only','code':'fixture-only'})
            elif path.endswith('/weblogin'):self.reply({'vid':1,'accessToken':'fixture-only','refreshToken':'fixture-only'},cookies=True)
            else:self.reply({})
    server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler);server.daemon_threads=True
    threading.Thread(target=server.serve_forever,daemon=True).start();results=[]
    def geometry(w,h,dpi,corner,circular):
        i=math.ceil((min(w,h)/2 if circular else corner)*(1-2**-.5));dp=lambda n:max(1,int(n*dpi/160+.5))
        sw,sh=w-2*i,h-2*i;head=min(dp(44),sh//5);foot=min(dp(40),sh//5);gap=max(4,dp(8));rows=max(1,min(10,(sh-dp(100))//max(dp(48),32)))
        return i,dp,sw,sh,head,foot,gap,rows
    def changed(first,last,g):
        i,dp,sw,sh,head,foot,gap,rows=g;box=(i+gap,i+head,i+sw-gap,i+sh-foot)
        return ImageChops.difference(Image.open(first).convert('RGB').crop(box),Image.open(last).convert('RGB').crop(box)).getbbox()
    def open_reader(app,g):
        i,dp,sw,sh,head,foot,gap,rows=g
        app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(i+sw//3,i+head+(sh-head-foot-gap)//rows//2)
        app.wait(lambda:'WEREAD READER READY' in app.logs())
    def enter_marks(app,g):
        i,dp,sw,sh,head,foot,gap,rows=g
        menu_h=max(24,min(dp(42),(sh-head-foot-3*gap)//3))
        app.tap(app.width//2,app.height//2);app.tap(i+sw-min(dp(64),sw//3)//2,i+head+menu_h//2)
    def open_marks(app,g):
        before=app.logs().count('WEREAD BOOKMARKS READY');enter_marks(app,g)
        app.wait(lambda:app.logs().count('WEREAD BOOKMARKS READY')>before)
    def add(app,g):
        i,dp,sw,sh,head,foot,gap,rows=g;before=app.logs().count('WEREAD BOOKMARKS SAVED')
        app.tap(i+sw-min(dp(52),sw//3)//2,i+(head-gap//2)//2)
        app.wait(lambda:app.logs().count('WEREAD BOOKMARKS SAVED')>before)
    def back_reading(app):app.command('KEY BACK');app.command('KEY BACK');time.sleep(.15)
    def go_first(app,g,count):
        i,dp,sw,sh,head,foot,gap,rows=g
        for _ in range((count-1)//rows):
            if rows==1:
                before=app.shot('before-bookmark-swipe');mark=len(calls)
                y=i+head+(sh-head-foot-gap)//2;x=i+sw//2
                app.command(f'TOUCH DOWN 0 {x} {y}')
                app.command(f'TOUCH MOVE 0 {x} {min(app.height-1,y+dp(32))}')
                app.command(f'TOUCH UP 0 {x} {min(app.height-1,y+dp(32))}')
                app.wait(lambda:ImageChops.difference(Image.open(before).convert('RGB'),Image.open(app.shot('after-bookmark-swipe')).convert('RGB')).getbbox())
                assert len(calls)==mark,'Local bookmark swipe refetched content'
            else:app.tap(i+min(dp(44),sw//4)//2,i+sh-foot//2)
    def row(app,g,index=0,remove=False):
        i,dp,sw,sh,head,foot,gap,rows=g;rh=(sh-head-foot-gap)//rows
        app.tap(i+sw-min(dp(44),sw//4)//2 if remove else i+sw//3,i+head+index*rh+(rh-gap)//2)
    try:
        with tempfile.TemporaryDirectory(prefix='weread-bookmarks-') as directory:
            layouts=[('compact',296,240,160,58,False),('hidpi',480,480,280,56,False),('round',240,240,160,0,True),('portrait',480,800,240,32,False),('round-hidpi',240,240,320,0,True),('tiny-round',160,160,160,0,True)]
            for label,w,h,dpi,corner,circular in layouts:
                state=Path(directory)/label;state.mkdir();g=geometry(w,h,dpi,corner,circular)
                def start(suffix):
                    app=Simulator(a,a.output/(label+suffix),state,w,h,dpi,corner,circular);app.width=w;app.height=h;return app
                app=start('');bookmark_file=None
                try:
                    open_reader(app,g);first=app.shot('first-page');open_marks(app,g);app.shot('empty')
                    add(app,g);bookmark_file=next(state.rglob('bookmarks-*.json'));saved=json.loads(bookmark_file.read_text())
                    assert saved['account']=='1' and saved['book']=='1' and len(saved['items'])==1 and saved['items'][0]['offset']==0
                    add(app,g);assert len(json.loads(bookmark_file.read_text())['items'])==1,'Duplicate current page was added'
                    back_reading(app);app.tap(w-g[0]-g[1](10),h//2);second=app.shot('second-page');assert changed(first,second,g)
                    open_marks(app,g);add(app,g);saved=json.loads(bookmark_file.read_text());assert len(saved['items'])==2 and saved['items'][1]['offset']>0
                    app.shot('two-bookmarks');go_first(app,g,2);before=len(calls);row(app,g)
                    restored=app.shot('restored-first');assert not changed(first,restored,g),'Same-chapter bookmark restored different text'
                    assert len(calls)==before,'Same-chapter local bookmark refetched chapter'
                    open_marks(app,g);raw=bookmark_file.read_bytes();row(app,g,remove=True)
                    assert bookmark_file.read_bytes()==raw,'First delete click bypassed confirmation'
                    row(app,g,remove=True);app.wait(lambda:len(json.loads(bookmark_file.read_text())['items'])==1)
                    app.shot('one-bookmark');back_reading(app);app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                finally:app.close()
                app=start('-restart')
                try:
                    open_reader(app,g);open_marks(app,g);assert bookmark_file and len(json.loads(bookmark_file.read_text())['items'])==1
                    before=len(calls);row(app,g);reopened=app.shot('restored-second');assert not changed(second,reopened,g),'Bookmark changed across restart'
                    assert len(calls)==before;open_marks(app,g);row(app,g,remove=True);row(app,g,remove=True)
                    app.wait(lambda:len(json.loads(bookmark_file.read_text())['items'])==0);app.shot('deleted-all')
                    assert 'WEREAD ERROR' not in app.logs() and 'DRAW ERROR' not in app.logs()
                    app.command('KEY HOME');app.process.wait(timeout=10);assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    results.append({'layout':label,'add_deduplicate_restore_delete_restart':True,'same_chapter_no_network':True,'confirmation':True,'released':True})
                finally:app.close()
            # Reuse one synthetic saved nonzero position for independent fault
            # branches. Each branch gets a new directory and runtime process.
            w,h,dpi,corner,circular=296,240,160,58,False;g=geometry(w,h,dpi,corner,circular)
            seed=Path(directory)/'seed';seed.mkdir()
            def start_case(name,state):
                app=Simulator(a,a.output/name,state,w,h,dpi,corner,circular);app.width=w;app.height=h;return app
            app=start_case('seed',seed)
            try:
                open_reader(app,g);app.tap(w-g[0]-10,h//2);second=app.shot('saved-position');open_marks(app,g);add(app,g)
                seed_file=next(seed.rglob('bookmarks-*.json'));saved=seed_file.read_bytes();relative=seed_file.relative_to(seed)
                assert json.loads(saved)['items'][0]['offset']>0
                back_reading(app);app.tap(g[0]+10,h//2);app.command('KEY HOME');app.process.wait(timeout=10)
            finally:app.close()
            for fault in ('owner','book','version','malformed','oversized','draft','updated','write-failure','cross-chapter','cancel'):
                mode='normal';state=Path(directory)/fault;shutil.copytree(seed,state);target=state/relative
                value=json.loads(saved)
                if fault=='owner':value['account']='another-account'
                if fault=='book':value['book']='another-book'
                if fault=='version':value['version']=2
                if fault in ('owner','book','version'):target.write_text(json.dumps(value))
                if fault=='malformed':target.write_text('{')
                if fault=='oversized':target.write_bytes(b' ' * 65537)
                if fault=='draft':target.with_name(target.name+'.tmp').write_text('unfinished draft')
                if fault=='updated':mode='updated'
                original=target.read_bytes();app=start_case(fault,state)
                try:
                    open_reader(app,g);current=app.shot('before')
                    if fault in ('owner','book','version','malformed','oversized'):
                        enter_marks(app,g);app.wait(lambda:'WEREAD BOOKMARKS UNAVAILABLE' in app.logs())
                        before=len(calls);i,dp,sw,sh,head,foot,gap,rows=g
                        app.tap(i+sw-min(dp(52),sw//3)//2,i+head//2);time.sleep(.1)
                        assert target.read_bytes()==original and len(calls)==before
                    elif fault in ('cross-chapter','cancel'):
                        i,dp,sw,sh,head,foot,gap,rows=g;menu_h=max(24,min(dp(42),(sh-head-foot-3*gap)//3))
                        app.tap(w//2,h//2);app.tap(i+sw//4,i+head+menu_h+gap+menu_h//2)
                        rh=(sh-head-foot-gap)//rows;ready=app.logs().count('WEREAD READER READY')
                        app.tap(i+sw//2,i+head+rh+(rh-gap)//2)
                        app.wait(lambda:app.logs().count('WEREAD READER READY')>ready)
                        chapter2=app.shot('chapter-two');open_marks(app,g);ready=app.logs().count('WEREAD READER READY')
                        mode='wrong-book' if fault=='cross-chapter' else 'blocked';blocked.clear();release.clear();row(app,g)
                        if fault=='cross-chapter':
                            app.wait(lambda:'WEREAD BOOKMARK CHAPTER UNAVAILABLE' in app.logs())
                            assert app.logs().count('WEREAD READER READY')==ready and target.read_bytes()==original
                            mode='normal';row(app,g);app.wait(lambda:app.logs().count('WEREAD READER READY')>ready)
                            time.sleep(2.5);assert not changed(second,app.shot('restored-cross-chapter'),g)
                        else:
                            app.wait(blocked.is_set);app.command('KEY BACK');release.set();mode='normal';time.sleep(.5)
                            app.command('KEY BACK')
                            # KEY acknowledges enqueueing; wait for the actual
                            # restored frame before comparing retained content.
                            app.wait(lambda:not changed(chapter2,app.shot('cancelled'),g))
                            assert app.logs().count('WEREAD READER READY')==ready and target.read_bytes()==original
                    else:
                        open_marks(app,g)
                        if fault=='write-failure':
                            draft=target.with_name(target.name+'.tmp');draft.mkdir();before=app.logs().count('WEREAD ERROR')
                            i,dp,sw,sh,head,foot,gap,rows=g;app.tap(i+sw-min(dp(52),sw//3)//2,i+head//2)
                            app.wait(lambda:app.logs().count('WEREAD ERROR')>before)
                            assert target.read_bytes()==original;draft.rmdir();add(app,g)
                            assert len(json.loads(target.read_text())['items'])==2
                        else:
                            row(app,g)
                            if fault=='updated':
                                app.wait(lambda:'WEREAD BOOKMARK TEXT CHANGED' in app.logs());time.sleep(2.5)
                                assert not changed(current,app.shot('updated-start'),g)
                            else:assert not changed(second,app.shot('draft-ignored'),g)
                            assert target.read_bytes()==original
                    app.command('KEY HOME');app.process.wait(timeout=10)
                    assert 'remaining_internal=0 remaining_external=0' in app.logs()
                    results.append({'fault':fault,'old_file_preserved_or_atomic_retry_passed':True,'released':True})
                finally:release.set();app.close()
    finally:release.set();server.shutdown();server.server_close()
    (a.output/'results.json').write_text(json.dumps({'passed':results},ensure_ascii=False,indent=2)+'\n')
    print('Six-layout local bookmarks and ten storage/identity/cancel/content-change branches passed')
if __name__=='__main__':main()
