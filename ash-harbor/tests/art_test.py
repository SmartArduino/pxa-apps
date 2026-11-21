"""Check complete UI ornaments, texture limits and the default asset budget."""
from pathlib import Path
import json,re

root=Path(__file__).resolve().parents[1]
layout=json.loads((root/'art/layout.json').read_text())
resources=json.loads((root/'resources.json').read_text())['assets']
palette=list(map(int,re.search(r'palette=\{(.*?)\};',(root/'art.hpp').read_text()).group(1).split(',')))
for name,(page,x,y,w,h) in layout.items():
    resource=resources[page]
    assert x>=0 and y>=0 and x+w<=resource['width'] and y+h<=resource['height'],name
for resource in resources:
    assert resource['width']<=256 and resource['height']<=256
static_count=int(re.search(r'static_pages=(\d+)',(root/'art.hpp').read_text()).group(1))
static_bytes=sum(r['width']*r['height'] for r in resources[:static_count])
largest=max(r['width']*r['height'] for r in resources[static_count:])
assert static_bytes+2*largest+32*1024<496*1024, 'Two active actor pages and core sound cache must fit'
for scene in range(3):
    first,last=layout[f'story_{scene}_0'],layout[f'story_{scene}_1']
    assert first[3]==256 and 0<last[3]<=256
    assert first[4]==last[4]<=256
    from PIL import Image
    source=['stage-harbor-traditional','story-1','story-tournament'][scene]
    image=Image.open(root/f'art/source/{source}.png')
    assert abs((first[3]+last[3])/first[4]-image.width/image.height)<.004,'Story art was stretched'
assert len(resources)>48, 'Expansion must exercise texture-slot reuse'
page,x,y,w,h=layout['timer'];resource=resources[page]
raw=(root/resource['source']).read_bytes()
def brass_cap(rows):
    for row in rows:
        for col in range(w//3,w*2//3):
            value=palette[raw[(y+row)*resource['width']+x+col]]
            red=(value>>11)*255//31;green=((value>>5)&63)*255//63;blue=(value&31)*255//31
            if red>150 and green>100 and blue<150:return True
    return False
assert brass_cap(range(4)),'Timer top bevel was cropped off'
assert brass_cap(range(h-4,h)),'Timer bottom bevel was cropped off'
assert layout['bracket_left'][3:]==[14,28] and layout['bracket_right'][3:]==[14,28]
assert '陆炎' in json.loads((root/'tools/chinese-labels.json').read_text())
# The registered palm must be inside the actual release silhouette, even though
# the source cells were trimmed to differently sized atlas rectangles.
anchors=re.search(r'firePalms=(.*?);',(root/'roster.hpp').read_text()).group(1)
anchors=[tuple(map(int,p)) for p in re.findall(r'\{(\d+),(\d+)\}',anchors)]
offsets=json.loads(re.search(r'fighter_offsets\[4\]\[32\]=(.*?);',(root/'art.hpp').read_text()).group(1).replace('{','[').replace('}',']'))
assert len(anchors)==4
for c,name in enumerate(['jin','lan','zhen','ling']):
    page,x,y,w,h=layout[f'{name}_0_25'];ox,oy=offsets[c][25];px,py=anchors[c]
    assert 0<=px-ox<w and 0<=py-oy<h,name
    resource=resources[page];raw=(root/resource['source']).read_bytes()
    assert raw[(y+py-oy)*resource['width']+x+px-ox]!=0,(name,'Palm anchor is transparent')
print('UI borders complete; all tiles in bounds; textures <=256px; asset budget fits 512KiB.')
