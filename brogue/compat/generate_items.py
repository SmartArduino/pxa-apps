#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Generate the pinned Brogue item heat traversal compatibility unit."""
from pathlib import Path
import hashlib
import sys
EXPECTED_SOURCE_SHA256 = 'cf2d43c2af89777d047cd7e4c5fbf6e4a4cd2039fef65fb1169e6a8f7ba21a30'
ORIGINAL_FUNCTION = 'static void fillItemSpawnHeatMap(unsigned short heatMap[DCOLS][DROWS], unsigned short heatLevel, pos loc) {\n    if (pmapAt(loc)->layers[DUNGEON] == DOOR) {\n        heatLevel += 10;\n    } else if (pmapAt(loc)->layers[DUNGEON] == SECRET_DOOR) {\n        heatLevel += 3000;\n    }\n    if (heatMap[loc.x][loc.y] > heatLevel) {\n        heatMap[loc.x][loc.y] = heatLevel;\n    }\n    for (enum directions dir = 0; dir < 4; dir++) {\n        pos neighbor = posNeighborInDirection(loc, dir);\n        if (isPosInMap(neighbor)\n            && !cellHasTerrainFlag(neighbor, T_IS_DEEP_WATER | T_LAVA_INSTA_DEATH | T_AUTO_DESCENT)\n            && isPassableOrSecretDoor(neighbor)\n            && heatLevel < heatMap[neighbor.x][neighbor.y]) {\n\n            fillItemSpawnHeatMap(heatMap, heatLevel, neighbor);\n        }\n    }\n}\n'

def generate(source: Path, output: Path):
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != EXPECTED_SOURCE_SHA256:
        raise SystemExit("Brogue Items.c differs from the reviewed upstream source")
    text = raw.decode('utf-8')
    if text.count(ORIGINAL_FUNCTION) != 1:
        raise SystemExit("Expected exactly one pinned fillItemSpawnHeatMap")
    replacement = Path(__file__).with_name('item_heat_stack.c.inc').read_text(encoding='utf-8')
    if replacement.count('void fillItemSpawnHeatMap(') != 1:
        raise SystemExit("Expected exactly one item heat adapter")
    generated = text.replace(ORIGINAL_FUNCTION, replacement, 1)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generated, encoding='utf-8', newline='\n')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: generate_items.py UPSTREAM_ITEMS OUTPUT')
    generate(Path(sys.argv[1]), Path(sys.argv[2]))
