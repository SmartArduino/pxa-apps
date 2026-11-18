#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Require visible, independent locomotion rather than sliding a static body."""
import argparse,json,re
from pathlib import Path
from PIL import Image

p=argparse.ArgumentParser();p.add_argument('--export',type=Path,required=True)
p.add_argument('--report',type=Path,required=True);args=p.parse_args()
header=(Path(__file__).resolve().parents[1]/'art.hpp').read_text()
raw=re.search(r'frames=\{\{(.*?)\}\};',header,re.S)[1]
frames=[tuple(map(int,v.split(','))) for v in re.findall(r'\{([^{}]+)\}',raw)]
metrics={}
for name in ['dustimp','copperbeetle','armorsnail','swiftmoth']:
    start=int(re.search(r'Clip '+name+r'_idle=\{(\d+)',header)[1]);values=[[],[],[],[]]
    for i in range(12):
        f=frames[start+i];image=Image.new('RGBA',(64,64))
        image.paste(Image.open(args.export/'frames'/f'{name}_idle_{i}.png'),f[5:7])
        def center(box,axis):
            points=[((x,y)[axis],image.getpixel((x,y))[3])
                    for y in range(box[1],box[3]) for x in range(box[0],box[2])]
            return sum(v*a for v,a in points)/max(1,sum(a for v,a in points))
        for out,box,axis in zip(values,[(12,46,32,56),(32,46,56,56),(34,30,62,47),(18,46,46,54)],[0,0,0,1]):
            out.append(center(box,axis))
    spans=[max(v)-min(v) for v in values]
    metrics[name]={'left_foot':spans[0],'right_foot':spans[1],'wing':spans[2],'crawl':spans[3]}
    if name in {'dustimp','copperbeetle'}:
        assert spans[0]>.8 and spans[1]>.4,(name,spans)
        relative=[a-b for a,b in zip(values[0],values[1])]
        assert max(relative)-min(relative)>1.,(name,'feet moving together')
    if name=='armorsnail':assert spans[3]>.08,('missing crawl',spans)
    if name=='swiftmoth':assert spans[2]>1.,('missing wing flap',spans)
args.report.parent.mkdir(parents=True,exist_ok=True)
args.report.write_text(json.dumps({'passed':True,'source_pixel_motion':metrics},indent=2)+'\n')
print('Independent footsteps, crawling foot and wing flapping passed')
