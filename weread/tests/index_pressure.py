#!/usr/bin/env python3
"""Exercise the optional index_pressure.cpp Guest on the actual PXA simulator.

Build in an isolated source copy; never install this fixture over a user's app.
Uses no network, account, private files or runtime allocation fault hooks.
"""
import argparse, json, tempfile
from pathlib import Path
from smoke import Simulator


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--trials',type=int,default=3)
    a=p.parse_args()
    if not 1<=a.trials<=10:p.error('--trials must be between 1 and 10')
    a.output.mkdir(parents=True,exist_ok=True);results=[]
    for trial in range(a.trials):
        with tempfile.TemporaryDirectory(prefix='weread-index-pressure-')as directory:
            app=Simulator(a,a.output/str(trial),Path(directory),296,240,160,58)
            try:
                app.wait(lambda:any(x in app.logs()for x in ('WEREAD INDEX PRESSURE COMPLETE','WEREAD INDEX PRESSURE FAILED','Guest exception:')),30)
                assert 'WEREAD INDEX PRESSURE COMPLETE'in app.logs(),app.logs()
                assert 'Guest exception:'not in app.logs()and'FAILED'not in app.logs()
                memory=app.wait_memory(lambda m:m.get('linear_peak_bytes',0)>65536)
                assert memory['linear_peak_bytes']<=2097152
                app.command('KEY HOME');app.process.wait(timeout=10)
                assert 'remaining_internal=0 remaining_external=0'in app.logs()
                results.append({'trial':trial,'rejected_without_trap':True,'same_parser_recovered_after_release':True,'memory':memory,'released':True})
            finally:
                app.close();(a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(f'{len(results)} Guest index-exhaustion/retry/stop checks passed')


if __name__=='__main__':main()
