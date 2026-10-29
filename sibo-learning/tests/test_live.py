#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Opt-in live checks using existing public content only (no AI/TTS requests)."""
import argparse
import json
from pathlib import Path
import sys
import time
import urllib.request

HERE=Path(__file__).resolve()
sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
# This test shares the Sibo fixture/control helpers, not the common module
# with the same filename.
import importlib.util
spec=importlib.util.spec_from_file_location('sibo_flows',HERE.with_name('test_simulator.py'))
flows=importlib.util.module_from_spec(spec);spec.loader.exec_module(flows)

def wait(sim,predicate,seconds):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if sim.process.poll() is not None:raise AssertionError('simulator exited')
        if predicate():return
        time.sleep(.1)
    raise AssertionError(sim.log_path.read_text())

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['workspace','package-root','executable','output']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--wait-completion',action='store_true')
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    base=flows.ORIGIN+'/oss/shici'
    index=json.load(urllib.request.urlopen(base+'/index.json',timeout=30))
    prepared=next(item for item in index if item['id']=='p001')
    meta=json.load(urllib.request.urlopen(base+'/'+prepared['meta'],timeout=30))
    (a.output/'course-index.json').write_text(json.dumps(index,ensure_ascii=False))
    with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',240,284,'live',executable=a.executable.resolve()) as sim:
        # Seed the prepared metadata so the test can never fall through to the
        # app's user-triggered synthesis endpoints if the public service fails.
        flows.seed(sim,base+'/index.json',index)
        flows.seed(sim,base+'/'+prepared['meta'],meta)
        sim.key('ENTER');sim.key('ENTER');sim.key('ENTER')
        flows.choose(sim,'听一听朗读')
        wait(sim,lambda:'sibo PCM started' in sim.log_path.read_text(),35)
        time.sleep(2);flows.focus(sim,'暂停');sim.screenshot('streaming')
        sim.key('ENTER');sim.expect('继续听');sim.screenshot('paused')
        time.sleep(1);sim.key('ENTER');sim.expect('暂停');time.sleep(1)
        flows.choose(sim,'看看插图');wait(sim,lambda:sim.focus()=='回到书页',25);sim.screenshot('illustration')
        flows.returned(sim)
        if a.wait_completion:
            wait(sim,lambda:'sibo audio: 这一段听完啦' in sim.log_path.read_text(),150)
            sim.screenshot('natural-end')
        else:flows.choose(sim,'停止朗读')
        flows.choose(sim,'听一听朗读')
        wait(sim,lambda:sim.log_path.read_text().count('sibo PCM started')>=2,35)
        flows.choose(sim,'停止朗读');sim.screenshot('stopped')
        assert 'sibo HTTP body error=' not in sim.log_path.read_text()
        flows.returned(sim);flows.returned(sim);flows.returned(sim)
        flows.choose(sim,'读出声来');sim.key('ENTER');wait(sim,lambda:sim.focus()=='声律启蒙',35)
        sim.key('ENTER');wait(sim,lambda:sim.focus().startswith('上卷'),35)
        private=next((sim.state/'private-files').iterdir())
        files=list(private.glob('cache-*.bin'))
        # The real catalog and book hash to the same old direct-mapped slot.
        # Both must survive as independent bounded caches.
        heads=[f.read_text().split('\n',1)[0] for f in files]
        assert any('/recite/catalog.json?' in h for h in heads)
        assert any('/recite/items/mengxue-shenglvqimeng.json?' in h for h in heads)
        assert len(files)<=4 and all(f.stat().st_size<=96*1024 for f in files)
        prior={f.name:(f.read_bytes(),f.stat().st_mtime_ns) for f in files}
        flows.returned(sim);flows.returned(sim);flows.returned(sim)
        flows.restart(sim);flows.choose(sim,'读出声来');sim.key('ENTER');sim.expect('声律启蒙');sim.key('ENTER');sim.expect('上卷')
        assert all((private/name).read_bytes()==data and (private/name).stat().st_mtime_ns==mtime for name,(data,mtime) in prior.items())
        sim.screenshot('cached-book-after-restart')
        flows.returned(sim);flows.returned(sim);flows.returned(sim)
        catalog={'items':[{'id':'story-0001','title':'龟兔赛跑','item':'items/story-0001.json','ageMin':3,'ageMax':8,'minutes':2}]}
        flows.seed(sim,flows.ORIGIN+'/oss/stories/catalog.json',catalog)
        flows.seed(sim,flows.ORIGIN+'/oss/stories/items/story-0001.json',{'text':'听一个寓言故事。','audio':'audio/story-0001.mp3'})
        flows.choose(sim,'故事小书架');sim.expect('龟兔赛跑');sim.key('ENTER');sim.expect('开始阅读');flows.choose(sim,'听一听朗读')
        wait(sim,lambda:'sibo PCM started 24000Hz 1ch' in sim.log_path.read_text(),35)
        time.sleep(1);sim.screenshot('mono-story');flows.choose(sim,'停止朗读')
    print('Public MP3 PCM, pause/resume, restart/stop, illustration, catalog/book cache coexistence and restart passed',flush=True)
if __name__=='__main__':main()
