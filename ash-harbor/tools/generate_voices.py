#!/usr/bin/env python3
"""Perform dialogue using the selected original voice references on Qwen3-TTS."""
from pathlib import Path
import argparse,hashlib,json,os

os.environ['HF_HUB_OFFLINE']='1'
os.environ['TRANSFORMERS_OFFLINE']='1'
os.environ['HF_HUB_DISABLE_TELEMETRY']='1'
import numpy as np
import soundfile as sf
import torch
from qwen_tts import Qwen3TTSModel

def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1048576),b''):digest.update(block)
    return digest.hexdigest()

def main():
    p=argparse.ArgumentParser();p.add_argument('--models',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--config',type=Path,required=True);p.add_argument('--cues',nargs='+');args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True);config=json.loads(args.config.read_text());cloning='base' in config['models'];model_path=args.models/config['models']['base' if cloning else 'custom']
    names=['model.safetensors','speech_tokenizer/model.safetensors','config.json','generation_config.json']
    hashes={n:sha(model_path/n) for n in names}
    if cloning:hashes={'base':hashes,'design':{n:sha(args.models/config['models']['design']/n) for n in names}}
    (args.output/'model-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
    torch.set_num_threads(6)
    model=Qwen3TTSModel.from_pretrained(str(model_path),device_map='cuda:0',dtype=torch.bfloat16,attn_implementation='sdpa',local_files_only=True)
    for cast in config['cast']:
        prompt=None
        if cloning:
            reference=args.output/(cast['id']+'-reference.flac')
            assert sha(reference)==cast['selection']['sha256'],(cast['id'],'Selected voice reference changed')
            prompt=model.create_voice_clone_prompt(ref_audio=str(reference),ref_text=cast['reference_text'],x_vector_only_mode=False)
        lines={'reference':cast['reference_text'],**cast['lines']}
        for index,(cue,text) in enumerate(lines.items()):
            if args.cues and cue not in args.cues:continue
            if not cloning and cue in cast['nonverbal_limits'] and cast.get('nonverbal_model')!='Qwen3-TTS CustomVoice':continue
            path=args.output/(cast['id']+'-'+cue+'.flac')
            if path.exists():continue
            seed=cast.get('seeds',{}).get(cue,cast['seed']+index)
            torch.manual_seed(seed);torch.cuda.manual_seed_all(seed)
            performance=cast['performance'].get(cue,cast['performance']['intro'])
            settings=dict(config['sampling']);settings['max_new_tokens']=512 if cue=='reference' else 64 if cue in cast['nonverbal_limits'] else 192
            if cloning:
                # Base transfers the selected timbre and reference delivery; unlike
                # CustomVoice it does not support per-line instruction control.
                wavs,sr=model.generate_voice_clone(text=cast.get('synthesis_text',{}).get(cue,text),language=config['language'],voice_clone_prompt=prompt,non_streaming_mode=True,**settings)
            else:
                wavs,sr=model.generate_custom_voice(text=text,language=config['language'],speaker=cast['speaker'].lower(),instruct=cast['instruct']+performance,non_streaming_mode=True,**settings)
            sf.write(path,np.asarray(wavs[0]),sr,subtype='PCM_16');print(cast['id'],cue,len(wavs[0])/sr,flush=True)

if __name__=='__main__':main()
