#!/usr/bin/env python3
"""Generate typed state copies; fail on unknown declarations to retain all fields.

Passing live globals to byte I/O prevents LLVM's global scalar replacement.
Keeping field accesses visible preserves that optimization in the frame loop.
"""
import re
import sys
from pathlib import Path
source=re.sub(r'/\*.*?\*/|//[^\n]*','',Path(sys.argv[1]).read_text(),flags=re.S)
output=['/* Generated from pico_int.h by tests/generate_state_fields.py. */','#pragma once']
allowed={'unsigned char','unsigned short','unsigned int','char','signed char','int','signed int','short'}
for name in ['PicoMisc','PicoVideo','PicoTiming','PicoSound']:
 body=re.search(r'\bstruct\s+'+name+r'\s*\{([^}]+)\};',source)
 if not body:raise ValueError('missing '+name)
 output.append(f'static inline __attribute__((always_inline)) void pxa_md_state_copy_{name}(struct {name} *dst,const struct {name} *src) {{')
 for declaration in body[1].split(';'):
  parts=declaration.strip().split(',')
  if not parts[0]:continue
  first=re.search(r'([A-Za-z_]\w*)(\[[^]]+\])?$',parts[0]);assert first,declaration
  ctype=' '.join(parts[0][:first.start()].split());assert ctype in allowed,(name,ctype)
  for part in [first[0],*parts[1:]]:
   field=re.fullmatch(r'\s*([A-Za-z_]\w*)(?:\[([^]]+)\])?\s*',part);assert field,declaration
   key,count=field.groups()
   if count:output.append(f'    for(unsigned n=0;n<({count});++n)dst->{key}[n]=src->{key}[n];')
   else:output.append(f'    dst->{key}=src->{key};')
 output.append('}')
target=Path(sys.argv[2]);target.parent.mkdir(parents=True,exist_ok=True);target.write_text('\n'.join(output)+'\n')
