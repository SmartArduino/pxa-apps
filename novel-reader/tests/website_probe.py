"""Check the shipped website rules against the live site and optional full-system simulator.

Uses an isolated QA identity and temporary storage. Reports metadata/hashes only;
chapter text stays in memory. Network failures are failures, not fixture passes.
"""
import argparse, hashlib, json, os, re, socket, subprocess, tempfile, time
from pathlib import Path
from urllib.parse import quote, urljoin
from urllib.request import Request, urlopen

ROOT=next(p for p in Path(__file__).resolve().parents if (p/'tools/app.sh').exists())
APP=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--package',type=Path,help='Optional instrumented QA package')
p.add_argument('--binary',type=Path,default=ROOT/'build/simulator/reader-validation/pxsys_desktop_simulator')
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
driver=a.output/'source_probe'
subprocess.run([os.environ.get('CXX','clang++'),'-std=c++2c','-O3','-Wno-attributes',
    '-I'+str(ROOT/'deps/pxa-system/sdk/guest-cpp/include'),str(APP/'tests/source_probe.cpp'),'-o',str(driver)],check=True)
def parse(mode,data=b''):
    return subprocess.run([str(driver),mode],input=data,capture_output=True,check=True).stdout
rules=json.loads(parse('config'))
default_count=len(json.loads(parse('configs')))
requests=[]
def fetch(url,body=None):
    begin=time.monotonic()
    req=Request(url,data=body,headers={'Content-Type':'application/x-www-form-urlencoded'} if body else {})
    with urlopen(req,timeout=20) as response:
        data=response.read(96*1024+1)
        assert response.status==200 and len(data)<=96*1024
        requests.append(dict(url=url,method='POST' if body else 'GET',status=response.status,
            final_url=response.url,bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),ms=round((time.monotonic()-begin)*1000,2)))
        return data
searches={}
for query in ['三体','长夜难明','紫金陈']:
    data=fetch(rules['searchUrl'],rules['search']['body'].replace('{{key}}',quote(query,safe='')).encode())
    searches[query]=json.loads(parse('books',data))['shelf']
    assert searches[query],f'No real website results for {query}'
book=next(b for b in searches['长夜难明'] if b['title']=='长夜难明')
book_id=parse('id',fetch(book['toc'])).decode()
chapters=[];total=0;pages=1;page=0
while page<pages:
    url=urljoin(book['toc'],rules['toc']['paged']['url'].replace('{{id}}',quote(book_id,safe='')).replace('{{page}}',str(page)))
    result=json.loads(parse('page',fetch(url)))
    assert not page or (total==result['total'] and pages==result['pages'])
    total=result['total'];pages=result['pages'];chapters+=result['chapters'];page+=1
assert len(chapters)==total and total>10 and len({c['url'] for c in chapters})==total
body_checks=[]
for index in [0,11,total-1]:
    text=parse('content',fetch(chapters[index]['url']))
    assert len(text)>30 and '\ufffd' not in text.decode('utf-8')
    body_checks.append(dict(chapter=index,title=chapters[index]['title'],bytes=len(text),sha256=hashlib.sha256(text).hexdigest()))
report=dict(passed=True,queries={q:[{'title':b['title'],'author':b['author'],'toc':b['toc']} for b in books] for q,books in searches.items()},
    toc=dict(total=total,pages=pages,unique_urls=True),bodies=body_checks,requests=requests)

