#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run on the inference machine with its existing Qwen3-TTS installation.

Design one original reference per unit, then reuse its Base-model clone prompt
for every line. Keep output in a task artifact directory. Nothing is downloaded.
"""
from pathlib import Path
import argparse, gc, hashlib, json, os

os.environ['HF_HUB_OFFLINE'] = '1'
os.environ['TRANSFORMERS_OFFLINE'] = '1'
os.environ['HF_HUB_DISABLE_TELEMETRY'] = '1'
import numpy as np
import soundfile as sf
import torch
from qwen_tts import Qwen3TTSModel


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''): h.update(block)
    return h.hexdigest()


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--models',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--config',type=Path,default=Path(__file__).with_name('voice-prompts.json'))
    p.add_argument('--stage',choices=('all','design','clone'),default='all')
    p.add_argument('--unit',help='Generate just one cast member for audition')
    p.add_argument('--line',nargs='+',help='Generate only these unit-cue names')
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    config=json.loads(args.config.read_text());cast=[c for c in config['cast'] if not args.unit or c['id']==args.unit]
    torch.set_num_threads(6)
    source={}
    for role in ('design','clone'):
        model_dir=args.models/config['models'][role]
        for name in ('model.safetensors','speech_tokenizer/model.safetensors','config.json','generation_config.json'):
            source[role+'/'+name]=sha(model_dir/name)
    (args.output/'model-hashes.json').write_text(json.dumps(source,indent=2)+'\n')
    def load(role):
        return Qwen3TTSModel.from_pretrained(str(args.models/config['models'][role]),device_map='cuda:0',dtype=torch.bfloat16,attn_implementation='sdpa',local_files_only=True)
    if args.stage in ('all','design'):
        model=load('design')
        for c in cast:
            out=args.output/(c['id']+'-reference.flac')
            if out.exists(): continue
            torch.manual_seed(c['seed']);torch.cuda.manual_seed_all(c['seed'])
            settings=dict(config['sampling'],max_new_tokens=512)
            wavs,sr=model.generate_voice_design(text=c['reference_text'],language=config['language'],instruct=c['instruct'],**settings)
            sf.write(out,np.asarray(wavs[0]),sr,subtype='PCM_16')
            print('reference',c['id'],len(wavs[0])/sr,flush=True)
        del model;gc.collect();torch.cuda.empty_cache()
    if args.stage in ('all','clone'):
        model=load('clone')
        for c in cast:
            reference=args.output/(c['id']+'-reference.flac')
            prompt=model.create_voice_clone_prompt(ref_audio=str(reference),ref_text=c['reference_text'],x_vector_only_mode=False)
            # This identity prompt is reused unchanged for all of this unit's cues.
            for index,(cue,text) in enumerate(c['lines'].items()):
                if args.line and c['id']+'-'+cue not in args.line: continue
                out=args.output/(c['id']+'-'+cue+'.flac')
                if out.exists(): continue
                seed=c.get('line_seeds',{}).get(cue,c['seed']+index+1)
                torch.manual_seed(seed);torch.cuda.manual_seed_all(seed)
                wavs,sr=model.generate_voice_clone(text=text,language=config['language'],voice_clone_prompt=prompt,non_streaming_mode=True,**config['sampling'])
                sf.write(out,np.asarray(wavs[0]),sr,subtype='PCM_16')
                print('line',c['id'],cue,len(wavs[0])/sr,flush=True)
        del model;gc.collect();torch.cuda.empty_cache()


if __name__=='__main__': main()
