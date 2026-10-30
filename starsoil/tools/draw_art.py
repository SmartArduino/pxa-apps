#!/usr/bin/env python3
"""Original grid-authored sprites. Rebuilds the exact atlases used by the game.

Only Pillow is needed. No external sprite packs, downloaded textures or assets.
The generated cover is encoded separately; it is not used as gameplay imagery.
"""
from pathlib import Path
import math
import re
from PIL import Image, ImageDraw, ImageFont
import characters
import visuals

ROOT = Path(__file__).resolve().parents[1]
OUTLINE = '#202c38'

def shade(c, amount):
    if isinstance(c, str):
        c = tuple(bytes.fromhex(c.removeprefix('#')))
    return tuple(max(0, min(255, v + amount)) for v in c[:3])

def canvas(w=16, h=16):
    im = Image.new('RGBA', (w, h))
    return im, ImageDraw.Draw(im)

def noise(x, y, seed=1):
    n = (x * 374761393 + y * 668265263 + seed * 1274126177) & 0xffffffff
    n = ((n ^ (n >> 13)) * 1274126177) & 0xffffffff
    return (n ^ (n >> 16)) & 0xffffffff

COLORS = ['000000','966246','659854','777e8d','d6ad70','d9e5e5','80b8cc','a1714e','4d8c60','ac7b55','8d929c',
          'c78c68','abb8bc','dfc273','494657','81cfc4','c66a83','53485f','4c4963','6c8663','5682b6','dd774b',
          'b2855b','9f6e49','e4b568','a17b56','8e8791','92a4ad','b58a4e','d996b4','c8797e','88a265','6c9d62',
          '518365','a9a16d','aa8190','90c6bd']

