#!/usr/bin/env python3
"""Compile deterministic frame,pad-mask CSV into a diagnostic-only input track."""
import csv
from pathlib import Path
import sys
rows = []
for row in csv.reader(Path(sys.argv[1]).read_text().splitlines()):
    if not row:
        continue
    if len(row) != 2:
        raise ValueError('Expected frame,held_core_pad_mask')
    frame, mask = map(int, row)
    if not 0 <= frame <= 0xffffffff or not 0 <= mask < 4096:
        raise ValueError('Frame or pad mask out of range')
    if rows and frame <= rows[-1][0]:
        raise ValueError('Input frames must be strictly increasing')
    rows.append((frame, mask))
text = '/* Generated diagnostic input; never enable in a release build. */\n'
text += 'static unsigned gba_bench_input(unsigned frame) {\n    unsigned held=0;\n'
for frame, mask in rows:
    text += f'    if(frame<{frame}u)return held; held={mask}u;\n'
text += '    return held;\n}\n'
Path(sys.argv[2]).write_text(text)
