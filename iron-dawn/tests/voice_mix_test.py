#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Locate each delivered PCM response in actual SDL mixer captures."""
from pathlib import Path
import argparse,json,numpy as np
p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,required=True);p.add_argument('--report',type=Path,required=True);args=p.parse_args()
app=Path(__file__).resolve().parents[1];config=json.loads((app/'tools/voice-prompts.json').read_text());checks=[]
for cast in config['cast']:
 signal=np.frombuffer((args.capture/cast['id']/'mix.raw').read_bytes(),dtype='<i2').astype(float)/32768
 refs={cue:np.frombuffer((app/'assets/audio/voices'/(cast['id']+'-'+cue+'.s16')).read_bytes(),dtype='<i2').astype(float)/32768 for cue in cast['lines']}
 size=1<<(len(signal)+max(map(len,refs.values()))-1).bit_length();spectrum=np.fft.rfft(signal,size)
 for cue,ref in refs.items():
  correlation=np.fft.irfft(spectrum*np.conj(np.fft.rfft(ref,size)),size)[:len(signal)-len(ref)+1];at=int(np.argmax(correlation));energy=float(np.dot(ref,ref));gain=float(correlation[at]/energy);rho=float(correlation[at]/np.sqrt(energy*np.dot(signal[at:at+len(ref)],signal[at:at+len(ref)])))
  checks.append({'unit':cast['id'],'cue':cue,'start_seconds':at/16000,'correlation':rho,'gain':gain});assert rho>.55 and gain>.05,(cast['id'],cue,rho,gain)
args.report.write_text(json.dumps({'passed':True,'checks':checks},indent=2)+'\n');print(f'All {len(checks)} delivered voice waveforms located in actual mixer output; minimum correlation {min(x["correlation"] for x in checks):.3f}')
