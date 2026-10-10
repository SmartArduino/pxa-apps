#!/usr/bin/env python3
"""Compare fixed 296x240 scenes. Output native Host timings, not displayed FPS."""
import argparse,json,re,subprocess,tempfile,statistics
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('build',type=Path);parser.add_argument('--repeat',type=int,default=3);args=parser.parse_args()
rows=[]
with tempfile.TemporaryDirectory(prefix='pxa-jump-reference-')as temporary:
 for skip in (0,62):
  for run in range(args.repeat):
   for language,program in [('c','jump_reference_preview'),('cpp','jump_preview')]:
    command=[str(args.build/program),'--frames','30','--auto','8','--width','296','--height','240','--skip',str(skip),'--out',temporary+'/frame']
    log=subprocess.check_output(command,text=True)
    samples=[list(map(int,m))for m in re.findall(r'bytes=(\d+) commands=(\d+) covered=(\d+) raster_us=(\d+)',log)]
    assert len(samples)==38
    row=dict(language=language,skip=skip,run=run,seed='0x51ed2701',frames=len(samples))
    row.update(zip(['draw_bytes','commands','covered','raster_us'],map(statistics.mean,zip(*samples))))
    rows.append(row)
print(json.dumps(rows,indent=2))
