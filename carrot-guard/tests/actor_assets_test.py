#!/usr/bin/env python3
"""Keep generated expressions distinct and the carrot's ground pivot stable."""
from pathlib import Path
import hashlib
from PIL import Image

root=Path(__file__).resolve().parents[1]
atlas=Image.open(root/'art/runtime/carrot_states.png').convert('RGBA')
assert atlas.size==(288,288)
areas=[]
for row in range(4):
    frames=[atlas.crop((col*72,row*72,(col+1)*72,(row+1)*72)) for col in range(4)]
    assert len({hashlib.sha256(frame.tobytes()).digest() for frame in frames})==4
    for frame in frames:
        bounds=frame.getchannel('A').getbbox()
        assert bounds and bounds[0]>=4 and bounds[1]>=4 and bounds[2]<=68
        assert bounds[3]==68, 'Frame moved off the shared ground pivot'
    areas.append(sum(frames[0].getchannel('A').getdata()))
assert all(a>b for a,b in zip(areas,areas[1:])), 'Bite damage must remove more carrot at each health stage'
print('16 distinct generated frames; shared root pivot, clear margins and increasing bite damage passed')
