#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the actual shipped PCM sounds and decoded music, without playback."""
import array, json, math, subprocess, sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
rows=[];total=0
for path in sorted((root/'assets/sfx').glob('*.pcm')):
    data=path.read_bytes();samples=[(x-128)/128 for x in data];total+=len(data)
    peak=max(map(abs,samples));rms=math.sqrt(sum(x*x for x in samples)/len(samples))
    assert 0<len(data)<=16000 and data[0]==data[-1]==128, path
    assert abs(sum(samples)/len(samples))<.01 and .01<rms<.3 and peak<.65, path
    rows.append({'name':path.name,'bytes':len(data),'peak':peak,'rms':rms})
assert len(rows)==8 and total<=32768
pcm=subprocess.check_output(['ffmpeg','-hide_banner','-loglevel','error','-i',str(root/'assets/temple.ogg'),'-ar','16000','-ac','1','-f','s16le','pipe:1'])
music=array.array('h',pcm)
if sys.byteorder!='little':music.byteswap()
peak=max(map(abs,music))/32768
assert len(music)==640000 and peak<.4 and abs(music[-1]-music[0])<512
# Even the maximum allowed peak in all three categories plus music has headroom.
ceiling=.65*(10**(-11/20)+10**(-12/20)+10**(-10/20))+peak*10**(-9/20)
assert ceiling<.75
result={'passed':True,'clips':rows,'resident_pcm_bytes':total,'music_seconds':len(music)/16000,'decoded_music_peak':peak,'conservative_mix_peak_bound':ceiling}
(out/'quality.json').write_text(json.dumps(result,indent=2)+'\n')
print('Audio passed: eight bounded PCM clips, silent endpoints, DC/headroom, 40-second loop seam and three-track mix headroom')
