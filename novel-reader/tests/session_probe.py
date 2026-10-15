"""Exercise search, real HTML rules, on-demand reading and offline cleanup.

Uses only a local HTTP fixture, a private app identity and a temporary state.
No user simulator books/settings or physical device are changed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import threading
import time
from http.server import ThreadingHTTPServer
from urllib.parse import urlparse,parse_qs
from fixture_server import Handler as BaseHandler, body

ROOT=next(p for p in Path(__file__).resolve().parents if (p/'tools/app.sh').exists())
RULES=dict(format='pxa-reader-source-2',name='静态 HTML 书站',kind='html',searchUrl='/search.html?q={{key}}',
           search=dict(items='#books .book',title='a.title',link='a.title@href',author='.author',next='a.next@href'),
           toc=dict(items='#chapters a',title='',link='@href'),content=dict(selector='#content',remove=['.ad']))
requests=[]
class Handler(BaseHandler):
    fail_page=False
    fail_toc_once=False
    fail_part_once=False
    def log_message(self,*args):pass
    def do_GET(self):
        requests.append(dict(path=self.path,range=self.headers.get('range')))
        path=urlparse(self.path).path
        if path=='/book.txt' and Handler.fail_page:return self.reply(503,b'try again')
        if path=='/web-source.json':return self.reply(200,json.dumps(RULES,ensure_ascii=False).encode(),mime='application/json')
        if path=='/search.html':return self.reply(200,b'<div id="books"><article class="book"><a class="title" href="/book/1/index.html">'+ '山间来信'.encode()+b'</a><span class="author">'+ '林舟'.encode()+b'</span></article></div>',mime='text/html; charset=utf-8')
        if path=='/book/1/index.html':return self.reply(200,('<p id="toc-count">1/3</p><div id="chapters" data-id="1">'+''.join(f'<a href="./{i}.html">第{i}章 林间旅途</a>' for i in range(1,2 if a.paged or a.html_paged else 4))+'</div>').encode(),mime='text/html; charset=utf-8')
        if path=='/toc-html':
            page=int(parse_qs(urlparse(self.path).query)['page'][0]);assert 1<=page<=3
            if page==2 and Handler.fail_toc_once:
                Handler.fail_toc_once=False;return self.reply(503,b'retry later')
            return self.reply(200,f'<p id="toc-count">{page}/3</p><div id="chapters"><a href="/book/1/{page}.html">第{page}章 林间旅途</a></div>'.encode(),mime='text/html; charset=utf-8')
        if path=='/toc-api':
            page=int(parse_qs(urlparse(self.path).query)['page'][0]);assert 0<=page<3
            if page==1 and Handler.fail_toc_once:
                Handler.fail_toc_once=False;return self.reply(503,b'retry later')
            return self.reply(200,json.dumps(dict(total='3',totalPage=3,list=[dict(title=f'第{page+1}章 林间旅途',pic=f'/book/1/{page+1}.html')]),ensure_ascii=False).encode(),mime='application/json')
        if re.fullmatch('/book/1/[123](?:_2)?.html',path):
            second='_2' in path
            if second and Handler.fail_part_once:
                Handler.fail_part_once=False;return self.reply(503,b'retry later')
            chapter=int(path.rsplit('/',1)[1][0]);marker=f'<p id="part-count">{2 if second else 1}/2</p>' if a.html_paged else ''
            text=marker+f'<div id="content"><p>第{chapter}章的清晨，林舟打开书屋的门。</p><div class="ad">广告不应显示</div><script>evil()</script>'+'<p>白鸟带来一封信，河边响起了铃声。</p>'*(18 if a.html_paged else 36)+('分页尾段不可丢失' if second else '')+'</div>'
            return self.reply(200,text.encode(),mime='text/html; charset=utf-8')
        return super().do_GET()

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--package',type=Path,required=True)
p.add_argument('--binary',type=Path,default=ROOT/'build/simulator/reader-validation/pxsys_desktop_simulator')
p.add_argument('--output',type=Path,required=True)
p.add_argument('--width',type=int,default=296);p.add_argument('--height',type=int,default=240);p.add_argument('--dpi',type=int,default=160)
p.add_argument('--round',action='store_true')
p.add_argument('--stress',action='store_true',help='Cross window/history boundaries, fail a read and restart without body files')
p.add_argument('--paged',action='store_true',help='Use paged JSON TOC; fail and retry a chapter-boundary directory request')
p.add_argument('--html-paged',action='store_true',help='Use paged HTML TOC and two-part chapters; reject an incomplete chapter and retry')
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
if a.paged:RULES['toc']=dict(paged=dict(url='/toc-api?id={{id}}&page={{page}}',id='#chapters@data-id',items='list',title='title',link='pic',pages='totalPage',total='total'))
if a.html_paged:
    RULES['toc']['paged']=dict(format='html',url='/toc-html?page={{page}}',start=1,pages='#toc-count')
    RULES['content']['paged']=dict(url='{{stem}}_{{page}}.html',pages='#part-count')
server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
base=f'http://127.0.0.1:{server.server_port}'
key=ROOT/'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der';publisher=hashlib.sha256(key.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='reader-session-') as tmp:
    state=Path(tmp)/'state';state.mkdir();control=Path(tmp)/'control.sock';binary=a.binary.resolve()
    subprocess.run([str(binary.parent/'pxsys_package_installer'),'--storage-root',str(state),'--publisher-key',str(key),'--source',str(a.package.resolve()),'--expected-id','pxa-novel-reader-qa'],check=True,capture_output=True)
    private=state/'private-files'/f'{publisher}-pxa-novel-reader-qa';private.mkdir(parents=True)
    saved=dict(version=2,settings=dict(font=16,gap=6,theme=3,seconds=15),sources=[dict(name='本地 TXT 书源',url=base+'/catalog.json?q={{key}}',kind=1,enabled=True)],shelf=[])
    (private/'reader-state.json').write_text(json.dumps(saved,ensure_ascii=False))
    log_path=a.output/'runtime.log'
    def logs():
        raw=re.sub(r'\x1b\[[0-9;]*m','',log_path.read_text(errors='replace'))
        return re.sub(r'READER-HITS ([0-9,;]+)',lambda m:'\n'.join('READER-HIT action=%s arg=%s x=%s y=%s'%tuple(row.split(',')) for row in m[1].split(';') if row),raw)
    def wait(predicate,timeout=20):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            text=logs();assert process.poll() is None,text[-5000:]
            assert 'READER-ERROR' not in text and 'unreachable' not in text,text[-5000:]
            if predicate(text):return text
            time.sleep(.02)
        raise AssertionError(logs()[-6000:])
    def cmd(value):
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(15);client.connect(str(control));client.sendall((value+'\n').encode());data=b''
            while block:=client.recv(65536):data+=block
        assert not data.startswith(b'ERR'),data
        return data
    def screen():return logs()[logs().rfind('READER-UI '):]
    def tap(action,arg=0):
        before=len(logs());hit=re.search(rf'READER-HIT action={action} arg={arg} x=(\d+) y=(\d+)',screen());assert hit,(action,arg,screen())
        cmd(f'TAP {hit[1]} {hit[2]}');wait(lambda s:'READER-UI ' in s[before:]);time.sleep(.15)
    def enter(value):
        hit=re.search(r'READER-HIT action=30 arg=0 x=(\d+) y=(\d+)',screen());assert hit,screen()
        cmd(f'TAP {hit[1]} {hit[2]}');time.sleep(.2)
        for at in range(0,len(value),40):cmd('TEXT '+value[at:at+40]);time.sleep(.04)
        capture('typing')
        # Hiding a narrow K9 keyboard must not search. Wide QWERTY has a real
        # confirmation key; verify that event independently of the app button.
        if a.width==296 and a.height==240:
            before=len(logs());cmd('TAP 250 185');time.sleep(.2)
            assert 'READER-SEARCH ' not in logs()[before:]
            tap(20 if 'screen=7 ' in screen() else 17)
        elif a.width==800:
            before=len(logs());cmd('TAP 725 431');wait(lambda s:'READER-UI ' in s[before:]);time.sleep(.2)
        else:tap(20 if 'screen=7 ' in screen() else 17)
    def capture(name):
        header,png=cmd('SCREENSHOT').split(b'\n',1);assert header==f'PNG {len(png)}'.encode();(a.output/(name+'.png')).write_bytes(png)
    def read_turn(direction=1):
        before=len(logs());cmd(f'TAP {a.width*(5 if direction>0 else 1)//6} {a.height//2}');wait(lambda s:'READER-PAGE ' in s[before:]);time.sleep(.1)
    def menu():
        before=len(logs());cmd(f'TAP {a.width//2} {a.height//2}');wait(lambda s:'READER-UI screen=4 ' in s[before:]);time.sleep(.1)
    args=[str(binary),'--width',str(a.width),'--height',str(a.height),'--density-dpi',str(a.dpi),'--safe-insets','12,12,12,12','--locale','zh-CN','--light','--gestures','--installed-packages-root',str(state/'packages'),'--product-runner',str(binary.parent/'pxsys_product_simulator'),'--publisher-key',str(key),'--state-root',str(state),'--pxadb-control-socket',str(control),'--launch','pxa-novel-reader-qa']+(['--round'] if a.round else ['--corner-radius','58'])
    with log_path.open('w') as log:
        process=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
        try:
            wait(lambda s:'READER-UI screen=0 busy=0 ' in s and 'READER-HIT action=1 ' in s);time.sleep(.3);tap(1);capture('search-empty')
            assert not any(f'text={title}' in screen() for title in ('西游记','红楼梦'))
            tap(17);assert not requests,'Empty search made a request'
            enter('林间');wait(lambda s:'READER-SEARCH results=1' in s);capture('search-result')
            tap(4);wait(lambda s:'READER-PAGE ' in s);read_turn();read_turn(-1);capture('txt-on-demand')
            assert not list(private.glob('b*.txt')) and not list(private.glob('b*.part')),'On-demand TXT wrote body files'
            if a.stress:
                for _ in range(60):read_turn()
                for _ in range(32):read_turn(-1)
                Handler.fail_page=True
                for _ in range(80):
                    before=len(logs());old=int(re.findall(r'READER-PAGE offset=(\d+)',logs())[-1])
                    cmd(f'TAP {a.width*5//6} {a.height//2}')
                    text=wait(lambda s:'READER-PAGE ' in s[before:] or 'READER-STATUS 书源暂时不可用' in s[before:])
                    if 'READER-STATUS 书源暂时不可用' in text[before:]:break
                    time.sleep(.1)
                else:raise AssertionError('Did not reach a window miss')
                time.sleep(1.3)
                assert json.loads((private/'reader-state.json').read_text())['shelf'][0]['position']==old,'Failed turn advanced saved progress'
                Handler.fail_page=False;read_turn();read_turn(-1)
                assert int(re.findall(r'READER-PAGE offset=(\d+)',logs())[-1])==old
                menu();tap(0)
                checkpoint=json.loads((private/'reader-state.json').read_text())['shelf'][0]['position']
                process.terminate();process.wait(timeout=10);before=len(logs())
                process=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
                wait(lambda s:'READER-UI screen=0 busy=0 ' in s[before:]);time.sleep(.3)
                before=len(logs());tap(4);wait(lambda s:'READER-PAGE ' in s[before:])
                assert int(re.findall(r'READER-PAGE offset=(\d+)',logs())[-1])==checkpoint
                read_turn(-1);assert int(re.findall(r'READER-PAGE offset=(\d+)',logs())[-1])<checkpoint
                assert not list(private.glob('b*.txt')) and not list(private.glob('b*.part'))
                capture('resumed-without-cache')
            menu();tap(0);tap(1);tap(33);capture('search-history');tap(29)
            wait(lambda s:screen().find('READER-HIT action=4 ')>=0)
            tap(2);tap(19);enter(base+'/web-source.json');wait(lambda s:'书源已添加' in s);capture('html-source-imported')
            tap(1);assert 'READER-HIT action=4 ' not in screen(),'Old source results survived import'
            tap(33);enter('山间');wait(lambda s:'READER-SEARCH results=1' in s[s.rfind('READER-STATUS 正在搜索'):]);capture('html-search')
            Handler.fail_part_once=a.html_paged
            before=len(logs());tap(4)
            if a.html_paged:
                wait(lambda s:'READER-STATUS 章节加载失败' in s[before:]);assert 'READER-PAGE ' not in logs()[before:]
                assert not list(private.glob('b*.txt')) and not list(private.glob('b*.part'))
                read_turn()
            else:wait(lambda s:'READER-PAGE ' in s[before:])
            capture('html-reading')
            assert '广告不应显示' not in logs() and 'evil()' not in logs()
            read_turn();read_turn(-1)
            if a.paged or a.html_paged:
                assert f'READER-REMOTE-TOC chapters=1 total={0 if a.html_paged else 3} pages=1/3' in logs()
                Handler.fail_toc_once=True
                for _ in range(40):
                    before=len(logs());cmd(f'TAP {a.width*5//6} {a.height//2}')
                    text=wait(lambda s:'READER-PAGE ' in s[before:] or 'READER-STATUS 无法读取目录' in s[before:])
                    if 'READER-STATUS 无法读取目录' in text[before:]:break
                    time.sleep(.1)
                else:raise AssertionError('Did not reach a paged TOC boundary')
                time.sleep(1.3)
                saved=json.loads((private/'reader-state.json').read_text());assert saved['shelf'][1]['chapter']==0
                before=len(logs());read_turn();assert f'READER-REMOTE-TOC chapters=2 total={0 if a.html_paged else 3} pages=2/3' in logs()[before:]
            menu();tap(8)
            if a.paged or a.html_paged:
                # The loaded prefix has only two chapters, fewer than one UI
                # page. Completing that page must not jump past chapter 3.
                tap(24)
                wait(lambda s:'READER-HIT action=25 arg=2 ' in screen() and 'busy=0 ' in screen())
                assert 'READER-HIT action=25 arg=0 ' in screen()
            capture('html-contents');tap(25,1);capture('html-second-chapter')
            read_turn();chapter_position=int(re.findall(r'READER-PAGE offset=(\d+)',logs())[-1])
            menu();tap(0);tap(4,1);read_turn(-1)
            assert int(re.findall(r'READER-PAGE offset=(\d+)',logs())[-1])<chapter_position
            assert not list(private.glob('b*.txt')) and not list(private.glob('b*.part')),'On-demand HTML wrote body files'
            menu();tap(9);wait(lambda s:'READER-DOWNLOAD complete=1' in s,40);time.sleep(.3);capture('explicit-offline')
            files=list(private.glob('b*.txt'));assert len(files)==1 and not list(private.glob('b*.part'))
            offline=files[0].read_text();assert '第3章' in offline and '广告' not in offline and 'evil()' not in offline
            if a.html_paged:assert offline.count('分页尾段不可丢失')==3
            if 'screen=3 ' in screen():menu()
            tap(37);capture('cache-management');tap(36);wait(lambda s:'正文缓存已清除' in s)
            assert not list(private.glob('b*.txt')) and not list(private.glob('b*.part'))
            data=json.loads((private/'reader-state.json').read_text());assert not data['settings']['offline'];assert len(data['shelf'])==2
            tap(16);tap(3);capture('settings');tap(11);tap(12);tap(13);capture('changed-preferences')
            if a.stress:
                for _ in range(4):tap(15)
                tap(14)
            tap(37);tap(35);tap(35);tap(16);before=len(logs());tap(16)
            wait(lambda s:re.search(r'READER-UI screen=3 ',s[s.rfind('READER-UI '):]) is not None)
            wait(lambda s:'READER-NUMBER ' in s[before:],20)
            time.sleep(.3);capture('reading-after-cache-clear')
            if a.stress:
                before=len(logs());wait(lambda s:'READER-PAGE ' in s[before:],8)
                menu();tap(3);tap(14);tap(16)
            menu();tap(0);tap(1);tap(18);tap(33);enter('错误')
            before=len(logs());tap(4);wait(lambda s:'正文为空或不是有效 UTF-8' in s[before:])
            assert not list(private.glob('b*.txt')) and not list(private.glob('b*.part'))
            capture('invalid-utf8-rejected')
            time.sleep(.2);data=json.loads((private/'reader-state.json').read_text())
            assert data['settings']['font']==18 and data['settings']['gap']==8 and data['settings']['theme']==0
            report=dict(passed=True,width=a.width,height=a.height,dpi=a.dpi,round=a.round,requests=requests,
                        source_import=True,txt_no_body_files=True,html_no_body_files=True,explicit_offline=True,cache_cleanup=True,invalid_utf8_rejected=True,
                        searches=data['searches'],state_bytes=(private/'reader-state.json').stat().st_size,stress=a.stress,preferences=True,auto_turn=a.stress,paged_toc=a.paged,html_paged=a.html_paged,toc_partial_page_completed=a.paged or a.html_paged,
                        pool_peak=max(map(int,re.findall(r'pool_peak=(\d+)',logs()))),
                        ime_submit=a.width==800,ime_hide_keeps_query=a.width==296 and a.height==240,
                        allocation_failures=max(map(int,re.findall(r'failures=(\d+)',logs()))))
            (a.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='requests'},ensure_ascii=False))
        finally:
            process.terminate();process.wait(timeout=10)
server.shutdown();server.server_close()
