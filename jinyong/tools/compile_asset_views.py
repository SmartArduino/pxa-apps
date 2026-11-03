#!/usr/bin/env python3
"""Make bounded resource shards while keeping the original archive intact."""
import argparse,hashlib,os,re
from pathlib import Path
from compile_scripts import ORIGINAL_SHA,ENTRY,write_changed
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--input-index',type=Path,required=True);p.add_argument('--metadata',type=Path,required=True);a=p.parse_args()
root=a.root.resolve();raw=(root/'assets/jinyong.bundle').read_bytes();assert hashlib.sha256(raw).hexdigest()==ORIGINAL_SHA
index=a.input_index.read_text();views=[root/('assets-'+target) for target in ['esp32s3','esp32s31','simulator']]
old=set(re.findall(r'"assets/(game/[^\"]+)"',a.metadata.read_text())) if a.metadata.exists() else set()
limit=512*1024;payload={};entries={};small=bytearray();pending=[];number=0

def flush():
 global small,pending,number
 if not pending:return
 path=f'game/part-{number:03d}.bundle';number+=1;payload[path]=bytes(small)
 for name,size,offset in pending:entries[name]=(path,size,offset,len(small))
 small=bytearray();pending=[]
for m in ENTRY.finditer(index):
 name,size,offset,total=m.groups();size,offset,total=map(int,[size,offset,total]);assert total==len(raw) and offset+size<=total
 data=raw[offset:offset+size]
 if size>limit:
  path=f'game/large-{len(payload):03d}-{hashlib.sha256(name.encode()).hexdigest()[:8]}.bundle';payload[path]=data;entries[name]=(path,size,0,size)
 else:
  if len(small)+size>limit:flush()
  pending.append((name,size,len(small)));small.extend(data)
flush();assert len(payload)<80,'asset inventory must fit the published manifest limit'
# The ignored target directories are reserved generated package inputs. Remove
# only stale paths that the previous generated metadata explicitly owned.
for view in views:
 view.mkdir(parents=True,exist_ok=True)
 for relative in old-set(payload):
  path=view/relative
  if path.is_file():path.unlink()
for relative,data in payload.items():
 target=views[0]/relative;write_changed(target,data)
 for view in views[1:]:
  dest=view/relative;dest.parent.mkdir(parents=True,exist_ok=True)
  if dest.exists() and os.path.samefile(target,dest):continue
  temporary=dest.with_name(dest.name+'.tmp');temporary.unlink(missing_ok=True);os.link(target,temporary);temporary.replace(dest)
for source in [root/'assets/jinyong-scripts.bundle',root/'assets/jinyong-images.bundle']:
 for view in views:
  dest=view/source.name
  if dest.exists() and os.path.samefile(source,dest):continue
  temporary=dest.with_name(dest.name+'.tmp');temporary.unlink(missing_ok=True);os.link(source,temporary);temporary.replace(dest)
def replace(m):
 path,size,offset,total=entries[m[1]];return '{"'+m[1]+'","assets/'+path+'",'+f'{size}u,{offset}u,{total}u'+'}'
write_changed(a.metadata,('/* Bounded resource shards; the original archive remains unchanged. */\n'+ENTRY.sub(replace,index)).encode())
print(f'Prepared {len(entries)} original files in {len(payload)} streamed resource shards')
