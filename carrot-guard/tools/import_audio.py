#!/usr/bin/env python3
"""Export this game's ComfyUI sources to bounded PXA audio resources.

Requires ffmpeg and numpy. Original model outputs and API workflows remain in
art/audio; this exporter never reads another game's audio or song tables.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import numpy as np

APP = Path(__file__).resolve().parents[1]
SOURCE = APP / 'art/audio'
ASSETS = APP / 'assets'
LIMITS = {'tap': .12, 'begin': .5, 'build': .28, 'upgrade': .65, 'sell': .35,
          'coin': .3, 'alert': .38, 'freeze': .8, 'fire': .15, 'ice': .24,
          'firework': .4, 'clear': .5, 'win': 1.5, 'lose': 1.2}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def decode(path, rate):
    raw = subprocess.check_output([
        'ffmpeg', '-v', 'error', '-i', str(path), '-ac', '1', '-ar', str(rate),
        '-af', f'highpass=f=75,lowpass=f={min(6800, rate // 2 - 100)}',
        '-f', 'f32le', '-'])
    return np.frombuffer(raw, dtype='<f4').copy()


def export():
    result = {'schema': 'carrot-audio-1', 'reference_audio': None, 'effects': {}}
    for name, limit in LIMITS.items():
        source = SOURCE / f'{name}_00001.flac'
        x = decode(source, 16000)
        energy = np.convolve(x*x, np.ones(160)/160, 'same')
        active = np.flatnonzero(energy > max(1e-9, energy.max()*.025))
        if not len(active):
            raise ValueError(f'Generated sound is silent: {source}')
        start, end = max(0, int(active[0])-80), min(len(x), int(active[-1])+160)
        x = x[start:min(end, start+int(limit*16000))]
        x -= x.mean()
        x *= .62 / max(1e-8, float(np.max(np.abs(x))))
        fade, tail = min(80, len(x)//3), min(240, len(x)//3)
        x[:fade] *= np.linspace(0, 1, fade)
        x[-tail:] *= np.linspace(1, 0, tail)
        dest = ASSETS / f'sfx-{name}.s16'
        dest.write_bytes(np.rint(x*32767).astype('<i2').tobytes())
        result['effects'][name] = {
            'source': str(source.relative_to(APP)), 'source_sha256': sha(source),
            'path': str(dest.relative_to(APP)), 'sample_rate': 16000, 'channels': 1,
            'duration': len(x)/16000, 'trim_start': start/16000,
            'peak': float(np.abs(x).max()), 'rms': float(np.sqrt(np.mean(x*x))),
            'sha256': sha(dest)}
    source = SOURCE / 'patrol-theme_00001.flac'
    x = decode(source, 22050)
    # Fold the ending into the opening: equal-time crossfade, no hard sample
    # discontinuity and no silence added at the looping boundary.
    overlap = int(.35 * 22050)
    blend = np.linspace(0, 1, overlap)
    opening = x[-overlap:]*(1-blend) + x[:overlap]*blend
    x = np.concatenate([opening, x[overlap:-overlap]])
    x -= x.mean()
    x *= .72 / max(1e-8, float(np.max(np.abs(x))))
    dest = ASSETS / 'patrol.ogg'
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', '22050',
                    '-ac', '1', '-i', '-', '-c:a', 'libvorbis', '-q:a', '2',
                    '-map_metadata', '-1', str(dest)], input=x.astype('<f4').tobytes(),
                   check=True)
    result['music'] = {
        'source': str(source.relative_to(APP)), 'source_sha256': sha(source),
        'path': str(dest.relative_to(APP)), 'sha256': sha(dest),
        'sample_rate': 22050, 'channels': 1, 'duration': len(x)/22050,
        'loop_crossfade_seconds': .35}
    (SOURCE/'manifest.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f'Exported {len(LIMITS)} generated effects and {len(x)/22050:.2f}s music')


if __name__ == '__main__':
    export()
