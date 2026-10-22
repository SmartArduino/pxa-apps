#!/usr/bin/env python3
"""Generate our own cartridge diagnostics; no commercial ROM data."""
import struct,sys
from pathlib import Path
system,out=sys.argv[1:3]
vblank="--vblank" in sys.argv[3:]
if vblank and system not in ("sms","gg"):raise SystemExit("--vblank is for Z80 platforms")
if system in ('sms','gg'):
    rom=bytearray(32768);code=bytearray();origin=0x100 if vblank else 0
    def emit(*data):code.extend(data)
    def output(value,port):emit(0x3e,value,0xd3,port)
    emit(0xf3,0x31,0xf0,0xdf) # DI; LD SP,$dff0
    # Mode 4, display enabled. No BIOS is required.
    for register,value in [(0,4),(1,0x60 if vblank else 0x40),(2,0x0e),(5,0x7e),(6,0),(7,1),(8,0),(9,0)]:
        output(value,0xbf);output(0x80|register,0xbf)
    # The background entry uses CRAM 17; fill every entry to also test RGB packing.
    output(0,0xbf);output(0xc0,0xbf)
    for _ in range(32):
        if system=='gg':output(0x00,0xbe);output(0x0f,0xbe) # full blue, 12-bit palette
        else:output(0x30,0xbe) # full blue, 6-bit palette
    # PSG square tone and noise; timer and audio keep advancing during skips.
    for value in (0x84,0x12,0x90,0xe4,0xf4):output(value,0x7f)
    # A stable display with frame-independent input writes to cartridge RAM.
    if vblank:emit(0xfb) # EI; wait for VBlank instead of polling throughout the frame
    loop=origin+len(code);emit(0xdb,0xdc,0x32,0x00,0xc0)
    if vblank:
        emit(0x76,0x3a,0x01,0xc0,0x3c,0x32,0x01,0xc0,0xd3,0xbf,0x3e,0x88,0xd3,0xbf)
    emit(0xc3,loop&255,loop>>8)
    rom[origin:origin+len(code)]=code
    if vblank:
        rom[:3]=bytes([0xc3,0x00,0x01]);rom[0x38:0x3f]=bytes([0xf5,0xdb,0xbf,0xf1,0xfb,0xed,0x4d]);rom[0x66:0x68]=bytes([0xed,0x45])
    rom[0x7ff0:0x7ff8]=b'TMR SEGA';rom[0x7fff]=0x6c if system=='gg' else 0x4c
elif system=='snes':
    rom=bytearray(32768);code=bytearray([0x78,0xd8])
    def write(value,address):code.extend([0xa9,value,0x8d,address&255,address>>8])
    write(0x80,0x2100);write(0,0x2121);write(0,0x2122);write(0x7c,0x2122);write(0x0f,0x2100)
    loop=0x8000+len(code);code.extend([0x4c,loop&255,loop>>8]);rom[:len(code)]=code
    rom[0x7fc0:0x7fd5]=b'PXA COLOR DIAGNOSTIC'.ljust(21);rom[0x7fd5:0x7fdc]=bytes([0x20,0,5,0,1,0,0])
    struct.pack_into('<HH',rom,0x7fdc,0xffff,0);struct.pack_into('<HHHH',rom,0x7ff8,0x8000,0x8000,0x8000,0x8000)
elif system=='pce':
    rom=bytearray(32768);code=bytearray([0x78,0xd8,0xa2,0xff,0x9a])
    def store(value,address):code.extend([0xa9,value,0x8d,address&255,address>>8])
    for reg,low,high in [(11,31,0),(12,2,15),(13,223,0),(5,0,0)]:code.extend([0x03,reg,0x13,low,0x23,high])
    for address,value in [(0x402,0),(0x403,0),(0x404,7),(0x405,0)]:store(value,address)
    loop=0xe000+len(code);code.extend([0xad,0x00,0x10,0x8d,0x00,0x20,0x4c,loop&255,loop>>8]);rom[:len(code)]=code
    struct.pack_into('<H',rom,0x1ffe,0xe000)
elif system=='md':
    rom=bytearray(0x20000);struct.pack_into('>II',rom,0,0x00fffe00,0x200)
    rom[0x100:0x110]=b'SEGA MEGA DRIVE '
    rom[0x120:0x150]=b'PXA COLOR DIAGNOSTIC'.ljust(48)
    rom[0x150:0x180]=b'PXA COLOR DIAGNOSTIC'.ljust(48)
    struct.pack_into('>IIII',rom,0x1a0,0,len(rom)-1,0xff0000,0xffffff)
    rom[0x1b0:0x1b4]=b'RA'+bytes([0xf8,0x20]);struct.pack_into('>II',rom,0x1b4,0x200001,0x201fff)
    rom[0x1f0:0x200]=b'U'.ljust(16)
    code=bytearray.fromhex('46fc2700')
    def word(v,address):code.extend(bytes.fromhex('33fc')+struct.pack('>HI',v,address))
    def long(v,address):code.extend(bytes.fromhex('23fc')+struct.pack('>II',v,address))
    def byte(v,address):code.extend(bytes.fromhex('13fc')+struct.pack('>HI',v,address))
    long(0x53454741,0xa14000);word(0x100,0xa11100);word(0,0xa11200)
    for reg,value in [(0,4),(1,0x44),(2,0x30),(3,0x3c),(4,7),(5,0x7c),(7,0),(10,255),(11,0),(12,0x81),(13,0x3f),(15,2),(16,1)]:word(0x8000|(reg<<8)|value,0xc00004)
    long(0xc0000000,0xc00004);word(0x0e00,0xc00000)
    for v in (0x84,0x12,0x90,0xe4,0xf4):byte(v,0xc00011)
    byte(0x40,0xa10009);byte(0x40,0xa10003)
    code.extend(bytes.fromhex('103900a1000313c000ff000060f2'))
    rom[0x200:0x200+len(code)]=code
else:raise SystemExit('Unknown cartridge system: '+system)
Path(out).write_bytes(rom)
