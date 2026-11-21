"""Validate lossless bank offsets and bounded two-fighter decoded working sets."""
from pathlib import Path
import json,re
root=Path(__file__).resolve().parents[1]
resources=json.loads((root/'resources.json').read_text())['assets']
bank=(root/'art/frame-cache-rle.bin').read_bytes();header=(root/'frame_dimensions.hpp').read_text()
def field(name):return list(map(int,re.search(r' '+name+r'\[\]=\{([^}]+)\}',header).group(1).split(',')))
offsets=field('page_offset');widths=field('page_width');heights=field('page_height')
assert len(offsets)==len(resources)==len(widths)==len(heights)
lengths=field('page_rle_bytes')
static_count=int(re.search(r'static_pages=(\d+)',(root/'art.hpp').read_text()).group(1))
end=0
for i,r in enumerate(resources):
 raw=(root/r['source']).read_bytes();assert offsets[i]==end
 assert widths[i]==r['width'] and heights[i]==r['height']
 encoded=bank[end:end+lengths[i]];decoded=bytearray();pos=0
 while pos<len(encoded):
  control=encoded[pos];pos+=1;count=(control&127)+1
  if control&128:decoded.extend(bytes([encoded[pos]])*count);pos+=1
  else:decoded.extend(encoded[pos:pos+count]);pos+=count
 assert decoded==raw,(i,'Atlas pixels changed')
 end+=lengths[i]
assert end==len(bank)
embedded=bytes(int(v,16) for v in re.findall(r'\\x([0-9a-f]{2})',header))
assert embedded==bank,'Embedded bank differs from generated source'
layout=json.loads((root/'art/layout.json').read_text());characters=['jin','lan','zhen','ling'];static_count=int(re.search(r'static_pages=(\d+)',(root/'art.hpp').read_text()).group(1))
def pages(c):
 names=[f'{characters[c]}_0_{i}' for i in range(32)]+[f'extra_{c}_{i}' for i in range(8)]+[f'walk_{c}_{i}' for i in range(8)]
 return {layout[n][0] for n in names}
maximum=0
for a in range(4):
 for b in range(4):
  needed=pages(a)|pages(b)
  assert all(i>=static_count for i in needed)
  total=sum(widths[i]*heights[i] for i in needed)
  assert total<2*1024*1024,(a,b,total)
  maximum=max(maximum,total)
print(f'{len(resources)} original atlases verified byte-for-byte; all 16 fighter pairs below 2 MiB (max {maximum} bytes).')

# One stage plus the two actors must fit the renderer's existing warm budget.
for scene in range(4):
 stage={layout[f'stage_{scene}_{i}'][0] for i in range(4)}
 for a in range(4):
  for b in range(4):
   total=sum(widths[i]*heights[i] for i in stage|pages(a)|pages(b))
   assert total<=1152*1024,(scene,a,b,total)
print('All 64 stage/fighter working sets fit the existing 1152 KiB warm budget.')
