#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Real three-key offline annotation plus existing public follow-along MP3."""
import argparse
import importlib.util
from pathlib import Path
import sys
import time
import urllib.request

HERE = Path(__file__).resolve()
sys.path.insert(0, str(HERE.parents[2] / 'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec = importlib.util.spec_from_file_location('sibo_flows', HERE.with_name('test_simulator.py'))
f = importlib.util.module_from_spec(spec); spec.loader.exec_module(f)

def wait(sim, text, count=1, seconds=50):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        assert sim.process.poll() is None
        if sim.log_path.read_text().count(text) >= count:
            return
        time.sleep(.1)
    raise AssertionError(sim.log_path.read_text()[-5000:])

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('workspace', 'package-root', 'executable', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--offline-only', action='store_true')
    a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    with ThreeKey(a.workspace.resolve(), a.package_root.resolve(), a.output.resolve(),
                  'sibo-learning', 240, 284, 'offline-follow', executable=a.executable.resolve()) as sim:
        sim.key('ENTER'); sim.key('ENTER'); sim.key('ENTER')
        f.choose(sim, '拼音伴读'); sim.expect('下一页')
        sim.screenshot('offline-pinyin'); sim.key('ENTER'); sim.screenshot('offline-page2')
        assert 'HTTP' not in sim.log_path.read_text(), 'Built-in annotations accessed the network'
        assert not list(sim.state.rglob('cache-*.bin')), 'Built-in annotations wrote a JSON cache'
        f.returned(sim)
        if not a.offline_only:
            request = urllib.request.Request(f.ORIGIN + '/oss/shici/poems/p001/follow.mp3', method='HEAD')
            with urllib.request.urlopen(request, timeout=20) as response:
                expected_bytes = int(response.headers['Content-Length'])
            f.choose(sim, '跟着读一读'); wait(sim, 'sibo PCM started')
            sim.screenshot('follow-playing')
            f.choose(sim, '拼音伴读'); sim.expect('下一页')
            f.choose(sim, '暂停'); sim.expect('继续'); sim.screenshot('pinyin-paused')
            sim.key('ENTER'); sim.expect('暂停')
            f.returned(sim); sim.key('ENTER'); sim.expect('下一页','读完啦')
            f.choose(sim, '暂停'); sim.expect('继续'); sim.screenshot('reader-paused')
            sim.key('ENTER'); sim.expect('暂停')
            f.returned(sim); f.choose(sim, '拼音伴读')
            wait(sim, 'sibo audio: 这一段听完啦')
            wait(sim, f'sibo audio cache complete bytes={expected_bytes}')
            sim.screenshot('follow-with-pinyin')
            before = sim.log_path.read_text().count('sibo audio cache network')
            f.returned(sim); f.choose(sim, '跟着读一读')
            wait(sim, 'sibo audio cache hit memory'); wait(sim, 'sibo PCM started', 2)
            assert sim.log_path.read_text().count('sibo audio cache network') == before
            f.choose(sim, '停止朗读')
    print('PASS: offline pinyin without network/cache; follow MP3, annotation while playing and cached replay' if not a.offline_only else 'PASS: offline pinyin without network/cache', flush=True)

if __name__ == '__main__': main()
