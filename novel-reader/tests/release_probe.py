"""Replay the paging reference against the signed, uninstrumented release."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time
from PIL import Image

ROOT=next(p for p in Path(__file__).resolve().parents if (p/'tools/app.sh').exists())
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--package',type=Path,required=True)
p.add_argument('--reference',type=Path,required=True,help='Completed paging_probe output')
p.add_argument('--output',type=Path,required=True)
p.add_argument('--binary',type=Path,default=ROOT/'build/simulator/reader-validation/pxsys_desktop_simulator')
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
reference=json.loads((a.reference/'report.json').read_text());w,h,dpi=(reference[k] for k in ('width','height','dpi'))
book=''.join(f'第{i}段 山间来信\n清晨的风沿着山路吹来，林舟打开书页，记下今天的旅程。白鸟带来消息，河边的书屋又亮起了灯。\n\n' for i in range(1,1601)).encode()
assert hashlib.sha256(book).hexdigest()==reference['book_sha256']
key=ROOT/'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der';publisher=hashlib.sha256(key.read_bytes()).hexdigest()
url='http://127.0.0.1:18764/paging-book.txt';value=14695981039346656037
for byte in url.encode():value=((value^byte)*1099511628211)&((1<<64)-1)
def digest(png):
    image=Image.open(io.BytesIO(png)).convert('RGB')
    return hashlib.sha256(image.crop((0,h//5,w,h*4//5)).tobytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='reader-release-') as tmp:
    state=Path(tmp)/'state';state.mkdir();control=Path(tmp)/'control.sock';binary=a.binary.resolve()
    subprocess.run([str(binary.parent/'pxsys_package_installer'),'--storage-root',str(state),'--publisher-key',str(key),'--source',str(a.package.resolve()),'--expected-id','pxa-novel-reader'],check=True,capture_output=True)
    private=state/'private-files'/f'{publisher}-pxa-novel-reader';private.mkdir(parents=True)
    body=private/f'b{value:x}.txt';body.write_bytes(book)
    saved=dict(version=2,settings=dict(font=16,gap=6,theme=3,seconds=15),sources=[dict(name='经典直读',url='https://www.gutenberg.org/cache/epub/',kind=2,enabled=True)],shelf=[dict(title='林间长篇',author='原创 CC0',url=url,toc='',position=0,total=len(book),complete=True,validator='')])
    (private/'reader-state.json').write_text(json.dumps(saved,ensure_ascii=False))
    def cmd(command):
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(15);client.connect(str(control));client.sendall((command+'\n').encode());data=b''
            while block:=client.recv(65536):data+=block
        assert not data.startswith(b'ERR'),data
        return data
    def shot():
        header,png=cmd('SCREENSHOT').split(b'\n',1);assert header==f'PNG {len(png)}'.encode();return png
    log_path=a.output/'runtime.log'
    args=[str(binary),'--width',str(w),'--height',str(h),'--density-dpi',str(dpi),'--safe-insets','12,12,12,12','--locale','zh-CN','--light','--gestures','--installed-packages-root',str(state/'packages'),'--product-runner',str(binary.parent/'pxsys_product_simulator'),'--publisher-key',str(key),'--state-root',str(state),'--pxadb-control-socket',str(control),'--launch','pxa-novel-reader']+(['--round'] if reference['round'] else ['--corner-radius','58'])
    with log_path.open('w') as log:
        process=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
        try:
            end=time.monotonic()+20
            while not control.exists():
                assert process.poll() is None and time.monotonic()<end,log_path.read_text();time.sleep(.02)
            time.sleep(.6)
            hit=re.search(r'(?:READER-HITS |;)4,0,(\d+),(\d+);',(a.reference/'runtime.log').read_text())
            assert hit
            cmd(f'TAP {hit[1]} {hit[2]}')
            expected=digest((a.reference/'first-page.png').read_bytes())
            matches=0
            for sample in [None]+reference['samples']:
                if sample:
                    direction=sample['direction'];cmd(f'TAP {w*(5 if direction>0 else 1)//6} {h//2}');expected=sample['body_sha256']
                end=time.monotonic()+20
                while digest(png:=shot())!=expected:
                    assert process.poll() is None and time.monotonic()<end,log_path.read_text()[-3000:]
                    time.sleep(.01)
                matches+=1;time.sleep(.04)
            time.sleep(1.5)
            persisted=json.loads((private/'reader-state.json').read_text())
            assert persisted['shelf'][0]['position']==reference['samples'][-1]['offset']
            assert body.read_bytes()==book and not list(private.glob('*.part'))
            assert 'READER-ERROR' not in log_path.read_text()
            (a.output/'last-page.png').write_bytes(png)
            report=dict(passed=True,production=True,matching_pages=matches,progress_preserved=True,cache_unchanged=True,
                        package_sha256=hashlib.sha256(a.package.read_bytes()).hexdigest(),reference_sha256=reference['package_sha256'])
            (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
        finally:
            process.terminate();process.wait(timeout=10)
