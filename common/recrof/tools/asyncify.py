#!/usr/bin/env python3
"""Instrument engine suspension points while keeping WASI libc synchronous.

libc's internal indirect callbacks (formatters, allocators, qsort comparators)
never suspend in these ports. Instrumenting them consumes the small WAMR call
stack even for Host-to-Guest malloc and makes AOT compilation unnecessarily
large. Application stdio wrappers remain instrumented and can await PXA FS.
"""
import argparse, re, subprocess
from pathlib import Path

def leb(data, at):
    value=shift=0
    while True:
        byte=data[at];at+=1;value|=(byte&127)<<shift
        if byte<128:return value,at
        shift+=7

def string(data, at):
    n,at=leb(data,at);return data[at:at+n].decode(),at+n

def function_names(data):
    at=8;names=[]
    while at<len(data):
        kind=data[at];length,at=leb(data,at+1);end=at+length
        if kind==0:
            name,pos=string(data,at)
            if name=='name':
                while pos<end:
                    sub=data[pos];size,pos=leb(data,pos+1);subend=pos+size
                    if sub==1:
                        count,pos=leb(data,pos)
                        for _ in range(count):
                            _,pos=leb(data,pos);name,pos=string(data,pos);names.append(name)
                    pos=subend
        at=end
    return names

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('wasm',type=Path);p.add_argument('wasm_opt',type=Path)
    p.add_argument('llvm_nm',type=Path);p.add_argument('libc',type=Path)
    a=p.parse_args()
    rows=subprocess.check_output([str(a.llvm_nm),'--defined-only','--format=posix',str(a.libc)],text=True,stderr=subprocess.DEVNULL)
    libc={r.split()[0] for r in rows.splitlines() if len(r.split())>=2 and r.split()[1] in ('T','t','W','w')}
    names=function_names(a.wasm.read_bytes())
    removed=[n for n in names if re.sub(r'\.\d+$','',n) in libc]
    response=a.wasm.with_suffix('.asyncify-libc.txt');response.write_text(','.join(removed))
    out=a.wasm.with_suffix('.async.wasm')
    subprocess.run([str(a.wasm_opt),str(a.wasm),'--asyncify',
        '--pass-arg=asyncify-ignore-imports',f'--pass-arg=asyncify-removelist@@{response}',
        '-O2','--always-inline-max-function-size=0',
        '--one-caller-inline-max-function-size=0','--flexible-inline-max-function-size=0',
        '-g','--enable-bulk-memory','--enable-sign-ext',
        '--enable-nontrapping-float-to-int','--enable-mutable-globals','-o',str(out)],check=True)
    out.replace(a.wasm)
    print(f'Asyncify: {len(removed)} synchronous libc functions excluded')

if __name__=='__main__':main()
