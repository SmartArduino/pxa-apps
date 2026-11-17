#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate runtime audio bounds, loop edges and immutable delivery hashes."""
from pathlib import Path
import hashlib,json,subprocess,numpy as np
app=Path(__file__).resolve().parents[1];meta=json.loads((app/'tools/audio-prompts.json').read_text());checks=meta['processing'];total=0
for name,info in checks['effects'].items():
 p=app/'assets/audio'/str(name+'.pcm');data=p.read_bytes();assert 0<len(data)<=16000 and len(data)==round(info['duration']*16000);assert hashlib.sha256(data).hexdigest()==info['sha256'];a=np.frombuffer(data,dtype='u1').astype(float)-128;assert np.max(np.abs(a))<=91 and np.sqrt(np.mean(a*a))>.5;assert abs(a[0])<=1 and abs(a[-1])<=1;total+=len(data)
for name,info in checks['music'].items():
 p=app/'assets/audio'/str(name+'.ogg');assert hashlib.sha256(p.read_bytes()).hexdigest()==info['sha256'];probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_streams','-of','json',str(p)]))['streams'][0];assert int(probe['sample_rate'])==16000 and probe['channels']==1 and probe['codec_name']=='vorbis';a=np.frombuffer(subprocess.check_output(['ffmpeg','-v','error','-i',str(p),'-ac','1','-f','f32le','-']),dtype='<f4');assert np.isfinite(a).all() and max(abs(a))<1 and np.sqrt(np.mean(a*a))>.02;assert abs(len(a)/16000-info['duration'])<.1;assert abs(a[-1]-a[0])<.03
print(f'10 bounded U8 effects ({total} resident bytes), two 16kHz mono Vorbis loops, hashes, non-silence, peaks and seams passed')
