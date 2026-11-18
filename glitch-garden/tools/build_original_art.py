#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compile generated RGBA sprite sheets, lettering and UI into bounded PXA atlases.

Compile original generated artwork, and bake coherent cutout animation from its
character textures. Lettering remains the generated artwork, never a font.
"""
from pathlib import Path
import argparse,hashlib,json,math,shutil,struct
from PIL import Image,ImageOps

APP=Path(__file__).resolve().parents[1]
SRC=APP/'art/generated'
parser=argparse.ArgumentParser();parser.add_argument('--export',type=Path);args=parser.parse_args()
NAMES=['dewflower','coralflower','leafshield','pinwheel','dustimp','copperbeetle','frostbell','emberpod','cloverling','armorsnail','swiftmoth']
images=[];names=[];records=[];clips=[];source_hashes=[];masks=set()

def load(name):
    file=SRC/f'{name}.png';im=Image.open(file).convert('RGBA')
    source_hashes.append({'file':str(file.relative_to(APP)),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'size':list(im.size)})
    if name not in ('terrain','garden-v2'):
        assert im.getchannel('A').getextrema()[0]<20, f'{name} needs genuine alpha'
    return im

def cell(im,x,y,cols,rows):
    return im.crop((round(x*im.width/cols),round(y*im.height/rows),round((x+1)*im.width/cols),round((y+1)*im.height/rows)))

def add(name,im,max_size,actor=False,mask=False,actor_box=None):
    # Trim only as a packing optimization. Store the offset inside the unchanged
    # logical canvas so bobbing, recoil and shrinking deaths retain their pivots.
    if actor:
        if actor_box is None:raise ValueError('actors require a fixed per-clip canvas crop')
        box=actor_box;ox,oy=box[:2];cw,ch=64,50;im=im.crop(box)
    else:
        box=im.getchannel('A').point(lambda a:255 if (a>2 if mask else a>=96) else 0).getbbox() or (0,0,1,1)
        im=im.crop(box);im.thumbnail(max_size,Image.Resampling.LANCZOS);ox=oy=0;cw,ch=im.size
    index=len(images);images.append(im);names.append(name);records.append((ox,oy,cw,ch))
    if mask:masks.add(index)
    if args.export:
        out=args.export/'frames'/f'{name}.png';out.parent.mkdir(parents=True,exist_ok=True);im.save(out)
    return index

def matte_sheet(sheet,rows):
    # Estimate the exported alpha matte from guaranteed empty cell borders.
    border=[]
    for y in range(rows):
        for x in range(6):
            alpha=cell(sheet,x,y,6,rows).getchannel('A');w,h=alpha.size
            for box in [(0,0,w,2),(0,h-2,w,h),(0,2,2,h-2),(w-2,2,w,h-2)]:border.extend(alpha.crop(box).getdata())
    border.sort();floor=min(48,border[round((len(border)-1)*.99)])
    if floor<=4:floor=0
    if floor:
        alpha=sheet.getchannel('A').point(lambda a:max(0,round((a-floor)*255/(255-floor))))
        sheet=sheet.copy();sheet.putalpha(alpha)
    return sheet,floor

from coherent_motion import anchor_texture,clip as coherent_clip
registration=[]
for name in NAMES:
    rows=5 if name=='copperbeetle' else 4
    original,floor=matte_sheet(load(name),rows)
    texture=anchor_texture(cell(original,0,0,6,rows))
    registration.append({'character':name,'source_pose':[0,0],'alpha_floor':floor,
                         'motion':'continuous-rooted-cutout','supersampling':4})
    for state in ['idle','action','hurt','defeat','jump'][:rows]:
        first=len(images);frames,box=coherent_clip(texture,name,state)
        for x,im in enumerate(frames):add(f'{name}_{state}_{x}',im,(64,64),actor=True,actor_box=box)
        clips.append((name+'_'+state,first,12,.6 if state in ('hurt','defeat','jump') else 1.0))

for source,name,box in [('frostbell','frost_shot',(962,394,1016,434)),('emberpod','ember_shot',(744,365,798,409))]:
    im=Image.open(SRC/f'{source}.png').convert('RGBA')
    add(name,im.crop(box),(20,14))

garden=load('garden-v2').resize((448,448),Image.Resampling.LANCZOS)
for y in range(2):
    for x in range(2):add(f'garden_{y*2+x}',garden.crop((x*224,y*224,(x+1)*224,(y+1)*224)),(224,224))
ui=load('ui')
for i,name in enumerate(['tray','seed','seed_selected','seed_disabled','resource','pause','help','trowel','sign','wave','stone','medal','seed_shot','leaf_shot','dew','impact']):
    if name=='sign':continue
    size=(192,55) if name=='tray' else (110,32) if name=='wave' else (44,64) if name.startswith('seed') and name!='seed_shot' else (48,48)
    add('ui_'+name,cell(ui,i%4,i//4,4,4),size)
ui2=load('ui-v2')
for i,(name,size) in enumerate([('pause_panel',(224,224)),('button',(176,30)),('start_button',(160,44)),('title_board',(224,62)),('shadow',(48,16)),('notice',(144,36))]):
    add('ui_'+name,cell(ui2,i%2,i//2,2,3),size,mask=name=='shadow')
add('ui_focus_leaf',load('focus-leaf'),(48,40))
# Join the original wooden button's rounded end caps at their native height.
# The small navigation button keeps its border/leaf scale instead of squeezing
# the whole long button or putting a chevron on the level-selection rock.
button=images[names.index('ui_button')];cap=button.height//2
page_button=Image.new('RGBA',(cap*2,button.height))
page_button.paste(button.crop((0,0,cap,button.height)),(0,0))
page_button.paste(button.crop((button.width-cap,0,button.width,button.height)),(cap,0))
add('ui_page_button',page_button,(30,30))

letters=load('lettering')
TEXT=['title','start','resume','again','choose','paused','won','lost','next','restart','home','help','back','exit','funds','occupied','empty','sound','stage','kills']
for i,name in enumerate(TEXT):add('text_'+name,cell(letters,i%4,i//4,4,5),(192,48 if name in ('title','paused','won','lost') else 40))
digits=load('digits')
for i,name in enumerate(['0','1','2','3','4','5','6','7','8','9','slash','colon','plus','minus','percent','a','b','x','question','bang','ordinal','stage','seconds','wave','dot']):
    add('glyph_'+name,cell(digits,i%5,i//5,5,5),(17,23))

# A single shared 255-color palette prevents texture-specific palette swaps.
sample=Image.new('RGB',(512,math.ceil(sum(im.width*im.height for im in images)/512)+2048),(94,135,52))
sx=sy=shelf=0
for im in images:
    if sx+im.width>512:sx=0;sy+=shelf;shelf=0
    sample.paste(im,(sx,sy),im);sx+=im.width;shelf=max(shelf,im.height)
sample=sample.crop((0,0,512,sy+shelf));palette=sample.quantize(colors=255,method=Image.Quantize.MEDIANCUT);pal=palette.getpalette()[:765]
colors=[0]+[((pal[i]&248)<<8)|((pal[i+1]&252)<<3)|(pal[i+2]>>3) for i in range(0,765,3)]

# MaxRects uses the strips around a large panel instead of wasting a full shelf.
positions=[None]*len(images);pages=[]
def contains(a,b):return a[0]<=b[0] and a[1]<=b[1] and a[0]+a[2]>=b[0]+b[2] and a[1]+a[3]>=b[1]+b[3]
for index in sorted(range(len(images)),key=lambda i:(-max(images[i].size),-images[i].width*images[i].height)):
    im=images[index];best=None
    for page,free in enumerate(pages):
        for x,y,w,h in free:
            if im.width<=w and im.height<=h:
                score=(min(w-im.width,h-im.height),max(w-im.width,h-im.height),page,y,x)
                if best is None or score<best[0]:best=(score,page,x,y)
    if best is None:
        pages.append([(0,0,256,256)]);best=(None,len(pages)-1,0,0)
    _,page,px,py=best;positions[index]=(page,px,py);right=px+im.width;bottom=py+im.height;new=[]
    for x,y,w,h in pages[page]:
        if x>=right or x+w<=px or y>=bottom or y+h<=py:new.append((x,y,w,h));continue
        if x<px:new.append((x,y,px-x,h))
        if x+w>right:new.append((right,y,x+w-right,h))
        if y<py:new.append((x,y,w,py-y))
        if y+h>bottom:new.append((x,bottom,w,y+h-bottom))
    new=list(dict.fromkeys(new));pages[page]=[a for i,a in enumerate(new) if not any(i!=j and contains(b,a) for j,b in enumerate(new))]
dimensions=[]
for page in range(len(pages)):
    placed=[(pos,im) for pos,im in zip(positions,images) if pos[0]==page]
    dimensions.append((max(pos[1]+im.width for pos,im in placed),max(pos[2]+im.height for pos,im in placed)))
decoded_bytes=sum(w*h for w,h in dimensions)
assert len(pages)<=32 and decoded_bytes+len(pages)*32<=2048*1024, f'{decoded_bytes} decoded bytes exceed the source-plane budget'
resources=APP/'resources';resources.mkdir(exist_ok=True)
for f in resources.glob('*.index8'):f.unlink()
entries=[];rgba_pages=[]
for page in range(len(pages)):
    width,height=dimensions[page];data=bytearray(width*height);coverage=bytearray(width*height)
    for index,(im,(p,x,y)) in enumerate(zip(images,positions)):
        if p!=page:continue
        indexed=im.convert('RGB').quantize(palette=palette,dither=Image.Dither.NONE).tobytes();alpha=im.getchannel('A').tobytes()
        for yy in range(im.height):
            for xx in range(im.width):
                at=yy*im.width+xx;dest=(y+yy)*width+x+xx
                coverage[dest]=round(alpha[at]*.35) if index in masks else 255 if alpha[at]>=240 else 0 if alpha[at]<=(24 if names[index].startswith('frostbell_') else 8) else alpha[at]
                data[dest]=indexed[at]+1 if coverage[dest] else 0
    (resources/f'original-{page}.index8').write_bytes(data)
    entries.append({'path':f'assets/original-{page}.pxr','kind':'index8','source':f'resources/original-{page}.index8','width':width,'height':height})
    rgba_pages.append(bytes(data)+bytes(coverage))

# The application reads these two planes into its bounded sampler. Keeping
# coverage independent of palette indices preserves every antialiased edge.
from art_codec import pack,unpack
# Runtime source stores only each row's visible interval, not atlas padding.
source=bytearray();source_rows=[];source_first=[]
for im,(page,x,y) in zip(images,positions):
    width,height=dimensions[page];data=rgba_pages[page];source_first.append(len(source_rows))
    for yy in range(im.height):
        alpha=[data[width*height+(y+yy)*width+x+xx] for xx in range(im.width)]
        live=[xx for xx,a in enumerate(alpha) if a];left=min(live) if live else 0;right=max(live)+1 if live else 0
        count=right-left;opaque_begin=opaque_count=0;xx=left
        while xx<right:
            if alpha[xx]!=255:xx+=1;continue
            begin=xx
            while xx<right and alpha[xx]==255:xx+=1
            if xx-begin>opaque_count:opaque_begin=begin-left;opaque_count=xx-begin
        source_rows.append((len(source),left,count,opaque_begin,opaque_count))
        source.extend(data[(y+yy)*width+x+xx] for xx in range(left,right))
        source.extend(alpha[left+i] for i in range(count) if not opaque_begin<=i<opaque_begin+opaque_count)
raw=bytes(source);packed=pack(raw);assert unpack(packed,len(raw))==raw
blob='// Generated original artwork, CC0 to extent of rights held.\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace garden_art {\n'
blob+=f'inline constexpr std::array<uint8_t,{len(packed)}> packed_pixels={{{{'+','.join(map(str,packed))+'}};\n'
# Bake the map and the actual grid at common native viewports, avoiding any
# full-screen scaling at launch. Other geometries retain the filtered fallback.
backgrounds=[];back_data=bytearray()
for width,height,left,top,right,bottom in [(480,480,12,12,12,24),(296,240,0,0,0,0),(240,240,0,0,0,0)]:
    scale=max(1,min(width/296,height/240));lw=round(width/scale);lh=round(height/scale)
    l=math.ceil(left/scale);t=math.ceil(top/scale);r=math.ceil(right/scale);b=math.ceil(bottom/scale)
    cell_size=max(8,min((lw-l-r-54)//9,(lh-t-b-84)//5));bx=l+((lw-l-r)-cell_size*9)//2+3;by=t+53+max(0,((lh-t-b)-84-cell_size*5)//2)
    bg=garden.resize((width,height),Image.Resampling.LANCZOS)
    def pixel(v):return int(math.floor(v*scale+.5))
    def lawn_axis(position,count):
        if position<=0 or position>=count:return 0.
        edge=max(0.,min(1.,min(position,count-position)/.2))
        return max(-1.,min(1.,math.sin(position*math.pi)*3))*edge*edge*(3-2*edge)
    columns=[lawn_axis(((x+.5)/scale-bx)/cell_size,9) for x in range(width)]
    lines=[lawn_axis(((y+.5)/scale-by)/cell_size,5) for y in range(height)]
    indexed=bg.convert('RGB').quantize(palette=palette,dither=Image.Dither.NONE).tobytes();pixels=bytearray();
    for i,v in enumerate(indexed):
        color=colors[v+1];q=256+round(14*columns[i%width]*lines[i//width])
        color=(min(31,((color>>11)*q+128)//256)<<11)|(min(63,(((color>>5)&63)*q+128)//256)<<5)|min(31,((color&31)*q+128)//256)
        pixels.extend((color&255,color>>8))
    encoded=pack(pixels);assert unpack(encoded,len(pixels))==pixels;backgrounds.append((width,height,bx,by,cell_size,len(back_data),len(encoded)));back_data.extend(encoded)
def cache_rows(values,width,height):
    words=[0]*height
    for y in range(height):
        row=values[y*width:(y+1)*width];live=[x for x,v in enumerate(row) if v>>16]
        if not live:continue
        left=min(live);right=max(live)+1;words[y]=len(words);words.append((left<<16)|(right-left));words.extend(row[left:right])
    return struct.pack('<'+str(len(words))+'I',*words)
prepared=[];prepared_data=bytearray();seen=set()
def bake_ui(name,r):
    id=names.index(name);im=images[id];x,y,w,h=r;ww=min(w,h*im.width//im.height);hh=min(h,w*im.height//im.width);x+=(w-ww)//2;y+=(h-hh)//2
    scale=480/296
    pw=round((x+ww)*scale)-round(x*scale);ph=round((y+hh)*scale)-round(y*scale)
    if (id,pw,ph) in seen:return
    seen.add((id,pw,ph));target=im.resize((pw,ph),Image.Resampling.BILINEAR);values=[]
    for red,green,blue,a in target.getdata():
        a=255 if a>=240 else 0 if a<=8 else a
        color=(((red*a//255)&248)<<8)|(((green*a//255)&252)<<3)|(blue*a//255>>3)
        values.append(color|(a<<16))
    data=cache_rows(values,pw,ph);encoded=pack(data);assert unpack(encoded,len(data))==data;prepared.append((id,pw,ph,len(prepared_data),len(encoded),len(data)//4));prepared_data.extend(encoded)
for name,r in [('ui_pause_panel',(36,32,224,224)),('ui_title_board',(40,23,216,65)),('text_title',(56,33,184,43)),('ui_start_button',(71,220,154,43)),('text_start',(86,228,124,27))]:bake_ui(name,r)
for i,label in enumerate(['resume','help','restart','home','exit']):
    bake_ui('ui_button',(63,73+i*32,170,29));bake_ui('text_'+label,(73,76+i*32,150,23))
blob+='struct Prepared {unsigned id,width,height,offset,size,words;};\ninline constexpr std::array<Prepared,'+str(len(prepared))+'> prepared={{'+','.join('{'+','.join(map(str,v))+'}' for v in prepared)+'}};\n'
blob+=f'inline constexpr std::array<uint8_t,{len(prepared_data)}> packed_prepared={{{{'+','.join(map(str,prepared_data))+'}};\n'
print('Prepared native UI',len(prepared),'packed bytes',len(prepared_data))
blob+='struct BakedBackground {unsigned width,height,bx,by,cell,offset,size;};\ninline constexpr std::array<BakedBackground,3> backgrounds={{{'+','.join('{'+','.join(map(str,v))+'}' for v in backgrounds)+'}};\n'
blob+=f'inline constexpr std::array<uint8_t,{len(back_data)}> packed_backgrounds={{{{'+','.join(map(str,back_data))+'}};\n}\n'
# Correct aggregate brace spelling without huge generated source lines.
blob=blob.replace('backgrounds={{{','backgrounds={{')
(APP/'art_data.hpp').write_text(blob)
old=APP/'assets/filtered-art.bin'
if old.exists():old.unlink()
print('Embedded packed source',len(packed),'backgrounds',len(back_data))

h='// Generated by tools/build_original_art.py. Original AI-generated art and lettering.\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace garden_art {\n'
h+=f'inline constexpr unsigned pages={len(pages)};\ninline constexpr std::array<uint16_t,256> palette={{'+','.join(map(str,colors))+'};\n'
h+='struct Page {unsigned offset,width,height;};\ninline constexpr std::array<Page,pages> page_info={{'
offset=0
page_records=[]
for width,height in dimensions:
    page_records.append('{'+f'{offset},{width},{height}'+'}');offset+=width*height*2
h+=','.join(page_records)+'}};\n'
h+=f'inline constexpr unsigned pixel_bytes={len(source)};\n'
h+='struct SourceRow {unsigned offset;uint16_t left,count,opaque_begin,opaque_count;};\n'
h+=f'inline constexpr std::array<SourceRow,{len(source_rows)}> source_rows={{{{'+','.join('{'+','.join(map(str,r))+'}' for r in source_rows)+'}};\n'
h+=f'inline constexpr std::array<unsigned,{len(source_first)}> source_first={{'+','.join(map(str,source_first))+'};\n'
h+='struct Frame {uint8_t page,x,y;uint16_t w,h;int8_t ox,oy;uint16_t cw,ch;};\n'
h+=f'inline constexpr std::array<Frame,{len(images)}> frames={{{{'
h+=','.join('{'+','.join(map(str,[*pos,im.width,im.height,*r]))+'}' for pos,im,r in zip(positions,images,records))+'}};\n'
h+='struct Clip {unsigned first,count;float seconds;};\n'
for name,first,count,seconds in clips:h+=f'inline constexpr Clip {name}={{{first},{count},{seconds}f}};\n'
for i,name in enumerate(names):
    if not any(name.startswith(n+'_') for n in NAMES):h+=f'inline constexpr unsigned {name}={i};\n'
h+='}\n';(APP/'art.hpp').write_text(h);(APP/'resources.json').write_text(json.dumps({'assets':[]},indent=2)+'\n')
reference_sheets=[{'file':str(f.relative_to(APP)),'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'usage':'design reference; not alternated during playback'} for f in sorted(SRC.glob('*-inbetween.png'))]
(APP/'asset-sources.json').write_text(json.dumps({'generator':'built-in image_gen','animation_generator':'tools/coherent_motion.py; continuous cutout mesh, 4x supersampling','art_license':'CC0-1.0 to the extent of rights held','files':source_hashes,'reference_sheets':reference_sheets,'frames':len(images),'atlas_pages':len(pages),'decoded_texture_bytes':decoded_bytes,'runtime_source_bytes':len(source),'animation_frames':sum(c[2] for c in clips),'animation_clips':len(clips),'animation_rigs':registration},indent=2)+'\n')
if args.export:
    compiled_images=[]
    for index,(im,(page,x,y)) in enumerate(zip(images,positions)):
        width,height=dimensions[page];raw=rgba_pages[page];pixels=[]
        for yy in range(im.height):
            for xx in range(im.width):
                at=(y+yy)*width+x+xx;c=colors[raw[at]];pixels.append(((c>>11)*255//31,((c>>5)&63)*255//63,(c&31)*255//31,raw[width*height+at]))
        compiled=Image.new('RGBA',im.size);compiled.putdata(pixels);compiled_images.append(compiled);compiled.save(args.export/'frames'/f'{names[index]}.png')
    for name,first,count,seconds in clips:
        preview=[]
        for i in range(first,first+count):
            im=compiled_images[i];ox,oy,cw,ch=records[i];bg=Image.new('RGBA',(80,80),(111,153,69,255));bg.alpha_composite(im,(8+ox,8+oy));preview.append(bg.convert('RGB'))
        out=args.export/'previews';out.mkdir(exist_ok=True)
        preview[0].save(out/f'{name}.gif',save_all=True,append_images=preview[1:],duration=round(.6/count*1000),loop=0)
print(f'{len(clips)} clips, {len(images)} sprites, {len(pages)} atlas pages / {decoded_bytes} decoded bytes')
