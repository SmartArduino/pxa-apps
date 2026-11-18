#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the actual palette/coverage exports for temporal material/anchor faults."""
from pathlib import Path
import argparse,re,math,json
from PIL import Image
parser=argparse.ArgumentParser();parser.add_argument('--export',type=Path,required=True);parser.add_argument('--report',type=Path,required=True);args=parser.parse_args()
app=Path(__file__).resolve().parents[1];header=(app/'art.hpp').read_text();raw=re.search(r'frames=\{\{(.*?)\}\};',header,re.S)[1];frames=[tuple(map(int,s.split(','))) for s in re.findall(r'\{([^{}]+)\}',raw)]
clips=re.findall(r'inline constexpr Clip (\w+)=\{(\d+),(\d+),([\d.]+)f\}',header);report=[];problems=[]
for name,start,count,seconds in clips:
 start,count=int(start),int(count);assert count==12,(name,count)
 assert len({f[3:] for f in frames[start:start+count]})==1,(name,'changing crop/sampling geometry')
 if not name.endswith('_idle'):continue
 luma=[];mass=[];cx=[];cy=[];foot=[]
 for i in range(count):
  f=frames[start+i];im=Image.open(args.export/'frames'/f'{name}_{i}.png');assert im.size==f[3:5]
  total=light=xsum=ysum=0;ground=-1
  for y in range(im.height):
   for x in range(im.width):
    r,g,b,a=im.getpixel((x,y));total+=a;light+=(.2126*r+.7152*g+.0722*b)*a;xsum+=(x+f[5])*a;ysum+=(y+f[6])*a
    if 12<=x+f[5]<52 and a>=128:ground=max(ground,y+f[6])
  luma.append(light/total);mass.append(total/255);cx.append(xsum/total);cy.append(ysum/total);foot.append(ground)
 parity=abs(sum(luma[::2])/6-sum(luma[1::2])/6)
 delta_x=max(abs(cx[(i+1)%12]+cx[(i-1)%12]-2*cx[i]) for i in range(12));delta_y=max(abs(cy[(i+1)%12]+cy[(i-1)%12]-2*cy[i]) for i in range(12))
 if parity>1.5:problems.append((name,'alternating brightness',parity))
 if max(foot)-min(foot)>1:problems.append((name,'moving foot',foot))
 if delta_x>.65:problems.append((name,'horizontal jitter',delta_x))
 if delta_y>.8:problems.append((name,'vertical jitter',delta_y))
 report.append({'clip':name,'alternating_luminance':parity,'max_center_acceleration':[delta_x,delta_y],'foot_range':max(foot)-min(foot)})
args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps({'passed':not problems,'problems':problems,'idle_clips':report,'all_clips_fixed_geometry':len(clips)},indent=2));assert not problems,problems;print('All 45 clips use fixed sampling; 11 idle loops pass brightness, foot and center continuity checks')
