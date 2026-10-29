#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Opt-in checks of existing science artwork at touch-screen sizes."""
import argparse
import importlib.util
from pathlib import Path
import sys
import time
HERE=Path(__file__).resolve();sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec=importlib.util.spec_from_file_location('sibo_flows',HERE.with_name('test_simulator.py'))
f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('workspace','package-root','executable','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    for w,h in ((320,480),(480,800),(800,480)):
        with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',w,h,'large-picture',executable=a.executable.resolve()) as sim:
            f.seed(sim,f.ORIGIN+'/oss/vistep/catalog.json',{'items':[{'id':'aperture','title':'光圈的小秘密','categoryName':'光与声音','minAge':8,'item':'items/aperture.json'}]})
            f.choose(sim,'科学发现馆')
            f.seed(sim,f.ORIGIN+'/oss/vistep/items/aperture.json',{'id':'aperture','sections':[{'title':'一张照片里的距离','text':'同一朵花，两种清晰的选择。','image':'images/aperture/01.jpg'}]})
            sim.key('ENTER');sim.key('ENTER');f.choose(sim,'看看插图')
            until=time.monotonic()+35
            while sim.focus()!='放大':
                assert sim.process.poll() is None
                if time.monotonic()>until:raise AssertionError(sim.log_path.read_text()[-3000:])
                time.sleep(.1)
            sim.screenshot('science-artwork')
            # Header Back remains reachable by touch independent of the
            # image's source aspect ratio and the available canvas height.
            sim.tap(w-48,36);sim.expect('开始阅读')
        print(f'{w}x{h}: existing science image and touch Back passed',flush=True)
if __name__=='__main__':main()
