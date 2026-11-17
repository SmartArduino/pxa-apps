#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Convert selected generated FLAC masters to bounded PXA audio assets.

Usage: prepare_audio.py --source local/artifacts/.../raw --report local/artifacts/.../audio-checks.json
No model or decoder is linked into the game. Full generation candidates stay local.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess
import numpy as np
app=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--report',type=Path,required=True);args=p.parse_args();rate=16000
runtime=app/'assets/audio';masters=app/'art/audio';runtime.mkdir(exist_ok=True);masters.mkdir(exist_ok=True)
def decode(path):
 b=subprocess.check_output(['ffmpeg','-v','error','-i',str(path),'-af','pan=mono|c0=0.5*c0+0.5*c1','-ar',str(rate),'-f','f32le','-']);a=np.frombuffer(b,dtype='<f4').copy();return a-a.mean()
def save(name,a,music=False):
 a=a.astype('<f4');src=masters/(name+'.flac');subprocess.run(['ffmpeg','-v','error','-y','-f','f32le','-ar',str(rate),'-ac','1','-i','-','-c:a','flac','-sample_fmt','s16',str(src)],input=a.tobytes(),check=True)
 if music:
  dst=runtime/(name+'.ogg');subprocess.run(['ffmpeg','-v','error','-y','-i',str(src),'-c:a','libvorbis','-q:a','4',str(dst)],check=True)
 else:dst=runtime/(name+'.pcm');dst.write_bytes(np.clip(np.rint(a*127+128),0,255).astype('u1').tobytes())
 return {'duration':len(a)/rate,'sample_rate':rate,'channels':1,'peak_dbfs':float(20*np.log10(max(1e-9,np.abs(a).max()))),'rms_dbfs':float(20*np.log10(max(1e-9,np.sqrt(np.mean(a*a))))),'seam_step':float(abs(a[0]-a[-1])),'bytes':dst.stat().st_size,'sha256':hashlib.sha256(dst.read_bytes()).hexdigest(),'master_sha256':hashlib.sha256(src.read_bytes()).hexdigest()}
report={'music':{},'effects':{},'source_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in args.source.glob('*.flac')}}
for name,bpm,offset in [('command',88,4),('battle',112,8)]:
 a=decode(args.source/(name+'.flac'));length=round(16*4*60/bpm*rate);fade=round(4*60/bpm*rate);start=offset*rate;near=a[start+fade-320:start+fade+321];cross=np.nonzero(near[:-1]*near[1:]<=0)[0];shift=int(cross[np.argmin(np.abs(near[cross])+np.abs(near[cross+1]))])-320 if len(cross) else 0;start+=shift
 section=a[start:start+length+fade];assert len(section)==length+fade;w=np.linspace(0,1,fade);loop=np.concatenate([section[fade:length],section[length:]*(1-w)+section[:fade]*w]);gain=min(.63/max(abs(loop)),.12/max(1e-9,np.sqrt(np.mean(loop*loop))));loop*=gain;report['music'][name]=dict(save(name,loop,True),bpm=bpm,bars=16,source_start=start/rate,crossfade_seconds=fade/rate)
limits={'order':.32,'rifle':.48,'tank':.8,'artillery':.98,'rocket':.8,'impact':.92,'build':.78,'ready':.6,'upgrade':.85}
for name,seconds in limits.items():
 a=decode(args.source/(name+'.flac'));frame=160;env=np.sqrt(np.mean(a[:len(a)//frame*frame].reshape(-1,frame)**2,axis=1));peak=int(np.argmax(env));threshold=env[peak]*.13;onset=peak
 while onset>0 and (env[max(0,onset-3):onset]>threshold).any():onset-=1
 start=max(0,onset*frame-96);clip=a[start:start+round(seconds*rate)].copy();clip*=.70/max(1e-9,max(abs(clip)));fade_in=min(32,len(clip));fade_out=min(round(.07*rate),len(clip)//4);clip[:fade_in]*=np.linspace(0,1,fade_in);clip[-fade_out:]*=np.linspace(1,0,fade_out);report['effects'][name]=dict(save(name,clip),source_start=start/rate)
 if name=='order':
  click=clip[:round(.085*rate)].copy();click[-320:]*=np.linspace(1,0,320);report['effects']['ui']=save('ui',click)
args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:{n:round(v['duration'],3) for n,v in d.items()}for k,d in report.items() if k in ['music','effects']},indent=2))
