#!/usr/bin/env python3
"""Export generated campaign artwork, preserving frame registration and alpha."""
from pathlib import Path
from PIL import Image, ImageOps
import hashlib
import json
import math
import subprocess
import sys
from import_ui import cells, trim

ROOT = Path(__file__).resolve().parents[1]
ART, ASSETS = ROOT / 'art', ROOT / 'assets'
LABELS = ['theme0','theme1','theme2','theme3','theme4','theme5',
          'dart_name','tesla_name','bomb_name','mission0','mission1','mission2',
          'mission3','mission4','mission5','mission6','unlock','pageprev','pagenext',
          'collection','objective_treasure','objective_guard','objective_time','treasure_left']
REGIONS = []


def compact(image):
    image = image.convert('RGBA')
    image.putalpha(image.getchannel('A').point(lambda a: 0 if a < 8 else 255 if a > 247 else a))
    return image.quantize(colors=256, method=Image.Quantize.FASTOCTREE,
                          dither=Image.Dither.NONE)


def pack(name, items):
    width = sum(im.width + 4 for _, im in items)
    height = max(im.height for _, im in items) + 4
    sheet = Image.new('RGBA', (width, height))
    x = 2
    for identifier, im in items:
        sheet.alpha_composite(im, (x, 2))
        REGIONS.append((identifier, name, x, 2, im.width, im.height, width, height))
        x += im.width + 4
        (ASSETS / f'{identifier}.png').unlink(missing_ok=True)
    compact(sheet).save(ASSETS / 'ui' / f'{name}.png', optimize=True)


def adaptive_cells(path, columns, rows):
    atlas = Image.open(path).convert('RGBA')
    alpha = atlas.getchannel('A').point(lambda a: 255 if a > 64 else 0)
    counts = [alpha.crop((0, y, atlas.width, y + 1)).getbbox() is not None for y in range(atlas.height)]
    cuts = [0]
    for row in range(1, rows):
        ideal = atlas.height * row / rows
        radius = int(atlas.height / rows * .18)
        cut = min(range(round(ideal)-radius,round(ideal)+radius+1),
                  key=lambda y: (counts[y], abs(y-ideal)))
        cuts.append(cut)
    cuts.append(atlas.height)
    for top,bottom in zip(cuts,cuts[1:]):
        for column in range(columns):
            yield atlas.crop((round(column*atlas.width/columns),top,
                              round((column+1)*atlas.width/columns),bottom))


