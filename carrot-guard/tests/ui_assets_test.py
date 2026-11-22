#!/usr/bin/env python3
"""Guard complete digit slicing and consistent visible cap heights/baselines."""
from pathlib import Path
import sys
import re
from PIL import Image

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from import_ui import digit_cells

def asset(name):
    header=(root/'expansion_art.hpp').read_text()
    match=re.search(r'\{"'+name+r'","([^\"]+)",(\d+),(\d+),(\d+),(\d+),',header)
    if match:
        sheet,x,y,w,h=match.groups();x,y,w,h=map(int,(x,y,w,h))
        return Image.open(root/f'art/runtime/ui/{sheet}.png').convert('RGBA').crop((x,y,x+w,y+h))
    return Image.open(root/f'art/runtime/{name}.png').convert('RGBA')

for index, cell in enumerate(digit_cells(root / 'art/ui-digits-v2.png')):
    alpha = cell.getchannel('A')
    for row in (0, cell.height - 1):
        assert alpha.crop((0, row, cell.width, row + 1)).getextrema()[1] < 64, (
            f'digit {index}: slice intersects a stroke')

caps = []
for index in range(16):
    image = asset(f'n{index}')
    bounds = image.getchannel('A').point(lambda a: 255 if a > 128 else 0).getbbox()
    assert bounds and bounds[0] >= 4 and bounds[2] <= image.width - 4
    assert bounds[1] >= 4 and bounds[3] <= image.height - 4
    if index < 10:
        caps.append((bounds[1], bounds[3]))
assert len(set(caps)) == 1, 'Numerals have different visible cap heights or baselines'
print('All 16 symbols retain full strokes and margins; 0–9 share visible cap height and baseline.')
