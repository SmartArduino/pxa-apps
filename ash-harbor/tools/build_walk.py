#!/usr/bin/env python3
"""Build a pose reference for image generation; never used by runtime assets."""
from pathlib import Path
import math
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
N=Image.Resampling.NEAREST

def cell(im,n,cols,rows):
    return im.crop((round(n%cols*im.width/cols),round(n//cols*im.height/rows),
                    round((n%cols+1)*im.width/cols),round((n//cols+1)*im.height/rows)))

def trim(im):
    return im.crop(im.getchannel('A').point(lambda a:255 if a>=96 else 0).getbbox())

def ankle(frame,near):
    phase=(frame+(0 if near else 4))%8
    if phase<=4:return (20-phase*10,112.)
    t=(phase-4)/4
    return (-20+40*t,112-11*math.sin(math.pi*t))

def joints(frame,near):
    bob=[0,1,0,-1,0,1,0,-1][frame]
    hip=(49. if near else 44.,66.+bob)
    dx,y=ankle(frame,near);foot=(hip[0]+dx,y)
    vx,vy=foot[0]-hip[0],foot[1]-hip[1]
    distance=math.hypot(vx,vy)
    theta=math.atan2(vy,vx)-math.acos(max(-1,min(1,(27**2+distance**2-25**2)/(54*distance))))
    knee=(hip[0]+27*math.cos(theta),hip[1]+27*math.sin(theta))
    return hip,knee,foot

def segment(part,start,end,pivot,length):
    dx,dy=end[0]-start[0],end[1]-start[1]
    d=math.hypot(dx,dy);ux,uy=dx/d,dy/d;s=d/length
    cx,cy=pivot;ax,ay=start
    matrix=(uy,-ux,cx-uy*ax+ux*ay,ux/s,uy/s,cy-(ux*ax+uy*ay)/s)
    return part.transform((96,128),Image.Transform.AFFINE,matrix,N)

def build(destination):
    source=Image.open(ROOT/'art/source/leg-rig.png').convert('RGBA')
    atlas=Image.new('RGBA',(768,256))
    for actor,name in enumerate(('jin','lan')):
        original=Image.open(ROOT/f'art/source/{name}.png').convert('RGBA')
        torso=cell(original,0,8,4).resize((96,128),N).crop((0,0,96,74))
        parts=[]
        for n,size in enumerate([(20,32),(14,30),(18,9)]*2):
            parts.append(trim(cell(source,actor*6+n,6,2)).resize(size,N))
        for frame in range(8):
            target=Image.new('RGBA',(96,128))
            # Far leg always behind the near leg, independent of leading foot.
            for near,offset in [(False,3),(True,0)]:
                hip,knee,foot=joints(frame,near)
                boot=parts[offset+2]
                target.alpha_composite(boot,(round(foot[0]-5),round(foot[1]-1)))
                target.alpha_composite(segment(parts[offset+1],knee,foot,(7,3),25))
                target.alpha_composite(segment(parts[offset],hip,knee,(10,3),27))
            bob=[0,1,0,-1,0,1,0,-1][frame]
            target.alpha_composite(torso,(0,bob))
            atlas.paste(target,(frame*96,actor*128))
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True);atlas.save(destination)
    return destination

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',required=True,type=Path,help='Pose reference PNG, normally under local/artifacts')
    print(build(parser.parse_args().output))
