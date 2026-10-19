#!/usr/bin/env python3
"""Native WeRead integration fixture. All accounts/books/QR pixels are synthetic.

Build with WEREAD_FIXTURE_PORT=18934. Never contacts Tencent or accepts a real
login token. Tests use the actual PXA runtime, native fonts, networking and PNG
decoder, not a Python reimplementation of the app.
"""
import argparse, base64, io, json, os, signal, socket, struct, subprocess
import tempfile, threading, time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit
from PIL import Image, ImageDraw, ImageChops


class Simulator:
    def __init__(self, args, out, state, width, height, dpi, corner, circular=False):
        self.output = out; out.mkdir(parents=True, exist_ok=True)
        self.scratch = tempfile.TemporaryDirectory(prefix='pxa-weread-')
        self.socket = Path(self.scratch.name)/'control.sock'
        self.log_path = out/'runtime.log'; self.log = self.log_path.open('wb')
        command = [str(args.simulator.resolve()), '--package', str(args.package.resolve()),
                   '--publisher-key', str(args.publisher_key.resolve()), '--state-root', str(state),
                   '--width', str(width), '--height', str(height), '--density-dpi', str(dpi),
                   '--pxadb-control-socket', str(self.socket)]
        command += ['--round'] if circular else ['--corner-radius', str(corner)]
        self.process = subprocess.Popen(command, env=dict(os.environ, SDL_VIDEODRIVER='dummy',
            SDL_AUDIODRIVER='dummy',PXA_IME_PACKAGE=str(args.ime_package.resolve()),
            PXA_WEREAD_MEMORY_REPORT=str((out/'guest-memory.json').resolve())), stdout=self.log, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            self.wait(lambda: self.socket.exists())
        except BaseException:
            self.close()
            raise
    def logs(self): return self.log_path.read_text(errors='replace')
    def wait(self, predicate, timeout=20):
        end = time.monotonic()+timeout
        while not predicate():
            assert self.process.poll() is None, self.logs()
            assert time.monotonic()<end, self.logs()[-3000:]
            time.sleep(.03)
        time.sleep(.08)
    def wait_memory(self, predicate, timeout=20):
        value = None
        def ready():
            nonlocal value
            try:
                value = json.loads((self.output/'guest-memory.json').read_text())
            except (FileNotFoundError, json.JSONDecodeError):
                # The diagnostic writer truncates and rewrites this file.
                # Retry an incomplete snapshot; do not hide schema/assertion errors.
                return False
            return predicate(value)
        self.wait(ready, timeout)
        return value
    def command(self, text):
        assert self.socket.exists(),('Control socket missing',self.process.poll(),str(self.socket))
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(15); client.connect(str(self.socket)); client.sendall((text+'\n').encode())
            output = b''
            while data := client.recv(65536): output += data
        assert not output.startswith(b'ERR'), output[:200]
        return output
    def tap(self, x, y):
        self.command(f'TOUCH DOWN 0 {x} {y}'); time.sleep(.06)
        self.command(f'TOUCH UP 0 {x} {y}'); time.sleep(.12)
    def shot(self, name):
        header, png = self.command('SCREENSHOT').split(b'\n', 1)
        assert header == f'PNG {len(png)}'.encode()
        path = self.output/(name+'.png'); path.write_bytes(png); return path
    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try: self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(self.process.pid, signal.SIGKILL); self.process.wait()
        self.log.close()
        (self.output/'process-exit.json').write_text(json.dumps({'returncode':self.process.returncode,'socket_exists':self.socket.exists(),'directory_exists':Path(self.scratch.name).exists()})+'\n')
        self.scratch.cleanup()



def primary_button(path):
    # The system IME can scroll the app to keep the native input line visible.
    # Hit the control users actually see, instead of stale window coordinates.
    picture=Image.open(path).convert('RGB');w,h=picture.size
    points={(x,y) for y in range(h) for x in range(w)
        if (lambda p:p[2]>170 and p[2]-p[0]>90 and p[1]>55 and p[1]-p[0]>20)(picture.getpixel((x,y)))}
    best=[]
    while points:
        first=points.pop();todo=deque([first]);component=[first]
        while todo:
            x,y=todo.popleft()
            for point in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
                if point in points:points.remove(point);todo.append(point);component.append(point)
        if len(component)>len(best):best=component
    assert len(best)>100,'Native search action is not visible above the IME'
    return (min(x for x,y in best)+max(x for x,y in best))//2,(min(y for x,y in best)+max(y for x,y in best))//2


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('simulator', 'package', 'publisher-key', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--ime-package',type=Path,required=True)
    p.add_argument('--port', type=int, default=18934)
    a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    confirm = threading.Event(); counts = {}; mode = 'success'; shelf_requests=[]
    image = Image.new('RGB', (180, 180), 'white'); pen = ImageDraw.Draw(image)
    for x in range(12, 168, 6):
        for y in range(12, 168, 6):
            if (x//6+y//6)%2: pen.rectangle((x, y, x+5, y+5), fill='black')
    png = io.BytesIO(); image.save(png, 'PNG')
    image_uri = 'data:image/png;base64,'+base64.b64encode(png.getvalue()).decode()
    mapping = struct.unpack('<20992H', (Path(__file__).parents[1]/'assets/weread-cjk.bin').read_bytes())
    reverse = {chr(value): chr(0x4e00+i) for i, value in enumerate(mapping)}
    def encode(s): return ''.join(reverse.get(c, c) for c in s)
    def book(index): return {'bookId': str(index), 'title': f'原生阅读测试 {index}', 'author': '合成测试数据'}
    expired_session=threading.Event()
    pending_reply=threading.Event();pending_reply.set()
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_): pass
        def data(self, obj, cookies=False, mime='application/json'):
            payload = obj.encode() if isinstance(obj, str) else json.dumps(obj).encode()
            self.send_response(200); self.send_header('Content-Type', mime)
            self.send_header('Content-Length', str(len(payload)))
            if cookies:
                self.send_header('Set-Cookie', 'wr_vid=1; Path=/; HttpOnly')
                self.send_header('Set-Cookie', 'wr_skey=fixture-only; Path=/; HttpOnly')
            self.end_headers()
            try: self.wfile.write(payload)
            except (BrokenPipeError, ConnectionResetError): pass
        def do_GET(self):
            path = urlsplit(self.path).path; counts[path] = counts.get(path, 0)+1
            if path.endswith('/getuid'): self.data({'uid': 'synthetic-uid'})
            elif path.endswith('/qrcode'):
                self.data({'succ': 1, 'data': image_uri if mode!='bad-qr' else 'data:text/html;base64,AAAA'})
            elif path.endswith('/shelfloadmore'):
                assert 'wr_skey=fixture-only' in self.headers.get('Cookie', '')
                if expired_session.is_set():
                    self.data({'succ':0,'errCode':-1,'data':{'errcode':-2012,'errmsg':'登录超时'}});return
                query=parse_qs(urlsplit(self.path).query); idx=int(query.get('idx',['0'])[0]);key=int(query.get('kkIdx',['-1'])[0])
                shelf_requests.append((idx,key))
                if mode=='shelf-malformed' and idx:
                    self.data({'books':[{'invalid':True}]});return
                total=7 if mode in ('shelf-pages','shelf-malformed') else 3
                values=range(idx+1,min(idx+4,total+1))
                self.data({'books': [book(i) for i in values], 'bookReaderUrls':
                    [{'bId': str(i), 'param': f'fixture-{i}'} for i in values], 'totalCount': total, 'kkIdx': idx+3})
            elif path=='/api/store/search':
                assert 'wr_skey=fixture-only' in self.headers.get('Cookie','')
                assert parse_qs(urlsplit(self.path).query)['keyword']==['测试']
                self.data({'sid':'fixture-search','results':[{'type':1,'scope':17,'scopeCount':1,'currentCount':1,
                    'books':[{'bookInfo':book(1)}]}]})
            elif path.endswith('/catalogloadmore'):
                self.data({'succ':1,'chapterInfos':[{'chapterUid':1,'title':'第一章'}],
                    'readerUrlParmas':[{'cUid':1,'param':'fixture-1'}]})
            elif path.endswith('/reader'):
                assert 'wr_skey=fixture-only' in self.headers.get('Cookie', '')
                body_html = '<p>'+encode('这是一段用于验证原生排版与翻页的测试正文。'*120)+'</p>'
                reader = {'bookInfo': book(1), 'curChapter': '__shared_chapter__',
                    'contentText': '',
                    'curChapterUrlParam': {'param': 'fixture-1'}, 'prevChapterUrlParam': {},
                    'nextChapterUrlParam': {'param': 'fixture-2'}, 'curChapterSection': 0,
                    'chapterSections': [1], 'chapterInfoCount': 2,
                    'chapterIndexes': [{'title': '第一章', 'chapterUid': 1}, {'title': '第二章', 'chapterUid': 2}],
                    'readerUrlParams': [{'cUid': 1, 'param': 'fixture-1'}, {'cUid': 2, 'param': 'fixture-2'}]}
                # Match the real page: shared chapter objects, >128 aliases,
                # sparse-array notation and body HTML outside serialized data.
                parameters = ['a', 'b', 'c']+[f'd{i}' for i in range(870)]
                arguments = ['{}', json.dumps('第一章 原生阅读'), '1']+['0']*870
                data = json.dumps({'state': {'reader': reader}}).replace('"__shared_chapter__"', 'a')
                initializers='a.title=b;a.chapterUid=c;a.chapterIdx=c;'
                if mode in ('large-reader','reader-budget'):
                    parameters=['a','b','c']+[f'd{i}' for i in range(5114)]
                    arguments=['{}',json.dumps('第一章 原生阅读'),'1','Array(900)','Array(900)']+['0']*5112
                    reader['chapterInfoCount']=900
                    reader['chapterIndexes']='__large_chapters__'
                    reader['readerUrlParams']='__large_params__'
                    reader['contentText']='x'*190000 if mode=='reader-budget' else ''
                    data=json.dumps({'state':{'debugInfo':'PAD_TOKEN' if mode=='reader-budget' else 'fixture-padding-'*14000,'reader':reader}})
                    data=data.replace('"__shared_chapter__"','a').replace('"__large_chapters__"','d0').replace('"__large_params__"','d1')
                    for i in range(900):
                        initializers+=f'd0[{i}]={{chapterUid:{i+1},chapterIdx:{i},title:b}};d1[{i}]={{cUid:{i+1},param:"fixture-1"}};'
                if mode=='catalog-window':
                    reader['bookInfo']['lastChapterIdx']=34
                    reader['chapterInfoCount']=635
                    reader['chapterIndexes']=[{'chapterUid':i*34//635+1,'chapterIdx':i*34//635+1,'title':f'Heading {i}'} for i in range(635)]
                    reader['readerUrlParams']=[{'cUid':i+1,'param':f'fixture-{i+1}'} for i in range(34)]
                    data=json.dumps({'state':{'reader':reader}}).replace('"__shared_chapter__"','a')
                if mode in ('content-denied','authorization-denied','limited-free'):
                    reader['bookInfo'].update(paid=0,payType=64 if mode=='limited-free' else 1,maxFreeChapter=8)
                    reader['isCurChapterNeedToPay']=mode!='limited-free'
                    if mode=='content-denied':reader['errorInfo']={'code':-505}
                    if mode!='limited-free':body_html=''
                    data=json.dumps({'state':{'reader':reader}}).replace('"__shared_chapter__"','a')
                payload='<div id="readerContentRenderContainer">'+body_html+'</div><script>window.__NUXT__=(function('+','.join(parameters)+'){'+initializers+'return '+data+'}('+','.join(arguments)+'));</script>'
                if mode=='reader-budget':
                    payload=payload.replace('PAD_TOKEN','p'*(760000-len(payload.encode())+len('PAD_TOKEN')))
                    assert len(payload.encode())==760000
                self.data(payload,mime='text/html')
            else: self.data({'sessionTimeout': 1})
        def do_POST(self):
            path = urlsplit(self.path).path; counts[path] = counts.get(path, 0)+1
            body = json.loads(self.rfile.read(int(self.headers.get('Content-Length', 0))))
            if path.endswith('/getlogininfo'):
                if mode=='pending-login' and not confirm.is_set():
                    pending_reply.wait(5);self.data({'scan':0});return
                assert body['uid']=='synthetic-uid'; confirm.wait(15)
                self.data({'vid': 1, 'skey': 'fixture-only', 'code': 'fixture-only'} if mode in ('success','shelf-pages','shelf-malformed','large-reader','pending-login','catalog-window') else {'scan': 0})
            elif path.endswith('/weblogin'):
                assert type(body['vid']) is int and body['vid']==1
                assert body['pf']==2 and body['skey']=='fixture-only'
                expired_session.clear()
                self.data({'vid': 1, 'accessToken': 'fixture-only', 'refreshToken': 'fixture-only'}, cookies=True)
            else: self.data({})
    server = ThreadingHTTPServer(('127.0.0.1', a.port), Handler); server.daemon_threads = True
    thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
    results = []
    try:
        with tempfile.TemporaryDirectory(prefix='pxa-weread-state-') as directory:
            for label, w, h, dpi, corner, circular in (
                ('compact', 296, 240, 160, 58, False),
                ('hidpi', 480, 480, 280, 56, False),
                ('round', 240, 240, 160, 0, True),
                ('portrait', 480, 800, 240, 32, False)):
                state = Path(directory)/label; state.mkdir(); confirm.clear()
                app = Simulator(a, a.output/label, state, w, h, dpi, corner, circular)
                try:
                    app.wait(lambda: 'WEREAD QR READY' in app.logs()); app.shot('qr-fixture')
                    assert not list(state.rglob('login-qr.png')), 'Temporary QR source retained'
                    confirm.set(); app.wait(lambda: 'WEREAD SHELF READY' in app.logs()); app.shot('shelf')
                    # First card begins immediately below the heading. The
                    # conservative safe rectangle is shared with the C++ UI.
                    inset = int(__import__('math').ceil((min(w,h)/2 if circular else corner)*(1-2**-.5)))
                    safe_h = h-2*inset; head = min(round(44*dpi/160), safe_h//5)
                    app.tap(w//2, inset+head+15)
                    app.wait(lambda: 'WEREAD READER READY' in app.logs()); app.shot('reading')
                    # Auth is immutable while turning pages: no extra atomic
                    # session writes should contend with rendering/font I/O.
                    auth_path=next(state.rglob('session.json'));auth_stamp=auth_path.stat().st_mtime_ns
                    before = counts.get('/wrwebsimplenjlogic/reader', 0)
                    for turn in range(4):
                        app.tap(w*3//4, h//2)
                        if turn==2: app.shot('page-three')
                    assert counts.get('/wrwebsimplenjlogic/reader', 0)==before, 'Local pages unexpectedly refetched'
                    app.shot('turned')
                    assert ImageChops.difference(Image.open(a.output/label/'reading.png').convert('RGB'), Image.open(a.output/label/'turned.png').convert('RGB')).getbbox()
                    app.tap(w//2, h//2); app.shot('reading-menu')
                    def saved_position():
                        for path in state.rglob('state.json'):
                            value=json.loads(path.read_text())
                            if value.get('progress'): return value['progress'][0]['offset']
                        return 0
                    app.wait(lambda: saved_position()>0)
                    position=saved_position()
                    assert auth_path.stat().st_mtime_ns==auth_stamp,'Page turn rewrote authentication'
                    assert 'WEREAD ERROR' not in app.logs() and 'DRAW ERROR' not in app.logs(), app.logs()
                    results.append({'layout': label, 'login': True, 'reading': True, 'local_turns': 4, 'saved_offset': position})
                finally: app.close()
                # Same private storage must restore login without obtaining a QR.
                uid_before = counts.get('/wrwebsimplenjlogic/api/getuid', 0)
                app = Simulator(a, a.output/(label+'-restart'), state, w, h, dpi, corner, circular)
                try:
                    app.wait(lambda: 'WEREAD SHELF READY' in app.logs())
                    assert counts.get('/wrwebsimplenjlogic/api/getuid', 0)==uid_before
                    app.tap(w//2,inset+head+15)
                    app.wait(lambda:'WEREAD READER READY' in app.logs())
                    app.shot('resumed')
                    assert not ImageChops.difference(Image.open(a.output/label/'turned.png').convert('RGB'),
                        Image.open(a.output/(label+'-restart')/'resumed.png').convert('RGB')).getbbox(), 'Reading position did not restore'
                    requests_before=counts.get('/wrwebsimplenjlogic/reader',0)
                    app.tap(w//4,h//2)
                    app.wait(lambda: 'WEREAD READER READY' in app.logs())
                    app.shot('previous-restored')
                    assert not ImageChops.difference(Image.open(a.output/label/'page-three.png').convert('RGB'),
                        Image.open(a.output/(label+'-restart')/'previous-restored.png').convert('RGB')).getbbox(), 'Restored previous page skipped chapter'
                    assert counts.get('/wrwebsimplenjlogic/reader',0)==requests_before
                    # Exercise settings through real pointer routing, then log
                    # out while an older debounced preference write is pending.
                    app.tap(w//2,h//2)
                    gap=max(4,round(8*dpi/160)); foot=min(round(40*dpi/160),safe_h//5)
                    safe_w=w-2*inset
                    menu_h=max(24,min(round(42*dpi/160),(safe_h-head-foot-3*gap)//3))
                    app.tap(inset+safe_w*3//4,inset+head+menu_h+gap+menu_h//2)
                    count=4 if safe_h-head-foot>=4*(round(34*dpi/160)+gap) else 2
                    setting_h=max(16,min(round(42*dpi/160),(safe_h-head-foot-gap*(count+1))//count))
                    app.tap(inset+safe_w*3//4,inset+head+setting_h+gap+setting_h//2)
                    app.tap(w//2,inset+head+setting_h//2)
                    app.shot('settings')
                    # The final settings page offers line height and opt-in
                    # volume keys; capture is released outside the reader.
                    for _ in range(2 if count==2 else 1):app.tap(w-inset-12,h-inset-foot//2)
                    app.tap(w//2,inset+head+setting_h//2)
                    app.tap(w//2,inset+head+setting_h+gap+setting_h//2)
                    app.tap(w//2,h-inset-foot//2)
                    app.tap(w//2,inset+head+menu_h//2)
                    before_key=app.shot('before-volume-key')
                    app.command('KEY VOLUME_DOWN');time.sleep(.25)
                    after_key=app.shot('after-volume-key')
                    assert ImageChops.difference(Image.open(before_key).convert('RGB'),Image.open(after_key).convert('RGB')).getbbox(), 'Volume paging did not change pixels'
                    app.tap(w//2,h//2)
                    app.tap(inset+safe_w*3//4,inset+head+menu_h+gap+menu_h//2)
                    if count==2:app.tap(w-inset-12,h-inset-foot//2)
                    confirm.clear()
                    logout_y=inset+head+(3 if count==4 else 1)*(setting_h+gap)+setting_h//2
                    app.tap(inset+safe_w//4,logout_y)
                    app.wait(lambda:'WEREAD QR READY' in app.logs())
                    assert not list(state.rglob('session.json*')), 'Logout retained an old session or temporary credential'
                    profile=json.loads(next(state.rglob('state.json')).read_text())
                    assert not profile['progress'] and profile['preferences']['night'] and profile['preferences']['font']==20 and profile['preferences']['leading']==160 and profile['preferences']['volume']
                    app.shot('logged-out')
                    results[-1].update(previous_after_restart=True,preferences=True,logout=True,volume_keys=True,line_height=True)
                finally: app.close()
            mode='shelf-pages';confirm.set()
            state=Path(directory)/'paged-shelf';state.mkdir()
            app=Simulator(a,a.output/'paged-shelf',state,296,240,160,58)
            try:
                app.wait(lambda:'WEREAD SHELF READY' in app.logs())
                app.tap(255,203) # Within the initial batch.
                mark=len(shelf_requests);mode='shelf-malformed'
                app.tap(255,203)
                app.wait(lambda:'WEREAD SHELF UNAVAILABLE' in app.logs())
                assert shelf_requests[mark:]==[(3,3)]
                # Retry the same cursor: a failed request must not skip books.
                mode='shelf-pages';app.tap(255,203)
                app.wait(lambda:app.logs().count('WEREAD SHELF READY')==2)
                assert shelf_requests[-1]==(3,3)
                app.tap(35,203)
                app.wait(lambda:app.logs().count('WEREAD SHELF READY')==3)
                assert shelf_requests[-1]==(0,-1),shelf_requests
                app.shot('previous-batch')
                assert 'WEREAD ERROR' not in app.logs()
                results.append({'shelf_cursor_rollback':True,'previous_server_batch':True})
            finally:app.close()
            for label,w,h,dpi,corner,circular in (
                ('compact',296,240,160,58,False),('hidpi',480,480,280,56,False),
                ('round',240,240,160,0,True),('portrait',480,800,240,32,False)):
                mode='success';confirm.set()
                state=Path(directory)/('search-'+label);state.mkdir()
                app=Simulator(a,a.output/('search-'+label),state,w,h,dpi,corner,circular)
                try:
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs())
                    inset=int(__import__('math').ceil((min(w,h)/2 if circular else corner)*(1-2**-.5)))
                    safe_h=h-2*inset;safe_w=w-2*inset;head=min(round(44*dpi/160),safe_h//5)
                    foot=min(round(40*dpi/160),safe_h//5);gap=max(4,round(8*dpi/160))
                    if label in ('round','portrait'):
                        app.tap(w-inset-head//2,inset+head//2)
                        capacity=4 if safe_h-head-foot>=4*(round(34*dpi/160)+gap) else 2
                        sh=max(16,min(round(42*dpi/160),(safe_h-head-foot-gap*(capacity+1))//capacity))
                        app.tap(w//2,inset+head+sh//2)
                        app.tap(w//2,h-inset-foot//2)
                    app.tap(w-inset-3*head//2-gap//2,inset+head//2)
                    app.shot('search-empty')
                    input_h=min(round(42*dpi/160),safe_h//5);go=min(round(56*dpi/160),safe_w//3)
                    app.tap(inset+safe_w//3,inset+head+input_h//2)
                    app.command('TEXT 测试');time.sleep(.15)
                    keyboard=app.shot('search-keyboard')
                    app.tap(*primary_button(keyboard))
                    app.wait(lambda:'WEREAD SEARCH READY' in app.logs())
                    app.shot('search-result')
                    n=max(1,min(10,(safe_h-round(140*dpi/160))//max(round(48*dpi/160),32)))
                    top=inset+head+input_h+gap
                    row=max(12,(h-inset-foot-gap-top)//n)
                    app.tap(w//2,top+row//2)
                    app.wait(lambda:'WEREAD READER READY' in app.logs())
                    app.shot('search-reader')
                    assert 'WEREAD ERROR' not in app.logs()
                    results.append({'layout':label,'native_search':True,'official_catalog_entry':True})
                finally:app.close()
            mode='large-reader';confirm.set()
            state=Path(directory)/'large-reader';state.mkdir()
            app=Simulator(a,a.output/'large-reader',state,480,480,280,56)
            try:
                app.wait(lambda:'WEREAD SHELF READY' in app.logs())
                app.tap(240,135)
                app.wait(lambda:'WEREAD READER READY' in app.logs(),timeout=45)
                assert 'chapters=300' in app.logs()
                assert 'WEREAD ERROR' not in app.logs()
                app.shot('large-reader')
                results.append({'large_reader':True,'aliases':5117,'chapters':900,'window':300,'guest_maximum_bytes':2097152})
                # Each individual response/DTO can fit its own cap while their
                # combined live allocation would exceed the safe working set.
                # The rejected load must keep the previous book and respond.
                mode='reader-budget';before=app.logs().count('WEREAD READER READY')
                app.tap(240,240);app.tap(125,219);app.tap(240,141)
                app.wait(lambda:'WEREAD READER HYDRATION ERROR' in app.logs(),timeout=45)
                app.shot('budget-limit-message')
                assert app.logs().count('WEREAD READER READY')==before
                app.command('KEY BACK');time.sleep(3)
                restored=app.shot('budget-retained-reader')
                a_image=Image.open(app.output/'large-reader.png').convert('RGB').crop((40,96,440,350))
                b_image=Image.open(restored).convert('RGB').crop((40,96,440,350))
                assert ImageChops.difference(a_image,b_image).getbbox() is None,'Budget failure replaced the old page'
                results.append({'aggregate_reader_budget_rejected':True,'response_bytes':760000,'previous_reading_pixels_retained':True})
            finally:app.close()
            mode='catalog-window';confirm.set()
            state=Path(directory)/'catalog-window';state.mkdir()
            app=Simulator(a,a.output/'catalog-window',state,480,800,160,32)
            try:
                app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(240,75)
                app.wait(lambda:'WEREAD READER READY' in app.logs())
                app.tap(240,400);app.tap(120,125)
                before=counts.get('/wrwebsimplenjlogic/api/catalogloadmore',0)
                for window,size in ((300,300),(600,35)):
                    for _ in range(30):app.tap(240,774)
                    app.wait(lambda:f'offset={window} ' in app.logs())
                    assert f'count={size} offset={window} ' in app.logs()
                app.shot('final-outline-window')
                fetched=counts.get('/wrwebsimplenjlogic/reader',0)
                for _ in range(6):app.tap(240,774)
                assert counts.get('/wrwebsimplenjlogic/reader',0)==fetched
                assert counts.get('/wrwebsimplenjlogic/api/catalogloadmore',0)==before
                for _ in range(4):app.tap(80,774)
                app.wait(lambda:app.logs().count('offset=300 ')>=2)
                app.shot('previous-outline-window')
                assert 'WEREAD ERROR' not in app.logs() and 'HYDRATION ERROR' not in app.logs()
                results.append({'outline_rows':635,'actual_chapters':34,'window_limit':300,'forward_backward_windows':True,'terminal_requests_stopped':True})
            finally:app.close()
            # Access hints must remain visible until dismissed, preserve the
            # previous page, and never block a service-authorized free chapter.
            for label,w,h,dpi,corner,circular in (
                ('compact',296,240,160,58,False),('hidpi',480,480,280,56,False),
                ('round',240,240,160,0,True),('portrait',480,800,240,32,False)):
                mode='success';confirm.set()
                state=Path(directory)/('access-'+label);state.mkdir()
                app=Simulator(a,a.output/('access-'+label),state,w,h,dpi,corner,circular)
                try:
                    inset=int(__import__('math').ceil((min(w,h)/2 if circular else corner)*(1-2**-.5)))
                    safe_w,safe_h=w-2*inset,h-2*inset
                    dp=lambda n:max(1,int(n*dpi/160+.5))
                    head,foot=min(dp(44),safe_h//5),min(dp(40),safe_h//5);gap=max(4,dp(8))
                    n=max(1,min(10,(safe_h-dp(100))//max(dp(48),32)))
                    row=(safe_h-head-foot-gap)//n
                    mh=max(24,min(dp(42),(safe_h-head-foot-3*gap)//3))
                    app.wait(lambda:'WEREAD SHELF READY' in app.logs());app.tap(w//2,inset+head+15)
                    app.wait(lambda:'WEREAD READER READY' in app.logs());before=app.shot('original')
                    def choose_second():
                        app.tap(w//2,h//2)
                        app.tap(inset+(safe_w-gap)//4,inset+head+mh+gap+mh//2)
                        if n==1:app.tap(w//2,h-inset-foot//2)
                        app.tap(w//2,inset+head+(row if n>1 else 0)+row//2)
                    for denied,event in (('content-denied','WEREAD READER CONTENT UNAVAILABLE'),
                        ('authorization-denied','WEREAD READER AUTHORIZATION REQUIRED')):
                        mode=denied;choose_second();app.wait(lambda:event in app.logs())
                        message=app.shot(denied);time.sleep(.4);persistent=app.shot(denied+'-persistent')
                        assert ImageChops.difference(Image.open(message).convert('RGB'),Image.open(persistent).convert('RGB')).getbbox() is None
                        app.tap(w//2,h-inset-foot//2);app.command('KEY BACK');time.sleep(.3)
                        restored=app.shot(denied+'-retained')
                        assert ImageChops.difference(Image.open(before).convert('RGB'),Image.open(restored).convert('RGB')).getbbox() is None,'Denied load replaced the previous page'
                    mode='limited-free';ready=app.logs().count('WEREAD READER READY');choose_second()
                    app.wait(lambda:app.logs().count('WEREAD READER READY')>ready);app.shot('limited-free-readable')
                    results.append({'layout':label,'persistent_access_messages':True,'denied_page_preserved':True,'limited_free_readable':True})
                finally:app.close()
            # A successful long poll with scan:0 is pending, not expiry. Opening
            # login settings and using system BACK must keep polling the UID.
            mode='pending-login';confirm.clear();pending_reply.clear()
            state=Path(directory)/'pending-login';state.mkdir()
            before_poll=counts.get('/wrwebsimplenjlogic/api/getlogininfo',0)
            before_uid=counts.get('/wrwebsimplenjlogic/api/getuid',0)
            before_commit=counts.get('/wrwebsimplenjlogic/api/weblogin',0)
            app=Simulator(a,a.output/'pending-login',state,296,240,160,58)
            try:
                app.wait(lambda:'WEREAD QR READY' in app.logs())
                app.wait(lambda:counts.get('/wrwebsimplenjlogic/api/getlogininfo',0)>before_poll)
                app.tap(258,34);app.command('KEY BACK')
                pending_reply.set()
                app.wait(lambda:counts.get('/wrwebsimplenjlogic/api/getlogininfo',0)>=before_poll+3)
                assert counts.get('/wrwebsimplenjlogic/api/getuid',0)==before_uid+1
                assert counts.get('/wrwebsimplenjlogic/api/weblogin',0)==before_commit
                assert 'WEREAD ERROR' not in app.logs()
                app.shot('still-waiting')
                confirm.set();app.wait(lambda:'WEREAD SHELF READY' in app.logs())
                assert counts.get('/wrwebsimplenjlogic/api/weblogin',0)==before_commit+1
                results.append({'pending_scan_repolled':True,'login_settings_system_back':True,'uid_reused':True,'no_ambiguous_commit':True})
            finally:pending_reply.set();app.close()
            state_file=next(state.rglob('state.json'))
            value=json.loads(state_file.read_text());value['progress']=[{'book':'1','param':'fixture-1','chapter':1,'section':0,'offset':96,'text':1}]
            state_file.write_text(json.dumps(value));expired_session.set();confirm.clear()
            app=Simulator(a,a.output/'expired-session',state,296,240,160,58)
            try:
                app.wait(lambda:'WEREAD LOGIN EXPIRED' in app.logs())
                app.shot('explicit-expiry')
                app.tap(148,204);app.wait(lambda:'WEREAD QR READY' in app.logs())
                confirm.set();app.wait(lambda:'WEREAD SHELF READY' in app.logs())
                after=json.loads(state_file.read_text())
                assert after['progress']==value['progress'] and after['account']=='1'
                assert after['preferences']['remember'] is True
                results.append({'expired_session_renewal':True,'same_account_position_retained':True,'remember_preference_retained':True})
            finally:app.close()
            mode='bad-qr';confirm.clear()
            state=Path(directory)/'bad-qr';state.mkdir()
            app=Simulator(a,a.output/'bad-qr',state,296,240,160,58)
            try:
                app.wait(lambda:'WEREAD ERROR' in app.logs())
                assert not list(state.rglob('login-qr.png')) and not list(state.rglob('session.json'))
                app.shot('invalid-image')
                results.append({'invalid_qr_rejected':True})
            finally:app.close()
        (a.output/'results.json').write_text(json.dumps({'passed': results, 'requests': counts}, indent=2)+'\n')
    finally:
        (a.output/'request-counts.json').write_text(json.dumps(counts,indent=2)+'\n')
        confirm.set(); server.shutdown(); server.server_close()
    print('Native WeRead login/shelf/reading/local-turn/session restart passed in four screen layouts')

if __name__=='__main__': main()
