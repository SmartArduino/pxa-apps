#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Convert generated collapse/wreck effects to bounded direct-play S16 clips."""
from pathlib import Path
import argparse,hashlib,json,subprocess
import numpy as np
app=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--workflows',type=Path,required=True);p.add_argument('--report',type=Path,required=True);args=p.parse_args();report={}
for name,seconds in (('building_destroy',2.4),('vehicle_destroy',1.3)):
 src=args.source/(name+'.flac');a=np.frombuffer(subprocess.check_output(['ffmpeg','-v','error','-i',str(src),'-ac','1','-ar','16000','-af','highpass=f=70,lowpass=f=6200','-f','f32le','-']),dtype='<f4').copy();env=np.sqrt(np.mean(a[:len(a)//160*160].reshape(-1,160)**2,axis=1));peak=int(np.argmax(env));onset=peak
 while onset>0 and (env[max(0,onset-3):onset]>env[peak]*.08).any():onset-=1
 start=max(0,onset*160-128);clip=a[start:start+round(seconds*16000)].copy();clip-=clip.mean();clip*=min(.74/max(abs(clip)),.20/max(1e-9,np.sqrt(np.mean(clip*clip))));clip[:48]*=np.linspace(0,1,48);clip[-1920:]*=np.linspace(1,0,1920);data=np.clip(np.rint(clip*32767),-32768,32767).astype('<i2').tobytes();dst=app/'assets/audio'/(name+'.s16');dst.write_bytes(data);master=app/'art/audio'/(name+'.flac');subprocess.run(['ffmpeg','-v','error','-y','-f','s16le','-ar','16000','-ac','1','-i',str(dst),'-c:a','flac',str(master)],check=True)
 report[name]={'duration':len(data)/32000,'sample_rate':16000,'channels':1,'runtime_bytes':len(data),'source_start':start/16000,'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'runtime_sha256':hashlib.sha256(data).hexdigest(),'master_sha256':hashlib.sha256(master.read_bytes()).hexdigest()}
config={'tool':'remote ComfyUI; existing Stable Audio 3 Small SFX and T5Gemma models','input_boundary':'original text prompts only; no reference audio or third-party game samples; models are not included','license':'original project outputs MIT; upstream model license and model digests in audio-prompts.json','workflows':json.loads(args.workflows.read_text()),'processing':{'sample_rate':16000,'encoding':'S16LE','highpass_hz':70,'lowpass_hz':6200,'peak_limit':.74,'rms_limit':.20,'clips':report}}
(app/'tools/destruction-prompts.json').write_text(json.dumps(config,ensure_ascii=False,indent=2)+'\n');args.report.write_text(json.dumps(report,indent=2)+'\n');print('Prepared distinct building collapse and armored wreck effects')
