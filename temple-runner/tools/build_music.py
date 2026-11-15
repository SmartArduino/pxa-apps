#!/usr/bin/env python3
"""Synthesize the original seamless 12-second percussion/woodwind loop.
Requires ffmpeg as a build-time converter; no external samples are used.
Temporary WAV stays in the supplied ignored artifact directory.
"""
import math, random, struct, subprocess, sys, wave
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
rate=16000;seconds=12;audio=[0.0]*(rate*seconds);rng=random.Random(82)
notes=[220,0,261.63,293.66,0,329.63,293.66,261.63,220,0,196,0,220,261.63,196,0]
for i in range(len(audio)):
    t=i/rate;beat=t%.375;bar=int(t/.375)
    drum=math.sin(2*math.pi*(70*beat+2*(1-math.exp(-beat*24))))*math.exp(-beat*24)*(.19 if bar%4==0 else .09)
    shaker=(rng.random()*2-1)*math.exp(-((t+.1875)%.375)*90)*.018
    melody_t=t%.75;note=notes[int(t/.75)]
    env=min(1,melody_t/.06)*max(0,1-melody_t/.66)**2
    flute=(math.sin(2*math.pi*note*t)+.22*math.sin(4*math.pi*note*t))*.075*env if note else 0
    audio[i]=drum+shaker+flute
wav=out/'temple-original.wav'
with wave.open(str(wav),'wb') as w:
    w.setparams((1,2,rate,0,'NONE','not compressed'))
    w.writeframes(b''.join(struct.pack('<h',int(max(-1,min(1,v))*32767)) for v in audio))
subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(wav),'-c:a','libvorbis','-q:a','2',str(root/'assets/temple.ogg')],check=True)
