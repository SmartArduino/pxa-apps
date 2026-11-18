#!/usr/bin/env python3
"""Verify the generated native title and the actual RLE consumed by the UI."""
from pathlib import Path
import re
from PIL import Image
root=Path(__file__).resolve().parents[1]
header=(root/'title_art.hpp').read_text()
def values(name):
    return [int(v)for v in re.search(r'\b'+name+r'\[\d+\]=\{([^}]+)\}',header)[1].split(',')]
palette,rows,runs=(values(name)for name in ('palette','rows','runs'))
image=Image.open(root/'art/generated/title-logo-v1.png').convert('RGBA')
assert image.size==(224,48) and len(rows)==49 and rows[0]==0 and rows[-1]==len(runs)
pixels=[]
for y in range(48):
    row=[]
    for at in range(rows[y],rows[y+1],2):
        length,index=runs[at:at+2];assert 0<length<=224 and index<len(palette)
        row += [palette[index]]*length
    assert len(row)==224;pixels+=row
expected=[(((r>>3)<<11)|((g>>2)<<5)|(b>>3))or 1 if a else 0 for r,g,b,a in image.getdata()]
assert pixels==expected
assert set(image.getchannel('A').getdata())=={0,255}
assert image.getbbox()[0]>0 and image.getbbox()[1]>0 and image.getbbox()[2]<224 and image.getbbox()[3]<48
assert len(runs)+len(rows)*2+len(palette)*2<224*48
print('PASS 224x48 transparent title, native bounds, exact RGB565/RLE round trip and compressed storage')
