#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Generate one pinned Brogue translation unit without modifying upstream."""
from pathlib import Path
import hashlib
import sys

EXPECTED_SOURCE_SHA256 = '3d6e99a7bc9be8135b48bacd0bda51cb90495ef55c753ff9cbb5a32053f24f80'
ORIGINAL_FUNCTION = 'static short fillContiguousRegion(short **grid, short x, short y, short fillValue) {\n    enum directions dir;\n    short newX, newY, numberOfCells = 1;\n\n    grid[x][y] = fillValue;\n\n    // Iterate through the four cardinal neighbors.\n    for (dir=0; dir<4; dir++) {\n        newX = x + nbDirs[dir][0];\n        newY = y + nbDirs[dir][1];\n        if (!coordinatesAreInMap(newX, newY)) {\n            break;\n        }\n        if (grid[newX][newY] == 1) { // If the neighbor is an unmarked region cell,\n            numberOfCells += fillContiguousRegion(grid, newX, newY, fillValue); // then recurse.\n        }\n    }\n    return numberOfCells;\n}\n'


def generate(source: Path, output: Path):
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != EXPECTED_SOURCE_SHA256:
        raise SystemExit("Brogue Grid.c differs from the reviewed upstream source")
    text = raw.decode('utf-8')
    if text.count(ORIGINAL_FUNCTION) != 1:
        raise SystemExit("Expected exactly one pinned fillContiguousRegion implementation")
    replacement = Path(__file__).with_name('fill_region_stack.c.inc').read_text(encoding='utf-8')
    if replacement.count('static short fillContiguousRegion(') != 1:
        raise SystemExit("Expected exactly one blob-fill adapter")
    generated = text.replace(ORIGINAL_FUNCTION, replacement, 1)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generated, encoding='utf-8', newline='\n')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: generate_grid.py UPSTREAM_GRID OUTPUT')
    generate(Path(sys.argv[1]), Path(sys.argv[2]))
