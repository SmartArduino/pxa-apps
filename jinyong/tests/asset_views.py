#!/usr/bin/env python3
"""Compare generated stream views with the independent immutable input archive."""
import argparse,hashlib,json,re,struct
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime-index',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
pattern=r'\{"([^"]+)","([^"]+)",(\d+)u,(\d+)u,(\d+)u\}'
def entries(text):return {m[0]:(m[1],*map(int,m[2:])) for m in re.findall(pattern,text)}
original=entries((root/'engine/jy_asset_index.c').read_text());runtime=entries(a.runtime_index.read_text());assert original.keys()==runtime.keys() and len(original)==1371
raw=(root/'assets/jinyong.bundle').read_bytes();assert hashlib.sha256(raw).hexdigest()=='adb7971b6d2314d181b67b2d16b4eb41838b53ab61dc6504220ce198e8aabe0f'
view=root/'assets-esp32s31';resources={};counts={'original_files':0,'lua_chunks':0,'images':0}
for name,(path,size,offset,total) in runtime.items():
 assert path.startswith('assets/');physical=view/path[7:]
 if path not in resources:resources[path]=physical.read_bytes()
 data=resources[path];assert len(data)==total and offset+size<=total
 data=data[offset:offset+size];_,old_size,old_offset,_=original[name];source=raw[old_offset:old_offset+old_size]
 if name.endswith('.lua'):
  assert data[:12]==bytes.fromhex('1b4c75615100010404040800') and ('@./'+name).encode()+b'\0' in data;counts['lua_chunks']+=1
 elif name.lower().endswith('.png'):
  assert source[:8]==b'\x89PNG\r\n\x1a\n' and data[:4]==b'JYPI';w,h,c=struct.unpack_from('<III',data,4);assert (w,h)==struct.unpack_from('>II',source,16) and c in (3,4) and len(data)==16+w*h*c;counts['images']+=1
 else:assert data==source,name;counts['original_files']+=1
for target in ['esp32s3','simulator']:
 for relative,data in resources.items():assert (root/('assets-'+target)/relative[7:]).read_bytes()==data
for path,data in resources.items():
 if '/part-' in path:assert len(data)<=512*1024
assert len(resources)<80
report={'passed':True,'logical_files':len(runtime),'resource_files':len(resources),'counts':counts,'original_sha256':hashlib.sha256(raw).hexdigest()};(a.output/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
