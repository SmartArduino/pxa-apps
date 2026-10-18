"""Import original PC data without changing geometry, samples or game scripts."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import tempfile
import urllib.request
from game_data import ROOT, catalog, fingerprint, inspect_phd
from audio_bank import make_bank
from disc_data import extract_phd

OPENLARA_COMMIT = '3b268ca4bf986c380f44a26ffd9218bf6d376249'
DEMO_COMMIT = '121815b61ae16884a6f775dc0966173cea5e891f'
BASE = {
    'title.scr':'0c50621380fb9f8f6d4ba37f3c9299a748dda25fe43a8c86878078c77953e60c',
    'tracks.ad4':'80931c24839936cf35bd17e135ef83d3a8b1221072cc05b7d9948a7e9d85ba38',
}
DEMO = {
    'title.phd':'e1f4a11b7566fc9ba3be5208806915bf5bfc30a890ce0e610b5be697d664580b',
    'level2.phd':'efaae10c309b266b6ad0c27369b9a4e227ae9d62b9aff6d5f8e076a535e83199',
}


def fetch(url, expected, cached=None):
    if cached and cached.is_file():
        data = cached.read_bytes()
        if hashlib.sha256(data).hexdigest() == expected:
            return data
    with urllib.request.urlopen(url,timeout=45) as response:
        data = response.read(7*1024*1024)
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f'SHA-256 mismatch: {url}')
    return data


def prepare(destination, source=None, audio=None, numbering='auto', upstream=None,
            demo=False, allow_partial=False):
    destination = Path(destination).resolve()
    if source and Path(source).is_file():
        original = Path(source)
        with tempfile.TemporaryDirectory(prefix='openlara-disc-') as temporary:
            disc = extract_phd(original,temporary)
            manifest = prepare(destination,temporary,audio,numbering,upstream,demo,allow_partial)
            manifest['disc_source'] = {'file':original.name,**fingerprint(original),**disc}
            (destination/'data-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
            return manifest
    entries = catalog()
    manifest_path = destination/'data-manifest.json'
    if not source and not demo and manifest_path.exists():
        manifest = json.loads(manifest_path.read_text())
        if manifest.get('edition') == 'tr1-pc-classic':
            for entry in manifest['files']:
                if fingerprint(destination/entry['file']) != {k:entry[k] for k in ('bytes','sha256')}:
                    raise ValueError('installed media changed: '+entry['file'])
            if not audio and any(f['file']=='title.pal' for f in manifest['files']):
                return manifest
            # Reuse the original levels while replacing only their music bank.
            source = destination
            allow_partial = not manifest['campaign_data_complete']
    originals = {}
    previous_sources = {}
    if source and Path(source).resolve()==destination and manifest_path.exists():
        previous_sources={e['file']:e['source'] for e in json.loads(manifest_path.read_text()).get('levels',[])}
    if source:
        source = Path(source)
        if not source.is_dir():
            raise ValueError(f'game data directory does not exist: {source}')
        for path in source.rglob('*'):
            if path.is_file() and path.suffix.lower() == '.phd':
                key = path.stem.lower()
                if key in originals and fingerprint(path) != fingerprint(originals[key]):
                    raise ValueError(f'conflicting copies of {path.name}; select a single DATA directory')
                originals[key] = path
        missing = [e['file'].replace('.pkd','.PHD') for e in entries
                   if Path(e['file']).stem not in originals]
        if missing and not allow_partial:
            raise ValueError('missing original PC data: '+', '.join(missing)+
                             '\nNo media was changed; --allow-partial is for development only.')
    destination.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.openlara-classic-',dir=destination.parent) as temporary:
        stage = Path(temporary)
        levels, files, mask = [], [], 0
        for entry in entries:
            name = entry['file'].replace('.pkd','.phd')
            target = stage/name
            if source:
                original = originals.get(Path(name).stem)
                if original is None:
                    continue
                shutil.copyfile(original,target)
                provenance = previous_sources.get(name,{'file':original.name})
            elif name in DEMO:
                url = f'https://raw.githubusercontent.com/LostArtefacts/TRX-data/{DEMO_COMMIT}/tr1-demo-pc/ship/games/tr1-demo-pc/levels/{name}'
                target.write_bytes(fetch(url,DEMO[name],destination/name))
                provenance = {'url':url,'commit':DEMO_COMMIT}
            else:
                continue
            info = inspect_phd(target)
            files.append({'file':name,**fingerprint(target)})
            levels.append({**entry,'file':name,**info,'source':provenance})
            mask |= 1 << entry['id']
        if not mask&1 or not any(e['kind']==2 for e in levels):
            raise ValueError('a title and at least one playable level are required')
        for name, expected in BASE.items():
            target = stage/name
            if name=='tracks.ad4' and audio:
                music = make_bank(audio,target,numbering)
            else:
                local = Path(upstream)/'src/platform/gba/data'/name.upper() if upstream else None
                url = f'https://raw.githubusercontent.com/XProger/OpenLara/{OPENLARA_COMMIT}/src/platform/gba/data/{name.upper()}'
                data = fetch(url,expected,local if local and local.is_file() else destination/name)
                target.write_bytes(data)
                if name=='tracks.ad4':
                    music = {'format':'legacy-gba-ad4','source':url,'full_soundtrack':False,
                             **fingerprint(target)}
            files.append({'file':name,**fingerprint(target)})
        # TITLE.SCR uses the GBA title's quantized palette, not TITLE.PHD's
        # unrelated model palette. Keep the verified art palette as media.
        name='TITLE.PKD'
        url=f'https://raw.githubusercontent.com/XProger/OpenLara/{OPENLARA_COMMIT}/src/platform/gba/data/{name}'
        cached=Path(upstream)/'src/platform/gba/data'/name if upstream else destination/'title.pkd'
        packed=fetch(url,'556fce53180036d2660a149370d9da89d8a94228f82109b703d6bbb8bfca368b',cached)
        offset=struct.unpack_from('<I',packed,32)[0]
        palette=struct.unpack_from('<256H',packed,offset)
        rgb565=[((c&31)<<11)|(((c>>5)&31)<<6)|(((c>>5)&31)>>4<<5)|((c>>10)&31) for c in palette]
        (stage/'title.pal').write_bytes(struct.pack('<256H',*rgb565))
        files.append({'file':'title.pal',**fingerprint(stage/'title.pal')})
        (stage/'levels.idx').write_bytes(struct.pack('<4sIII',b'OLCI',1,mask,len(entries)))
        files.append({'file':'levels.idx',**fingerprint(stage/'levels.idx')})
        missing = [e['file'].replace('.pkd','.phd') for e in entries if not mask&(1<<e['id'])]
        manifest = {'format':'openlara-pxa-data-1','edition':'tr1-pc-classic',
                    'engine_commit':OPENLARA_COMMIT,'campaign_data_complete':not missing,
                    'campaign_levels':sum(e['kind']==2 for e in levels),'missing':missing,
                    'scope':'original game assets; separate from the BSD engine license',
                    'levels':levels,'music':music,'files':files}
        (stage/'data-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        destination.mkdir(parents=True,exist_ok=True)
        for entry in files:
            os.replace(stage/entry['file'],destination/entry['file'])
        os.replace(stage/'data-manifest.json',manifest_path)
    return manifest
