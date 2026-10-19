#!/usr/bin/env python3
"""Import original PC TR1 levels and audio, or prepare the pinned public PC demo."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import time
from classic_data import prepare
from game_data import ROOT


def inbox_data(directory):
    """Choose an explicit user drop-in before falling back to the public demo."""
    directory=Path(directory)
    for name in ('DATA','data'):
        folder=directory/name
        if folder.is_dir() and any(p.is_file() and p.suffix.lower()=='.phd' for p in folder.rglob('*')):
            return folder
    images=sorted(p for p in directory.glob('*') if p.is_file() and p.suffix.lower() in ('.gog','.iso','.bin'))
    if len(images)>1:
        raise ValueError('multiple game-data disc images; select one with --game-data')
    return images[0] if images else None


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-data',type=Path,help='original DATA directory, game root, or GAME.GOG/ISO/BIN disc image')
    parser.add_argument('--audio',type=Path,help='original numbered MP3/OGG/WAV/FLAC directory')
    parser.add_argument('--audio-numbering',choices=('auto','pc','psx'),default='auto')
    parser.add_argument('--demo',action='store_true',help='explicitly replace the catalog with the public PC demo')
    parser.add_argument('--allow-partial',action='store_true',help='development only: import an incomplete original data set')
    parser.add_argument('--upstream',type=Path,help='reuse pinned title/AD4 assets from an OpenLara checkout')
    parser.add_argument('--output',type=Path,default=ROOT/'assets-simulator/game')
    parser.add_argument('--port',help='explicit PXADB port; omit to prepare simulator media only')
    parser.add_argument('--baud',default='2000000')
    parser.add_argument('--serial',help='required expected device identity when copying to SD')
    parser.add_argument('--wait-seconds',type=float,default=120,help='wait for another task to release the serial device')
    parser.add_argument('--publisher-id',default='17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171')
    parser.add_argument('--pxadb-dir',type=Path,default=ROOT.parents[2]/'tools/pxadb')
    args=parser.parse_args()
    if args.port and not args.serial:
        parser.error('--serial is required with --port')
    if args.demo and args.game_data:
        parser.error('--demo and --game-data are mutually exclusive')
    try:
        if not args.game_data and not args.demo:
            args.game_data=inbox_data(ROOT/'game-data')
            if args.game_data:
                print('Using original game-data inbox: '+str(args.game_data),flush=True)
                audio_inbox=ROOT/'game-data/audio'
                if not args.audio and any(p.is_file() and p.suffix.lower() in ('.mp3','.ogg','.wav','.flac') for p in audio_inbox.glob('*')):
                    args.audio=audio_inbox
        publisher=bytes.fromhex(args.publisher_id)
        if len(publisher)!=32:
            raise ValueError('--publisher-id must encode 32 bytes')
        manifest=prepare(args.output,args.game_data,args.audio,args.audio_numbering,
                         args.upstream,args.demo,args.allow_partial)
    except Exception as error:
        parser.exit(1,str(error)+'\n')
    app=b'pxa-openlara'
    key=hashlib.sha256(b'PXA-APPKEY-1'+publisher+struct.pack('<H',len(app))+app).hexdigest()
    destination='/sdcard/PXA/media/'+key
    print(f"Prepared {manifest['campaign_levels']}/15 campaign levels; complete_data={manifest['campaign_data_complete']}",flush=True)
    print('App media directory: '+destination,flush=True)
    if args.output.resolve()==(ROOT/'assets-simulator/game').resolve():
        (args.output.parent/'imported-game-data.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if not args.port:
        return
    os.environ['PXADB_BAUD']=args.baud
    os.environ.setdefault('PXADB_UART_UPLOAD_CHUNK','1024')
    sys.path.insert(0,str(args.pxadb_dir))
    import pxadb
    deadline=time.monotonic()+max(0,args.wait_seconds)
    while True:
        try:
            client=pxadb.PxaDbClient(args.port,30)
            break
        except OSError as error:
            if 'exclusively lock' not in str(error) or time.monotonic()>=deadline:
                raise
            print('Waiting for another task to release '+args.port,flush=True)
            time.sleep(1)
    with client:
        hello=client.hello()
        if pxadb.info_properties(hello).get('serial')!=args.serial:
            raise RuntimeError('Unexpected device; no writes performed')
        files=pxadb.NormalFsClient(client)
        for folder in ('/sdcard/PXA','/sdcard/PXA/media',destination):
            try:
                files.list(folder)
            except pxadb.PxaDbRejectedError:
                files.mkdir(folder)
        # The catalog is copied after all media. Existing private saves and
        # every other app's media remain outside this identity-scoped directory.
        names=[f['file'] for f in manifest['files'] if f['file']!='levels.idx']
        names+=['data-manifest.json','levels.idx']
        for name in names:
            files.put(args.output/name,destination+'/'+name)


if __name__=='__main__':
    main()
