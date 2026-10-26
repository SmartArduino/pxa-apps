#!/usr/bin/env python3
"""Rebuild the shipped SOL resources using Neverball 1.6.0's mapc.

The intermediate work directory must be outside the source tree. The mapc
compiler can be built from the revision recorded in vendor/UPSTREAM.json.
"""
import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mapc',type=Path,required=True)
    parser.add_argument('--work-dir',type=Path,required=True)
    args=parser.parse_args()
    app=Path(__file__).resolve().parents[1]
    work=args.work_dir.resolve()
    if work.is_relative_to(app):parser.error('Use an ignored artifact directory outside the app')
    if work.exists() and any(work.iterdir()):parser.error('Work directory must be empty')
    work.mkdir(parents=True,exist_ok=True)
    data=work/'data'
    shutil.copytree(app/'vendor/data',data)
    maps=sorted(data.rglob('*.map'))
    for path in maps:
        relative=path.relative_to(data)
        result=subprocess.run([str(args.mapc.resolve()),str(path),str(data)],capture_output=True,text=True)
        (work/(relative.as_posix().replace('/','_')+'.log')).write_text(result.stdout+result.stderr)
        output=path.with_suffix('.sol')
        if result.returncode or not output.exists():raise RuntimeError(f'mapc failed: {relative}')
        target=app/'media'/relative.with_suffix('.sol')
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(output,target)
    print(f'Rebuilt {len(maps)} SOL resources')


if __name__=='__main__':main()
