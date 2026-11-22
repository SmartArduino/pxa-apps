#!/usr/bin/env python3
"""Reject acknowledged input with stale frames; still review original PNGs."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from PIL import Image
from collections import Counter

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('evidence', type=Path)
parser.add_argument('--journey', choices=('basic', 'complete', 'loss', 'placement', 'grid'), default='basic')
args = parser.parse_args()
result = json.loads((args.evidence / 'result.json').read_text())
assert result['runtime_passed'], 'Simulator runtime failed'
for profile in result['profiles']:
    name = profile['name']
    assert profile['steps'], f'{name}: no interaction was exercised'
    log = (args.evidence / name / 'run.log').read_text()
    assert not re.search(r'PXA_UI_ERROR|display list overflow|PXA Guest exception|callback failed|software display list rejected|Surface .*failed', log), name
    assert not any(int(n) for n in re.findall(r'\bdenied=(\d+)', log)), name
    captures = {Path(s['path']).stem: s for s in profile['screenshots']}
    for capture in captures.values():
        path = args.evidence / capture['path']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == capture['sha256']
        assert Image.open(path).convert('RGB').getcolors(64) is None, f'{name}: blank/solid screenshot {path.name}'
    if args.journey == 'basic':
        states = ['guide', 'ready', 'build-menu', 'tower-menu', 'combat', 'paused', 'level-map']
        assert len({captures[s]['sha256'] for s in states}) == len(states), (
            f'{name}: different screens reused the same pixels')
        assert captures['guide']['sha256'] != captures['persisted-guide-dismissal']['sha256']
        assert log.count('carrot build ') >= 3, f'{name}: missing towers'
        assert 'carrot wave ' in log and 'carrot progress saved' in log
    elif args.journey == 'complete':
        assert 'carrot win view=5 level=0' in log, 'No real victory'
        assert 'carrot start view=2 level=1' in log, 'Next level did not unlock after relaunch'
        upgrades=Counter(re.findall(r'carrot upgrade [^\n]*slot=(\d+)',log))
        assert sum(upgrades.values())>=3 and max(upgrades.values(),default=0)>=2, 'No legal progression to tier 3'
        assert 'carrot progress saved' in log.split('carrot win', 1)[1]
        assert captures['victory']['sha256'] != captures['second-map']['sha256']
    elif args.journey == 'grid':
        expected = (106,105,74) if name=='esp-mosaico' else (97,119,106)
        for slot in expected:
            assert re.search(r'carrot build [^\n]*slot='+str(slot)+r'\b',log), f'Grid cell {slot} was not built'
        assert 'carrot wave ' in log
        for stem in ('first','adjacent-edge','roadside') if name=='esp-mosaico' else ('west','east','center'):
            assert captures[stem+'-menu']['sha256']!=captures[stem+'-tower']['sha256']
    elif args.journey == 'placement':
        assert log.count('carrot build ') >= 3
        assert 'carrot clear ' in log, 'Obstacle focus was not exercised'
        for slot in (105,200,201):
            assert re.search(r'carrot build [^\n]*slot='+str(slot)+r'\b',log), f'No tower built in cell {slot}'
        assert captures['new-land-tower']['sha256'] != captures['reclaimed-tower']['sha256']
    else:
        assert 'carrot loss view=5 level=0' in log, 'No real defeat'
        assert log.count('carrot start view=2 level=0') >= 2, 'Retry did not reset the level'
        assert captures['defeat']['sha256'] != captures['retry-ready']['sha256']
    print(f'{name}: {args.journey} state transitions, original hashes and resource budget passed')