def legacy_tile(t, mask):
    if t == 37:
        return legacy_tile(28, mask)
    im, d = canvas()
    if not t:
        return im
    col = COLORS[t]
    terrain = t in list(range(1, 7)) + list(range(9, 20))
    if terrain:
        base = '966246' if t == 2 else '777e8d' if 11 <= t <= 16 else col
        d.rectangle((0, 0, 15, 15), fill='#'+base)
        # Sparse hand-placed clusters, distributed by a repeatable pixel hash.
        for y in range(2, 15, 3):
            for x in range(1, 15, 4):
                n = noise(x, y, t)
                xx, yy = x + (n % 3), y + ((n >> 4) % 2)
                c = shade(base, -14 if n % 2 else 13)
                d.line((xx, yy, min(15, xx+1+n%2), yy), fill=c)
                if n % 3 == 0:
                    d.point((xx, yy+1), fill=shade(base, -21))
        if t == 9:
            for y in (4, 10):
                d.line((0, y, 15, y), fill=shade(base,-30))
                d.line((0,y+1,15,y+1),fill=shade(base,16))
            d.line((9,0,9,4),fill=shade(base,-24));d.line((3,5,3,10),fill=shade(base,-24))
        if t == 10:
            for y in (0, 7, 14):
                d.line((0,y,15,y), fill=shade(base,-30))
            d.line((8,0,8,7),fill=shade(base,-30));d.line((3,7,3,14),fill=shade(base,-30))
        if t == 6:
            d.line((1,9,9,1),fill=shade(col,39));d.line((7,14,14,7),fill=shade(col,24))
        if 11 <= t <= 16:
            for x,y in ((3,4),(9,2),(7,9),(12,12)):
                d.polygon([(x,y-1),(x+2,y),(x+1,y+3),(x-1,y+2)],fill=shade(col,-26))
                d.line((x,y,x+1,y+1),fill=shade(col,35))
                d.point((x+1,y+2),fill='#'+col)
        if not mask&1:
            d.line((0,0,15,0),fill=shade(base,31));d.line((0,1,15,1),fill=shade(base,9))
        if not mask&2:
            d.line((15,0,15,15),fill=shade(base,-38))
        if not mask&4:
            d.line((0,15,15,15),fill=shade(base,-47));d.line((0,14,15,14),fill=shade(base,-20))
        if not mask&8:
            d.line((0,0,0,15),fill=shade(base,18))
        if t in (2,19) and not mask&1:
            for x in range(16):
                h=2+noise(x,0,t)%3
                d.line((x,0,x,h),fill='#'+col)
                d.point((x,0),fill=shade(col,36))
            for x in (1,6,12):
                d.point((x,1),fill=shade(col,50))
        return im
    if t == 7:
        d.rectangle((5,0,11,15),fill='#78513e');d.rectangle((6,0,9,15),fill='#a67851')
        for x,y in ((6,2),(9,7),(6,11)):
            d.line((x,y,x,y+3),fill='#513e37');d.point((x+1,y),fill='#bf9565')
    elif t == 8:
        d.rounded_rectangle((0,0,15,15),radius=4,fill='#315e46')
        for x,y in ((1,2),(6,0),(10,3),(3,8),(9,10),(13,7)):
            d.rectangle((x,y,min(15,x+3),y+3),fill='#4f8c57');d.line((x,y,x+2,y),fill='#76ac65')
    elif t in (20,21):
        d.rectangle((0,2,15,15),fill='#'+col)
        d.line((0,2,15,2),fill=shade(col,60));d.line((1,5,6,5),fill=shade(col,23));d.line((9,10,15,10),fill=shade(col,18))
    elif t == 22:
        d.rectangle((0,0,15,3),fill='#73543e');d.line((0,0,15,0),fill='#d1a171')
        for x in (1,8,14):d.point((x,2),fill=OUTLINE)
    elif t == 23:
        d.rectangle((4,0,13,15),fill=OUTLINE);d.rectangle((5,1,12,15),fill='#a3744d')
        d.rectangle((7,2,10,10),outline='#d4a276');d.point((11,11),fill='#f4d181')
    elif t == 24:
        d.line((7,7,7,15),fill='#986c48');d.line((8,8,8,15),fill='#d0a26a')
        d.polygon([(7,0),(11,5),(10,8),(5,8),(4,5)],fill='#e9954c')
        d.polygon([(7,2),(9,6),(8,8),(6,7)],fill='#ffe8a5')
    elif t == 25:
        d.rectangle((0,7,15,9),fill=OUTLINE);d.rectangle((0,6,15,7),fill='#c39968')
        d.rectangle((2,10,4,15),fill='#7b5844');d.rectangle((12,10,14,15),fill='#7b5844')
        d.rectangle((9,4,12,5),fill='#a6bdc3');d.line((3,4,7,3),fill='#e0c894')
    elif t == 26:
        d.rounded_rectangle((0,1,15,15),radius=2,fill='#6b6571');d.rectangle((2,3,13,14),outline='#a09b9b')
        d.rectangle((4,8,11,14),fill='#382f3e');d.polygon([(5,13),(6,9),(8,12),(10,9),(11,13)],fill='#f3b35d')
        d.line((2,6,13,6),fill='#474759')
    elif t == 27:
        d.polygon([(0,4),(15,4),(13,7),(10,9),(10,12),(13,15),(2,15),(5,12),(5,8),(2,7)],fill=OUTLINE)
        d.line((0,4,15,4),fill='#d1dde0');d.rectangle((4,5,12,7),fill='#8e9fae');d.rectangle((6,9,9,12),fill='#6e7a89')
    elif t == 28:
        d.rounded_rectangle((1,4,14,15),radius=2,fill=OUTLINE);d.rectangle((2,5,13,14),fill='#a77b49')
        d.line((2,6,13,6),fill='#eac480');d.line((2,9,13,9),fill='#513e36')
        d.line((3,5,3,14),fill='#d3ae60');d.line((12,5,12,14),fill='#d3ae60');d.rectangle((7,8,8,11),fill='#f2d68a')
    elif t in (29,31):
        d.line((8,7,8,15),fill='#497c50');d.line((8,12,4,10),fill='#7ba565');d.line((8,10,12,8),fill='#7ba565')
        c='#e7a5be' if t==29 else '#ead08a'
        for x,y in ((6,4),(10,4),(8,2),(8,6)):d.rectangle((x-1,y-1,x+1,y+1),fill=c)
        d.point((8,4),fill='#f5e1a6')
    elif t == 30:
        d.rectangle((6,10,9,15),fill='#dec7ad');d.pieslice((1,3,14,14),180,360,fill='#b85e6d')
        d.line((2,9,13,9),fill='#de9297');d.point((5,6),fill='#f5ddbb');d.point((10,7),fill='#f5ddbb')
    elif t == 32:
        d.rectangle((5,0,10,15),fill='#4a7857');d.line((7,1,7,15),fill='#88ae6b')
        d.line((2,6,2,10),fill='#6b9b62');d.line((2,10,5,10),fill='#6b9b62');d.line((13,2,13,7),fill='#6b9b62');d.line((10,7,13,7),fill='#6b9b62')
        for y in (2,6,12):d.point((9,y),fill='#b0be84')
    elif t == 33:
        d.line((8,0,6,15),fill='#49764d')
        for y in (2,7,12):d.ellipse((3,y,8,y+3),fill='#729859');d.ellipse((8,y+2,12,y+5),fill='#547b4c')
    elif t == 34:
        d.line((7,9,7,15),fill='#95764c');d.ellipse((3,5,7,10),fill='#82a567');d.ellipse((8,3,12,8),fill='#62915c')
    elif t == 35:
        d.rectangle((0,9,15,14),fill='#7f5946');d.rectangle((1,7,14,11),fill='#b87885')
        d.rectangle((2,6,5,9),fill='#e0cbb3');d.line((1,11,14,11),fill='#e4a3a2');d.line((1,14,1,15),fill=OUTLINE);d.line((14,14,14,15),fill=OUTLINE)
    elif t == 36:
        d.polygon([(8,0),(13,6),(8,12),(3,6)],fill='#81caba');d.line((8,1,8,10),fill='#def2c5')
        d.rectangle((3,13,12,14),fill='#858fa1');d.rectangle((1,15,14,15),fill='#55546b')
    return im

