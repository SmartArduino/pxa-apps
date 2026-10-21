#!/usr/bin/env python3
"""Compare a PXA screenshot with FCEUX's unfiltered 256x240 GD capture.

Requires Pillow. Reads images without modifying them. Both emulators must use
the same named palette; RGB565 quantization is applied to the RGB reference.
Use a static scene, since this comparison does not synchronize game frames.
"""
import argparse
import collections
import json
import pathlib
from PIL import Image

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--reference-gd', type=pathlib.Path, required=True)
p.add_argument('--screenshot', type=pathlib.Path, required=True)
p.add_argument('--palette', type=pathlib.Path, required=True, help='Reference emulator 192-byte .pal')
p.add_argument('--area', nargs=4, type=int, required=True, metavar=('X','Y','WIDTH','HEIGHT'))
p.add_argument('--ignore-top', type=int, default=0, help='Rows occupied by the host FPS overlay')
p.add_argument('--ignore-rect', nargs=4, type=int, action='append', default=[], metavar=('X','Y','WIDTH','HEIGHT'), help='Exclude a screen-space floating control rectangle')
p.add_argument('--corner-radius', type=int, default=0, help='Exclude simulator rounded-panel clipping')
p.add_argument('--min-match', type=float, default=.99)
p.add_argument('--output', type=pathlib.Path, required=True)
a = p.parse_args()
raw = a.reference_gd.read_bytes()
if len(raw) != 11+256*240*4:
    p.error('Expected gui.gdscreenshot(true): 11-byte header + 256x240 ARGB')
palette = a.palette.read_bytes()
if len(palette) != 192:
    p.error('Expected a 64-color RGB palette')
def quantize(r,g,b):
    # Compare encoded RGB565. Device PNGs expand channels by bit replication;
    # desktop PNGs use integer scaling, which can differ by one RGB8 unit.
    return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3)
allowed = {quantize(*palette[i:i+3]) for i in range(0,192,3)}
im = Image.open(a.screenshot).convert('RGB')
x0,y0,w,h = a.area
if min(x0,y0)<0 or min(w,h)<=0 or x0+w>im.width or y0+h>im.height or not 0<=a.ignore_top<h:
    p.error('Invalid game area')
equal = total = invalid = excluded = 0
colors = collections.Counter()
for y in range(a.ignore_top,h):
    for x in range(w):
        sx,sy=x+x0,y+y0
        if any(rx<=sx<rx+rw and ry<=sy<ry+rh for rx,ry,rw,rh in a.ignore_rect):
            excluded+=1;continue
        radius=a.corner_radius
        if radius:
            dx=sx-min(max(sx,radius),im.width-radius-1)
            dy=sy-min(max(sy,radius),im.height-radius-1)
            if dx*dx+dy*dy>=radius*radius:excluded+=1;continue
        offset = 11+((y*240//h)*256+x*256//w)*4
        expected = quantize(*raw[offset+1:offset+4])
        rgb = im.getpixel((sx,sy))
        actual = quantize(*rgb)
        total += 1
        equal += expected == actual
        invalid += actual not in allowed
        colors[rgb] += 1
report = {
    'reference': str(a.reference_gd), 'screenshot': str(a.screenshot),
    'palette': str(a.palette), 'area': a.area, 'ignored_top_rows': a.ignore_top,
    'pixels': total, 'matching_pixels': equal, 'match_fraction': equal/total if total else 0,
    'excluded_pixels': excluded, 'excluded_rectangles': a.ignore_rect, 'corner_radius':a.corner_radius,
    'non_palette_pixels': invalid,
    'dominant_rgb': [{'rgb': rgb, 'pixels': n} for rgb,n in colors.most_common(12)],
    'comparison': 'Encoded RGB565, independent of PNG RGB8 expansion rounding',
    'scope': 'Static scene with matched palette and RGB565 quantization; not LCD optical calibration',
}
report['passed'] = bool(total) and equal/total >= a.min_match and invalid == 0
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
if not report['passed']:
    raise SystemExit('Color comparison failed; inspect frame timing, palette and channel order')
