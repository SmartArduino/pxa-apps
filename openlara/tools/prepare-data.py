#!/usr/bin/env python3
"""Fetch pinned upstream GBA data, verify SHA-256, optionally copy to app-scoped SD."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import urllib.request

COMMIT='3b268ca4bf986c380f44a26ffd9218bf6d376249'
FILES={
    'TITLE.SCR':'0c50621380fb9f8f6d4ba37f3c9299a748dda25fe43a8c86878078c77953e60c',
    'TRACKS.AD4':'80931c24839936cf35bd17e135ef83d3a8b1221072cc05b7d9948a7e9d85ba38',
    'TITLE.PKD':'556fce53180036d2660a149370d9da89d8a94228f82109b703d6bbb8bfca368b',
    'GYM.PKD':'c9b32008bc976f4049962d0df61e75b818dc487b18da7d4fb1de5129ff45d715',
    'LEVEL1.PKD':'511bbc9dc65f71a7aa85307faf6f8e100ff42e04b4f561fc9a036afe5ae14d77',
    'LEVEL2.PKD':'8d63f2a161a229b802befcf6f026c534a08dcea617ba38afa64727007b7b552d',
}
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--upstream',type=Path,help='existing OpenLara checkout, instead of downloading')
p.add_argument('--port',help='explicit physical PXADB port; omit to prepare simulator assets only')
p.add_argument('--baud',default='2000000')
p.add_argument('--serial',help='required expected device identity when copying to SD')
p.add_argument('--publisher-id',default='17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171',
               help='lineage root publisher key ID; change this when using your own signing identity')
p.add_argument('--pxadb-dir',type=Path,default=Path(__file__).resolve().parents[4]/'tools/pxadb')
a=p.parse_args()
if a.port and not a.serial:p.error('--serial is required with --port')
publisher=bytes.fromhex(a.publisher_id)
if len(publisher)!=32:p.error('--publisher-id must encode 32 bytes')
app=b'pxa-openlara'
key=hashlib.sha256(b'PXA-APPKEY-1'+publisher+struct.pack('<H',len(app))+app).hexdigest()
root=Path(__file__).resolve().parents[1]
assets=root/'assets-simulator/game';assets.mkdir(parents=True,exist_ok=True)
provenance=[]
for name,expected in FILES.items():
    dest=assets/name.lower()
    url=f'https://raw.githubusercontent.com/XProger/OpenLara/{COMMIT}/src/platform/gba/data/{name}'
    if dest.exists() and hashlib.sha256(dest.read_bytes()).hexdigest()==expected:
        data=dest.read_bytes()
    elif a.upstream:
        data=(a.upstream/'src/platform/gba/data'/name).read_bytes()
    else:
        with urllib.request.urlopen(url,timeout=60) as response:data=response.read(4000000)
    if hashlib.sha256(data).hexdigest()!=expected:raise RuntimeError('SHA-256 mismatch: '+name)
    dest.write_bytes(data)
    provenance.append({'file':dest.name,'bytes':len(data),'sha256':expected,'source':url})
    print(f'{dest.name}: {len(data)} bytes verified',flush=True)
(assets.parent/'imported-game-data.json').write_text(json.dumps({
    'upstream_commit':COMMIT,'scope':'upstream game data; separate from the BSD engine license',
    'files':provenance},indent=2)+'\n')
destination='/sdcard/PXA/media/'+key
print('App media directory: '+destination,flush=True)
if a.port:
    os.environ['PXADB_BAUD']=a.baud
    sys.path.insert(0,str(a.pxadb_dir));import pxadb
    with pxadb.PxaDbClient(a.port,15) as client:
        hello=client.hello()
        if pxadb.info_properties(hello).get('serial')!=a.serial:raise RuntimeError('Unexpected device; no writes performed')
        files=pxadb.NormalFsClient(client)
        # Every directory is in this app's public media area; no system unlock,
        # format, removal of other apps, or private save directory is involved.
        for folder in ('/sdcard/PXA','/sdcard/PXA/media',destination):
            try:files.list(folder)
            except pxadb.PxaDbRejectedError:files.mkdir(folder)
        for name in FILES:
            files.put(assets/name.lower(),destination+'/'+name.lower())
