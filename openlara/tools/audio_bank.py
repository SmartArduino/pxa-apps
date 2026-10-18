"""Build bounded-memory PCM8 banks from original PC/PSX numbered audio."""
from pathlib import Path
import re
import shutil
import struct
import subprocess
from game_data import fingerprint

# OpenLara src/gameflow.h: PSX trigger number -> PC numbered soundtrack file.
PC_MAPPING = [2,2,2,11,12,3,13,14,15,16,17,18,19,60,20,21,22,23,24,25,26,27,
              7,8,9,10,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,
              47,48,49,50,51,52,53,54,55,56,57,58,59,28,4,5,6]


def make_bank(source, destination, numbering='auto'):
    if not shutil.which('ffmpeg'):
        raise ValueError('ffmpeg is required to import original music')
    source = Path(source)
    if not source.is_dir():
        raise ValueError(f'audio directory does not exist: {source}')
    files = {}
    for path in source.rglob('*'):
        if not path.is_file() or path.suffix.lower() not in ('.mp3','.ogg','.wav','.flac'):
            continue
        match = re.fullmatch(r'(track[_ -]?)?(\d+)', path.stem, re.IGNORECASE)
        if not match:
            continue
        prefixed, number = match.groups()
        mode = 'psx' if numbering == 'psx' or (numbering == 'auto' and prefixed) else 'pc'
        key = (mode, int(number))
        if key in files:
            raise ValueError(f'duplicate audio track {key}: {files[key].name} and {path.name}')
        files[key] = path
    if not files:
        raise ValueError('no numbered MP3/OGG/WAV/FLAC tracks found')
    table = bytearray(2048)
    converted, entries = {}, []
    destination = Path(destination)
    with destination.open('wb') as bank:
        bank.write(table)
        for track in range(1,64):
            path = files.get(('psx',track))
            if path is None and track < len(PC_MAPPING):
                path = files.get(('pc',PC_MAPPING[track]))
            if path is None:
                continue
            if path not in converted:
                result = subprocess.run(['ffmpeg','-nostdin','-v','error','-i',str(path),
                                         '-vn','-ac','1','-ar','10512','-f','s8','-c:a','pcm_s8','pipe:1'],
                                        check=True, stdout=subprocess.PIPE)
                pcm = result.stdout
                if not pcm or len(pcm) >= 0x80000000:
                    raise ValueError(f'invalid decoded audio length: {path}')
                offset = bank.tell()
                bank.write(pcm)
                bank.write(b'\0' * (-len(pcm) % 4))
                converted[path] = (offset,len(pcm))
            offset, size = converted[path]
            struct.pack_into('<II',table,track*8,offset,size|0x80000000)
            entries.append({'track':track,'codec':'pcm_s8','rate':10512,'samples':size,
                            'source':{'file':path.name,**fingerprint(path)}})
        bank.seek(0)
        bank.write(table)
    return {'format':'openlara-ad4-pcm8-directory-1','numbering':numbering,
            'tracks':entries,**fingerprint(destination)}
