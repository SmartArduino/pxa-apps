#!/usr/bin/env python3
"""Build generated game art and licensed Fusion Pixel glyph atlases."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont
from sprite_frames import fighter_frames
from atlas_pack import pack_tiles
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"resources";OUT.mkdir(exist_ok=True)
N=Image.Resampling.NEAREST
actors=("jin","lan","zhen","ling")
images={n:Image.open(ROOT/"art/source"/(n+".png")).convert("RGBA")
        for n in ("stage",*actors,"title","effects","extras","extras-new",
                  "ui-kit","ui-digits","ui-signals","ui-meters","story-0","story-1","story-2","effects-new","stage-panorama")}
import statistics
stage_sources=['stage-harbor-traditional','stage-bamboo','stage-snow','stage-lantern']
for source in stage_sources+['dpad-eight-clean']:
    images[source]=Image.open(ROOT/'art/source'/(source+'.png')).convert('RGBA')
images['stage-panorama']=images[stage_sources[0]]
images['story-0']=images[stage_sources[0]]
images['story-2']=Image.open(ROOT/'art/source/story-tournament.png').convert('RGBA')
def stage_raster(source):
    ratio=512/304
    if source.width/source.height>ratio:
        w=round(source.height*ratio);x=(source.width-w)//2;source=source.crop((x,0,x+w,source.height))
    else:
        h=round(source.width/ratio);y=(source.height-h)//2;source=source.crop((0,y,source.width,y+h))
    return source.resize((512,304),N)
stage_rasters=[stage_raster(images[name]) for name in stage_sources]
# Each selected input contains two complete, independently finished figures.
paired_sets=[[Image.open(ROOT/f"art/source/{folder}/pose-{i}.png").convert("RGBA") for i in range(8)] for folder in ("walk-frames","walk-frames-new")]
walk=Image.new("RGBA",(768,512))
for actor,name in enumerate(actors):
    # Helpers are defined below; the source cells are complete image halves.
    side=actor%2
    frames=[im.crop((round(side*im.width/2),0,round((side+1)*im.width/2),im.height)) for im in paired_sets[actor//2]]
    boxes=[im.getchannel("A").point(lambda a:255 if a>=96 else 0).getbbox() for im in frames]
    base=images[name];reference=base.crop((0,0,base.width//8,base.height//4))
    box=reference.getchannel("A").point(lambda a:255 if a>=96 else 0).getbbox()
    standing_height=(box[3]-box[1])*128/reference.height
    factor=standing_height/statistics.median(b[3]-b[1] for b in boxes)
    baseline=statistics.median(b[3] for b in boxes)*factor
    for i,im in enumerate(frames):
        f=standing_height/(boxes[i][3]-boxes[i][1]) if actor>=2 else factor
        im=im.resize((round(im.width*f),round(im.height*f)),N)
        x=round(48-(boxes[i][0]+boxes[i][2])*f/2) if actor>=2 else (96-im.width)//2
        y=round(120-boxes[i][3]*f) if actor>=2 else round(120-baseline)
        target=Image.new("RGBA",(96,128));target.paste(im,(x,y))
        walk.paste(target,(i*96,actor*128))
images["walk"]=walk
training=Image.new("RGB",(1024,((len(images)+3)//4)*384),(12,20,35))
for i,im in enumerate(images.values()):
    thumb=im.copy();thumb.thumbnail((256,384),N)
    training.paste(thumb,((i%4)*256,(i//4)*384),thumb.getchannel("A"))
palette_image=training.quantize(colors=255,method=Image.Quantize.MEDIANCUT)
palette=palette_image.getpalette()[:765]
rgb565=[0]+[((r>>3)<<11)|((g>>2)<<5)|(b>>3)
           for r,g,b in zip(palette[::3],palette[1::3],palette[2::3])]
pages=[];names={};walk_offsets={};fighter_offsets=[];extras_offsets={}
def page(w,h):
    pages.append(Image.new("RGBA",(w,h)));return len(pages)-1
def put(name,im,p,x,y):
    assert x+im.width<=pages[p].width and y+im.height<=pages[p].height
    pages[p].paste(im,(x,y));names[name]=[p,x,y,im.width,im.height]
def cell(im,n,cols,rows):
    x,y=n%cols,n//cols
    return im.crop((round(x*im.width/cols),round(y*im.height/rows),
                    round((x+1)*im.width/cols),round((y+1)*im.height/rows)))
def trim(im):
    box=im.getchannel("A").point(lambda a:255 if a>=96 else 0).getbbox()
    assert box;return im.crop(box)
pixel_font=ImageFont.truetype(str(ROOT/"tools/fonts/fusion-pixel-12px-monospaced-zh_hans.ttf"),12,layout_engine=ImageFont.Layout.BASIC)
def font_raster(text,width):
    mask=Image.new("L",(width,12))
    draw=ImageDraw.Draw(mask);draw.fontmode="1"
    draw.text((0,10),text,font=pixel_font,fill=255,anchor="ls")
    target=Image.new("RGBA",mask.size,(255,233,180,255));target.putalpha(mask)
    return target
ui=page(256,256)
logo=trim(images["title"]);logo.thumbnail((224,72),N);put("logo",logo,ui,0,0)
chars="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-:/.!?><=()abcdefghijklmnopqrstuvwxyz"
font_metrics={}
for i,ch in enumerate(chars):
    target=font_raster(ch,6);box=target.getchannel("A").getbbox();assert box,("missing glyph",ch)
    put(f"glyph_{i}",target,ui,(i%8)*6,96+(i//8)*12)
    font_metrics[ch]={"left":box[0],"top":box[1],"height":box[3]-box[1],"width":box[2]-box[0],"baseline":10,"advance":6}
(ROOT/"art/font-metrics.json").write_text(json.dumps(font_metrics,indent=2)+"\n")
for i in range(12):
    put(f"effect_0_{i}",cell(images["effects"],i,4,3).resize((32,32),N),
        ui,128+(i%4)*32,96+(i//4)*32)
    names[f"effect_1_{i}"]=names[f"effect_0_{i}"]
for actor in range(4):
    for i in range(4):
        if actor<2:names[f"shot_{actor}_{i}"]=names[f"effect_0_{actor*4+i}"]
        else:put(f"shot_{actor}_{i}",cell(images['effects-new'],(actor-2)*4+i,4,2).resize((32,32),N),ui,((actor-2)*4+i)*32,224)
labels=json.loads((ROOT/"tools/chinese-labels.json").read_text())
label_glyphs=[];label_metrics=[]
cp=page(256,256);cx=cy=0
for i,label in enumerate(labels):
    advances=[round(pixel_font.getlength(ch)) for ch in label]
    w=sum(advances);target=font_raster(label,w);first=len(label_glyphs);offset=0
    for ch,advance in zip(label,advances):
        box=target.getchannel('A').crop((offset,0,offset+advance,12)).getbbox()
        ink=(box[0],box[1],box[2]-box[0],box[3]-box[1]) if box else (0,0,0,0)
        label_glyphs.append((offset,advance,*ink));offset+=advance
    label_metrics.append((first,len(label)))
    if cx+w>256:cx=0;cy+=12
    if cy+12>256:
        pages[cp]=pages[cp].crop((0,0,256,cy));cp=page(256,256);cx=cy=0
    put(f"cn_{i}",target,cp,cx,cy);cx+=w
pages[cp]=pages[cp].crop((0,0,256,cy+12))
# Native-size UI pieces. Resampling happens once here, never while drawing.
# Transparent sprites are clipped from regular source cells, with alpha preserved.
up=page(256,256)
kit=images["ui-kit"]
def ui_cell(name,index,size,x,y):
    im=trim(cell(kit,index,2,4)).resize(size,N)
    put(name,im,up,x,y)
ui_cell("button_idle",0,(112,26),0,0)
ui_cell("button_focus",1,(112,26),128,0)
ui_cell("health_left",2,(124,36),0,32)
ui_cell("health_right",3,(124,36),128,32)
ui_cell("modal",4,(112,56),0,72)
ui_cell("action_idle",5,(28,28),192,72)
ui_cell("action_pressed",6,(28,28),224,72)
stick=trim(images['dpad-eight-clean']).resize((64,64),N)
put('dpad',stick,up,128,72)
arrow_regions=[(26,0,12,14),(46,8,12,13),(50,26,14,12),(46,46,12,12),(26,50,12,14),(7,46,13,12),(0,26,14,12),(7,8,13,13)]
for i,(x,y,w,h) in enumerate(arrow_regions):
    put(f'dpad_arrow_{i}',stick.crop((x,y,x+w,y+h)),page(w,h),0,0)
for i,stage in enumerate(stage_rasters):
    put(f'stage_preview_{i}',stage.resize((64,38),N),page(64,38),0,0)
for i in range(10):
    im=trim(cell(images["ui-digits"],i,5,2));im.thumbnail((16,20),N)
    digit=Image.new("RGBA",(16,20));digit.paste(im,((16-im.width)//2,20-im.height))
    put(f"digit_{i}",digit,up,i*20,136)
meters=images["ui-meters"]
for i in range(6):
    im=trim(cell(meters,i,2,4))
    # Only the painted fill is needed inside the HUD's existing bezel.
    im=im.crop((round(im.width*.15),round(im.height*.28),round(im.width*.85),round(im.height*.78)))
    put(f"fill_{i}",im.resize((110,6),N),up,(i%2)*128,164+(i//2)*8)
def contain(im,size):
    """Preserve artwork proportions and center it on an integer pixel canvas."""
    im=trim(im);im.thumbnail(size,N)
    target=Image.new("RGBA",size)
    target.paste(im,((size[0]-im.width)//2,(size[1]-im.height)//2))
    return target
# The generator placed the last-row ornaments ABOVE the nominal row boundary.
# Expanded, non-overlapping ROIs preserve their complete bevel and corner tips.
mw,mh=meters.size
timer_source=meters.crop((0,round(mh*.64),round(mw/2),mh))
put("timer",contain(timer_source,(40,40)),up,0,192)
# The empty gap is layout space, not part of either selection bracket.
brackets=meters.crop((round(mw/2),round(mh*.64),mw,mh))
put("bracket_left",contain(cell(brackets,0,2,1),(14,28)),up,48,192)
put("bracket_right",contain(cell(brackets,1,2,1),(14,28)),up,68,192)
signals=images["ui-signals"]
vp=page(256,192)
for name,index,limit,x,y in [("ko",0,(112,64),0,0),("fight",1,(112,40),128,0),
                            ("round",2,(80,22),0,72),("win",3,(80,24),128,72)]:
    x0=round((index%2)*signals.width/2);x1=round((index%2+1)*signals.width/2)
    y0,y1=(0,round(signals.height*.35)) if index<2 else (round(signals.height*.35),round(signals.height*.56))
    im=trim(signals.crop((x0,y0,x1,y1)));im.thumbnail(limit,N);put(name,im,vp,x,y)
icon_names=["pause","help","back","play","flame","wave","star","empty_star",
            "left","right","punch","kick","evade","skill","trophy","cursor"]
for i,name in enumerate(icon_names):
    group=cell(signals,4+i//4,2,4);im=trim(cell(group,i%4,4,1));im.thumbnail((12,12),N)
    icon=Image.new("RGBA",(12,12));icon.paste(im,((12-im.width)//2,(12-im.height)//2))
    put("icon_"+name,icon,vp,(i%8)*16,112+(i//8)*16)
# Repack the UI and text before adding scenery, without scaling any tile.
old_pages,old_names=pages,names
unique={tuple(t):old_pages[t[0]].crop((t[1],t[2],t[1]+t[3],t[2]+t[4])) for t in old_names.values()}
pages,packed_refs=pack_tiles(unique.items())
names={name:packed_refs[tuple(t)] for name,t in old_names.items()}
# Only UI/labels/previews use the shared Assets cache. Current scenery is
# uploaded from the lossless bank alongside fighter poses, never all at once.
static_pages=len(pages)
for scene,stage in enumerate(stage_rasters):
    for i in range(4):
        x=i%2*256;y=i//2*256;im=stage.crop((x,y,x+256,min(304,y+256)))
        put(f'stage_{scene}_{i}',im,page(im.width,im.height),0,0)
portrait_frames=[]
for actor in actors:
    frames,offsets=fighter_frames(images[actor]);fighter_offsets.append(offsets)
    portrait=frames[0];left=max(0,(portrait.width-58)//2)
    portrait_frames.append(portrait.crop((left,0,min(portrait.width,left+58),min(portrait.height,56))))
    for group in range(8):
        selected=frames[group*4:group*4+4]
        stride_x=max(im.width for im in selected)+2
        stride_y=max(im.height for im in selected)+2
        p=page(stride_x*2,stride_y*2)
        for n in range(4):
            i=group*4+n
            # A transparent gutter also guards mirrored affine sampling at tile edges.
            put(f"{actor}_0_{i}",selected[n],p,(n%2)*stride_x+1,(n//2)*stride_y+1)
            names[f"{actor}_1_{i}"]=names[f"{actor}_0_{i}"]
portrait_page=page(128,128)
for i,portrait in enumerate(portrait_frames):put(f"portrait_{i}",portrait,portrait_page,i%2*64+3,i//2*64+3)
for actor in range(4):
    source=images["extras" if actor<2 else "extras-new"]
    frames,offsets=fighter_frames(source,4,4,128/(source.width/4))
    for group in range(2):
        selected=[((actor%2)*8+group*4+n,frames[(actor%2)*8+group*4+n]) for n in range(4)]
        packed,refs=pack_tiles(selected);start=len(pages);pages.extend(packed)
        for n in range(4):
            i=group*4+n
            ref=refs[(actor%2)*8+i].copy();ref[0]+=start;names[f"extra_{actor}_{i}"]=ref
            extras_offsets[actor,i]=offsets[(actor%2)*8+i]
# Eight alternating-foot frames, four per page (Host texture limit 256x256).
for actor,name in enumerate(actors):
    for group in range(2):
        frames=[cell(images["walk"],actor*8+i,8,4) for i in range(group*4,group*4+4)]
        boxes=[im.getchannel("A").point(lambda a:255 if a>=96 else 0).getbbox() for im in frames]
        mw=max(b[2]-b[0] for b in boxes);mh=max(b[3]-b[1] for b in boxes)
        p=page((mw+2)*2,(mh+2)*2)
        for n,(im,box) in enumerate(zip(frames,boxes)):
            i=group*4+n;put(f"walk_{actor}_{i}",im.crop(box),p,n%2*(mw+2)+1,n//2*(mh+2)+1)
            walk_offsets[actor,i]=[box[0],box[1]]
story_sizes=[]
for scene in range(3):
    source=images[f"story-{scene}"]
    factor=min(512/source.width,256/source.height)
    w,h=round(source.width*factor),round(source.height*factor)
    story=source.resize((w,h),N);story_sizes.append((w,h))
    assert 256<w<=512 and h<=256
    for part,(x,width) in enumerate(((0,256),(256,w-256))):
        put(f"story_{scene}_{part}",story.crop((x,0,x+width,h)),page(width,h),0,0)
assert len(pages)<=128
for stale in OUT.glob("atlas-*.index8"):stale.unlink()
entries=[]
for i,im in enumerate(pages):
    indexed=im.convert("RGB").quantize(palette=palette_image,dither=Image.Dither.NONE)
    data=bytes(0 if alpha<96 else min(254,color)+1
               for color,alpha in zip(indexed.tobytes(),im.getchannel("A").tobytes()))
    (OUT/f"atlas-{i}.index8").write_bytes(data)
    entries.append(dict(path=f"assets/atlas-{i}.pxr",kind="index8",
        source=f"resources/atlas-{i}.index8",width=im.width,height=im.height))
(ROOT/"resources.json").write_text(json.dumps(dict(assets=entries),indent=2)+"\n")
(ROOT/"art/layout.json").write_text(json.dumps(names,indent=2)+"\n")
def ref(name):return "{"+",".join(map(str,names[name]))+"}"
def glyph_ink(ch):
    m=font_metrics[ch];return '{'+','.join(str(m[k]) for k in ('left','top','width','height'))+'}'
text=["// Generated by tools/build_art.py. UI glyphs use Fusion Pixel Font, OFL-1.1; see assets/licenses.",
      "#pragma once","#include <cstdint>","#include <array>","namespace ash::art {",
      "struct Tile {uint8_t page;uint16_t x,y,w,h;};",
      f"inline constexpr unsigned pages={len(pages)}, static_pages={static_pages};",
      "inline constexpr unsigned page_bytes[]={"+",".join(str(im.width*im.height) for im in pages)+"};",
      "inline constexpr std::array<uint16_t,256> palette={"+",".join(map(str,rgb565))+"};",
      "inline constexpr Tile stages[4][4]={"+",".join("{"+",".join(ref(f'stage_{c}_{i}') for i in range(4))+"}" for c in range(4))+"};",
      "inline constexpr Tile stage_previews[]={"+",".join(ref(f'stage_preview_{i}') for i in range(4))+"};",
      "inline constexpr Tile dpad_arrows[]={"+",".join(ref(f'dpad_arrow_{i}') for i in range(8))+"};",
      "inline constexpr int stage_width=512,stage_height=304,stage_ground=256;",
      "inline constexpr Tile projectiles[4][4]={"+",".join("{"+",".join(ref(f'shot_{actor}_{i}') for i in range(4))+"}" for actor in range(4))+"};",
      "inline constexpr Tile fighters[4][2][32]={"]
for actor in actors:
    text.append("{"+",".join("{"+",".join(ref(f"{actor}_{flip}_{i}") for i in range(32))+"}" for flip in range(2))+"},")
text+=["};",
       "struct Ink {int16_t x,y,w,h;};",
       "struct LabelGlyph {uint16_t x,advance;Ink ink;};",
       "inline constexpr Ink glyph_ink[]={"+','.join(glyph_ink(ch) for ch in chars)+"};",
       "inline constexpr LabelGlyph label_glyphs[]={"+','.join('{'+str(x)+','+str(a)+',{'+','.join(map(str,(l,t,w,h)))+'}}' for x,a,l,t,w,h in label_glyphs)+"};",
       "struct ChineseLabel {const char* text;Tile tile;uint16_t first,count;};",
       "inline constexpr ChineseLabel chinese[]={"+",".join("{"+json.dumps(label,ensure_ascii=False)+","+ref(f"cn_{i}")+","+','.join(map(str,label_metrics[i]))+"}" for i,label in enumerate(labels))+"};",
       "inline constexpr Tile portraits[]={"+",".join(ref(f"portrait_{i}") for i in range(4))+"};",
       "inline constexpr unsigned story_width[3]={"+",".join(str(w) for w,h in story_sizes)+"};",
       "inline constexpr unsigned story_height[3]={"+",".join(str(h) for w,h in story_sizes)+"};",
       "inline constexpr Tile story[3][2]={"+",".join("{"+",".join(ref(f"story_{scene}_{part}") for part in range(2))+"}" for scene in range(3))+"};",
       "inline constexpr Tile extras[4][8]={"+",".join("{"+",".join(ref(f"extra_{actor}_{i}") for i in range(8))+"}" for actor in range(4))+"};",
       "inline constexpr Tile walk[4][8]={"+",".join("{"+",".join(ref(f"walk_{actor}_{i}") for i in range(8))+"}" for actor in range(4))+"};",
       "struct Offset {int16_t x,y;};",
       "inline constexpr Offset extras_offsets[4][8]={"+",".join("{"+",".join("{"+",".join(map(str,extras_offsets[actor,i]))+"}" for i in range(8))+"}" for actor in range(4))+"};",
       "inline constexpr Offset fighter_offsets[4][32]={"+",".join("{"+",".join("{"+",".join(map(str,o))+"}" for o in row)+"}" for row in fighter_offsets)+"};",
       "inline constexpr Offset walk_offsets[4][8]={"+",".join("{"+",".join("{"+",".join(map(str,walk_offsets[actor,i]))+"}" for i in range(8))+"}" for actor in range(4))+"};",
       "inline constexpr Tile glyphs[]={"+",".join(ref(f"glyph_{i}") for i in range(len(chars)))+"};",
       "inline constexpr Tile effects[2][12]={"+",".join("{"+",".join(ref(f"effect_{flip}_{i}") for i in range(12))+"}" for flip in range(2))+"};",
       "inline constexpr Tile logo="+ref("logo")+";",
       "namespace ui {",
       "inline constexpr Tile digits[]={"+",".join(ref(f"digit_{i}") for i in range(10))+"};",
       "inline constexpr Tile fills[]={"+",".join(ref(f"fill_{i}") for i in range(6))+"};"]
for name in ["button_idle","button_focus","health_left","health_right","modal","action_idle",
             "action_pressed","dpad","timer","bracket_left","bracket_right","ko","fight","round","win"]+[
             "icon_"+name for name in icon_names]:
    text.append("inline constexpr Tile "+name+"="+ref(name)+";")
text += ["}","}"]
(ROOT/"art.hpp").write_text("\n".join(text)+"\n")
icon=Image.new("RGBA",(128,128),(14,23,38,255))
small=logo.copy();small.thumbnail((120,96),N)
icon.alpha_composite(small,((128-small.width)//2,(128-small.height)//2))
icon.convert("RGB").save(ROOT/"assets/icon.png")
print(f"{len(pages)} streamed pages. Complete fighter silhouettes retain half-source pixel size and original foot positions.")

# Preserve the same pixels for the runtime fighter-bank cache.
from prepare_frame_cache import prepare
prepare()
