#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check every pinyin page's actual LVGL text bounds using three keys."""
import argparse
import importlib.util
import os
from pathlib import Path
import re
import sys
import time

HERE = Path(__file__).resolve()
sys.path.insert(0, str(HERE.parents[2] / 'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec = importlib.util.spec_from_file_location('sibo_flows', HERE.with_name('test_simulator.py'))
f = importlib.util.module_from_spec(spec)
spec.loader.exec_module(f)

def labels(sim):
    time.sleep(.15)
    log = sim.log_path.read_text()
    frames = log.split('STUDY_LAYOUT_BEGIN\n')
    complete = [frame.split('STUDY_LAYOUT_END\n')[0] for frame in frames[1:]
                if 'STUDY_LAYOUT_END\n' in frame]
    assert complete, 'Rebuild the navigation simulator with layout tracing support'
    out = []
    for line in complete[-1].splitlines():
        if line.startswith('STUDY_LABEL '):
            fields = line.split()
            out.append((bytes.fromhex(fields[-1]).decode(), list(map(int, fields[1:-1]))))
    return out

def check_page(sim):
    current = labels(sim)
    counter = next(text for text, _ in current if re.fullmatch(r'拼音 \d+ / \d+', text))
    page, total = map(int, re.findall(r'\d+', counter))
    content = []
    for text, values in current:
        if text in ('返回', '拼音伴读', '上一页', '下一页') or text == counter:
            continue
        x1, y1, x2, y2, cx1, cy1, cx2, cy2, tw, th = values
        assert (x1, y1, x2, y2) == (cx1, cy1, cx2, cy2), (counter, text, 'clipped', values)
        assert tw <= x2 - x1 + 2 and th <= y2 - y1 + 2, (counter, text, 'wrapped/too tall', values)
        content.append(text)
    assert content, counter
    return page, total, content

def exercise(sim):
    # Original Jiangnan lines plus a synthetic line covering long syllables.
    mapping = dict(zip('江南可采莲叶何田鱼戏间东西北庄霜窗',
                       'jiāng nán kě cǎi lián yè hé tián yú xì jiān dōng xī běi zhuāng shuāng chuāng'.split()))
    lines = ['江南可采莲，莲叶何田田。', '鱼戏莲叶间。',
             '鱼戏莲叶东，鱼戏莲叶西。', '鱼戏莲叶南，鱼戏莲叶北。', '庄霜窗。']
    rows = [{'items': [{'char': char, 'pinyin': mapping.get(char, '')} for char in line]} for line in lines]
    expected = [value for row in rows for cell in row['items']
                for value in (cell['pinyin'], cell['char']) if value]
    # p004 has no built-in annotations, so this covers the online-cache path.
    f.seed(sim, f.ORIGIN + '/oss/shici/index.json', [{'id':'p004','meta':'poems/p004/meta.json'}])
    f.seed(sim, f.ORIGIN + '/oss/shici/poems/p004/meta.json', {'pinyin':rows})
    f.choose(sim, '诗词小天地'); sim.key('ENTER'); f.choose(sim, '咏鹅')
    f.choose(sim, '拼音伴读'); sim.expect('下一页')
    actual = []
    for _ in range(100):
        page, total, content = check_page(sim)
        actual.extend(content)
        if page in (1, 2, total): sim.screenshot(f'pinyin-{page}')
        if page == total: break
        sim.expect('下一页'); sim.key('ENTER')
    else: raise AssertionError('Pagination did not finish')
    assert actual == expected, ('Missing/reordered pinyin or characters', actual, expected)
    sim.expect('上一页')
    for expected_page in range(total - 1, 0, -1):
        sim.key('ENTER'); page, _, _ = check_page(sim)
        assert page == expected_page
    sim.expect('下一页')
    f.returned(sim); sim.expect('开始阅读')
    print(f'{sim.stem}: {total} pages complete, no clipping, long syllables fit, forward/backward keys passed', flush=True)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('workspace', 'package-root', 'executable', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--sizes', nargs='*', default=['240x284','320x240','160x160','800x480','480x800'])
    a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    os.environ['STUDY_TRACE_LAYOUT'] = '1'
    for size in a.sizes:
        w, h = map(int, size.split('x'))
        with ThreeKey(a.workspace.resolve(), a.package_root.resolve(), a.output.resolve(),
                      'sibo-learning', w, h, 'pinyin', executable=a.executable.resolve()) as sim:
            exercise(sim)

if __name__ == '__main__': main()
