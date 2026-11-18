#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Measure local contour/paint flutter, beyond global brightness/centroids."""
import argparse,json,re
from pathlib import Path
from PIL import Image

def measure(header, export):
    text=header.read_text()
    raw=re.search(r'frames=\{\{(.*?)\}\};',text,re.S)[1]
    frames=[tuple(map(int,s.split(','))) for s in re.findall(r'\{([^{}]+)\}',raw)]
    clips=re.findall(r'inline constexpr Clip (\w+)=\{(\d+),(\d+),([\d.]+)f\}',text)
    result=[]
    for name,start,count,_ in clips:
        if not name.endswith('_idle'):continue
        poses=[]
        for i in range(int(count)):
            f=frames[int(start)+i];im=Image.new('RGBA',(64,64))
            im.paste(Image.open(export/'frames'/f'{name}_{i}.png'),f[5:7])
            poses.append([(a,r*a/255,g*a/255,b*a/255) for r,g,b,a in im.getdata()])
        alpha=paint=active=0
        for i,current in enumerate(poses):
            for prev,now,nxt in zip(poses[i-1],current,poses[(i+1)%len(poses)]):
                if max(prev[0],now[0],nxt[0])<16:continue
                active+=1
                alpha+=abs(prev[0]+nxt[0]-2*now[0])
                paint+=sum(abs(prev[c]+nxt[c]-2*now[c]) for c in (1,2,3))/3
        result.append({'clip':name,'edge_acceleration':alpha/(active*255),
                       'paint_acceleration':paint/(active*255)})
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--export',type=Path,required=True)
    p.add_argument('--report',type=Path,required=True)
    p.add_argument('--baseline-header',type=Path);p.add_argument('--baseline-export',type=Path)
    args=p.parse_args();app=Path(__file__).resolve().parents[1]
    current=measure(app/'art.hpp',args.export)
    # These bounds allow the intended breathing and bending, but reject a
    # contour or material being redrawn between alternating frames.
    # Enemy locomotion intentionally moves feet/wing contours independently.
    # Keep its displacement budget separate from the small plant idle bend.
    walking={'dustimp_idle','copperbeetle_idle','armorsnail_idle','swiftmoth_idle'}
    problems=[r for r in current if r['edge_acceleration']>(.065 if r['clip'] in walking else .025)
              or r['paint_acceleration']>(.075 if r['clip'] in walking else .04)]
    report={'passed':not problems,'problems':problems,'idle_clips':current}
    if args.baseline_header:
        baseline=measure(args.baseline_header,args.baseline_export)
        report['baseline']=baseline
        report['mean_reduction']={key:1-sum(r[key] for r in current)/sum(r[key] for r in baseline)
                                  for key in ['edge_acceleration','paint_acceleration']}
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,indent=2)+'\n')
    assert not problems,problems
    print('Local contour and material continuity passed for all 11 idle loops')
    if 'mean_reduction' in report:print(report['mean_reduction'])
