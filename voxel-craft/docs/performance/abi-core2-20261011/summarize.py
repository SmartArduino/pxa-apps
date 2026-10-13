"""Summarize recorded windows, de-duplicating replayed serial messages.
Usage: python summarize.py DIRECTORY (containing the capture directories).
"""
from pathlib import Path
import json,re,statistics,sys
base=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).parent
modes=['device-before-unlocked','device-queued','device-borrowed','device-final-queued','device-final-borrowed']
def stages(folder):
    seen=set();samples=[];pipes=[];windows=[];start=None
    for line in (folder/'serial.jsonl').read_text().splitlines():
        row=json.loads(line);p=row.get('payload');t=row['seconds']
        if row['kind']=='COMMAND':
            if p=='PERF START':start=t
            elif p=='PERF STOP' and start is not None:windows.append((start,t));start=None
        if row['kind']!='LOG' or not isinstance(p,str) or p in seen:continue
        seen.add(p)
        if 'VOXEL-STAGES ' in p:
            v={k:int(n) for k,n in re.findall(r'(\w+)=(\d+)',p.split('VOXEL-STAGES ',1)[1])}
            samples.append((t,v))
        if 'VOXEL-PIPE ' in p:
            v={k:int(n) for k,n in re.findall(r'(\w+)=(\d+)',p.split('VOXEL-PIPE ',1)[1])}
            pipes.append((t,v))
    per_run=[]
    for lo,hi in windows:
        before=[v for t,v in samples if t<=lo and v['samples']>0]
        after=[v for t,v in samples if lo<t<=hi and v['samples']>0]
        a,b=before[-1],after[-1];n=b['samples']-a['samples'];assert n>0
        result={k:(b[k]*b['samples']-a[k]*a['samples'])/n for k in b if k!='samples'}
        pa=[v for t,v in pipes if t<=lo][-1];pb=[v for t,v in pipes if lo<t<=hi][-1]
        for key,count in [('queue_us','render'),('present_us','visible')]:
            result['host_'+key]=(pb[key]-pa[key])/(pb[count]-pa[count])
        per_run.append(result)
    return {'per_run':per_run,'mean_us':{k:statistics.mean(r[k] for r in per_run) for k in per_run[0]}}
out={}
for mode in modes:
    folder=base/mode
    if not (folder/'report.json').exists():continue
    report=json.loads((folder/'report.json').read_text());assert report['passed'];runs=report['runs']
    result={'fps_runs':[r['display_fps'] for r in runs],'fps_mean':statistics.mean(r['display_fps'] for r in runs),
      'raster_mean_us':statistics.mean(r['raster']['mean_us'] for r in runs),'stages':stages(folder),
      'surface':runs[0]['memory_warm']['surface'],'memory':{}}
    for kind in ['sram','psram']:
        result['memory'][kind]={'stable_mean':statistics.mean(r['steady_heap'][kind+'_used'] for r in runs),
            'stable_max':max(r['steady_heap'][kind+'_used'] for r in runs),
            'launch_true_peak_max':max(r['launch_heap'][kind+'_peak_used'] for r in runs),
            'steady_true_peak_max':max(r['steady_heap'][kind+'_peak_used'] for r in runs)}
    out[mode]=result
for mode in ['input-before','input-queued','input-final-queued','input-borrowed']:
    folder=base/mode
    if not (folder/'report.json').exists():continue
    report=json.loads((folder/'report.json').read_text());assert report['passed'];runs=report['runs'];metrics=[r['metrics'] for r in runs]
    out[mode]={s+'_mean_us':sum(m[s+'_total_us'] for m in metrics)/sum(m[s+'_count'] for m in metrics) for s in ['guest','present','visible']}
    out[mode]['visible_p95_runs_us']=[m['visible_p95_us'] for m in metrics]
(base/'comparison.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
