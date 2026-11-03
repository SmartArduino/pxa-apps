#!/usr/bin/env python3
"""Predecode original PNG pixels losslessly, keeping the source bundle intact."""
import argparse,hashlib,json,platform,re,struct,subprocess
from pathlib import Path
from compile_scripts import ORIGINAL_SHA,ENTRY,write_changed
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);p.add_argument('--input-index',type=Path,required=True);p.add_argument('--metadata',type=Path,required=True);p.add_argument('--cc',required=True);a=p.parse_args()
root=a.root.resolve();build=a.build_dir.resolve();build.mkdir(parents=True,exist_ok=True)
raw=(root/'assets/jinyong.bundle').read_bytes();assert hashlib.sha256(raw).hexdigest()==ORIGINAL_SHA
index=a.input_index.read_text();stb=root/'compat/third_party/stb/stb_image.h';source=root/'tools/image_compile.c';destination=root/'assets/jinyong-images.bundle';stamp=build/'stamp.json'
h=hashlib.sha256((ORIGINAL_SHA+a.cc+platform.machine()+index).encode())
for file in [Path(__file__),source,stb]:h.update(file.read_bytes())
key=h.hexdigest()
if stamp.exists() and destination.exists() and a.metadata.exists():
 old=json.loads(stamp.read_text())
 if old.get('key')==key and old.get('bundle')==hashlib.sha256(destination.read_bytes()).hexdigest() and old.get('metadata')==hashlib.sha256(a.metadata.read_bytes()).hexdigest():raise SystemExit(0)
compiler=build/'image-compile';subprocess.run([a.cc,'-O2','-I'+str(stb.parent),str(source),'-o',str(compiler)],check=True)
images={};bundle=bytearray()
for m in ENTRY.finditer(index):
 name,size,offset,total=m.groups()
 if not name.lower().endswith('.png'):continue
 size,offset,total=map(int,[size,offset,total]);assert total==len(raw) and offset+size<=total
 input=build/'input.png';output=build/'output.pixels';input.write_bytes(raw[offset:offset+size]);subprocess.run([str(compiler),str(input),str(output)],check=True)
 data=output.read_bytes();w,h,c=struct.unpack_from('<III',data,4);assert data[:4]==b'JYPI' and c in (3,4) and len(data)==16+w*h*c
 images[name]=(len(data),len(bundle));bundle.extend(data)
assert images
def replace(m):
 if m[1] not in images:return m[0]
 size,offset=images[m[1]];return '{"'+m[1]+'","assets/jinyong-images.bundle",'+f'{size}u,{offset}u,{len(bundle)}u'+'}'
metadata=('/* Original PNG pixels, decoded losslessly at build time. */\n'+ENTRY.sub(replace,index)).encode()
write_changed(destination,bundle);write_changed(a.metadata,metadata);write_changed(stamp,(json.dumps({'key':key,'images':len(images),'bundle':hashlib.sha256(bundle).hexdigest(),'metadata':hashlib.sha256(metadata).hexdigest()},indent=2)+'\n').encode());print(f'Prepared {len(images)} original PNG images as lossless RGB/RGBA pixels')
