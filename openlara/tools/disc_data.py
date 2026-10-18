"""Read PC TR1 PHD files from ISO/GOG/BIN images without modifying the image."""
from pathlib import Path
import struct
from game_data import catalog, LEVEL_CAPACITY


def extract_phd(source, destination):
    source, destination = Path(source), Path(destination)
    names = {e['file'].replace('.pkd','.phd') for e in catalog()}
    found = []
    with source.open('rb') as image:
        total = source.stat().st_size
        layout = None
        for stride, skip in ((2048,0),(2352,16),(2352,24),(2336,8)):
            image.seek(16*stride+skip)
            pvd = image.read(2048)
            if len(pvd)==2048 and pvd[:7]==b'\x01CD001\x01':
                layout = (stride,skip)
                break
        if layout is None:
            raise ValueError('no supported ISO9660 PC data track found in '+str(source))
        stride, skip = layout

        def read_extent(lba, count, limit):
            if count<0 or count>limit or (lba+(count+2047)//2048)*stride>total:
                raise ValueError('invalid or oversized ISO extent')
            out = bytearray()
            while len(out)<count:
                image.seek((lba+len(out)//2048)*stride+skip)
                data = image.read(min(2048,count-len(out)))
                if not data:
                    raise ValueError('truncated ISO sector')
                out.extend(data)
            return bytes(out)

        visited = set()

        def directory(lba, count, depth=0):
            if (lba,count) in visited:
                return
            if depth>8 or len(visited)>=256:
                raise ValueError('ISO directory nesting limit exceeded')
            visited.add((lba,count))
            data = read_extent(lba,count,1024*1024)
            pos = 0
            while pos<len(data):
                length = data[pos]
                if not length:
                    pos = (pos//2048+1)*2048
                    continue
                if length<34 or length>len(data)-pos or pos%2048+length>2048:
                    raise ValueError('invalid ISO directory record')
                record = data[pos:pos+length]
                pos += length
                n = record[32]
                if 33+n>length:
                    raise ValueError('invalid ISO filename')
                name = record[33:33+n]
                if name in (b'\0',b'\x01'):
                    continue
                name = name.decode('ascii',errors='ignore').split(';')[0].lower()
                extent = struct.unpack_from('<I',record,2)[0]
                size = struct.unpack_from('<I',record,10)[0]
                flags = record[25]
                if flags&2:
                    directory(extent,size,depth+1)
                elif name in names:
                    if flags&128 or name in found:
                        raise ValueError('multiple ISO extents for '+name)
                    destination.mkdir(parents=True,exist_ok=True)
                    (destination/name).write_bytes(read_extent(extent,size,LEVEL_CAPACITY))
                    found.append(name)
        directory(struct.unpack_from('<I',pvd,158)[0],struct.unpack_from('<I',pvd,166)[0])
    if not found:
        raise ValueError('disc image contains no recognized PC TR1 PHD files')
    return {'sector_bytes':stride,'data_offset':skip,'files':sorted(found)}
