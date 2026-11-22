#!/usr/bin/env python3
"""Check that visible roads and placement occupancy describe the same tiles."""
from pathlib import Path
import re
import sys
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from import_maps import load_maps

data = load_maps()
g = data['grid']
shift = (g['max_columns'] - g['base_columns']) // 2
header_masks = re.findall(r'0x([0-9a-fA-F]{8})U', (ROOT / 'map_data.hpp').read_text())
expected = [mask for m in data['maps'] for mask in m['blocked']]
assert [int(m, 16) for m in header_masks] == expected, 'Regenerate map_data.hpp'
for index, m in enumerate(data['maps']):
    alpha = Image.open(ROOT / 'art/runtime' / f'scene{index}.png').convert('RGBA').getchannel('A')
    roads = set(map(tuple, m['road_cells']))
    for row in range(g['rows']):
        for col in range(g['base_columns']):
            x = (col + 1) * g['cell_size'] + g['cell_size'] // 2
            y = g['origin_y'] + row * g['cell_size'] + g['cell_size'] // 2
            if (col, row) in roads:
                assert alpha.getpixel((x, y)) > 240
                assert m['blocked'][row] & (1 << (col + shift))
            else:
                # Grass interiors remain clear all the way to the road border.
                for dx in (-12, 0, 12):
                    for dy in (-12, 0, 12):
                        assert alpha.getpixel((x + dx, y + dy)) <= 5, (index, col, row)
print('All road textures, tile interiors and C++ occupancy masks agree')