if a.package:
    key=ROOT/'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'
    publisher=hashlib.sha256(key.read_bytes()).hexdigest();binary=a.binary.resolve()
    with tempfile.TemporaryDirectory(prefix='reader-website-') as tmp:
        state=Path(tmp)/'state';state.mkdir();control=Path(tmp)/'control.sock'
        subprocess.run([str(binary.parent/'pxsys_package_installer'),'--storage-root',str(state),'--publisher-key',str(key),
            '--source',str(a.package.resolve()),'--expected-id','pxa-novel-reader-qa'],check=True,capture_output=True)
        private=state/'private-files'/f'{publisher}-pxa-novel-reader-qa'
        log_path=a.output/'runtime.log'
        def logs():
            raw=re.sub(r'\x1b\[[0-9;]*m','',log_path.read_text(errors='replace'))
            return re.sub(r'READER-HITS ([0-9,;]+)',lambda m:'\n'.join('READER-HIT action=%s arg=%s x=%s y=%s'%tuple(row.split(',')) for row in m[1].split(';') if row),raw)
        def wait(predicate,timeout=90):
            end=time.monotonic()+timeout
            while time.monotonic()<end:
                s=logs();assert process.poll() is None,s[-5000:]
                assert all(error not in s for error in ['READER-ERROR','unreachable','READER-SEARCH failed=']),s[-5000:]
                if predicate(s):return s
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
            hit=re.search(rf'READER-HIT action={action} arg={arg} x=(\d+) y=(\d+)',screen());assert hit,(action,arg,screen())
            before=len(logs());cmd(f'TAP {hit[1]} {hit[2]}');wait(lambda s:'READER-UI ' in s[before:]);time.sleep(.15)
            if action in [23,24]:wait(lambda s:'busy=0 ' in s[s.rfind('READER-UI '):])
        def capture(name):
            header,png=cmd('SCREENSHOT').split(b'\n',1);assert header==f'PNG {len(png)}'.encode();(a.output/(name+'.png')).write_bytes(png)
        args=[str(binary),'--width','296','--height','240','--density-dpi','160','--safe-insets','12,12,12,12',
            '--corner-radius','58','--locale','zh-CN','--light','--gestures','--installed-packages-root',str(state/'packages'),
            '--product-runner',str(binary.parent/'pxsys_product_simulator'),'--publisher-key',str(key),'--state-root',str(state),
            '--pxadb-control-socket',str(control),'--launch','pxa-novel-reader-qa']
        with log_path.open('w') as log:
            process=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
            try:
                # No pre-seeded state: verify the actual new-install defaults.
                wait(lambda s:'READER-UI screen=0 busy=0 ' in s);time.sleep(.3);tap(1)
                assert '努努书坊' in screen() and all(old not in screen() for old in ['经典直读','GITenberg','古腾堡'])
                hit=re.search(r'READER-HIT action=30 arg=0 x=(\d+) y=(\d+)',screen());assert hit
                cmd(f'TAP {hit[1]} {hit[2]}');time.sleep(.2);cmd('TEXT 长夜难明');time.sleep(.2);cmd('TAP 250 185');time.sleep(.2)
                before=len(logs());begin=time.monotonic();tap(17)
                wait(lambda s:'READER-SEARCH results=' in s[before:]);search_ms=(time.monotonic()-begin)*1000;capture('search')
                index=next(i for i,b in enumerate(searches['长夜难明']) if b['title']=='长夜难明')
                while not re.search(rf'READER-HIT action=4 arg={index} ',screen()):tap(24)
                before=len(logs());begin=time.monotonic();tap(4,index);wait(lambda s:'READER-PAGE ' in s[before:])
                open_ms=(time.monotonic()-begin)*1000;capture('reading')
                first_catalog=re.findall(r'READER-REMOTE-TOC chapters=(\d+) total=(\d+) pages=(\d+)/(\d+)',logs())[-1]
                assert int(first_catalog[0])<total and int(first_catalog[2])==1,'Opening fetched the whole catalog'
                cmd('TAP 148 120');wait(lambda s:'READER-UI screen=4 ' in s);time.sleep(.2);tap(8);capture('toc')
                while 'READER-HIT action=25 arg=11 ' not in screen():tap(24)
                before=len(logs());tap(25,11);wait(lambda s:'READER-PAGE ' in s[before:]);capture('chapter-12')
                cmd('TAP 148 120');wait(lambda s:'READER-UI screen=4 ' in s);time.sleep(.2);tap(8)
                while f'READER-HIT action=25 arg={total-1} ' not in screen():tap(24)
                assert f'READER-REMOTE-TOC chapters={total} total={total} pages={pages}/{pages}' in logs()
                capture('toc-last');tap(16)
                time.sleep(1.5)
                saved=json.loads((private/'reader-state.json').read_text())
                assert saved['version']==4 and len(saved['sources'])==default_count and saved['sources'][0]['kind']==4
                assert next(b for b in saved['shelf'] if b['title']=='长夜难明')['chapter']==11
                assert not list(private.glob('b*.txt')) and not list(private.glob('b*.part'))
                cmd('TAP 148 120');wait(lambda s:'READER-UI screen=4 ' in s);time.sleep(.2);tap(0);tap(2)
                for _ in range(default_count):tap(22)
                tap(1);assert '添加书源' in screen();tap(17);assert '请先到书源页添加书源' in logs()
                saved=json.loads((private/'reader-state.json').read_text());assert saved['sources']==[]
                before=len(logs());process.terminate();process.wait(timeout=10)
                process=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
                wait(lambda s:'READER-UI screen=0 busy=0 ' in s[before:]);time.sleep(.3);tap(1)
                assert '添加书源' in screen() and '努努书坊' not in screen();tap(18)
                assert 'screen=7 ' in screen();capture('empty-sources')
                report['simulator']=dict(passed=True,new_install_defaults=True,real_search=True,complete_toc=total,jump_past_preview=True,
                    lazy_first_catalog_count=int(first_catalog[0]),remove_last_source=True,empty_sources_restart=True,
                    no_body_files=True,search_ms=round(search_ms,2),first_page_ms=round(open_ms,2),
                    pool_peak=max(map(int,re.findall(r'pool_peak=(\d+)',logs()))),
                    allocation_failures=max(map(int,re.findall(r'failures=(\d+)',logs()))))
            finally:
                process.terminate();process.wait(timeout=10)
(a.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='requests'},ensure_ascii=False))
