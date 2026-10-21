"""Generate our original NROM diagnostic; no commercial game data. CC0."""
import pathlib
import sys

code = bytearray()
labels = {}
fixups = []
def emit(*values): code.extend(values)
def label(name): labels[name] = 0xC000 + len(code)
def branch(op, name):
    emit(op, 0); fixups.append((len(code)-1, name, True))
def absolute(op, name):
    emit(op, 0, 0); fixups.append((len(code)-2, name, False))
def sta(addr): emit(0x8D, addr & 255, addr >> 8)
def lda(value): emit(0xA9, value)

label('reset')
emit(0x78, 0xD8, 0xA2, 0xFF, 0x9A)  # SEI, CLD, LDX, TXS
lda(0); sta(0x2000); sta(0x2001); sta(0x4010)
lda(0x40); sta(0x4017)
label('warmup'); emit(0x2C, 0x02, 0x20); branch(0x10, 'warmup')
lda(0x3F); sta(0x2006); lda(0); sta(0x2006)
emit(0xA2, 0)
label('palette'); absolute(0xBD, 'colors'); sta(0x2007)
emit(0xE8, 0xE0, 32); branch(0xD0, 'palette')
lda(0x20); sta(0x2006); lda(0); sta(0x2006)
emit(0xA0, 4, 0xA2, 0); lda(1)
label('tiles'); sta(0x2007); emit(0xE8); branch(0xD0, 'tiles')
emit(0x88); branch(0xD0, 'tiles')
lda(0xFF); emit(0xA2, 0)
label('oam'); emit(0x9D,0x00,0x02,0xE8); branch(0xD0, 'oam')
lda(100); sta(0x0200); lda(2); sta(0x0201); lda(0); sta(0x0202)
lda(120); sta(0x0203); lda(0); sta(0x2005); sta(0x2005)
lda(0x1E); sta(0x2001)
lda(1); sta(0x4015); lda(0xBF); sta(0x4000); lda(0xFD); sta(0x4002); lda(8); sta(0x4003)
label('loop')
emit(0x2C, 0x02, 0x20); branch(0x10, 'loop')
lda(1); sta(0x4016); lda(0); sta(0x4016); emit(0x85, 0, 0xA2, 8)
label('readpad'); emit(0xAD,0x16,0x40,0x4A,0x66,0,0xCA); branch(0xD0,'readpad')
emit(0xA5,0); sta(0x6001); emit(0xEE,0x02,0x60)
emit(0xA5,0,0x29,128); branch(0xF0,'left'); emit(0xEE,0x03,0x02)
label('left'); emit(0xA5,0,0x29,64); branch(0xF0,'up'); emit(0xCE,0x03,0x02)
label('up'); emit(0xA5,0,0x29,16); branch(0xF0,'down'); emit(0xCE,0x00,0x02)
label('down'); emit(0xA5,0,0x29,32); branch(0xF0,'dma'); emit(0xEE,0x00,0x02)
label('dma'); lda(0); sta(0x2003); lda(2); sta(0x4014)
lda(0x3F); sta(0x2006); lda(0); sta(0x2006)
emit(0xA5,0,0x29,1); branch(0xF0,'blue'); lda(0x16); branch(0xD0,'color')
label('blue'); lda(0x01)
label('color'); sta(0x2007); lda(0); sta(0x2000); sta(0x2005); sta(0x2005)
label('endblank'); emit(0x2C,0x02,0x20); branch(0x30,'endblank')
absolute(0x4C,'loop')
label('colors')
emit(*([0x01,0x21,0x11,0x30]*4+[0x01,0x30,0x16,0x27]*4))
for offset, name, relative in fixups:
    address = labels[name]
    if relative:
        delta=address-(0xC000+offset+1)
        assert -128 <= delta <= 127
        code[offset]=delta & 255
    else: code[offset:offset+2]=address.to_bytes(2,'little')
prg=code+bytearray([0xEA])*(16384-len(code))
prg[-6:]=labels['reset'].to_bytes(2,'little')*3
chr_data=bytearray(8192)
chr_data[16:32]=bytes([0xAA,0x55]*4+[0]*8)
chr_data[32:48]=bytes([0x3C,0x7E,0xDB,0xFF,0xFF,0xDB,0x66,0x3C]+[0]*8)
rom=b'NES\x1a'+bytes([1,1,2,0])+bytes(8)+prg+chr_data
path=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'pxa-demo.nes')
path.parent.mkdir(parents=True, exist_ok=True)
path.write_bytes(rom)
print(path, len(rom))
