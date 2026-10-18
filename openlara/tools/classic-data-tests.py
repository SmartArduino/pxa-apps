#!/usr/bin/env python3
"""Exercise import bounds, atomic failure, raw CD layouts and streamed PCM fixtures."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
import wave
from game_data import inspect_phd, fingerprint
from classic_data import prepare
from disc_data import extract_phd
from audio_bank import make_bank


def record(name,lba,size,directory=False):
    name = name if isinstance(name,bytes) else name.encode('ascii')
    n = 33+len(name)+(len(name)%2==0)
    out = bytearray(n)
    out[0] = n
    struct.pack_into('<I',out,2,lba)
    struct.pack_into('>I',out,6,lba)
    struct.pack_into('<I',out,10,size)
    struct.pack_into('>I',out,14,size)
    out[25] = 2 if directory else 0
    out[28:32] = b'\x01\0\0\x01'
    out[32] = len(name)
    out[33:33+len(name)] = name
    return out


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--media',type=Path,required=True,help='prepared PC demo media directory')
    parser.add_argument('--upstream',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    report={}
    originals={name:(args.media/name).read_bytes() for name in ('title.phd','level2.phd')}
    for name in originals:
        inspect_phd(args.media/name)
    with tempfile.TemporaryDirectory(prefix='openlara-data-test-') as temporary:
        root=Path(temporary)
        source=root/'original';source.mkdir()
        for name,data in originals.items():
            (source/name).write_bytes(data)
        destination=root/'media'
        try:
            prepare(destination,source,upstream=args.upstream)
        except ValueError as error:
            assert 'missing original PC data' in str(error)
        else:
            raise AssertionError('incomplete campaign was accepted as complete')
        assert not destination.exists()
        manifest=prepare(destination,source,upstream=args.upstream,allow_partial=True)
        assert not manifest['campaign_data_complete'] and manifest['campaign_levels']==1
        assert next(level for level in manifest['levels'] if level['id']==3)['lara_entities']==[
            {'index':91,'room':90,'position':[78336,-1024,12800],'rotation':0}]
        report['public_demo_lara_entity_identified']=True
        before={p.name:fingerprint(p) for p in destination.iterdir()}
        (source/'level2.phd').write_bytes(originals['level2.phd'][:-20])
        try:
            prepare(destination,source,upstream=args.upstream,allow_partial=True)
        except ValueError:
            pass
        else:
            raise AssertionError('truncated PHD was accepted')
        assert before=={p.name:fingerprint(p) for p in destination.iterdir()}
        report['strict_completeness_and_failed_import_keeps_media']=True
        # Build a minimal ISO with the real public demo bytes, then wrap it in
        # original Mode 1 and Mode 2 raw CD sectors. Extraction must be exact.
        extents={};next_lba=22
        for name,data in originals.items():
            extents[name]=next_lba;next_lba+=(len(data)+2047)//2048
        iso=bytearray(next_lba*2048)
        pvd=memoryview(iso)[16*2048:17*2048];pvd[:7]=b'\x01CD001\x01'
        pvd[156:190]=record(b'\0',20,2048,True)
        iso[20*2048:20*2048+38]=record('DATA',21,2048,True)
        records=b''.join(record(name.upper()+';1',extents[name],len(data)) for name,data in originals.items())
        iso[21*2048:21*2048+len(records)]=records
        for name,data in originals.items():
            iso[extents[name]*2048:extents[name]*2048+len(data)]=data
        report['disc_layouts']=[]
        for stride,skip in ((2048,0),(2352,16),(2352,24)):
            image=root/f'disc-{stride}-{skip}.gog'
            with image.open('wb') as out:
                for pos in range(0,len(iso),2048):
                    out.write(b'\0'*skip);out.write(iso[pos:pos+2048]);out.write(b'\0'*(stride-skip-2048))
            extracted=root/f'extracted-{stride}-{skip}'
            extract_phd(image,extracted)
            for name,data in originals.items():
                assert (extracted/name).read_bytes()==data
            report['disc_layouts'].append({'sector_bytes':stride,'data_offset':skip,'byte_exact':True})
        audio=root/'audio';audio.mkdir()
        unsigned=bytes(i%256 for i in range(10512))
        with wave.open(str(audio/'02.wav'),'wb') as out:
            out.setnchannels(1);out.setsampwidth(1);out.setframerate(10512);out.writeframes(unsigned)
        pcm=args.output/'pcm-assets'/'game';pcm.mkdir(parents=True,exist_ok=True)
        for name in ('title.phd','level2.phd','levels.idx','title.scr','title.pal'):
            shutil.copyfile(args.media/name,pcm/name)
        music=make_bank(audio,pcm/'tracks.ad4')
        bank=(pcm/'tracks.ad4').read_bytes()
        offset,size=struct.unpack_from('<II',bank,2*8)
        assert size==0x80000000|len(unsigned)
        assert bank[offset:offset+len(unsigned)]==bytes(v^128 for v in unsigned)
        report['pcm_conversion_signed_and_exact']=True
        large=args.output/'large-pcm-assets'/'game';large.mkdir(parents=True,exist_ok=True)
        for name in ('title.phd','level2.phd','levels.idx','title.scr','title.pal'):
            shutil.copyfile(args.media/name,large/name)
        table=bytearray(bank[:2048]);gap=8*1024*1024
        for i in range(256):
            off,n=struct.unpack_from('<II',table,i*8)
            if n:
                struct.pack_into('<I',table,i*8,off+gap)
        with (large/'tracks.ad4').open('wb') as out:
            out.write(table);out.write(bytes(gap));out.write(bank[2048:])
        report['large_bank_bytes']=(large/'tracks.ad4').stat().st_size
        report['music']=music
    (args.output/'data-tests.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()
