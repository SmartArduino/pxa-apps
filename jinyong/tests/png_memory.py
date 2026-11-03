#!/usr/bin/env python3
"""Check PNG pixels, ownership and a bounded decoder heap; outputs are local."""
from pathlib import Path
import argparse,struct,zlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);root=Path(__file__).resolve().parents[1]
def png(name,w,h,channels,pixel):
 def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
 raw=b''.join(b'\0'+b''.join(bytes(pixel(x,y)) for x in range(w)) for y in range(h))
 data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2 if channels==3 else 6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
 (out/name).write_bytes(data)
 (out/(name+'.pixels')).write_bytes(b'JYPI'+struct.pack('<III',w,h,channels)+b''.join(bytes(pixel(x,y)) for y in range(h) for x in range(w)))
png('rgb.png',640,480,3,lambda x,y:((x%8)*30,(y%8)*30,((x//80+y//80)%8)*30))
png('rgba.png',2,2,4,lambda x,y:(20+x,40+y,60,(x+y*2)*85))
exe=out/'png-test';cmd=['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections','-I'+str(root/'compat'),'-I'+str(root/'compat/SDL'),str(root/'tests/png_memory.c'),*[str(root/'compat'/n) for n in ['jy_surface.c','jy_sdl_host.c','jy_png.c','jy_rwops.c']],'-Wl,--gc-sections','-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free','-o',str(exe)]
with (out/'build.log').open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
with (out/'run.log').open('w') as log:subprocess.run([str(exe),str(out/'rgb.png'),str(out/'rgba.png'),str(out/'rgb.png.pixels'),str(out/'rgba.png.pixels')],stdout=log,stderr=subprocess.STDOUT,check=True)
print((out/'run.log').read_text(),end='')