def animation(source, names, extent):
    frames = list(adaptive_cells(ART / source, 4, len(names)))
    for row, name in enumerate(names):
        poses = frames[row * 4:row * 4 + 4]
        boxes = [p.getchannel('A').point(lambda a: 255 if a > 64 else 0).getbbox() for p in poses]
        assert all(boxes), (source, name)
        # Use one crop and one scale for the entire generated animation row.
        box = (min(b[0] for b in boxes), min(b[1] for b in boxes),
               max(b[2] for b in boxes), max(b[3] for b in boxes))
        sheet = Image.new('RGBA', (extent * 4, extent))
        for index, pose in enumerate(poses):
            pose = pose.crop(box)
            pose.thumbnail((extent - 8, extent - 8), Image.Resampling.LANCZOS)
            sheet.alpha_composite(pose, (index * extent + (extent - pose.width) // 2,
                                        (extent - pose.height) // 2))
        compact(sheet).save(ASSETS / f'{name}.png', optimize=True)


def export():
    REGIONS.clear()
    subprocess.run([sys.executable,str(ROOT/'tools/import_art.py')],check=True)
    for index, backdrop in enumerate(cells(ART / 'worlds-v1.png', 3, 2)):
        for native in (False, True):
            size = (799, 479) if native else (480, 480)
            image = ImageOps.fit(backdrop.convert('RGB'), size, Image.Resampling.LANCZOS)
            name = f'world{index}{"-korvo" if native else ""}'
            image.quantize(colors=96,method=Image.Quantize.MEDIANCUT,
                           dither=Image.Dither.NONE).save(ASSETS / f'{name}.png',optimize=True)
            (ASSETS / f'{name}.jpg').unlink(missing_ok=True)
    animation('enemies-v1.png', ['bat','snail','spiky','bee','snowpuff','imp'], 72)
    animation('bosses-v1.png', ['moth','crabking','scorpion','yeti','candyking'], 96)
    props = list(cells(ART / 'props-v1.png', 2, 6))
    for row in range(6):
        items=[]
        for col in range(2):
            prop=trim(props[row*2+col]);prop.thumbnail((64,64),Image.Resampling.LANCZOS)
            items.append((f'prop{row*2+col}',prop))
        pack(f'p{row}',items)
    atlas = Image.open(ART / 'turrets-expansion-v1.png').convert('RGBA')
    def cell(c,r):
        return atlas.crop((round(c*atlas.width/4),round(r*atlas.height/4),
                           round((c+1)*atlas.width/4),round((r+1)*atlas.height/4)))
    for row, icon in enumerate(('dart','tesla','bomb')):
        kind = row + 4
        base = trim(cell(row,3))
        base.thumbnail((64,64),Image.Resampling.LANCZOS)
        stationary = Image.new('RGBA',(64,64))
        stationary.alpha_composite(base,((64-base.width)//2,(64-base.height)//2))
        compact(stationary).save(ASSETS / f'base{kind}.png',optimize=True)
        extent=math.ceil(max(atlas.width/4,atlas.height/4)*1.5)
        stages=[]
        for key in range(4):
            head=cell(key,row)
            stage=Image.new('RGBA',(extent,extent))
            stage.alpha_composite(head,(round(extent/2-head.width/2),round(extent/2-head.height*.65)))
            stages.append(stage)
        for group in range(8):
            strip=Image.new('RGBA',(172,172))
            for direction in range(4):
                for key,stage in enumerate(stages):
                    rotated=stage.rotate(-(group*4+direction)*360/32,Image.Resampling.BICUBIC)
                    strip.alpha_composite(rotated.resize((43,43),Image.Resampling.LANCZOS),(direction*43,key*43))
            compact(strip).save(ASSETS / f'h{kind}-{group:02}.png',optimize=True)
        image=Image.new('RGBA',(96,96))
        image.alpha_composite(stationary.resize((48,48)),(24,24))
        image.alpha_composite(stages[0].resize((96,96),Image.Resampling.LANCZOS))
        compact(image).save(ASSETS / f'{icon}.png',optimize=True)
    # Shared bodies and icons avoid filesystem metadata for dozens of tiny
    # files. Every generated upgrade remains a distinct, selectable region.
    upgrades=list(cells(ART/'upgrade-bases-v1.png',2,6))
    bodies=[]
    for kind in range(1,7):
        bodies.append((f'body{kind}-1',Image.open(ASSETS/f'base{kind}.png').convert('RGBA')))
        for level in (2,3):
            body=trim(upgrades[(kind-1)*2+level-2]);body.thumbnail((64,64),Image.Resampling.LANCZOS)
            tile=Image.new('RGBA',(64,64));tile.alpha_composite(body,((64-body.width)//2,(64-body.height)//2))
            bodies.append((f'body{kind}-{level}',tile))
        (ASSETS/f'base{kind}.png').unlink()
    pack('bodies',bodies)
    pack('icons',[(name,Image.open(ASSETS/f'{name}.png').convert('RGBA')) for name in ('pea','ice','sun','dart','tesla','bomb')])
    pack('digits',[(f'n{i}',Image.open(ASSETS/f'n{i}.png').convert('RGBA')) for i in range(16)])
    # Generated control glyphs share a palette and do not need individual files.
    old_labels=[]
    import re
    layout=(ROOT/'art_layout.hpp').read_text()
    for name in re.findall(r'\{"([^\"]+)",\d+,\d+\}',layout):
        if name in ('title','meadow_name','mushroom','pond') or not (ASSETS/f'{name}.png').exists():continue
        old_labels.append((name,Image.open(ASSETS/f'{name}.png').convert('RGBA')))
    for start in range(0,len(old_labels),3):pack(f'c{start//3}',old_labels[start:start+3])
    pack('walk',[(f'walk{i}',Image.open(ASSETS/f'walk{i}.png').convert('RGBA')) for i in range(4)])
    pack('creatures',[(name,Image.open(ASSETS/f'{name}.png').convert('RGBA')) for name in ('rabbit','beetle','boss')])
    pack('stars',[(name,Image.open(ASSETS/f'{name}.png').convert('RGBA')) for name in ('star_on','star_off')])
    header=['// Generated by tools/import_expansion.py.','#pragma once',
            'inline constexpr ArtLabel expansionLabels[]={']
    texts=[]
    for name,image in zip(LABELS,cells(ART/'expansion-labels-v1.png',4,6)):
        image=trim(image);image.thumbnail((360,40),Image.Resampling.LANCZOS)
        texts.append((name,image));header.append('{"%s",%d,%d},'%(name,*image.size))
    for row in range(6):
        pack(f'e{row}',texts[row*4:row*4+4])
    if (ART/'expansion-help-v1.png').exists():
        names=[f'tower_hint{i}' for i in range(1,7)]+[f'boss_name{i}' for i in range(6)]
        texts=[]
        for name,image in zip(names,cells(ART/'expansion-help-v1.png',3,4)):
            image=trim(image);image.thumbnail((360,40),Image.Resampling.LANCZOS)
            texts.append((name,image));header.append('{"%s",%d,%d},'%(name,*image.size))
        for row in range(4):pack(f'd{row}',texts[row*3:row*3+3])
    if (ART/'progress-bars-v1.png').exists():
        bars=[]
        for name,image in zip(('wave_track','wave_fill','hp_track','hp_fill'),adaptive_cells(ART/'progress-bars-v1.png',1,4)):
            image=trim(image);image.thumbnail((256,32),Image.Resampling.LANCZOS)
            bars.append((name,image))
        pack('bars',bars)
    header += ['};','struct ArtRegion { const char *id,*sheet; int x,y,w,h,aw,ah; };',
               'inline constexpr ArtRegion artRegions[]={']
    for name,sheet,x,y,w,h,aw,ah in sorted(REGIONS):
        header.append('{"%s","%s",%d,%d,%d,%d,%d,%d},'%(name,sheet,x,y,w,h,aw,ah))
    header += ['};']
    (ROOT/'expansion_art.hpp').write_text('\n'.join(header)+'\n')
    sources={name:hashlib.sha256((ART/name).read_bytes()).hexdigest() for name in
             ('worlds-v1.png','enemies-v1.png','bosses-v1.png','turrets-expansion-v1.png','props-v1.png','expansion-labels-v1.png','expansion-help-v1.png','progress-bars-v1.png','upgrade-bases-v1.png')}
    (ART/'expansion-manifest.json').write_text(json.dumps({'tool':'built-in image_gen',
        'sources':sources,'animation_frames':4,'turret_directions':32,
        'head_size':43,
        'regions':{name:{'path':f'assets/ui/{sheet}.png','rect':[x,y,w,h]}
                   for name,sheet,x,y,w,h,aw,ah in REGIONS},
        'processing':'Slice generated poses with shared crops and pivots; rotate turret heads and resize once; alpha palettes, 96-color backgrounds and small UI atlases'},indent=2)+'\n')
    # Keep generated originals in art; remove obsolete runtime duplicates.
    for name in ('meadow','meadow-korvo','meadow_name','mushroom','pond'):
        (ASSETS / f'{name}.png').unlink(missing_ok=True)
    for path in ASSETS.rglob('*.png'):
        image=Image.open(path)
        if image.mode == 'RGBA':
            compact(image).save(path,optimize=True)


if __name__ == '__main__':
    export()
