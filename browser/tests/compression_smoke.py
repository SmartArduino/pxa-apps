#!/usr/bin/env python3
"""Exercise decoded HTTP bodies through a real TRACE PXA simulator package."""
import argparse,re,time
from pathlib import Path
from harness import Browser

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--simulator',type=Path,required=True,help='pxsys_product_simulator executable')
    p.add_argument('--package',type=Path,required=True)
    p.add_argument('--publisher-key',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True,help='ignored local/artifacts directory')
    p.add_argument('--base-url',default='http://127.0.0.1:31878')
    p.add_argument('--ime',type=Path,default=Path('/nonexistent-ime'))
    a=p.parse_args()
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime,a.output,width=480,height=480,dpi=160,corner=0)
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        decoded=[]
        for path in ['gzip','deflate','gzip-css','json','markdown','compressed-script','large']:
            start=len(b.logs());b.navigate(a.base_url.rstrip('/')+'/'+path)
            b.wait(lambda:'BROWSER DONE status=200' in b.logs()[start:],timeout=40)
            if path=='compressed-script':
                b.wait(lambda:'BROWSER JS READY' in b.logs()[start:] and 'BROWSER JS UPDATE' in b.logs()[start:],timeout=40)
            log=b.logs()[start:]
            assert 'BROWSER ERROR' not in log,log
            values=re.findall(r'BROWSER DECODE wire=(\d+) decoded=(\d+) heap_peak=(\d+)',log)
            assert values,log
            wire,size,heap=map(int,values[-1]);assert heap<=50*1024,(path,heap)
            if path=='gzip-css':assert size>1500000 and wire<10000,(wire,size)
            if path=='markdown':assert heap==0,heap
            if path=='large':assert re.search(r'text=4915\d',log),log
            decoded.append((path,wire,size,heap));b.shot(path)
        start=len(b.logs());b.nav(2)
        b.wait(lambda:'BROWSER BUFFERS before=' in b.logs()[start:])
        buffers=re.findall(r'BROWSER BUFFERS before=(\d+) home=(\d+)',b.logs()[start:])
        assert buffers and int(buffers[-1][0])>=49152 and int(buffers[-1][1])==0,buffers
        b.shot('home-released')
        start=len(b.logs());b.navigate(a.base_url.rstrip('/')+'/broken')
        b.wait(lambda:'BROWSER ERROR' in b.logs()[start:],timeout=40)
        assert 'BROWSER JS OPEN' not in b.logs()[start:]
        b.shot('broken')
        print('compression simulator regression passed:',decoded,flush=True)
    finally:b.close()
if __name__=='__main__':main()
