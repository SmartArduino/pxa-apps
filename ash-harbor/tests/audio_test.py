"""Validate generated clip coverage, clipping, formats and music loop joins."""
from pathlib import Path
import json,hashlib,subprocess,re
import numpy as np

root=Path(__file__).resolve().parents[1]
effects=json.loads((root/'tools/audio-prompts.json').read_text())
voices=json.loads((root/'tools/voice-prompts.json').read_text())
assert len(effects['effects'])==22 and len(effects['music'])==7
for name in effects['effects']:
    p=root/'assets/audio'/(name+'.pcm');data=p.read_bytes()
    a=(np.frombuffer(data,dtype='u1').astype(float)-128)/128
    assert 0<len(data)<=16000 and .005<float(np.sqrt(np.mean(a*a)))<.4,name
    assert abs(a).max()<.99,name
    assert hashlib.sha256(data).hexdigest()==effects['processed'][name]['sha256']
bank_count=int(re.search(r'voice_files\[(\d+)\]',(root/'voice_data.hpp').read_text()).group(1))
if 'base' in voices['models']:
    selected={'yan':'B','lan':'A','zhen':'A','ling':'B'}
    for cast in voices['cast']:
        assert cast['selection']['take']==selected[cast['id']]
        reference=root/'art/audio/voices'/(cast['id']+'-reference.flac')
        assert hashlib.sha256(reference.read_bytes()).hexdigest()==cast['selection']['sha256'],(cast['id'],'Selected casting reference changed')
for cast in voices['cast'][:bank_count]:
    assert len(cast['lines'])==15
    for cue in cast['lines']:
        name=cast['id']+'-'+cue;info=voices['processed'][name];p=root/'assets/audio/voices'/info.get('runtime_file',name+'.s16');data=p.read_bytes()
        a=(np.frombuffer(data,dtype='u1').astype(float)-128)/128 if p.suffix=='.pcm' else np.frombuffer(data,dtype='<i2').astype(float)/32768
        assert (.08 if cue in cast['nonverbal_limits'] else .15)<len(a)/16000<4.5 and .015<np.sqrt(np.mean(a*a))<.3,name
        if cue.startswith('hurt_'):
            assert len(a)/16000>=.25,(name,'Pain call truncated to a transient')
        if voices.get('complete_nonverbal_takes'):
            assert p.suffix=='.s16' and info['encoding']=='s16le',name
            if cue in cast['nonverbal_limits']:assert len(a)/16000<=cast['nonverbal_limits'][cue],name
        assert abs(a).max()<.9 and abs(a.mean())<.01,name
        assert hashlib.sha256(data).hexdigest()==voices['processed'][name]['sha256']
for name,spec in effects['music'].items():
    p=root/'assets/audio'/(name+'.ogg');assert p.read_bytes().startswith(b'OggS')
    if spec['loop']:
        data=subprocess.check_output(['ffmpeg','-v','error','-i',str(root/'art/audio'/(name+'.flac')),'-f','f32le','-'])
        a=np.frombuffer(data,dtype='<f4')
        assert abs(a[0]-a[-1])<np.quantile(abs(np.diff(a)),.99)*2,name
core=['ui_select','hit_light','hit_heavy','block','whoosh_punch','whoosh_kick']
assert sum((root/'assets/audio'/(name+'.pcm')).stat().st_size for name in core)<32768
print(f'22 effects, {bank_count*15} packaged character lines and {len(effects['music'])} music tracks: coverage, levels, hashes, limits and loop joins passed.')
if bank_count<len(voices['cast']):print('Voice banks awaiting source import:',','.join(c['id'] for c in voices['cast'][bank_count:]))
