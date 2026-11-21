"""Check generated font coverage, atlas bounds and shared baseline geometry."""
from pathlib import Path
import json
from PIL import ImageFont,Image,ImageDraw

root=Path(__file__).resolve().parents[1]
layout=json.loads((root/"art/layout.json").read_text())
resources=json.loads((root/"resources.json").read_text())["assets"]
metrics=json.loads((root/"art/font-metrics.json").read_text())
chars="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-:/.!?><=()abcdefghijklmnopqrstuvwxyz"
assert set(metrics)==set(chars)
for i,ch in enumerate(chars):
    page,x,y,w,h=layout[f"glyph_{i}"]
    assert (w,h)==(6,12)
    r=resources[page];data=(root/r["source"]).read_bytes()
    pixels=[data[(y+row)*r["width"]+x+col] for row in range(h) for col in range(w)]
    assert any(pixels),ch
    m=metrics[ch]
    assert m["baseline"]==10 and m["top"]+m["height"]<=12,ch
    if (ch.isupper() and ch!="Q") or ch.isdigit():assert m["top"]+m["height"]==10,ch
    if ch in "gjpqy":assert m["top"]+m["height"]==12,ch
labels=json.loads((root/"tools/chinese-labels.json").read_text())
assert len(labels)>=48
font=ImageFont.truetype(str(root/'tools/fonts/fusion-pixel-12px-monospaced-zh_hans.ttf'),12,layout_engine=ImageFont.Layout.BASIC)
for i,label in enumerate(labels):
    p,x,y,w,h=layout[f"cn_{i}"]
    assert w==round(font.getlength(label)) and h==12,label
    r=resources[p]
    assert x+w<=r["width"] and y+h<=r["height"],label
    # Compare the packed mask to the licensed font itself, including mixed
    # half-width '+' and full-width Chinese, rather than assuming 12 per char.
    expected=Image.new('L',(w,12));draw=ImageDraw.Draw(expected);draw.fontmode='1';draw.text((0,10),label,font=font,fill=255,anchor='ls')
    data=(root/r['source']).read_bytes();actual=bytes(255 if data[(y+row)*r['width']+x+col] else 0 for row in range(12) for col in range(w))
    assert actual==expected.tobytes(),label
print("74 mixed-case glyphs and Chinese labels: coverage, atlas bounds and metrics passed")
