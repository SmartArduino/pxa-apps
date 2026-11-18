#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compile immutable generated PNG masters into bounded RGB565 sprite rows.

This is the asset import/packing step. PNG masters are never overwritten.
The runtime needs no PNG decoder, image library, or atlas-sized RAM buffer.
"""
from pathlib import Path
from collections import Counter
import colorsys, hashlib, json, math
from PIL import Image

APP = Path(__file__).resolve().parents[1]
ROOT = APP / 'art/generated'
FRAMES, PALETTES, DATA, GROUPS, SOURCES = [], [], bytearray(), {}, {}
SHADOWS, SHADOW_COLUMNS = [], []
RIG = json.loads((APP/'art/rig.json').read_text())

def rgb565(c):
    r,g,b=c
    return ((r&248)<<8)|((g&252)<<3)|(b>>3)

def blend(a,b,n):
    return tuple(round((x*(16-n)+y*n)/16) for x,y in zip(a,b))

def sample_region(image, rect, size, trim=False):
    region=image.crop(rect)
    if trim:
        box=region.getchannel('A').point(lambda a:255 if a>=128 else 0).getbbox()
        if not box: raise ValueError('Empty sprite cell')
        region=region.crop(box)
        scale=min(size[0]/region.width,size[1]/region.height)
        content=region.resize((max(1,round(region.width*scale)),max(1,round(region.height*scale))),Image.Resampling.NEAREST)
        result=Image.new('RGBA',size)
        result.paste(content,((size[0]-content.width)//2,(size[1]-content.height)//2))
        return result
    return region.resize(size,Image.Resampling.NEAREST)

def bounds(pixels,w,h):
    # Ignore isolated alpha specks when choosing the registration point only.
    rows=[sum(pixels[y*w+x][3]>=128 for x in range(w)) for y in range(h)]
    ys=[y for y,n in enumerate(rows) if n>=2]
    if not ys: raise ValueError('Sprite has no opaque content')
    xs=[x for x in range(w) if sum(pixels[y*w+x][3]>=128 for y in ys)>=2]
    return min(xs),min(ys),max(xs)+1,max(ys)+1

def add_group(name,filename,cols,rows,size,anchor='center',trim=False,colors=128):
    path=ROOT/filename
    if not path.exists(): return
    image=Image.open(path).convert('RGBA')
    cells=[]
    for row in range(rows):
        for col in range(cols):
            rect=(round(col*image.width/cols),round(row*image.height/rows),round((col+1)*image.width/cols),round((row+1)*image.height/rows))
            cell=sample_region(image,rect,size,trim)
            cells.append(list(cell.getdata()))
    opaque=[p[:3] for pixels in cells for p in pixels if p[3]>=128]
    cloud=Image.new('RGB',(256,math.ceil(len(opaque)/256)))
    cloud.putdata(opaque+[(0,0,0)]*(cloud.width*cloud.height-len(opaque)))
    quant=cloud.quantize(colors=colors,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    raw=quant.getpalette()
    palette=[tuple(raw[i*3:i*3+3]) for i in range(colors)]
    normal,enemy,flash,enemy_flash,dim=[],[],[],[],[]
    for c in palette:
        hue,sat,value=colorsys.rgb_to_hsv(*(v/255 for v in c))
        is_team=.36<=hue<=.58 and sat>.24 and c[1]>c[0]*1.15
        foe=blend(c,(235,102,86),12) if is_team else c
        normal.append(rgb565(c));enemy.append(rgb565(foe));dim.append(rgb565(blend(c,(12,23,29),9)))
        flash.append(rgb565(blend(c,(224,229,207),6) if value>.25 else c))
        enemy_flash.append(rgb565(blend(foe,(224,229,207),6) if value>.25 else foe))
    palette_id=len(PALETTES)
    PALETTES.append([normal,enemy,flash,enemy_flash,dim])
    nearest={}
    def index(c):
        if c not in nearest:
            nearest[c]=min(range(colors),key=lambda i:sum((a-b)**2 for a,b in zip(c,palette[i])))
        return nearest[c]
    first=len(FRAMES)
    for cell_index,pixels in enumerate(cells):
        w,h=size;bx,by,ex,ey=bounds(pixels,w,h)
        if anchor=='infantry':
            points=[x for y in range(by,min(ey,by+5)) for x in range(w) if pixels[y*w+x][3]>=128]
            ox=round(sum(points)/len(points));oy=by+35
        elif anchor=='prop':
            ox=w//2;oy=ey-2
        elif anchor in ('vehicle','building'):
            points=[x for y in range(max(by,ey-(10 if anchor=='vehicle' else 15)),ey) for x in range(w) if pixels[y*w+x][3]>=128]
            ox=round(sum(points)/len(points));oy=ey-(8 if anchor=='vehicle' else 13)
        else: ox=w//2;oy=h//2
        if name in RIG: ox,oy=RIG[name]['anchors'][cell_index%len(RIG[name]['anchors'])]
        # A flat ellipse at the lowest row detaches from sloping tracks and
        # wheelbases. Pack their lower contour instead. Require a short solid
        # vertical run so antennas, thin barrels and stray alpha don't form feet.
        profile={}
        if anchor in ('vehicle','building','prop'):
            depth=min(36,max(14,round((ex-bx)*.55))) if anchor!='prop' else 10
            solid=3
            for x in range(bx,ex):
                if anchor=='vehicle' and sum(pixels[y*w+x][3]>=128 for y in range(max(by,ey-depth),ey))<10:continue
                for y in range(ey-1,max(by+solid-1,ey-depth)-1,-1):
                    if all(pixels[(y-j)*w+x][3]>=128 for j in range(solid)):
                        profile[x]=y;break
            # The lower convex support chain spans wheel gaps on the ground;
            # it must not climb up to the airborne underside of a truck chassis.
            chain=[]
            for point in sorted(profile.items()):
                while len(chain)>=2:
                    a,b=chain[-2:];c=point
                    if (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])<0:break
                    chain.pop()
                chain.append(point)
            for a,b in zip(chain,chain[1:]):
                for x in range(a[0],b[0]+1):profile[x]=round(a[1]+(b[1]-a[1])*(x-a[0])/(b[0]-a[0]))
        left=min(profile,default=0);right=max(profile,default=-1)+1
        SHADOWS.append((len(SHADOW_COLUMNS),left,right-left))
        SHADOW_COLUMNS.extend(profile.get(x,255) for x in range(left,right))
        offset=len(DATA);runs=0
        for y in range(h):
            x=0
            while x<w:
                if pixels[y*w+x][3]<128: x+=1;continue
                left=x;indices=[]
                while x<w and x-left<255 and pixels[y*w+x][3]>=128:
                    indices.append(index(pixels[y*w+x][:3]));x+=1
                DATA.extend((left&255,left>>8,y,len(indices)) if w>255 else (left,y,len(indices)))
                DATA.extend(indices);runs+=1
        FRAMES.append((w,h,ox,oy,bx,by,ex-bx,ey-by,offset,len(DATA),runs,palette_id))
    GROUPS[name]=(first,len(cells))
    SOURCES[filename]={'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'grid':[cols,rows],'native_cell':list(size),'frames':len(cells),'palette_colors':colors}

for name in ('rifle','rocket','tank','artillery','harvester'):
    infantry=name in ('rifle','rocket')
    add_group(name,(name+'-v2.png') if (ROOT/(name+'-v2.png')).exists() else (name+'.png'),4,4,(48,48) if infantry else (80,80),'infantry' if infantry else 'vehicle')
add_group('buildings','buildings.png',4,2,(96,96),'building')
add_group('terrain','terrain-v2.png',4,4,(40,20),trim=True,colors=96)
add_group('environment','environment-v2.png',4,2,(48,56),'prop',colors=96)
add_group('ui_skin','ui-skin-v1.png',4,2,(48,32),trim=True,colors=96)
add_group('icons','ui-icons.png',4,4,(24,24),trim=True,colors=64)
add_group('wordmark','wordmark.png',1,1,(432,144),colors=128)
add_group('badge','app-icon.png',1,1,(96,96),colors=128)
add_group('turret_heads','turret-v2.png',4,4,(64,64),colors=96)
add_group('bridge','bridge-v1.png',2,2,(40,28),trim=True,colors=96)
add_group('mission_skin','mission-skin-v1.png',1,4,(144,20),trim=True,colors=64)

header='// Generated by tools/generate_art.py from original ImageGen PNG masters.\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace iron::art {\n'
header+='struct Frame {uint16_t w,h;int16_t ox,oy;uint16_t bx,by,bw,bh;uint32_t offset,end,count;uint8_t palette;};\n'
header+=f'inline constexpr std::array<Frame,{len(FRAMES)}> frames={{{{'+','.join('{'+','.join(map(str,f))+'}'for f in FRAMES)+'}};\n'
header+=f'inline constexpr std::array<std::array<std::array<uint16_t,128>,5>,{len(PALETTES)}> palettes={{{{'
header+=','.join('{{'+','.join('{{'+','.join(map(str,row+[0]*(128-len(row))))+'}}' for row in group)+'}}' for group in PALETTES)+'}};\n'
header+=f'inline constexpr std::array<uint8_t,{len(DATA)}> bytes={{'+','.join(map(str,DATA))+'};\n'
for name,(first,count) in GROUPS.items():header+=f'inline constexpr unsigned {name}_first={first}, {name}_count={count};\n'
header+='inline constexpr std::array<unsigned,5> unit_first={'+','.join(str(GROUPS[n][0])for n in ('rifle','rocket','tank','artillery','harvester'))+'};\n'
header+='struct Socket {int16_t x,y;bool behind;};\n'
header+='struct ShadowProfile {uint16_t offset;uint8_t left,count;};\n'
header+=f'inline constexpr std::array<ShadowProfile,{len(SHADOWS)}> shadow_profiles={{{{'+','.join('{'+','.join(map(str,p))+'}' for p in SHADOWS)+'}};\n'
header+=f'inline constexpr std::array<uint8_t,{len(SHADOW_COLUMNS)}> shadow_columns={{'+','.join(map(str,SHADOW_COLUMNS))+'};\n'
header+='inline constexpr std::array<std::array<Socket,8>,4> muzzles={{'+','.join('{{'+','.join('{'+str(x)+','+str(y)+','+str(RIG[n]['muzzle_behind_body'][i]).lower()+'}' for i,(x,y) in enumerate(RIG[n]['muzzles']))+'}}' for n in ('rifle','rocket','tank','artillery'))+'}};\n'
header+='inline constexpr std::array<Socket,16> turret_muzzles={{'+','.join('{'+str(x)+','+str(y)+',false}' for x,y in RIG['turret_heads']['muzzles'])+'}};\n'
header+='inline constexpr int turret_mount_height='+str(RIG['turret_heads']['mount_height'])+';\n'
header+='struct InfantryRig {int16_t hip_x,hip_y,cut;};\n'
header+='inline constexpr std::array<std::array<InfantryRig,8>,2> infantry_rigs={{'+','.join('{{'+','.join('{'+str(x)+','+str(y)+','+str(RIG[n]['body_cut'][i])+'}' for i,(x,y) in enumerate(RIG[n]['hips']))+'}}' for n in ('rifle','rocket'))+'}};\n}\n'
(APP/'art_data.hpp').write_text(header)
manifest={'encoding':'opaque row spans: x (uint8, uint16 for width>255), uint8 y, uint8 length, followed by palette-index pixels','sources':SOURCES,'groups':GROUPS,'frame_count':len(FRAMES),'encoded_bytes':len(DATA),'palettes_bytes':len(PALETTES)*128*5*2,'frame_registration':FRAMES,'rig_sha256':hashlib.sha256((APP/'art/rig.json').read_bytes()).hexdigest(),'mode':'built-in image_gen masters; native code locomotion rig; PNG masters retained unchanged'}
(ROOT/'assets.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
# App package icon size is part of the SDK resource format.
Image.open(ROOT/'app-icon.png').convert('RGB').resize((96,96),Image.Resampling.NEAREST).save(APP/'assets/icon.png')
print(f'{len(FRAMES)} frames, {len(DATA)} span bytes, {len(PALETTES)*128*5*2} palette bytes')
# One continuous material is sampled across logical cells. Mirrored repeat
# removes wrap discontinuities without modifying the generated PNG master.
path=ROOT/'continuous-ground-v1.png'
material=Image.open(path).convert('RGB').resize((256,256),Image.Resampling.NEAREST).quantize(colors=96,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
raw=material.getpalette();normal=[];dim=[]
for i in range(96):
 c=tuple(raw[i*3:i*3+3]);normal.append(rgb565(c));dim.append(rgb565(blend(c,(12,23,29),9)))
text='// Generated by tools/generate_art.py from continuous-ground-v1.png.\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace iron::art {\n'
text+='inline constexpr std::array<uint8_t,65536> ground_texels={'+','.join(map(str,material.tobytes()))+'};\n'
text+='inline constexpr std::array<uint16_t,96> ground_material={'+','.join(map(str,normal))+'};\n'
text+='inline constexpr std::array<uint16_t,96> ground_material_dim={'+','.join(map(str,dim))+'};\n}\n'
(APP/'ground_texture_data.hpp').write_text(text)
manifest['continuous_ground']={'source':'continuous-ground-v1.png','sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'size':[256,256],'palette_colors':96,'addressing':'world-projected coordinates; mirrored repeat; no cell borders'}
(ROOT/'assets.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
