#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the real LVGL tree for lazy pages and retained navigation state."""
import argparse
import importlib.util
import json
import os
import re
from pathlib import Path
import sys
import time

HERE = Path(__file__).resolve()
sys.path.insert(0, str(HERE.parents[2] / 'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec = importlib.util.spec_from_file_location('sibo_flow', HERE.with_name('test_simulator.py'))
flow = importlib.util.module_from_spec(spec)
spec.loader.exec_module(flow)


def tree(sim):
    time.sleep(.1)
    frames = sim.log_path.read_text().split('STUDY_TREE_BEGIN\n')
    complete = [f.split('STUDY_TREE_END\n')[0] for f in frames[1:]
                if 'STUDY_TREE_END\n' in f]
    assert complete, 'Build the navigation simulator with tree tracing support'
    lines = complete[-1].splitlines()
    nodes = int(next(s.split()[1] for s in lines if s.startswith('STUDY_TREE_NODES ')))
    labels = [bytes.fromhex(s.split()[1]).decode() for s in lines
              if s.startswith('STUDY_TREE_LABEL ')]
    return nodes, labels


def exercise(sim):
    sim.expect('诗词小天地')
    initial, labels = tree(sim)
    # These labels occur on different learning pages, including hidden ones
    # in the old eager tree. A screenshot of the home page cannot catch this.
    for dormant in ('课本古诗 · 75 首', '耳朵听，脑袋想', '对照答案', '收获小星星'):
        assert dormant not in labels, ('eager page created at startup', dormant)
    sim.screenshot('lazy-home')
    flow.choose(sim, '诗词小天地')
    sim.expect('课本古诗')
    poetry_nodes, labels = tree(sim)
    assert poetry_nodes > initial and '把一首诗，读成一幅画' in labels
    assert '耳朵听，脑袋想' not in labels
    for _ in range(4):
        flow.returned(sim)
        sim.expect('诗词小天地')
        assert tree(sim)[0] == poetry_nodes, 'Returning home destroyed the retained page'
        sim.key('ENTER')
        sim.expect('课本古诗')
        assert tree(sim)[0] == poetry_nodes, 'Revisiting a page duplicated its widgets'
    flow.returned(sim)
    flow.choose(sim, '小书包与说明')
    sim.expect('阅读使用说明')
    about_nodes, labels = tree(sim)
    assert about_nodes > poetry_nodes and '我的小书包' in labels
    flow.returned(sim)
    # Earlier pages must keep their callback addresses after later pages mount.
    flow.choose(sim, '诗词小天地')
    sim.key('ENTER'); sim.expect('江南')
    sim.key('ENTER'); sim.expect('开始阅读')
    sim.screenshot('lazy-detail')
    flow.returned(sim); sim.expect('江南')
    flow.returned(sim); sim.expect('课本古诗')
    flow.returned(sim)
    sim.expect('诗词小天地')
    # Existing response bodies do not need to be read to make Home usable.
    private = next((sim.state / 'private-files').iterdir())
    cached = {}
    for i in range(4):
        path = private / f'cache-{i}.bin'
        cached[path] = f'cached-response-{i}?\n' + json.dumps({'text': 'x' * 10000})
        path.write_text(cached[path])
    (private / 'cache-order.json').write_text('[1,2,3,4]')
    sim.log.write('STUDY_STARTUP_RESTART\n'); sim.log.flush()
    flow.restart(sim)
    sim.expect('诗词小天地')
    for path, value in cached.items():
        assert path.read_text() == value, 'Opening Home changed an unused text cache'
    sim.process.terminate()
    sim.process.wait(timeout=3)
    restarted = sim.log_path.read_text().split('STUDY_STARTUP_RESTART\n')[-1]
    reads = re.findall(r'STUDY_CACHE_READ cache-[0-3]\.bin (\d+)', restarted)
    assert not reads, ('Home preloaded cached response bodies', reads)
    return {'initial_nodes': initial,
            'after_poetry_nodes': poetry_nodes, 'after_about_nodes': about_nodes,
            'retained_revisits': 4, 'startup_text_cache_read_bytes': 0,
            'unused_cache_files_preserved': 4, 'passed': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('workspace', 'package-root', 'executable', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--sizes', nargs='*', default=['240x284', '160x160', '800x480', '480x800'])
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    os.environ['STUDY_TRACE_TREE'] = '1'
    os.environ['STUDY_TRACE_CACHE_IO'] = '1'
    results = []
    for size in args.sizes:
        width, height = map(int, size.split('x'))
        with ThreeKey(args.workspace.resolve(), args.package_root.resolve(),
                      args.output.resolve(), 'sibo-learning', width, height,
                      'startup', executable=args.executable.resolve()) as sim:
            result = exercise(sim)
            result['size'] = [width, height]
            results.append(result)
            print(json.dumps(result, ensure_ascii=False), flush=True)
    (args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
