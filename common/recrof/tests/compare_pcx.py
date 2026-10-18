#!/usr/bin/env python3
"""Compare the streaming decoder against full-file PCX decoding for an owned PAK."""
import argparse
import ctypes as c
import hashlib
import json
import struct
from pathlib import Path
READ = c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_uint32,c.c_void_p,c.c_uint32)
class Source(c.Structure):_fields_=[('context',c.c_void_p),('bytes',c.c_uint32),('read',READ)]
class Info(c.Structure):_fields_=[('width',c.c_uint16),('height',c.c_uint16),('stride',c.c_uint16),('data_end',c.c_uint32)]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library',type=Path,required=True);parser.add_argument('--pak',type=Path,required=True)
    args=parser.parse_args();lib=c.CDLL(str(args.library.resolve()))
    lib.port_pcx_header.argtypes=[c.POINTER(Source),c.POINTER(Info)]
    lib.port_pcx_pixels.argtypes=[c.POINTER(Source),c.POINTER(Info),c.c_void_p,c.c_size_t]
    lib.port_pcx_palette.argtypes=[c.POINTER(Source),c.c_void_p]
    digest=hashlib.sha256();count=pixel_count=0
    with args.pak.open('rb') as pak:
        magic,offset,length=struct.unpack('<4sII',pak.read(12));assert magic==b'PACK'
        pak.seek(offset);directory=pak.read(length)
        for at in range(0,length,64):
            name,position,size=struct.unpack('<56sII',directory[at:at+64]);name=name.rstrip(b'\0')
            if not name.endswith(b'.pcx'):continue
            pak.seek(position);blob=pak.read(size)
            @READ
            def read(_context,offset,out,n):
                assert n<=4096 and offset+n<=len(blob)
                c.memmove(out,blob[offset:offset+n],n);return 1
            source=Source(None,len(blob),read);info=Info();assert lib.port_pcx_header(c.byref(source),c.byref(info)),name
            output=(c.c_uint8*(info.width*info.height))();palette=(c.c_uint8*768)()
            assert lib.port_pcx_pixels(c.byref(source),c.byref(info),output,len(output)),name
            assert lib.port_pcx_palette(c.byref(source),palette),name
            # Independent full-file reference: expand runs, then remove row padding.
            expanded=bytearray();cursor=128;total=info.stride*info.height
            while len(expanded)<total:
                value=blob[cursor];cursor+=1
                if value>=192:
                    n=value&63;value=blob[cursor];cursor+=1
                else:n=1
                expanded.extend(bytes([value])*n)
            expected=b''.join(expanded[y*info.stride:y*info.stride+info.width] for y in range(info.height))
            assert bytes(output)==expected and bytes(palette)==blob[-768:],name
            digest.update(name+b'\0'+expected+blob[-768:]);count+=1;pixel_count+=len(expected)
    assert count>0
    print(json.dumps({'passed':True,'images':count,'pixels':pixel_count,'pixel_palette_sha256':digest.hexdigest()},indent=2))
if __name__=='__main__':main()
