#!/usr/bin/env python3
"""Paired real-PXA snapshot counts; identical UI AOT, no device-speed claims."""
import argparse,hashlib,json,re,time
from pathlib import Path
from harness import Browser
from connection_fixture import Fixture
from connection_smoke import point,COLORS


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','before-package','after-package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();packages={'before':a.before_package,'after':a.after_package};hashes=[]
    for package in packages.values():
        ui=list((package/'artifacts').glob('main.*.aot'));assert len(ui)==1;hashes.append(hashlib.sha256(ui[0].read_bytes()).hexdigest())
    assert len(set(hashes))==1,'UI AOT must be identical'
    f=Fixture(31878).start();rows=[];a.output.mkdir(parents=True,exist_ok=True)
    try:
        for round in range(2):
            for label in (['before','after'] if round==0 else ['after','before']):
                folder=a.output/(label+'-'+str(round));b=Browser(a.simulator,packages[label],a.publisher_key,a.ime_package,folder,width=480,height=480,corner=0)
                try:
                    b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
                    for attached in (False,True):
                        mark=len(b.logs());b.navigate('http://127.0.0.1:31878/measure?attached='+str(int(attached)));b.wait(lambda:'BROWSER JS UPDATE revision=1 ' in b.logs()[mark:])
                        def batches():return int(re.findall(r'BROWSER JS UPDATE[^\n]*markup_turns=(\d+)',b.logs()[mark:])[-1])
                        initial=batches();initial_updates=len(re.findall(r'BROWSER JS UPDATE',b.logs()[mark:]));stage=point(b,folder,'stage-'+str(attached),COLORS['stage']);finish=point(b,folder,'finish-'+str(attached),COLORS['finish']);assert stage and finish
                        for n in range(1,7):
                            at=len(f.events);last=len(b.logs());b.tap(*stage);b.wait(lambda:len(f.events)==at+1);assert f.events[-1]==dict(event='stage',n=str(n),connected=str(int(attached)))
                            if attached:b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[last:])
                            time.sleep(.8)
                        at=len(f.events);last=len(b.logs());b.tap(*finish);b.wait(lambda:len(f.events)==at+1 and 'BROWSER JS UPDATE' in b.logs()[last:]);assert f.events[-1]['n']=='6'
                        assert 'BROWSER JS ERROR' not in b.logs()[mark:]
                        updates=len(re.findall(r'BROWSER JS UPDATE',b.logs()[mark:]))-initial_updates;assert updates==(7 if attached else 1),updates
                        rows.append(dict(round=round,package=label,attached=attached,callbacks=6,markup_batches=batches()-initial,visible_updates=updates));print(json.dumps(rows[-1]),flush=True)
                finally:b.close()
        for round in range(2):
            data={(r['package'],r['attached']):r['markup_batches'] for r in rows if r['round']==round}
            assert data['before',False]>3*data['after',False],data
        (a.output/'results.json').write_text(json.dumps(dict(ui_aot_sha256=hashes[0],note='Counts from actual script.wasm snapshots in the PXA simulator; not FPS or device timing.',results=rows),indent=2)+'\n')
    finally:f.close()


if __name__=='__main__':main()
