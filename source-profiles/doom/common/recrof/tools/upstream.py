#!/usr/bin/env python3
"""Reviewable three-way updates of vendored engines, without running upstream code."""
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
CONFIG = Path(__file__).resolve().parents[1] / 'upstreams.json'
MAX_BYTES = 100 * 1024 * 1024

def git(repo, *args, check=True):
    return subprocess.run(['git', '-C', str(repo), *args], capture_output=True, check=check)

def digest(data):
    return hashlib.sha256(data).hexdigest() if data is not None else None

def safe_path(root, name):
    p = Path(name)
    if not name or not p.parts or p.as_posix()!=name or p.is_absolute() or any(x in ('', '.', '..', '.git') for x in p.parts) or any(ord(c)<32 for c in name):
        raise ValueError(f'unsafe path: {name!r}')
    result = root / p
    for part in [result, *result.parents]:
        if part == root: break
        if part.is_symlink(): raise ValueError(f'symlink: {part}')
    if not result.resolve().is_relative_to(root.resolve()): raise ValueError('path escapes root')
    return result

def read_local(root, name):
    p = safe_path(root, name)
    return p.read_bytes() if p.exists() else None

def remote_head(url):
    result = subprocess.run(['git','ls-remote',url,'HEAD'],capture_output=True,check=True)
    value = result.stdout.decode().split()[0]
    if not re.fullmatch('[0-9a-f]{40}',value): raise ValueError('invalid remote revision')
    return value

def repository(cache, url, revisions):
    repo = cache / url.rstrip('/').rsplit('/',1)[-1].removesuffix('.git')
    if not repo.exists():
        repo.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(['git','clone','--mirror',url,str(repo)],check=True)
    for revision in dict.fromkeys(revisions):
        if git(repo,'cat-file','-e',revision+'^{commit}',check=False).returncode:
            git(repo,'fetch','--no-tags',url,revision)
        git(repo,'cat-file','-e',revision+'^{commit}')
    return repo

def snapshot(repo, revision, maps, cache=None):
    cache={} if cache is None else cache
    entries={};total=0;missing={}
    for source,destination in (maps.items() if isinstance(maps,dict) else maps):
        rows=git(repo,'ls-tree','-rlz',revision,'--',source).stdout.split(b'\0')
        found=False
        for row in rows:
            if not row:continue
            info,name=row.split(b'\t',1);mode,kind,oid,size=info.decode().split()
            name=name.decode();found=True
            if mode not in ('100644','100755') or kind!='blob':raise ValueError(f'nonregular upstream file: {name}')
            total+=int(size)
            if total>MAX_BYTES:raise ValueError('snapshot exceeds limit')
            relative=name[len(source):].lstrip('/')
            target=destination+('/'+relative if relative else '')
            if target in entries:raise ValueError(f'overlapping mapping: {target}')
            entries[target]=(oid,int(mode,8)&0o777)
            if oid not in cache:missing[oid]=int(size)
        if not found:raise ValueError(f'upstream mapping disappeared: {source}; update maps explicitly')
    if missing:
        result=subprocess.run(['git','-C',str(repo),'cat-file','--batch'],
            input=('\n'.join(missing)+'\n').encode(),capture_output=True,check=True).stdout
        at=0
        for oid,size in missing.items():
            end=result.find(b'\n',at)
            if end<0 or result[at:end].decode().split()!=[oid,'blob',str(size)]:raise ValueError('invalid Git batch header')
            at=end+1
            if at+size>=len(result) or result[at+size]!=10:raise ValueError('truncated Git batch')
            cache[oid]=result[at:at+size];at+=size+1
        if at!=len(result):raise ValueError('trailing Git batch output')
    return {name:(cache[oid],mode) for name,(oid,mode) in entries.items()}

