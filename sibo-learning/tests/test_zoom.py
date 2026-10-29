#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Opt-in pixel checks of actual zoom, pointer drag, double tap and key pan."""
import argparse
import importlib.util
from pathlib import Path
import sys
import re
import time
from PIL import Image,ImageChops
HERE=Path(__file__).resolve();sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec=importlib.util.spec_from_file_location('sibo_flows',HERE.with_name('test_simulator.py'));f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
spec=importlib.util.spec_from_file_location('sibo_touch',HERE.with_name('test_touch.py'));touch=importlib.util.module_from_spec(spec);spec.loader.exec_module(touch)
def viewport(path):
    boxes=touch.regions(path,(241,239,230));assert boxes,'Image viewport is missing'
    return min(b[0] for b in boxes)+2,min(b[1] for b in boxes)+2,max(b[2] for b in boxes)-2,max(b[3] for b in boxes)-2

def changed(a,b,box):
    with Image.open(a) as left,Image.open(b) as right:
        diff=ImageChops.difference(left.convert('RGB').crop(box),right.convert('RGB').crop(box)).convert('L')
        histogram=diff.histogram();return sum(histogram[9:])/(diff.width*diff.height)
def wait(sim):
    until=time.monotonic()+40
    while sim.focus()!='放大':
        assert sim.process.poll() is None
        if time.monotonic()>until:
            sim.screenshot('viewer-timeout')
            raise AssertionError(sim.log_path.read_text()[-3000:])
        time.sleep(.1)
def exercise(sim,w,h):
    f.seed(sim,f.ORIGIN+'/oss/vistep/catalog.json',{'items':[{'id':'aperture','title':'光圈的小秘密','categoryName':'光与声音','minAge':8,'item':'items/aperture.json'}]})
    f.choose(sim,'科学发现馆')
    f.seed(sim,f.ORIGIN+'/oss/vistep/items/aperture.json',{'id':'aperture','sections':[{'title':'一张照片里的距离','text':'看看花朵、背景和镜头的开口。哪些地方清楚，哪些地方模糊？把图放大，再移动图片，看看远处的灯光和近处的花瓣。','image':'images/aperture/01.jpg'}]})
    sim.key('ENTER');sim.key('ENTER');f.choose(sim,'看看插图');wait(sim)
    fit=sim.screenshot('fit');original_box=viewport(fit)
    controls=[b for b in touch.regions(fit,touch.COLORS[2]) if b[2]-b[0]>w*.2]
    top=min(b[1] for b in controls);plus=min([b for b in controls if abs(b[1]-top)<5],key=lambda b:b[0])
    touch.tap(sim,plus);zoom=sim.screenshot('zoom-2x')
    assert changed(fit,zoom,original_box)>.1,'Zoom did not enlarge the rendered image'
    # Zoom can cover every background pixel. The viewer expands to full width
    # in landscape, retaining the fit view's vertical bounds and outer margin.
    box=(original_box[0],original_box[1],w-original_box[0],original_box[3])
    cx=(box[0]+box[2])//2;cy=(box[1]+box[3])//2
    dy=max(12,(box[3]-box[1])//5)
    sim.command(f'POINTER DOWN {cx} {cy}');time.sleep(.12)
    sim.command(f'POINTER MOVE {cx} {cy+dy}');time.sleep(.18)
    sim.command(f'POINTER UP {cx} {cy+dy}');time.sleep(.35)
    dragged=sim.screenshot('dragged');assert changed(zoom,dragged,box)>.04,'Pointer drag did not move the picture'
    f.choose(sim,'↓');panned=sim.screenshot('key-pan');assert changed(dragged,panned,box)>.04,'Three-key pan did not move the picture'
    f.choose(sim,'全图');reset=sim.screenshot('fit-restored');assert changed(fit,reset,original_box)<.002,'Fit did not restore the original view'
    cx=(original_box[0]+original_box[2])//2;cy=(original_box[1]+original_box[3])//2
    sim.command(f'TAP {cx} {cy}');time.sleep(.12);sim.command(f'TAP {cx} {cy}');time.sleep(.45)
    doubled=sim.screenshot('double-tap');assert changed(reset,doubled,original_box)>.1,'Double tap did not zoom'
    for _ in range(4):f.choose(sim,'放大')
    sim.expect('最大');sim.screenshot('zoom-8x');f.choose(sim,'全图');f.returned(sim);sim.expect('开始阅读')
    first=re.findall(r'sibo image (\d+x\d+ -> \d+x\d+)',sim.log_path.read_text())[-1]
    for _ in range(2):
        f.choose(sim,'看看插图');wait(sim)
        decoded=re.findall(r'sibo image (\d+x\d+ -> \d+x\d+)',sim.log_path.read_text())[-1]
        assert decoded==first,('Reopening lost image detail; a display lease may be retained',first,decoded)
        f.returned(sim);sim.expect('开始阅读')
    print(f'{w}x{h}: 1–8x zoom, touch drag/double-tap, three-key pan, fit, Back and repeat detail passed',flush=True)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('workspace','package-root','executable','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sizes',nargs='*',default=['240x284','320x480','800x480'])
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    for size in a.sizes:
        w,h=map(int,size.split('x'))
        with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',w,h,'zoom',executable=a.executable.resolve()) as sim:exercise(sim,w,h)
if __name__=='__main__':main()
