#!/usr/bin/env python3
"""Compile original RGBA characters and hand-authored tools to RGB565 horizontal spans, without runtime PNGs.

The source alpha, authored silhouette and nearest-neighbor grid are preserved.
Character animation uses individually authored directional contact/passing poses.
"""
from pathlib import Path
from PIL import Image, ImageDraw
import json
import hashlib
import colorsys

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'art/world-v2'
CHARACTER_PALETTE=None

def native(image,size):
    image=image.convert('RGBA')
    mask=image.getchannel('A').point(lambda a:255 if a>=160 else 0)
    bounds=mask.getbbox()
    assert bounds,'empty source sprite'
    return image.crop(bounds).resize(size,Image.Resampling.NEAREST)

def rgb565(r,g,b):return ((r&248)<<8)|((g&252)<<3)|(b>>3)

def grade_sprite(im,name,quantize=True):
    """Bound saturation and palette size offline; preserve the authored alpha."""
    foliage=name in ('oak','oak_summer') or name.startswith(('birch','pine','orchard','clover','daisies','herbs'))
    character=name.startswith(('south','north','east','west','villager'))
    pixels=im.load();graded=Image.new('RGBA',im.size);target=graded.load();opaque=[]
    for y in range(im.height):
        for x in range(im.width):
            r,g,b,a=pixels[x,y]
            if a<160:continue
            h,s,v=colorsys.rgb_to_hsv(r/255,g/255,b/255)
            if foliage and 45/360<=h<=155/360:
                h+=(96/360-h)*.55
                s*=.90;v=min(.94,v*.99+.006)
            else:
                s*=.97 if character else .93
                v=min(.98,v*.99+.004)
            rr,gg,bb=(round(c*255)for c in colorsys.hsv_to_rgb(h,s,v))
            # Warm title-illustration light: cream highlights, amber/olive
            # shadows. Preserve blue tools, denim and water as cool accents.
            light=(rr+gg+bb)/765
            rr=min(255,round(rr*1.025+2));gg=min(255,round(gg*1.005+1))
            bb=round(bb*(.92 if light>.55 else .96))
            target[x,y]=(rr,gg,bb,255);opaque.append((rr,gg,bb))
    if not quantize:return graded
    count=40 if name=='cottage' else 32 if character else 24
    samples=Image.new('RGB',(len(opaque),1));samples.putdata(opaque)
    palette=samples.quantize(colors=count,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    if character and CHARACTER_PALETTE is not None:palette=CHARACTER_PALETTE
    mapped=graded.convert('RGB').quantize(palette=palette,dither=Image.Dither.NONE).convert('RGB')
    mapped.putalpha(graded.getchannel('A'))
    return mapped

def contact_profile(im,name):
    """Compile local ground contact once, following the opaque base contour.

    Character contact comes only from the planted boot on row 29. A lifted
    boot must not drag a dark contact shadow into the air. A faint, shallow
    footprint stays centred beneath the body throughout the gait.
    """
    character=name.startswith(('south','north','east','west','villager'))
    if name.startswith('held_'):return (22,22),[]
    bases={
        'oak':(27,64,55), 'oak_summer':(27,64,55),
        'oak_fall':(27,64,55), 'oak_winter':(27,64,55),
        'cottage':(48,88,72), 'rocks':(11,18,12), 'grass':(9,20,17),
        'chest':(12,21,14), 'well':(16,33,27), 'board':(13,30,28),
        'sprinkler':(8,18,15),
    }
    base=(12,30,29) if character else bases.get(name)
    if base is None and name.startswith(('oak_','birch','pine','orchard','rock_','clover','daisies','herbs','tree_seed','tree_stump')):
        base=(im.width//2,im.height,max(0,im.height-(8 if im.height>=30 and name.startswith(('oak_','birch','pine','orchard')) else 3)))
    if base is None:return (0,0),[]
    ax,ay,first=base
    pixels=im.load();points={}
    def mark(x,y,alpha):
        # Opaque source pixels hide their own ground shadow. Removing these
        # offline avoids repeated work and prevents self-darkening.
        if 0<=x<im.width and 0<=y<im.height and pixels[x,y][3]>=160:return
        points[x,y]=max(alpha,points.get((x,y),0))
    if character:
        for y,left,width in [(28,8,8),(29,5,14),(30,8,8)]:
            for x in range(left,left+width):mark(x,y,1)
    for x in range(im.width):
        bottom=next((y for y in range(im.height-1,first-1,-1) if pixels[x,y][3]>=160),None)
        if bottom is None:continue
        mark(x,bottom+1,3 if character else 2)
        mark(x-1,bottom+1,1);mark(x+1,bottom+1,1)
        if not character:mark(x,bottom+2,1)
    spans=[]
    for y in sorted({y for x,y in points}):
        xs=sorted(x for x,yy in points if yy==y);at=0
        while at<len(xs):
            x=xs[at];alpha=points[x,y];end=x+1;at+=1
            while at<len(xs) and xs[at]==end and points[xs[at],y]==alpha:
                end+=1;at+=1
            assert -128<=x<128 and -128<=y<128
            spans.append((y,x,end-x,alpha))
    assert spans,'grounded sprite has no base'
    return (ax,ay),spans

def compile_rig(out,name,im,runs,colors,shape=None):
    direction=name if not name.startswith("villager_") else ("south","east","north","west")[int(name[-1])]
    # Bind the neutral standing artwork to four independent limbs. Parts
    # reference the original RGB565 pixel array; no PNGs or runtime masks.
    assert im.size==(24,32), (name,im.size)
    side=direction in ('east','west');parts={key:set() for key in ('body','arm0','arm1','leg0','leg1')}
    pixels=(shape or im).load();lookup={};cloth=[];shirt=[]
    for y,x,count,offset in runs:
        for i in range(count):
            xx=x+i;lookup[xx,y]=offset+i;r,g,b,a=pixels[xx,y]
            denim=b>r*1.08 and b>g*.95
            if 16<=y<=22:
                if denim:cloth.append(colors[offset+i])
                elif g>r*.85 and g>b*1.2:shirt.append(colors[offset+i])
            arm=None
            end=24 if side else 23
            if 16<=y<=end and not denim:
                if side:
                    if (direction=='east' and xx<=12) or (direction=='west' and xx>=12):arm=0
                elif xx<=8:arm=0
                elif xx>=15:arm=1
            if arm is not None:parts['arm'+str(arm)].add((xx,y))
            elif y>=23:
                parts['leg0' if side or xx<12 else 'leg1'].add((xx,y))
            else:parts['body'].add((xx,y))
    if side:parts['arm1']=parts['arm0'];parts['leg1']=parts['leg0']
    for part,points in parts.items():
        spans=[];rows=[0]
        for y in range(32):
            xx=0
            while xx<24:
                if (xx,y) not in points:xx+=1;continue
                first=xx;off=lookup[xx,y];xx+=1
                while xx<24 and (xx,y) in points and lookup[xx,y]==off+xx-first:xx+=1
                spans.append((y,first,xx-first,off))
            rows.append(len(spans))
        assert spans and len(spans)<256,(name,part)
        key=name+'_'+part
        out.write(f'inline constexpr Span {key}_spans[]={{'+','.join('{'+','.join(map(str,v))+'}' for v in spans)+'};\n')
        out.write(f'inline constexpr Sprite {key}={{24,32,{len(spans)},{key}_spans,{name}_pixels,12,30,nullptr}};\n')
        if part!='body':
            out.write(f'inline constexpr uint8_t {key}_rows[]={{'+','.join(map(str,rows))+'};\n')
            out.write(f'inline constexpr Bone {key}_bone={{&{key},{key}_rows}};\n')
    from collections import Counter
    cloth_color=Counter(cloth).most_common(1)[0][0]
    shirt_color=Counter(shirt).most_common(1)[0][0]
    ends=[max(y for x,y in parts['arm'+str(i)]) for i in range(2)]
    out.write(f'inline constexpr Rig {name}_rig={{&{name}_body,{{&{name}_arm0_bone,&{name}_arm1_bone}},{{&{name}_leg0_bone,&{name}_leg1_bone}},{{{ends[0]},{ends[1]}}},{cloth_color},{shirt_color},{str(side).lower()}}};\n')

def compile_sprite(out,name,im,shape=None):
    im=grade_sprite(im,name)
    w,h=im.size
    pixels=im.load();colors=[];runs=[]
    for y in range(h):
        x=0
        while x<w:
            if pixels[x,y][3]<160:x+=1;continue
            start=x;offset=len(colors)
            while x<w and pixels[x,y][3]>=160:
                colors.append(rgb565(*pixels[x,y][:3]));x+=1
            runs.append((y,start,x-start,offset))
    out.write(f'inline constexpr uint16_t {name}_pixels[]={{'+','.join(map(str,colors))+'};\n')
    out.write(f'inline constexpr Span {name}_spans[]={{'+','.join('{'+','.join(map(str,r))+'}'for r in runs)+'};\n')
    if name in ('south','east','north','west') or name.startswith('villager_'):compile_rig(out,name,im,runs,colors,shape)
    anchor,contact=contact_profile(im,name)
    shadow='nullptr'
    if contact:
        out.write(f'inline constexpr ShadowSpan {name}_shadow_spans[]={{'+','.join('{'+','.join(map(str,r))+'}'for r in contact)+'};\n')
        out.write(f'inline constexpr Shadow {name}_shadow={{{len(contact)},{name}_shadow_spans}};\n')
        shadow='&'+name+'_shadow'
    out.write(f'inline constexpr Sprite {name}={{{w},{h},{len(runs)},{name}_spans,{name}_pixels,{anchor[0]},{anchor[1]},{shadow}}};\n')
    return dict(width=w,height=h,opaque_pixels=len(colors),spans=len(runs),colors=len(set(colors)),ground_anchor=anchor,contact_spans=len(contact))

def character_poses(sheet):
    """Uniform scale per row; align the hair centre and planted boot baseline.

    The scale is shared by ALL views and poses. Never stretch width or resize
    each pose to its own rectangle: that produces a breathing/tall character.
    """
    cells={}
    for row,direction in enumerate(('south','east','north','west')):
        for col in range(5):
            cell=sheet.crop((col*sheet.width//5,row*sheet.height//4,
                             (col+1)*sheet.width//5,(row+1)*sheet.height//4))
            mask=cell.getchannel('A').point(lambda a:255 if a>=160 else 0)
            cells[direction,col]=(cell,mask.getbbox())
    front=cells['south',0][1]
    scale=28/(front[3]-front[1])
    images={}
    for (direction,col),(cell,bounds) in cells.items():
        x0,y0,x1,y1=bounds
        body=cell.crop(bounds)
        width,height=round(body.width*scale),round(body.height*scale)
        body=body.resize((width,height),Image.Resampling.NEAREST)
        # Arms may protrude on either side. Register the head, not the outer
        # silhouette, to keep the torso centred during the counter-swing.
        hair=cell.getchannel('A').crop((x0,y0,x1,y0+round((y1-y0)*.40)))
        hair_bounds=hair.point(lambda a:255 if a>=160 else 0).getbbox()
        centre=(hair_bounds[0]+hair_bounds[2])/2
        canvas=Image.new('RGBA',(24,32))
        canvas.paste(body,(12-round(centre*scale),30-height))
        name=direction+('' if col==0 else str(col-1))
        images[name]=canvas
    return images

def opposite_contacts(sheet):
    images={}
    for column,direction in enumerate(('east','north','west')):
        cell=sheet.crop((column*sheet.width//3,0,(column+1)*sheet.width//3,sheet.height))
        mask=cell.getchannel('A').point(lambda a:255 if a>=160 else 0)
        x0,y0,x1,y1=mask.getbbox();body=cell.crop((x0,y0,x1,y1))
        scale=28/(y1-y0)
        hair=mask.crop((x0,y0,x1,y0+round((y1-y0)*.4))).getbbox()
        centre=(hair[0]+hair[2])/2
        body=body.resize((round(body.width*scale),28),Image.Resampling.NEAREST)
        canvas=Image.new('RGBA',(24,32))
        canvas.paste(body,(12-round(centre*scale),2))
        images[direction+'_opposite']=canvas
    return images

def main():
    output={}
    trees={}
    with (ROOT/'sprites.hpp').open('w')as out:
        out.write('// Compiled original character art and authored tool pixels by tools/build_sprites.py.\n#pragma once\n#include "pixel_sprite.hpp"\nnamespace sprites {\n')
        for name,size in [('oak',(54,64)),('cottage',(96,88))]:
            im=native(Image.open(SOURCE/(name+'.png')),size)
            output[name]=compile_sprite(out,name,im)
            if name=='oak':
                trees[name]=im
                for season in ['summer','fall','winter']:
                    variant=im.copy();px=variant.load()
                    for y in range(im.height):
                        for x in range(im.width):
                            r,g,b,a=px[x,y]
                            if a<160 or g<r*.86 or g<b*1.1:continue
                            lum=(r*3+g*6+b)//10
                            if season=='summer':r,g,b=int(r*.86),min(255,int(g*.96)),min(255,int(b*1.15))
                            elif season=='fall':r,g,b=min(255,int(lum*1.25+28)),int(lum*.77+18),int(lum*.29+12)
                            else:r,g,b=min(255,int(lum*.8+65)),min(255,int(lum*.8+78)),min(255,int(lum*.75+76))
                            px[x,y]=(r,g,b,a)
                    output['oak_'+season]=compile_sprite(out,'oak_'+season,variant)
                    trees['oak_'+season]=variant
        global CHARACTER_PALETTE
        actors=character_poses(Image.open(SOURCE/'farmer-gba-v5.png').convert('RGBA'))
        actors.update(opposite_contacts(Image.open(SOURCE/'farmer-opposite-contact-v5.png').convert('RGBA')))
        # One palette across all twenty poses prevents colour flicker at
        # animation boundaries. All grading and palette work remains offline.
        samples=[]
        for name,im in actors.items():
            samples.extend(px[:3] for px in grade_sprite(im,name,False).getdata() if px[3]>=160)
        pixels=Image.new('RGB',(len(samples),1));pixels.putdata(samples)
        CHARACTER_PALETTE=pixels.quantize(colors=32,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
        for name,im in actors.items():output[name]=compile_sprite(out,name,im)
        villager=actors['south'].copy();px=villager.load()
        for yy in range(14,32):
            for xx in range(24):
                r,g,b,a=px[xx,yy]
                if b>r*1.08 and b>g*.95:px[xx,yy]=(min(255,int(b*1.2)),int(g*.75),int(b*.85),a)
        CHARACTER_PALETTE=None
        output['villager']=compile_sprite(out,'villager',villager)
        # Villagers share the farmer silhouette, direction registration and
        # limb split. Offline dyes retain luminance, outlines and skin shading.
        dyes=[(.78,1.08,.83),(1.05,.78,.58),(.72,.93,1.18),(1.02,.74,.90)]
        for row in range(4):
            for col,direction in enumerate(('south','east','north','west')):
                im=actors[direction].copy();px=im.load()
                for yy in range(32):
                    for xx in range(24):
                        rr,gg,bb,aa=px[xx,yy]
                        if aa<160:continue
                        if 14<=yy<=26 and bb>rr*.95 and bb>gg*.8:
                            lum=(rr+gg+bb)/3;d=dyes[row]
                            px[xx,yy]=(int(min(255,lum*d[0])),int(min(255,lum*d[1])),int(min(255,lum*d[2])),aa)
                        elif row==3 and yy<14 and rr>gg and rr<170:
                            lum=int((rr+gg+bb)/3+45);px[xx,yy]=(lum,lum,min(255,lum+4),aa)
                name=f'villager_{row}_{col}'
                output[name]=compile_sprite(out,name,im,actors[direction])
        atlas=Image.open(SOURCE/'props.png').convert('RGBA')
        props=[('hoe',(20,20)),('axe',(19,21)),('pick',(22,21)),('can',(23,21)),('seeds',(16,20)),('rod',(21,25)),('basket',(22,19)),('chest',(24,21)),('rocks',(23,18)),('grass',(18,20)),('well',(32,33)),('board',(27,30)),('logs',(20,17)),('stones',(20,16)),('sprinkler',(16,18)),('fish',(22,15))]
        for i,(name,size)in enumerate(props):
            col,row=i%4,i//4
            cell=atlas.crop((col*atlas.width//4,row*atlas.height//4,(col+1)*atlas.width//4,(row+1)*atlas.height//4))
            im=native(cell,size)
            output[name]=compile_sprite(out,name,im)
            if name=='seeds':output['seed_handbag']=compile_sprite(out,'seed_handbag',im.resize((9,12),Image.Resampling.NEAREST))
            if name in ('hoe','axe','pick','can','rod'):
                # All tools use the same real grip (22,22), with angle 0
                # pointing north. Runtime never guesses an atlas-specific axis.
                for grade in range(3 if name in ('axe','pick') else 1):
                    canvas=Image.new('RGBA',(44,44));d=ImageDraw.Draw(canvas)
                    ink=(65,54,39,255);wood=(159,108,58,255);light=(216,170,103,255)
                    metal=[(140,155,148,255),(181,121,72,255),(188,203,197,255)][grade]
                    shine=(223,226,196,255)
                    if name=='can':
                        d.rectangle((17,14,27,24),fill=ink);d.rectangle((18,15,26,23),fill=(115,148,145,255))
                        d.rectangle((20,11,24,16),fill=ink);d.rectangle((21,8,23,15),fill=metal)
                        d.rectangle((19,7,25,9),fill=ink);d.rectangle((20,7,24,8),fill=shine)
                        d.line((18,17,25,17),fill=shine);d.rectangle((21,20,23,23),fill=light)
                    else:
                        length=2 if name=='rod' else 7
                        d.line((22,23,22,length),fill=ink,width=3)
                        d.line((22,22,22,length+1),fill=wood,width=1)
                        d.point((22,18),fill=light)
                        if name=='hoe':
                            d.polygon([(16,5),(28,5),(28,9),(25,10),(16,8)],fill=ink)
                            d.rectangle((17,6,27,8),fill=metal);d.line((17,6,27,6),fill=shine)
                        elif name=='axe':
                            d.polygon([(21,4),(29,3),(32,7),(30,12),(22,10)],fill=ink)
                            d.polygon([(23,5),(29,4),(31,7),(29,10),(23,9)],fill=metal)
                            d.line((30,5,30,9),fill=shine)
                        elif name=='pick':
                            d.polygon([(13,10),(16,5),(23,4),(29,6),(32,10),(26,8),(19,8)],fill=ink)
                            d.line((15,8,18,6,25,6,30,8),fill=metal,width=2)
                            d.line((18,5,25,5),fill=shine)
                        else:
                            d.line((22,2,24,1,25,12),fill=shine,width=1)
                            d.rectangle((20,16,24,19),fill=ink);d.rectangle((21,17,23,18),fill=metal)
                    for angle in range(8):
                        pose=canvas.rotate(-45*angle,resample=Image.Resampling.NEAREST,center=(22.5,22.5))
                        key=f'held_{name}{grade}_{angle}'
                        output[key]=compile_sprite(out,key,pose)
        scenery=Image.open(SOURCE/'scenery-v1.png').convert('RGBA')
        bands=[(0,525),(550,850),(875,1190)]
        assets=[('birch',(40,64)),('pine',(44,64)),('orchard',(54,60)),
                ('rock_granite',(24,18)),('rock_slate',(23,22)),('rock_weathered',(24,17)),
                ('clover',(20,12)),('daisies',(20,22)),('herbs',(20,20))]
        for i,(name,size) in enumerate(assets):
            col,row=i%3,i//3
            y0,y1=bands[row]
            im=native(scenery.crop((col*scenery.width//3,y0,(col+1)*scenery.width//3,y1)),size)
            output[name]=compile_sprite(out,name,im)
            if name in ('birch','pine','orchard'):trees[name]=im
            if name in ('birch','orchard'):
                for season in ('summer','fall','winter'):
                    variant=im.copy();px=variant.load()
                    for y in range(im.height):
                        for x in range(im.width):
                            r,g,b,a=px[x,y]
                            if a<160 or g<r*.86 or g<b*1.1:continue
                            lum=(r*3+g*6+b)//10
                            if season=='summer':r,g,b=int(r*.86),min(255,int(g*.96)),min(255,int(b*1.15))
                            elif season=='fall':r,g,b=min(255,int(lum*1.25+28)),int(lum*.77+18),int(lum*.29+12)
                            else:r,g,b=min(255,int(lum*.8+65)),min(255,int(lum*.8+78)),min(255,int(lum*.75+76))
                            px[x,y]=(r,g,b,a)
                    output[name+'_'+season]=compile_sprite(out,name+'_'+season,variant)
                    trees[name+'_'+season]=variant
        for name,im in trees.items():
            young=im.resize((max(18,im.width*3//5),im.height*3//5),Image.Resampling.NEAREST)
            output[name+'_young']=compile_sprite(out,name+'_young',young)
        for kind in range(4):
            im=Image.new('RGBA',(12,14));d=ImageDraw.Draw(im)
            d.rectangle((5,6,6,13),fill=(109,77,42,255))
            d.rectangle((4,13,7,13),fill=(114,79,43,255))
            if kind==2:
                for yy,wide in [(2,1),(5,3),(8,4)]:
                    d.polygon([(5,yy),(5-wide,yy+3),(6+wide,yy+3)],fill=(75,112,46,255))
                    d.line((5,yy,5-wide+1,yy+2),fill=(136,160,71,255))
            else:
                d.line((5,9,2,7),fill=(81,113,44,255))
                d.line((6,7,9,5),fill=(81,113,44,255))
                d.polygon([(0,5),(3,5),(4,7),(2,8),(0,7)],fill=(100,137,50,255))
                d.polygon([(7,4),(10,3),(11,5),(9,6),(7,5)],fill=(119,150,59,255))
                d.point((1,5),fill=(160,179,80,255));d.point((9,3),fill=(168,185,89,255))
            output['tree_seed'+str(kind)]=compile_sprite(out,'tree_seed'+str(kind),im)
        im=Image.new('RGBA',(12,8));d=ImageDraw.Draw(im)
        d.rectangle((2,2,9,6),fill=(105,69,37,255));d.rectangle((1,6,10,7),fill=(102,70,38,255))
        d.rectangle((3,1,8,2),fill=(191,141,75,255));d.rectangle((2,2,9,3),fill=(181,125,65,255))
        d.rectangle((4,2,7,3),fill=(131,85,42,255));d.line((3,4,3,6),fill=(158,104,52,255))
        output['tree_stump']=compile_sprite(out,'tree_stump',im)
        out.write('}\n')
    provenance={"compiler":"tools/build_sprites.py","encoding":"RGB565 contiguous opaque spans; thresholded source alpha; nearest-grid sampling; title-matched amber/cream lighting and olive foliage; 24/32/40-color median-cut palette","walk_frames":8,"character_scaling":"28px standing height, same uniform scale across all 20 authored views and poses, head centre / boot baseline registration","gait":"neutral-art limb binding; eight distance-driven phases; both arms exactly opposite their matching legs; independent planted/swinging feet","sprites":output,"sources":{p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in SOURCE.glob('*.png')}}
    (SOURCE/'compiled.json').write_text(json.dumps(provenance,indent=2)+'\n')
    print(json.dumps(output,indent=2))

if __name__=='__main__':main()
