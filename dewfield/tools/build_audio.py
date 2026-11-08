#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original small loop scores. Offline synthesis; PXA streams the Vorbis assets."""
from pathlib import Path
import argparse,json,subprocess,tempfile,wave
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
RATE=22050

def note(midi,seconds,kind):
 t=np.arange(int(RATE*seconds))/RATE;f=440*2**((midi-69)/12)
 if kind=='bell':
  y=np.sin(2*np.pi*f*t)*np.exp(-t*3.8)+.22*np.sin(2*np.pi*f*2.002*t)*np.exp(-t*7)
 elif kind=='flute':
  y=(np.sin(2*np.pi*f*t+.01*np.sin(2*np.pi*4*t))+.12*np.sin(4*np.pi*f*t))*np.exp(-t*1.9)
 else:
  y=(np.sin(2*np.pi*f*t)+.27*np.sin(4*np.pi*f*t)+.1*np.sin(6*np.pi*f*t))*np.exp(-t*4)
 return y*np.minimum(1,t/.018)*np.minimum(1,(seconds-t)/.09)

SCORES={
 'evening':{'bpm':72,'chords':[(57,60,64),(53,57,60),(48,52,55),(55,59,62)],'melody':[76,72,69,0,72,67,64,0,69,72,74,72,67,64,62,0],'kind':'bell'},
 'interior':{'bpm':84,'chords':[(48,52,55),(53,57,60),(50,53,57),(55,59,62)],'melody':[72,76,79,76,74,72,69,0,65,69,72,69,67,71,74,0],'kind':'plucked'},
 'cave':{'bpm':60,'chords':[(45,52,57),(41,48,53),(43,50,55),(40,47,52)],'melody':[69,0,64,0,65,0,60,0,67,0,62,0,64,0,59,0],'kind':'bell'},
 'rain':{'bpm':76,'chords':[(50,53,57),(46,50,53),(48,52,55),(45,52,57)],'melody':[77,74,69,0,72,70,65,0,76,72,67,0,69,64,62,0],'kind':'flute'}
}

def score(spec):
 beat=60/spec['bpm'];duration=32*beat;total=int(duration*RATE);out=np.zeros(total,dtype=np.float64)
 def put(midi,start,seconds,gain,kind):
  sound=note(midi,seconds,kind)*gain;indices=(int(start*RATE)+np.arange(len(sound)))%total;np.add.at(out,indices,sound)
 for bar in range(8):
  chord=spec['chords'][bar%4];start=bar*4*beat
  for tick in range(8):put(chord[tick%3]+12,start+tick*.5*beat,beat*1.7,.055,'plucked')
  put(chord[0]-12,start,beat*3.6,.07,'flute')
 for i,midi in enumerate(spec['melody']*2):
  if midi:put(midi,i*beat,beat*1.65,.10,spec['kind'])
 # Deterministic, gentle boundary: all tails wrap instead of being cut.
 peak=float(np.max(np.abs(out)));out*=.48/max(.48,peak)
 return np.round(out*32767).astype('<i2'),duration

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--preview',type=Path);args=parser.parse_args();metadata=[]
 for name,spec in SCORES.items():
  pcm,seconds=score(spec);destination=ROOT/'assets'/f'dewfield-{name}.ogg'
  with tempfile.TemporaryDirectory()as temp:
   wav=Path(temp)/'loop.wav'
   with wave.open(str(wav),'wb')as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(RATE);f.writeframes(pcm.tobytes())
   subprocess.run(['ffmpeg','-v','error','-y','-i',str(wav),'-c:a','libvorbis','-q:a','1',str(destination)],check=True)
   if args.preview:
    args.preview.mkdir(parents=True,exist_ok=True);(args.preview/f'{name}.wav').write_bytes(wav.read_bytes())
  metadata.append({'asset':destination.name,'seconds':round(seconds,3),'bpm':spec['bpm'],'original_midi_score':spec['melody'],'sample_rate':RATE,'channels':1})
 (ROOT/'art/audio/additional-scores.json').write_text(json.dumps({'license':'MIT','authorship':'Original Dewfield scores and procedural synthesis; no sampled recordings','generator':'tools/build_audio.py','loop':'Note tails wrap across the loop boundary','tracks':metadata},ensure_ascii=False,indent=2)+'\n')
 print('Generated',len(metadata),'original loop scores; mono 22050 Hz, Host decoding')
if __name__=='__main__':main()