def merge(local, base, upstream):
    if local==base: return upstream, False
    if upstream==base or local==upstream: return local, False
    if None in (local,base,upstream) or any(b'\0' in x for x in (local,base,upstream)):
        return local, True
    with tempfile.TemporaryDirectory(prefix='pxa-upstream-merge-') as tmp:
        paths = [Path(tmp)/x for x in ('local','base','upstream')]
        for p,data in zip(paths,(local,base,upstream)): p.write_bytes(data)
        r = subprocess.run(['git','merge-file','-p','-L','PXA','-L','base','-L','upstream',*map(str,paths)],capture_output=True)
        if r.returncode<0 or r.returncode>=128: raise ValueError(r.stderr.decode(errors='replace'))
        return r.stdout, bool(r.returncode)

def prepare(root, cfg, repo, target, output):
    if output.exists(): raise ValueError('review directory already exists')
    metadata = [json.loads(read_local(root,p)) for p in cfg['metadata']]
    if len({(d['url'],d['commit']) for d in metadata})!=1: raise ValueError('shared engine pins disagree')
    base = metadata[0]['commit']
    maps=[*cfg['maps'].items(),*cfg.get('extra_maps',{}).items()]
    cache={};old=snapshot(repo,base,maps,cache)
    new=old if target==base else snapshot(repo,target,maps,cache)
    items, patch = [], []
    output.mkdir(parents=True)
    for name in sorted(old.keys()|new.keys()):
        before = read_local(root,name); a = old.get(name,(None,0o644)); b = new.get(name,(None,0o644))
        merged, conflict = merge(before,a[0],b[0])
        mode = safe_path(root,name).stat().st_mode&0o777 if before is not None else b[1]
        if mode==a[1]: mode=b[1]
        # Export all local engine adaptations, even for a no-update review.
        if before!=a[0] and all(x is None or b'\0' not in x for x in (before,a[0])):
            patch.extend(difflib.diff_bytes(difflib.unified_diff,(a[0] or b'').splitlines(True),
                (before or b'').splitlines(True),('a/'+name).encode(),('b/'+name).encode()))
        if merged==before and not conflict and (before is None or mode==(safe_path(root,name).stat().st_mode&0o777)): continue
        old_mode=safe_path(root,name).stat().st_mode&0o777 if before is not None else None
        item = {'path':name,'before':digest(before),'before_mode':old_mode,'after':digest(merged),'mode':mode,'conflict':conflict}
        items.append(item)
        for directory,data in [('files',merged),('local',before),('upstream',b[0])]:
            if data is not None:
                p=safe_path(output,directory+'/'+name);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    if target!=base:
        for name,d in zip(cfg['metadata'],metadata):
            before=read_local(root,name);d['commit']=target;after=(json.dumps(d,indent=2)+'\n').encode()
            items.append({'path':name,'before':digest(before),'before_mode':safe_path(root,name).stat().st_mode&0o777,'after':digest(after),'mode':0o644,'conflict':False})
            p=safe_path(output,'files/'+name);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(after)
    plan={'schema':'pxa-upstream-plan-1','engine':cfg['metadata'][0].split('/')[0],'base':base,'target':target,'items':items}
    (output/'plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    (output/'adaptations.patch').write_bytes(b''.join(patch))
    return plan

def load_plan(output):
    plan=json.loads((output/'plan.json').read_text())
    if plan['schema']!='pxa-upstream-plan-1': raise ValueError('unsupported plan')
    return plan

def resolve(output, name, take):
    plan=load_plan(output)
    item=next((x for x in plan['items'] if x['path']==name),None)
    if not item or not item['conflict']: raise ValueError('path is not an unresolved conflict')
    p=safe_path(output,'files/'+name)
    if take!='merged':
        data=read_local(output,take+'/'+name)
        if data is None:
            if p.exists():p.unlink()
        else:p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    data=read_local(output,'files/'+name)
    if data and any(line.startswith((b'<<<<<<< ',b'||||||| ',b'=======',b'>>>>>>> ')) for line in data.splitlines()):
        raise ValueError('conflict markers remain')
    item['after']=digest(data);item['conflict']=False
    (output/'plan.json').write_text(json.dumps(plan,indent=2)+'\n')

def atomic_write(root,name,data,mode):
    p=safe_path(root,name)
    if data is None:
        if p.exists():p.unlink()
        return
    p.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=p.parent,delete=False) as f:
        temp=Path(f.name);f.write(data)
    try:temp.chmod(mode);os.replace(temp,p)
    finally:
        if temp.exists():temp.unlink()

