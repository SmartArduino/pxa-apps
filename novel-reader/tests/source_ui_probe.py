"""Verify real website rules in fresh full-system simulator sessions.

Run source_matrix_probe.py first. Every case starts a new process and private
storage, so per-run linear memory peaks do not inherit earlier books' peaks.
"""
import argparse, hashlib, json, os, re, socket, subprocess, tempfile, time
from pathlib import Path

ROOT=next(p for p in Path(__file__).resolve().parents if (p/'tools/app.sh').exists())
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--package',type=Path,required=True);p.add_argument('--native-report',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--binary',type=Path,default=ROOT/'build/simulator/reader-validation/pxsys_desktop_simulator')
p.add_argument('--width',type=int,default=800);p.add_argument('--height',type=int,default=480);p.add_argument('--dpi',type=int,default=160)
p.add_argument('--round',action='store_true');a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
matrix=json.loads(a.native_report.read_text());assert matrix['passed']
key=ROOT/'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der';publisher=hashlib.sha256(key.read_bytes()).hexdigest();binary=a.binary.resolve()
results=[]
for source_index,case in enumerate(matrix['sources']):
    output=a.output/case['key'];output.mkdir(exist_ok=True);log_path=output/'runtime.log'
    with tempfile.TemporaryDirectory(prefix='reader-source-ui-') as tmp:
        state=Path(tmp)/'state';state.mkdir();control=Path(tmp)/'control.sock'
        subprocess.run([str(binary.parent/'pxsys_package_installer'),'--storage-root',str(state),'--publisher-key',str(key),
            '--source',str(a.package.resolve()),'--expected-id','pxa-novel-reader-qa'],check=True,capture_output=True)
        private=state/'private-files'/f'{publisher}-pxa-novel-reader-qa'
        def logs():
            raw=re.sub(r'\x1b\[[0-9;]*m','',log_path.read_text(errors='replace'))
            return re.sub(r'READER-HITS ([0-9,;]+)',lambda m:'\n'.join('READER-HIT action=%s arg=%s x=%s y=%s'%tuple(row.split(',')) for row in m[1].split(';') if row),raw)
        def wait(predicate,timeout=60):
            end=time.monotonic()+timeout
            while time.monotonic()<end:
                text=logs();assert process.poll() is None,text[-5000:]
                assert all(bad not in text for bad in ['READER-ERROR','unreachable','READER-SEARCH failed=']),text[-5000:]
                if predicate(text):return text
                time.sleep(.02)
            raise AssertionError(logs()[-6000:])
        def cmd(value):
            with socket.socket(socket.AF_UNIX) as c:
                c.settimeout(15);c.connect(str(control));c.sendall((value+'\n').encode());data=b''
                while block:=c.recv(65536):data+=block
            assert not data.startswith(b'ERR'),data
            return data
        def screen():return logs()[logs().rfind('READER-UI '):]
        def tap(action,arg=0):
            hit=re.search(rf'READER-HIT action={action} arg={arg} x=(\d+) y=(\d+)',screen());assert hit,(action,arg,screen())
            cmd(f'TAP {hit[1]} {hit[2]}');time.sleep(.18)
        def capture(name):
            # The Guest trace precedes Host presentation; let the displayed
            # frame catch up before saving visual evidence.
            time.sleep(.3)
            header,png=cmd('SCREENSHOT').split(b'\n',1);assert header==f'PNG {len(png)}'.encode();(output/(name+'.png')).write_bytes(png)
        def menu():
            before=len(logs());cmd(f'TAP {a.width//2} {a.height//2}');wait(lambda s:'READER-UI screen=4 ' in s[before:]);time.sleep(.15)
        def data():return json.loads((private/'reader-state.json').read_text())
        args=[str(binary),'--width',str(a.width),'--height',str(a.height),'--density-dpi',str(a.dpi),'--safe-insets','12,12,12,12',
            '--locale','zh-CN','--light','--gestures','--installed-packages-root',str(state/'packages'),'--product-runner',str(binary.parent/'pxsys_product_simulator'),
            '--publisher-key',str(key),'--state-root',str(state),'--pxadb-control-socket',str(control),'--launch','pxa-novel-reader-qa']+(['--round'] if a.round else ['--corner-radius','58'])
        with log_path.open('w') as log:
            process=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
            try:
                wait(lambda s:'READER-UI screen=0 busy=0 ' in s);time.sleep(.3);tap(1)
                for _ in range(source_index):tap(18)
                assert case['name'] in screen(),(case['name'],screen())
                tap(30);cmd('TEXT '+case['query']);time.sleep(.2)
                if a.width==800:cmd('TAP 725 431')
                elif a.width==296:cmd('TAP 250 185');time.sleep(.2);tap(17)
                else:tap(17)
                wait(lambda s:re.search(r'READER-SEARCH results=[1-9]',s) is not None);capture('search')
                index=case['result_index']
                for _ in range(32):
                    if f'READER-HIT action=4 arg={index} ' in screen():break
                    tap(24)
                else:raise AssertionError('Result not reachable')
                before=len(logs());tap(4,index);wait(lambda s:'READER-PAGE ' in s[before:]);capture('reading')
                menu();tap(26)
                expected=case['bodies'][0]['bytes']+len(case['bodies'][0]['title'].encode())+3
                wait(lambda s:any(b['toc']==case['book']['toc'] and b['total']==expected for b in data()['shelf']))
                assert all(not row.get('rules') for row in data()['sources']),'Built-in JSON was duplicated in state'
                if case['key']=='biquche':
                    tap(8)
                    part_case=case['bodies'][1]
                    assert part_case['parts']>1
                    for _ in range(32):
                        if f"READER-HIT action=25 arg={part_case['index']} " in screen():break
                        tap(24)
                    else:raise AssertionError('Multipart chapter not reachable')
                    before=len(logs());tap(25,part_case['index']);wait(lambda s:'READER-PAGE ' in s[before:]);capture('multipart-chapter')
                    menu();tap(26)
                    part_bytes=part_case['bytes']+len(part_case['title'].encode())+3
                    wait(lambda s:any(b['toc']==case['book']['toc'] and b['total']==part_bytes for b in data()['shelf']))
                    tap(8)
                    for _ in range(110):
                        if 'READER-HIT action=25 arg=100 ' in screen():break
                        tap(24);wait(lambda s:'busy=0 ' in s[s.rfind('READER-UI '):])
                    else:raise AssertionError('Later HTML TOC page not reachable')
                    assert 'READER-REMOTE-TOC chapters=200 total=0 pages=2/9' in logs();capture('toc-page-2')
                    before=len(logs());tap(25,100);wait(lambda s:'READER-PAGE ' in s[before:]);capture('later-chapter')
                assert not list(private.glob('b*.txt')) and not list(private.glob('b*.part'))
                text=logs();pages=list(map(int,re.findall(r'linear_pages=(\d+)',text)))
                results.append(dict(key=case['key'],passed=True,width=a.width,height=a.height,dpi=a.dpi,round=a.round,
                    expected_first_chapter_bytes=expected,first_chapter_matches_native=True,no_body_files=True,compact_rules=True,
                    multipart_chapter_matches_native=case['key']=='biquche',later_html_toc_verified=case['key']=='biquche',
                    initial_linear_pages=pages[0],max_linear_pages=max(pages),
                    task_pool_peak=max(map(int,re.findall(r'pool_peak=(\d+)',text))),failures=max(map(int,re.findall(r'failures=(\d+)',text)))))
                print(json.dumps(results[-1],ensure_ascii=False),flush=True)
            finally:
                process.terminate();process.wait(timeout=10)
(a.output/'report.json').write_text(json.dumps(dict(passed=True,sources=results,package_sha256=hashlib.sha256(a.package.read_bytes()).hexdigest()),ensure_ascii=False,indent=2)+'\n')
