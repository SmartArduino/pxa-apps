#!/usr/bin/env python3
"""Author native 32x28 animal animation; compile shared RGB565 scanlines.

The concept board guides anatomy/palette only. All runtime pixels below are
authored on the native grid, without sampling or tracing the generated image.
No image decoding, allocation, rotation or palette expansion occurs at runtime.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageOps
import json

ROOT = Path(__file__).resolve().parents[1]
INK = '#594b3b'
EDGE = '#817059'
IVORY = '#f1e1b8'
LIGHT = '#fff0ce'
SHADE = '#c5ae81'
DEEP = '#a08c69'
HOOF = '#655647'
AMBER = '#c9954c'
BEAK = '#e4b657'
RUST = '#b96d4e'
SAGE = '#9ca882'
ROSE = '#dca58e'
PINK = '#e8b299'
PINK_LIGHT = '#f3c6a9'
PINK_DARK = '#bd8574'
POSES = 15  # idle, 8 walk poses, 6 lower/peck/chew poses


def animal(kind, stage, frame):
    im = Image.new('RGBA', (32, 28))
    d = ImageDraw.Draw(im)
    walking = 1 <= frame <= 8
    step = frame - 1 if walking else 0
    eating = frame >= 9
    eat = frame - 9 if eating else 0
    lower = [0, 2, 4, 5, 4, 2][eat] if eating else 0
    stride = [-2, -1, 0, 1, 2, 1, 0, -1]
    lift = [0, 0, 0, 1, 1, 1, 0, 0]
    bob = -int(walking and step in (2, 6))

    def box(bounds, color):
        if bounds[2] >= bounds[0] and bounds[3] >= bounds[1]:
            d.rectangle(bounds, fill=color)

    def oval(bounds, color, outline=None):
        d.ellipse(bounds, fill=color, outline=outline)

    def poly(points, color, outline=None):
        d.polygon(points, fill=color, outline=outline)

    def line(points, color):
        d.line(points, fill=color)

    if kind < 2:
        # Visible back/wing above the near-side breast, not a flat side profile.
        w, h = [(9, 10), (13, 14), (17, 18)][stage]
        l, r = (32-w)//2-1, (32-w)//2+w-2
        top = 26-h+bob
        foot = AMBER
        for side, x in enumerate((l+w//3, r-3)):
            phase = (step+side*4) % 8
            dx = (stride[phase]//2) if walking else 0
            up = lift[phase] if walking else 0
            box((x+dx, 23-up, x+dx, 25-up), foot)
            line([(x+dx-1, 26-up), (x+dx+2, 26-up)], INK)
            line([(x+dx, 25-up), (x+dx+2, 25-up)], BEAK)
        down = '#eac76d' if stage == 0 else IVORY
        glow = '#ffe3a0' if stage == 0 else LIGHT
        shadow = '#c49e55' if stage == 0 else SHADE
        tail_y = top+5+(int(walking and step >= 4))
        poly([(l+3, tail_y+4), (l-2, tail_y), (l-1, tail_y-3),
              (l+1, tail_y-2), (l+3, tail_y), (l+5, tail_y+1)], INK)
        poly([(l+2, tail_y+3), (l-1, tail_y), (l, tail_y-2),
              (l+3, tail_y+1)], shadow if kind == 0 else IVORY)
        line([(l, tail_y-1), (l+2, tail_y+1)], glow)
        oval((l, top+4, r, 24+bob), INK)
        oval((l+1, top+5, r-1, 23+bob), down)
        d.arc((l+1, top+5, r-1, 23+bob), 25, 155, fill=shadow)
        line([(l+3, top+5), (l+6, top+5)], glow)
        # Three stepped feather clusters, or a soft sage duck wing.
        if stage:
            wing = shadow if kind == 0 else SAGE
            poly([(l+3, top+7), (l+7, top+7), (l+9, top+9),
                  (l+8, top+12), (l+6, top+14), (l+3, top+12),
                  (l+2, top+9)], wing)
            line([(l+3, top+8), (l+6, top+8), (l+7, top+9)], glow if kind == 0 else '#c6cba3')
            if kind == 0:
                for x, y in [(l+3, top+11), (l+5, top+13), (l+8, top+11)]:
                    line([(x, y), (x+1, y)], DEEP)
        # A lowered neck overlaps the breast while the rump and soles stay put.
        hx = r-3+(int(lower >= 4))
        head_w = 5 if stage == 0 else 7
        head_h = 6 if stage == 0 else 8
        hy = min(top+int(kind == 1)+lower, 26-head_h)
        if lower:
            poly([(r-5, top+7), (hx+2, hy+2), (hx+1, hy+head_h),
                  (r-6, min(top+12, 23+bob))], shadow)
        oval((hx-2, hy, hx+head_w-3, hy+head_h), INK)
        oval((hx-1, hy+1, hx+head_w-4, hy+head_h-1), glow)
        line([(hx-1, hy+head_h-1), (hx+1, hy+head_h-1)], down)
        eye_x, eye_y = hx+head_w-5, hy+3
        d.point((eye_x, eye_y), fill=INK)
        if stage == 2:
            d.point((eye_x-1, eye_y+1), fill=shadow)
        bx, by = hx+head_w-3, hy+head_h-3
        if kind == 1:
            poly([(bx, by-1), (bx+3, by), (bx+3, by+2), (bx, by+2)], INK)
            box((bx, by, bx+2, by+1), BEAK)
            d.point((bx+2, by+1), fill=AMBER)
        else:
            poly([(bx, by-1), (bx+2, by), (bx+1, by+1), (bx, by+1)], AMBER)
            d.point((bx, by-1), fill=BEAK)
            if stage:
                line([(hx, hy-1), (hx, hy-2), (hx+1, hy-2),
                      (hx+2, hy-1), (hx+3, hy-1)], RUST)
                box((bx-1, by+2, bx, by+3), RUST)
        if eating and eat in (3, 4):
            d.point((min(31, bx+2), min(26, by+2)), fill=BEAK)
    else:
        w, h = [(15, 13), (22, 19), (28, 23)][stage]
        l, r = (32-w)//2, (32-w)//2+w-1
        top = 26-h+bob
        cream = PINK if kind == 4 else IVORY
        light = PINK_LIGHT if kind == 4 else LIGHT
        shade = PINK_DARK if kind == 4 else SHADE
        # Far feet end two pixels above near feet: a visible overhead ground plane.
        # Opposite diagonal pairs move together; a support foot remains planted.
        for near in (False, True):
            for front in (False, True):
                x = (r-8 if front else l+4)+(1 if near else 0)
                phase = (step+(0 if near == front else 4)) % 8
                dx = stride[phase] if walking else 0
                up = lift[phase] if walking else 0
                sole = (26 if near else 24)-up
                box((x+dx, 19, x+dx+2, sole), INK)
                box((x+dx, 20, x+dx+1, sole-2), cream if near else shade)
                box((x+dx, sole-1, x+dx+2, sole), HOOF)
                d.point((x+dx, sole-2), fill=shade)
        # Thin tail and tuft/curl live behind the body.
        tail = int(walking and step >= 4)
        if kind == 4:
            line([(l+2, top+7), (l-1, top+6), (l-1, top+3+tail),
                  (l+1, top+2+tail), (l+2, top+4+tail), (l, top+4+tail)], INK)
            d.point((l, top+3+tail), fill=light)
        else:
            tail_end = min(top+13, 24)
            line([(l+2, top+6), (l, min(top+10, 23)), (l-1+tail, tail_end)], EDGE)
            box((l-2+tail, tail_end-1, l+tail, tail_end+1), INK)
            d.point((l-1+tail, tail_end-1), fill=DEEP)
        # Stepped, asymmetric croup/shoulders; broad lit back, shaded lower belly.
        crown_l, crown_r = l+max(4, w//4), r-max(6, w//3)
        poly([(l+1, top+6), (l+3, top+3), (crown_l, top+1),
              (crown_r, top+1), (r-6, top+3), (r-3, top+7),
              (r-3, 20+bob), (r-7, 23+bob), (l+5, 23+bob),
              (l+1, 20+bob)], INK)
        poly([(l+2, top+6), (l+4, top+3), (crown_l, top+2),
              (crown_r, top+2), (r-7, top+4), (r-4, top+7),
              (r-4, 20+bob), (r-7, 22+bob), (l+5, 22+bob),
              (l+2, 20+bob)], cream)
        line([(l+4, 21+bob), (l+7, 22+bob), (r-8, 22+bob), (r-5, 20+bob)], shade)
        line([(l+3, top+6), (crown_l, top+3), (crown_r, top+3)], light)
        if kind == 4 and stage:
            poly([(l+4, top+8), (l+6, top+5), (l+10, top+4),
                  (l+12, top+5), (l+8, top+6), (l+6, top+9)], light)
            line([(l+3, top+12), (l+4, top+15), (l+6, top+16)], shade)
        if kind == 2:
            # Separate dark patches follow the dorsal plane and shoulder.
            patch = '#716452'
            poly([(l+7, top+2), (l+11, top+2), (l+12, top+5),
                  (l+10, top+7), (l+6, top+6)], patch)
            if stage == 0:
                poly([(l+2, top+7), (l+4, top+6), (l+6, top+8),
                      (l+5, top+10), (l+3, top+10)], patch)
            else:
                poly([(l+2, top+9), (l+5, top+8), (l+7, top+10),
                      (l+6, top+14), (l+3, top+13)], patch)
            if stage:
                poly([(r-11, top+10), (r-7, top+9), (r-5, top+12),
                      (r-8, top+16), (r-11, top+15)], patch)
            if stage == 2:
                box((r-13, 23, r-10, 24), ROSE)
        if kind == 3:
            # Scalloped wool clusters with restrained dark crease pixels.
            for cx, cy in [(l+5, top+4), (l+10, top+2),
                           (r-11, top+4), (r-7, top+7),
                           (l+3, top+10), (l+7, top+14), (r-11, top+14)]:
                if cy > 21 or cx < l+2:
                    continue
                oval((cx-3, cy-2, cx+3, cy+4), EDGE)
                oval((cx-2, cy-2, cx+2, cy+3), IVORY)
                line([(cx-1, cy-1), (cx+1, cy-1)], LIGHT)
                line([(cx, cy+3), (cx+2, cy+2)], SHADE)
        # Face projects toward the near-right corner; nose is below both eyes.
        hx = r-7+(int(lower >= 4))
        hy = top+(2 if stage == 0 else 6 if kind == 4 else 5)+lower
        hh = [6, 8, 10][stage]
        # Adult nose at lowered pose stays above the sole baseline.
        hy = min(hy, 25-hh)
        face = DEEP if kind == 3 else cream
        face_light = SHADE if kind == 3 else light
        oval((hx-2, hy, r-1, hy+hh), INK)
        oval((hx-1, hy+1, r-2, hy+hh-1), face)
        line([(hx, hy+2), (hx+2, hy+1), (hx+3, hy+2)], face_light)
        # Ears attach at the forehead, and move with the head during feeding.
        poly([(hx, hy+1), (hx-4, hy-1), (hx-4, hy+2), (hx-1, hy+4)], INK)
        line([(hx-3, hy+1), (hx-1, hy+2)], PINK_DARK if kind == 4 else DEEP)
        poly([(r-4, hy+1), (r, hy-1), (r+1, hy+1), (r-2, hy+3)], INK)
        line([(r-2, hy+1), (r-1, hy)], shade)
        if kind == 2 and stage:
            line([(hx, hy), (hx-1, hy-3), (hx-2, hy-3)], EDGE)
            d.point((hx-1, hy-2), fill=BEAK)
            line([(r-4, hy), (r-4, hy-3)], EDGE)
            d.point((r-4, hy-2), fill=IVORY)
            poly([(hx, hy+2), (hx+2, hy+2), (hx+3, hy+5), (hx+1, hy+6)], '#716452')
        if kind == 3:
            oval((hx-2, hy-2, r-3, hy+2), EDGE)
            oval((hx-1, hy-2, r-4, hy+1), LIGHT)
        ey = hy+(3 if stage == 0 else 4)
        d.point((hx, ey), fill=INK)
        d.point((r-3, ey+1), fill=INK)
        if kind in (2, 4):
            nose_y = hy+hh-3+(int(eating and eat in (3, 5)))
            oval((r-6, nose_y, r+1, nose_y+3), INK)
            box((r-5, nose_y, r-1, nose_y+2), ROSE if kind == 2 else PINK_LIGHT)
            d.point((r-4, nose_y+1), fill=PINK_DARK)
            d.point((r-1, nose_y+1), fill=RUST)
        else:
            line([(r-5, hy+hh-2), (r-3, hy+hh-2)], INK)
            if eating and eat in (3, 5):
                d.point((r-4, hy+hh-1), fill=IVORY)
    return im


def main():
    # Intern both opaque color sequences and complete scanlines across poses.
    # A frame is 28 row indices; drawing still copies native RGB565 spans.
    colors, color_map, runs, rows, row_map, frames, frame_map = [], {}, [], [(0, 0)], {(): 0}, [], {}
    bank = []
    sheet = Image.new('RGBA', (POSES*32, 5*3*28))
    for kind in range(5):
        ages = []
        for stage in range(3):
            headings = []
            for heading in range(2):
                poses = []
                for pose in range(POSES):
                    im = animal(kind, stage, pose)
                    if heading:
                        im = ImageOps.mirror(im)
                    else:
                        sheet.paste(im, (pose*32, (kind*3+stage)*28))
                    row_ids = []
                    for y in range(28):
                        segments, x = [], 0
                        while x < 32:
                            if not im.getpixel((x, y))[3]:
                                x += 1
                                continue
                            start, pixels = x, []
                            while x < 32 and im.getpixel((x, y))[3]:
                                r, g, b, _ = im.getpixel((x, y))
                                pixels.append(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3))
                                x += 1
                            key = tuple(pixels)
                            if key not in color_map:
                                color_map[key] = len(colors)
                                colors.extend(key)
                            segments.append((start, len(key), color_map[key]))
                        key = tuple(segments)
                        if key not in row_map:
                            row_map[key] = len(rows)
                            rows.append((len(runs), len(segments)))
                            runs.extend(segments)
                        row_ids.append(row_map[key])
                    key = tuple(row_ids)
                    if key not in frame_map:
                        frame_map[key] = len(frames)
                        frames.append(key)
                    poses.append(frame_map[key])
                headings.append(poses)
            ages.append(headings)
        bank.append(ages)
    # Also share short color sequences contained in longer ones. This is an
    # offline packing step; runtime stays a single contiguous RGB565 copy.
    import struct
    packed, offsets = bytearray(), {}
    for sequence, old in sorted(color_map.items(), key=lambda item: -len(item[0])):
        encoded = struct.pack('<'+'H'*len(sequence), *sequence)
        at = packed.find(encoded)
        while at >= 0 and at % 2:
            at = packed.find(encoded, at+1)
        if at < 0:
            at = len(packed)
            packed.extend(encoded)
        offsets[old] = at//2
    colors = list(struct.unpack('<'+'H'*(len(packed)//2), packed))
    runs = [(x, count, offsets[offset]) for x, count, offset in runs]
    assert len(colors) < 65536 and len(runs) < 65536 and len(rows) < 65536

    def array(values):
        return '{'+','.join(array(x) if isinstance(x, (list, tuple)) else str(x) for x in values)+'}'

    header = ('// SPDX-License-Identifier: MIT\n// Generated by tools/build_livestock.py; native shared scanlines.\n'
              '#pragma once\n#include <cstdint>\nnamespace animal_sprites {\n'
              'inline constexpr int Width=32,Height=28,AnchorX=16,AnchorY=27,Poses=15;\n'
              'struct Run {uint8_t x,count;uint16_t offset;};\n'
              'struct Row {uint16_t start;uint8_t count;};\n'
              'struct Frame {uint16_t rows[Height];};\n')
    header += f'inline constexpr uint16_t pixels[]={array(colors)};\n'
    header += f'inline constexpr Run runs[]={array(runs)};\n'
    header += f'inline constexpr Row rows[]={array(rows)};\n'
    header += 'inline constexpr Frame frames[]={'+','.join('{'+array(f)+'}' for f in frames)+'};\n'
    header += f'inline constexpr uint16_t animals[5][3][2][Poses]={array(bank)};\n'
    header += 'inline constexpr unsigned StorageBytes=sizeof(pixels)+sizeof(runs)+sizeof(rows)+sizeof(frames)+sizeof(animals);\n}\n'
    (ROOT/'animal_sprites.hpp').write_text(header)
    sheet.save(ROOT/'art/world-v2/livestock.png')
    meta = {'tool': 'tools/build_livestock.py', 'license': 'MIT',
            'reference': 'animals-concept-v2.png (AI concept; native pixels authored in Python)',
            'layout': 'rows: chicken/duck/cow/sheep/pig, each baby/juvenile/adult; columns: idle, walk 1-8, eat 1-6; runtime includes true west mirrors',
            'native_size': [32, 28], 'anchor': [16, 27], 'poses': POSES,
            'runtime': 'interned RGB565 opaque scanlines; no decoding, rotation, palette conversion or heap allocation',
            'stats': {'frames': len(frames), 'rows': len(rows), 'runs': len(runs), 'colors': len(colors),
                      'bytes': len(colors)*2+len(runs)*4+len(rows)*4+len(frames)*56+900}}
    (ROOT/'art/world-v2/livestock.json').write_text(json.dumps(meta, indent=2)+'\n')
    print(json.dumps(meta['stats']))


if __name__ == '__main__':
    main()
