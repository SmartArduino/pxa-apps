from pathlib import Path
import subprocess,csv,statistics,json,time,os
base=Path(__file__).resolve().parent
b=base.parent/'abi-audit-20261011/simulator'
base=base/'pinned';base.mkdir(exist_ok=True)
os.sched_setaffinity(0,{15})
report={'runs':[],'scope':'240x240 four-sprite fixed CPU workload, no VSync or real display FPS'}
for repeat in range(15):
    # Rotate order to limit temporal bias; never run CPU comparisons concurrently.
    modes=[('baseline','queued'),('current','queued'),('current','borrowed')]
    modes=modes[repeat%3:]+modes[:repeat%3]
    for exe,mode in modes:
        label=f'{exe}-{mode}-{repeat}'
        r=subprocess.run([str(b/f'pxsys_raster_bench_{exe}'),'5000',mode],text=True,capture_output=True,check=True)
        (base/(label+'.csv')).write_text(r.stdout)
        (base/(label+'.log')).write_text(r.stderr)
        rows=list(csv.DictReader(r.stdout.splitlines()))
        result={'mode':exe+'-'+mode,'repeat':repeat,'frames':len(rows),'stderr':r.stderr}
        for field in ['submit_us','execute_us','render_present_us','total_us']:
            v=[int(row[field]) for row in rows];result[field]={'mean':statistics.mean(v),'median':statistics.median(v),'p95':sorted(v)[int(.95*(len(v)-1))]}
        report['runs'].append(result)
        (base/'desktop-bench.json').write_text(json.dumps(report,indent=2)+'\n')
        print(label,result['total_us'],flush=True)