def tile(t,mask):
    return visuals.tile(t,mask,legacy_tile)

def human(frame, part, pose=0):
    return characters.human(frame, part, pose)

def creature(kind,frame):
    return visuals.creature(kind,frame)

def icon(kind,col,t=0,name=''):
    return visuals.icon(kind,col,t,name,tile)

def rgb565(im):
    values=[]
    for r,g,b,a in im.convert('RGBA').getdata():
        v=((r&248)<<8)|((g&252)<<3)|(b>>3) if a else 0
        values.append(v if v or not a else 1)
    return values

def array(out, name, rows, width, ctype='uint16_t'):
    out.write(f'inline constexpr {ctype} {name}[{len(rows)}][{width}]={{\n')
    for row in rows:
        out.write('{'+','.join(str(n) for n in row)+'},\n')
    out.write('};\n')

def main():
    (ROOT/'art').mkdir(exist_ok=True)
    ts=[tile(t,m) for t in range(38) for m in range(16)]
    hs=[human(f,p,pose) for pose in range(characters.POSES) for p in range(characters.PARTS) for f in range(characters.FRAMES)]
    unique_humans=[];human_ids={};human_rows=[]
    for im in hs:
        pixels=im.tobytes()
        if pixels not in human_ids:
            human_ids[pixels]=len(unique_humans);unique_humans.append(list(im.getdata()))
        human_rows.append(human_ids[pixels])
    cs=[creature(t,f) for t in range(5) for f in range(6)]
    defs=re.findall(r'\{\s*"([^"]*)"\s*,\s*"[^"]*"\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*\d+\s*,\s*\d+\s*,\s*0x([0-9a-f]+)\s*\}',(ROOT/'items.hpp').read_text())
    if len(defs) != 80:
        raise ValueError(f'Expected 80 nonempty item definitions, got {len(defs)}')
    # Item zero has a decimal color literal; account for it explicitly.
    tile_names=['Air','Dirt','Grass','Stone','Sand','Snow','Ice','Log','Leaves','Plank','Brick','CopperOre','IronOre','GoldOre','CoalOre','CrystalOre','RubyOre','Obsidian','Deepstone','Moss','Water','Lava','Platform','Door','Torch','Bench','Furnace','Anvil','Chest','Flower','Mushroom','Herb','Cactus','Vine','Sapling','Bed','Portal','StorageChest']
    icons=[canvas()[0]]+[icon(k,c,tile_names.index(t) if t in tile_names else 0,name) for name,k,t,c in defs]
    source=Image.open(ROOT/'art/cover.png').convert('RGB')
    source.resize((128,128),Image.Resampling.NEAREST).save(ROOT/'assets/icon.png')
    cover=Image.open(ROOT/'art/cover.png').convert('RGB').resize((144,144),Image.Resampling.NEAREST)
    with (ROOT/'art.hpp').open('w') as out:
        out.write('// SPDX-License-Identifier: MIT\n// Generated by tools/draw_art.py; edit the grid authoring source.\n#pragma once\n#include <cstdint>\nnamespace art {\ninline constexpr int HumanWidth=20, HumanHeight=32, HumanParts=20, HumanFrames=10, HumanPoses=4;\ninline constexpr int WalkFrames=8, StandFrame=8, JumpFrame=9;\n')
        array(out,'body_bob',[list(characters.BOB)],characters.FRAMES,'int8_t')
        array(out,'tiles',[rgb565(im) for im in ts],256)
        array(out,'foliage',[rgb565(visuals.foliage(mask,snow)) for snow in (True,False) for mask in range(16)],256)
        array(out,'logs',[rgb565(visuals.tree_log(mask)) for mask in range(8)],256)
        array(out,'humans',unique_humans,characters.WIDTH*characters.HEIGHT,'uint8_t')
        array(out,'human_rows',[human_rows[i:i+characters.FRAMES] for i in range(0,len(human_rows),characters.FRAMES)],characters.FRAMES,'uint16_t')
        array(out,'mail',[list(characters.mail(f,style).getdata()) for style in range(1,4) for f in range(characters.FRAMES)],640,'uint8_t')
        array(out,'outfits',[list(characters.outfit(f,style).getdata()) for style in range(3) for f in range(characters.FRAMES)],640,'uint8_t')
        array(out,'helmets',[list(characters.helmet(f,style).getdata()) for style in range(1,4) for f in range(characters.FRAMES)],characters.WIDTH*characters.HEIGHT,'uint8_t')
        rigs=[characters.rig(f,pose) for pose in range(characters.POSES) for f in range(characters.FRAMES)]
        anchors=[]
        for r in rigs:
            far,near=characters.leg_joints(r['far']),characters.leg_joints(r['near'])
            points=[r['main_hand'],r['support_hand'],r['neck'],r['camera_shoulder'],r['rear_shoulder'],r['camera_elbow'],r['rear_elbow'],
                    far[3],near[3],r['pendant'],far[0],near[0],far[1],near[1],far[2],near[2]]
            anchors.append([value for point in points for value in point])
        array(out,'anchors',anchors,32,'int8_t')
        grip_points=[(8,10)]
        offhand_points=[(-1,-1)]
        for i,(name,kind,t,col) in enumerate(defs,1):
            candidate=(5,11) if kind in ('Pick','Axe','Wand') else (4,12) if kind=='Sword' else (14,8) if kind=='Bow' else (8,12) if kind in ('Potion','Food') else (8,10)
            opaque=[(x,y) for y in range(16) for x in range(16) if icons[i].getpixel((x,y))[3]]
            if not icons[i].getpixel(candidate)[3]:
                if kind in ('Pick','Axe','Sword','Bow','Wand'):
                    raise ValueError(f'{name}: weapon grip is outside the sprite')
                candidate=min(opaque,key=lambda point:abs(point[0]-candidate[0])+abs(point[1]-candidate[1]))
            grip_points.append(candidate)
            offhand_points.append((7,8) if kind=='Bow' else (-1,-1))
        array(out,'item_grips',grip_points,2,'int8_t')
        array(out,'item_offhands',offhand_points,2,'int8_t')
        array(out,'creatures',[rgb565(im) for im in cs],576)
        array(out,'icons',[rgb565(im) for im in icons],256)
        array(out,'cover',[rgb565(cover)],144*144)
        out.write('}\n')
    atlas=Image.new('RGBA',(16*16,38*16))
    for i,im in enumerate(ts):atlas.paste(im,((i%16)*16,(i//16)*16))
    atlas.save(ROOT/'art/tiles.png')
    atlas=Image.new('RGBA',(characters.FRAMES*characters.WIDTH,characters.POSES*characters.PARTS*characters.HEIGHT))
    palette=[(0,0,0,0),(42,37,42,255),(247,172,125,255),(206,127,92,255),(79,48,43,255),(152,91,57,255),(43,64,77,255),(85,126,152,255),(142,163,169,255),(52,59,76,255),(89,106,126,255),(89,62,50,255),(255,208,155,255),(155,96,57,255),(245,230,194,255),(193,132,69,255)]
    garment_palette=[(0,0,0,0),(42,37,42,255),(45,66,80,255),(85,126,152,255),(147,167,174,255),(229,189,115,255),(112,145,163,255),(99,160,157,255),(238,227,201,255),(101,67,49,255),(180,124,75,255)]
    for i,im in enumerate(hs):
        colors=palette if (i//characters.FRAMES)%characters.PARTS in (0,9,10,13,16,17) else garment_palette
        colored=Image.new('RGBA',im.size);colored.putdata([colors[v] for v in im.getdata()]);atlas.paste(colored,((i%characters.FRAMES)*characters.WIDTH,(i//characters.FRAMES)*characters.HEIGHT))
    atlas.save(ROOT/'art/character-layers.png')
    atlas=Image.new('RGBA',(6*24,5*24))
    for i,im in enumerate(cs):atlas.paste(im,((i%6)*24,(i//6)*24))
    atlas.save(ROOT/'art/creatures.png')
    atlas=Image.new('RGBA',(16*16,math.ceil(len(icons)/16)*16))
    for i,im in enumerate(icons):atlas.paste(im,((i%16)*16,(i//16)*16))
    atlas.save(ROOT/'art/items.png')
    print(f'{len(ts)} connected tiles; {len(hs)} character layers; {len(cs)} creature frames; {len(icons)} item icons')

if __name__=='__main__':main()