def apply(root,output,cfg=None):
    plan=load_plan(output);updates=[];seen=set()
    cfg=cfg or json.loads(CONFIG.read_text()).get(plan.get('engine'))
    if not cfg:raise ValueError('unknown plan engine')
    destinations=[*cfg['maps'].values(),*cfg.get('extra_maps',{}).values()]
    def allowed(name):
        return name in cfg['metadata'] or any(name==p or name.startswith(p+'/') for p in destinations)
    for item in plan['items']:
        name=item['path']
        if not allowed(name):raise ValueError('path is outside engine mappings: '+name)
        if name in seen:raise ValueError('duplicate plan path')
        seen.add(name)
        if item['conflict']:raise ValueError(f'unresolved conflict: {name}')
        before=read_local(root,name);after=read_local(output,'files/'+name)
        if digest(before)!=item['before']:raise ValueError(f'local file changed after prepare: {name}')
        if digest(after)!=item['after']:raise ValueError(f'review file changed without resolve: {name}')
        mode=safe_path(root,name).stat().st_mode&0o777 if before is not None else 0o644
        if item['before_mode']!=(mode if before is not None else None):raise ValueError(f'local mode changed after prepare: {name}')
        if not isinstance(item['mode'],int) or item['mode']<0 or item['mode']>0o777:raise ValueError('invalid file mode')
        updates.append((name,before,after,mode,item['mode']))
    if sum(len(x or b'') for u in updates for x in u[1:3])>MAX_BYTES*2:raise ValueError('plan exceeds limit')
    done=[]
    try:
        for name,before,after,old_mode,new_mode in updates:
            done.append((name,before,old_mode));atomic_write(root,name,after,new_mode)
    except BaseException:
        for name,before,mode in reversed(done):atomic_write(root,name,before,mode)
        raise
    return len(updates)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--cache',type=Path,default=Path.home()/'.cache/pxa-upstreams')
    sub=parser.add_subparsers(dest='command',required=True)
    check=sub.add_parser('check');check.add_argument('game',nargs='?',default='all')
    prep=sub.add_parser('prepare');prep.add_argument('game');prep.add_argument('--ref',default='HEAD');prep.add_argument('--output',type=Path,required=True)
    put=sub.add_parser('apply');put.add_argument('review',type=Path)
    res=sub.add_parser('resolve');res.add_argument('review',type=Path);res.add_argument('path');res.add_argument('--take',choices=['merged','local','upstream'],default='merged')
    a=parser.parse_args();cfg=json.loads(CONFIG.read_text());root=a.root.resolve()
    if a.command=='apply':print(f'applied {apply(root,a.review.resolve())} files; inspect git diff, rebuild and validate on device');return
    if a.command=='resolve':resolve(a.review.resolve(),a.path,a.take);return
    names=sorted(cfg) if a.command=='check' and a.game=='all' else [a.game]
    for name in names:
        if name not in cfg:raise ValueError('unknown engine')
        pinned=json.loads(read_local(root,cfg[name]['metadata'][0]));latest=remote_head(pinned['url']) if a.command=='check' or a.ref=='HEAD' else None
        if a.command=='check':print(json.dumps({'engine':name,'pinned':pinned['commit'],'head':latest,'update':latest!=pinned['commit']}));continue
        target=latest if a.ref=='HEAD' else a.ref
        if not re.fullmatch('[0-9a-f]{40}',target):raise ValueError('--ref requires a full commit SHA or HEAD')
        repo=repository(a.cache,pinned['url'],[pinned['commit'],target])
        plan=prepare(root,cfg[name],repo,target,a.output.resolve())
        print(json.dumps({'base':plan['base'],'target':target,'files':len(plan['items']),'conflicts':sum(x['conflict'] for x in plan['items'])}))

if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.CalledProcessError) as e:raise SystemExit(str(e))
