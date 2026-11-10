#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Encode the project room PNGs using the shared indexed scene pipeline."""
from pathlib import Path
import json
import subprocess
import sys

app = Path(__file__).resolve().parents[1]
encoder = app.parent / 'common/pocket-studio/prepare_scene.py'
metadata = json.loads((app / 'art/rooms/sources.json').read_text())
anchors = {Path(image['path']).stem: image['rug'] for image in metadata['images']}
for name in ('seaside', 'moonlight', 'aquarium'):
    output = app / ('room_' + name + '.inc')
    subprocess.run([sys.executable, str(encoder), str(app / 'art/rooms' / (name + '.png')),
                    str(output), '--width', '384', '--height', '256'], check=True)
    rug = anchors[name]
    x = round(rug['x'] * 384 / rug['source_width'])
    y = round(rug['y'] * 256 / rug['source_height'])
    radius = round(rug['radius_x'] * 384 / rug['source_width'])
    encoded = output.read_text().replace('scene_', 'room_' + name + '_')
    encoded = encoded.replace('source: scene.png', 'source: art/rooms/' + name + '.png')
    output.write_text(encoded + f'\n#include "layout.hpp"\nnamespace {{\n'
                      f'constexpr fish::Rug room_{name}_rug{{{x}, {y}, {radius}}};\n}}\n')
