#!/usr/bin/env python3
"""Real PXA projection progress while polling; actual edits remain coherent."""
import argparse,json,re
from pathlib import Path
from harness import Browser
from connection_smoke import point,COLORS
from geometry import TOOL_ROWS,navigation
from snapshot_fixture import Fixture,CASES


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    p.add_argument('--measure',action='store_true',help='Allow the baseline to publish only after polling ends; skip native find checks')
    p.add_argument('--case',action='append',choices=CASES);a=p.parse_args();f=Fixture().start();rows=[]
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0)
    pattern=re.compile(r'BROWSER JS UPDATE revision=(\d+)[^\n]*markup_turns=(\d+)')
    def updates(mark):return [tuple(map(int,m)) for m in pattern.findall(b.logs()[mark:])]
    def find(term,count,first=False):
        if first:
            b.nav(4);x,y,w,h=b.box('content');b.tap(x+w-2,y+h-1);scroll=max(0,TOOL_ROWS*35-h);b.tap(x+w//2,y+(TOOL_ROWS-2)*35-scroll+16)
        else:
            x,y,w,h=b.box('address');b.tap(x+w-45,y+h//2)
        # A focused find editor retains its prior query. Use its visible X,
        # independently of the Host's caret/selection behavior.
        x,y,w,h=b.box('address');b.tap(x+w-45,y+h//2)
        mark=len(b.logs());b.enter(term);b.wait(lambda:re.search(r'BROWSER FIND result=\d+ count='+str(count)+r'\b',b.logs()[mark:]));b.shot(case+'-find-'+term.replace(' ','-'))
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        for case in a.case or CASES:
            mark=len(b.logs());b.navigate('http://127.0.0.1:'+str(f.server.server_port)+'/'+case);b.wait(lambda:updates(mark));initial=updates(mark)[-1][1]
            at=point(b,a.output,case+'-initial',COLORS['stage']);assert at;last=len(b.logs());b.tap(*at);b.wait(lambda:updates(last));first=updates(last)[0];observed=len(f.polls[case])
            if not a.measure:assert 0<observed<48,(case,observed)
            b.shot(case+'-first-update');b.wait(lambda:f.done.get(case)==48,timeout=30)
            assert f.polls[case]==list(range(48)),(case,f.polls[case]);expected=2 if case in ('changed','script-change') else 1
            if not a.measure:
                find('Head '+str(expected),1,True);find('Tail '+str(expected),1);find('Head 0',0)
                b.tap(*navigation({'toolbar':b.box('toolbar')},3,count=4))
            assert all(s not in b.logs()[mark:] for s in ('BROWSER JS ERROR','BROWSER DRAW ERROR','PXA Guest exception:'))
            rows.append(dict(flow=case,callbacks=48,poll_requests_at_first_update_observation=observed,batches_to_first_update=first[1]-initial,coherent_generation=expected,native_find_checked=not a.measure))
            if case.startswith('script-'):assert f.helpers.count(case)==1
        b.command('KEY HOME');b.wait(lambda:'PXA RESOURCE BUDGET' in b.logs());budget=[s for s in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in s][-1];assert 'remaining_internal=0 remaining_external=0' in budget
        (a.output/'results.json').write_text(json.dumps(dict(flows=rows,budget=budget,note='Poll count is observed after receiving the first update; it is an upper bound on requests at the exact commit time.'),indent=2)+'\n');print('Snapshot simulator PASS:',len(rows),'flows; polling progress, detached callbacks, coherent edits, scripts and stop budget',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
