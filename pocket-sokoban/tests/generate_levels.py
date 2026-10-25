#!/usr/bin/env python3
"""Convert pinned Boxoban ASCII unchanged; solve offline with push BFS.

Usage: python3 tests/generate_levels.py <pinned unfiltered/test/000.txt>
       --report <ignored artifact path>
Only generated reusable data/solution fixtures are written into the app.
"""
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import time

APP = Path(__file__).resolve().parents[1]
DIRS = ((0, -1, 'U'), (1, 0, 'R'), (0, 1, 'D'), (-1, 0, 'L'))
TUTORIALS = [
    ['##########', '#        #', '#        #', '#        #', '#  @$ .  #',
     '#        #', '#        #', '#        #', '#        #', '##########'],
    ['##########', '#        #', '#        #', '#    .   #', '#    $   #',
     '#   @    #', '#  $ .   #', '#        #', '#        #', '##########'],
]


def parse(text):
    result = []
    number, rows = None, []
    for line in text.splitlines() + ['; end']:
        if line.startswith(';'):
            if number is not None:
                assert len(rows) == 10 and all(len(row) == 10 for row in rows)
                result.append((number, rows))
            if line == '; end':
                break
            number, rows = int(line[1:].strip()), []
        elif line:
            rows.append(line)
    return result


def solve(rows, max_states=350000, seconds=30):
    floor = {y * 10 + x for y, row in enumerate(rows)
             for x, tile in enumerate(row) if tile != '#'}
    goals = {y * 10 + x for y, row in enumerate(rows)
             for x, tile in enumerate(row) if tile in '.*+'}
    boxes = tuple(sorted(y * 10 + x for y, row in enumerate(rows)
                         for x, tile in enumerate(row) if tile in '$*'))
    player = next(y * 10 + x for y, row in enumerate(rows)
                  for x, tile in enumerate(row) if tile in '@+')
    delta = [-10, 1, 10, -1]
    # Static dead squares: reverse pull requires two free floor cells.
    live, todo = set(goals), deque(goals)
    while todo:
        target = todo.popleft()
        for d in delta:
            previous = target - d
            if previous in floor and previous - d in floor and previous not in live:
                live.add(previous)
                todo.append(previous)

    def flood(p, occupied, paths=False):
        reachable, todo = {p: ''}, deque([p])
        while todo:
            at = todo.popleft()
            for d, (_, _, char) in zip(delta, DIRS):
                nxt = at + d
                if nxt in floor and nxt not in occupied and nxt not in reachable:
                    reachable[nxt] = reachable[at] + char if paths else ''
                    todo.append(nxt)
        return reachable

    key = boxes, min(flood(player, boxes))
    seen = {key: None}
    queue = deque([(key, player)])
    start = time.monotonic()
    while queue:
        key, player = queue.popleft()
        boxes = key[0]
        if set(boxes) == goals:
            pieces = []
            while seen[key] is not None:
                key, piece = seen[key]
                pieces.append(piece)
            return ''.join(reversed(pieces)), len(seen)
        if len(seen) > max_states or time.monotonic() - start > seconds:
            return None, len(seen)
        reach = flood(player, boxes, True)
        occupied = set(boxes)
        for box in boxes:
            for d, (_, _, char) in zip(delta, DIRS):
                behind, ahead = box - d, box + d
                if behind not in reach or ahead not in live or ahead in occupied:
                    continue
                moved = tuple(sorted((occupied - {box}) | {ahead}))
                child = moved, min(flood(box, moved))
                if child not in seen:
                    seen[child] = key, reach[behind] + char
                    queue.append((child, box))
    return None, len(seen)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--candidates', type=int, default=64)
    parser.add_argument('--count', type=int, default=32)
    args = parser.parse_args()
    source = args.source.read_bytes()
    solved, report = [], []
    for number, rows in parse(source.decode())[:args.candidates]:
        path, states = solve(rows)
        report.append({'dataset_id': number, 'states': states,
                       'solution_length': len(path) if path else None})
        print(json.dumps(report[-1]), flush=True)
        if path:
            solved.append((number, rows, path))
    solved.sort(key=lambda value: (len(value[2]), value[0]))
    assert len(solved) >= args.count, 'not enough solved candidates'
    selected = [(-1-i, rows, solve(rows)[0]) for i, rows in enumerate(TUTORIALS)]
    selected += solved[:args.count]
    output = ['// Generated ASCII conversion; geometry is unchanged. See SOURCES.json.',
              '// Tutorial boards 1-2: original CC0. Boxoban boards: Apache-2.0.',
              '#pragma once', '#include <stdint.h>', 'namespace sokoban {',
              'struct Level { int16_t dataset_id; const char *cells; };',
              'inline constexpr Level LEVELS[] = {']
    for number, rows, _ in selected:
        output.append(f'    {{{number},')
        output.extend('     "' + row + '"' for row in rows)
        output[-1] += '},'
    output += ['};', 'inline constexpr unsigned LEVEL_COUNT = sizeof(LEVELS) / sizeof(LEVELS[0]);', '}']
    (APP / 'levels.hpp').write_text('\n'.join(output) + '\n')
    fixtures = ['// Offline BFS solution fixtures; never compiled into the game.',
                '#pragma once', 'inline constexpr const char *SOLUTIONS[] = {']
    fixtures += [f'    "{path}",' for _, _, path in selected]
    fixtures += ['};']
    (APP / 'tests/solutions.hpp').write_text('\n'.join(fixtures) + '\n')
    (APP / 'upstream/boxoban/SELECTED.txt').write_text(
        ''.join(f'; {number}\n' + '\n'.join(rows) + '\n\n'
                for number, rows, _ in selected if number >= 0))
    args.report.write_text(json.dumps({'source_sha256': hashlib.sha256(source).hexdigest(),
                                     'candidates': report,
                                     'selected_ids': [x[0] for x in selected]}, indent=2) + '\n')

if __name__ == '__main__':
    main()
