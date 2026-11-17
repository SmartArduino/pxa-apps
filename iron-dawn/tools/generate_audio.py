#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Submit the recorded graphs to an existing ComfyUI and download FLAC candidates.

Keep --output under local/artifacts. Connect an SSH-forwarded loopback API with
--url; this script does not install models or change ComfyUI configuration.
"""
from pathlib import Path
import argparse,json,time,urllib.request,urllib.parse
p=argparse.ArgumentParser();p.add_argument('--url',required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True);config=json.loads(Path(__file__).with_name('audio-prompts.json').read_text())
for name,graph in config['workflows'].items():
 request=urllib.request.Request(args.url.rstrip('/')+'/prompt',json.dumps({'prompt':graph,'client_id':'iron-dawn-audio'}).encode(),headers={'Content-Type':'application/json'})
 with urllib.request.urlopen(request) as response:receipt=json.load(response)
 prompt_id=receipt['prompt_id'];deadline=time.monotonic()+1200
 while time.monotonic()<deadline:
  with urllib.request.urlopen(args.url.rstrip('/')+'/history/'+prompt_id) as response:history=json.load(response)
  if prompt_id in history:break
  time.sleep(1)
 else:raise TimeoutError(name)
 result=history[prompt_id];(args.output/(name+'-history.json')).write_text(json.dumps(result,indent=2)+'\n')
 if result['status']['status_str']!='success':raise RuntimeError(result['status']['messages'])
 files=[f for node in result['outputs'].values() for f in node.get('audio',[])];assert len(files)==1
 with urllib.request.urlopen(args.url.rstrip('/')+'/view?'+urllib.parse.urlencode(files[0])) as response:(args.output/(name+'.flac')).write_bytes(response.read())
 print(name,'generated',flush=True)
