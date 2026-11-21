#!/usr/bin/env python3
"""Prepare four generated stage themes without touching character voices/effects."""
from pathlib import Path
import argparse,json,hashlib,subprocess
import numpy as np
from prepare_audio import ROOT,RATE,decode,encode,metrics
p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);p.add_argument('--report',required=True,type=Path);a=p.parse_args()
config=json.loads((ROOT/'tools/scene-music-prompts.json').read_text());report={}
for name,spec in config['music'].items():
 samples=decode(a.source/(name+'.flac'));beat=60/spec['bpm'];length=round(64*beat*RATE);fade=round(4*beat*RATE);start=4*RATE
 section=samples[start:start+length+fade];assert len(section)==length+fade,name
 weight=np.linspace(0,1,fade);clip=np.concatenate([section[fade:length],section[length:]*(1-weight)+section[:fade]*weight])
 clip*=min(.68/max(1e-9,max(abs(clip))),.13/max(1e-9,float(np.sqrt(np.mean(clip*clip)))))
 master=ROOT/'art/audio'/f'{name}.flac';encode(master,clip)
 path=ROOT/'assets/audio'/(name+'.ogg');subprocess.run(['ffmpeg','-v','error','-y','-i',str(master),'-c:a','libvorbis','-q:a','4',str(path)],check=True)
 report[name]={**metrics(path,clip),'loop':True,'model':config['model'],'bpm':spec['bpm'],'master_sha256':hashlib.sha256(master.read_bytes()).hexdigest()}
old_path=ROOT/'tools/audio-prompts.json';old=json.loads(old_path.read_text())
for name in report:
 old['music'][name]=config['music'][name];old['workflows'][name]=config['workflows'][name];old['processed'][name]={k:v for k,v in report[name].items() if k in ['duration','bytes','sha256','master_sha256']}
old_path.write_text(json.dumps(old,ensure_ascii=False,indent=2)+'\n');a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n')
print('Prepared four scene themes with 64-beat loops; voices and effects unchanged.')
