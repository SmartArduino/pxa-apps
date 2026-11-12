#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify every building against pinned icon goldens and all atlas texels."""
from pathlib import Path
import argparse,hashlib,json,re,struct
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1]

def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 source=(ROOT/'art.hpp').read_text();arrays={n:[int(x,0) for x in s.split(',') if x] for n,s in re.findall(r'inline constexpr uint32_t (\w+)_pixels\[\]=\{([^}]+)\};',source)}
 sprites={n:(int(w),int(h),arrays[n]) for n,w,h in re.findall(r'inline constexpr Sprite (\w+)\{(\d+),(\d+),\w+_pixels\};',source)}
 golden=json.loads((ROOT/'tests/fixtures/building-icons-v154.json').read_text());assert len(golden['icons'])==28
 for n,r in golden['icons'].items():
  w,h,data=sprites[n];assert (w,h)==(r['width'],r['height']),n
  assert hashlib.sha256(struct.pack('<'+'I'*len(data),*data)).hexdigest()==r['sha256'],('upstream icon mismatch',n)
 atlas_source=(ROOT/'render_assets.hpp').read_text()
 regions=[(n,*map(int,nums)) for n,*nums in re.findall(r'\{&art::(\w+),(\d+),(\d+),(\d+),(\d+)\}',atlas_source)]
 palette=list(map(int,re.search(r'palette\[256\]=\{([^}]+)',atlas_source)[1].split(',')))
 atlas=list(map(int,re.search(r'atlas\[65536\]=\{([^}]+)',atlas_source)[1].split(',')));assert len(atlas)==65536
 seen=set()
 for n,x,y,w,h in regions:
  assert x>=0 and y>=0 and x+w<=256 and y+h<=256,n
  assert (w,h)==sprites[n][:2],n
  for yy in range(h):
   for xx in range(w):
    at=(y+yy)*256+x+xx;assert at not in seen,('overlapping atlas regions',n,x+xx,y+yy);seen.add(at)
    value=sprites[n][2][yy*w+xx];index=atlas[at]
    assert bool(index)==bool(value>>16),('alpha mismatch',n,xx,yy)
    if index:assert palette[index]==(value&65535),('color mismatch',n,xx,yy)
 # Review image is a local test artifact, never a replacement for emulator screenshots.
 entries=list(golden['icons']);sheet=Image.new('RGB',(7*136,4*152),(28,31,36));draw=ImageDraw.Draw(sheet)
 for i,n in enumerate(entries):
  w,h,data=sprites[n];im=Image.new('RGBA',(w,h));im.putdata([(((v>>11)&31)<<3,((v>>5)&63)<<2,(v&31)<<3,255 if v>>16 else 0) for v in data]);im.thumbnail((112,112))
  im=im.resize((112,112),Image.Resampling.NEAREST);x=i%7*136+12;y=i//7*152+8;sheet.paste(im,(x,y),im);draw.text((x,y+118),n,fill='white')
 sheet.save(a.output/'building-icons.png')
 result={'passed':True,'buildings':22,'reference_images':28,'atlas_regions':len(regions),'atlas_pixels_checked':len(seen),'atlas_size':[256,256]};(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
