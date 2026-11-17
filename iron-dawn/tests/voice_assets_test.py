#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check complete cast coverage, master/runtime identity and bounded direct-play PCM."""
from pathlib import Path
import hashlib,json,subprocess,numpy as np
app=Path(__file__).resolve().parents[1];config=json.loads((app/'tools/voice-prompts.json').read_text());total=0;refs=set();names=set()
for cast in config['cast']:
 ref=app/'art/audio/voices'/(cast['id']+'-reference.flac');h=hashlib.sha256(ref.read_bytes()).hexdigest();assert h==cast['reference_sha256'] and h not in refs;refs.add(h)
 for cue,text in cast['lines'].items():
  name=cast['id']+'-'+cue;names.add(name);info=config['processing']['lines'][name];data=(app/'assets/audio/voices'/(name+'.s16')).read_bytes();assert hashlib.sha256(data).hexdigest()==info['runtime_sha256'];a=np.frombuffer(data,dtype='<i2').astype(float)/32768
  assert len(data)==info['runtime_bytes'] and len(data)%2==0 and .3<len(a)/16000<4.5 and abs(len(a)/16000-info['duration'])<1/16000
  assert abs(a[0])<.0001 and abs(a[-1])<.0001 and np.max(abs(a))<.77 and np.sqrt(np.mean(a*a))>.025
  master=app/'art/audio/voices'/(name+'.flac');assert hashlib.sha256(master.read_bytes()).hexdigest()==info['master_sha256'];decoded=subprocess.check_output(['ffmpeg','-v','error','-i',str(master),'-f','s16le','-']);assert decoded==data;total+=len(data)
assert len(refs)==5 and len(names)==37 and names=={p.stem for p in (app/'assets/audio/voices').glob('*.s16')} and total<3*1024*1024
print(f'Five distinct immutable voice references and 37 direct-play 16kHz/S16 mono responses passed; {total} runtime bytes')
