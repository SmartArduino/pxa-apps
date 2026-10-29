#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Live prepared MP3: memory replay, persistent restart and corrupt-file recovery."""
import argparse,importlib.util,json,sys,time
from pathlib import Path
HERE=Path(__file__).resolve();sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec=importlib.util.spec_from_file_location('flows',HERE.with_name('test_simulator.py'));f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
def wait(sim,text,count=1,seconds=40):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  assert sim.process.poll() is None,'Simulator exited'
  if sim.log_path.read_text().count(text)>=count:return
  time.sleep(.1)
 sim.screenshot('timeout');raise AssertionError(sim.log_path.read_text()[-5000:])
def open_word(sim):
 f.seed(sim,f.ORIGIN+'/oss/vocab/catalog.json',{'packs':[{'id':'zk','name':'中考','count':1,'unitCount':1}]})
 f.choose(sim,'英语单词岛');sim.key('ENTER')
 f.seed(sim,f.ORIGIN+'/oss/vocab/packs/zk/001.json',{'words':[{'id':'be','word':'be','phonetic':'bi:','translation':'是','audio':{'path':'packs/zk/001.mp3','startMs':569,'endMs':1291}}]})
 sim.key('ENTER');sim.key('ENTER');f.choose(sim,'听这个单词')
def files(sim):return [p for p in sim.state.rglob('.sibo-audio-*.bin') if p.stat().st_size>32]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('workspace','package-root','executable','output'):p.add_argument('--'+key,type=Path,required=True)
 a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',240,284,'cache',executable=a.executable.resolve()) as sim:
  sim.key('ENTER');sim.key('ENTER');sim.key('ENTER');sim.key('ENTER')
  before=f.personal(sim)
  for _ in range(4):f.returned(sim)
  open_word(sim)
  wait(sim,'sibo audio cache saved bytes=')
  wait(sim,'sibo audio: 这一段听完啦');sim.screenshot('cached-first')
  saved=files(sim);assert len(saved)==1,saved
  assert sum(p.stat().st_size for p in saved)<=1024*1024
  unchanged=(saved[0].read_bytes(),saved[0].stat().st_mtime_ns)
  hits=sim.log_path.read_text().count('sibo audio cache network')
  f.choose(sim,'听这个单词');wait(sim,'sibo audio cache hit memory');wait(sim,'sibo audio: 这一段听完啦',2)
  assert sim.log_path.read_text().count('sibo audio cache network')==hits
  assert (saved[0].read_bytes(),saved[0].stat().st_mtime_ns)==unchanged,'Replay rewrote the flash cache'
  sim.screenshot('memory-replay')
  f.restart(sim);open_word(sim);wait(sim,'sibo audio cache hit disk');wait(sim,'sibo audio: 这一段听完啦',3)
  assert sim.log_path.read_text().count('sibo audio cache network')==hits
  sim.screenshot('disk-after-restart');assert f.personal(sim)==before
  # Truncation cannot masquerade as a completed recording.
  saved=files(sim)[0];saved.write_bytes(saved.read_bytes()[:100])
  f.restart(sim);open_word(sim);wait(sim,'sibo audio cache network',hits+1);wait(sim,'sibo audio cache saved bytes=',2)
  assert f.personal(sim)==before
  # Valid metadata but corrupted compressed data must fail its content checksum.
  saved=files(sim)[0];data=bytearray(saved.read_bytes());data[-32]^=1;saved.write_bytes(data)
  f.restart(sim);open_word(sim);wait(sim,'sibo audio cache invalid; retry network');wait(sim,'sibo audio cache saved bytes=',3)
  assert f.personal(sim)==before
  sim.screenshot('corrupt-cache-recovered')
  for _ in range(4):f.returned(sim)
  f.choose(sim,'小书包与说明');f.choose(sim,'清理在线缓存')
  end=time.monotonic()+10
  while files(sim) and time.monotonic()<end:time.sleep(.1)
  assert not files(sim),'Audio files survived explicit cache clearing'
  assert f.personal(sim)==before
 print('Encoded MP3 memory replay, disk restart, truncation/checksum recovery and cache clearing preserve personal data: PASS',flush=True)
if __name__=='__main__':main()
