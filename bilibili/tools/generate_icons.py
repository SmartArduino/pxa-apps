#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Render original small UI vectors. Development dependencies only."""
from pathlib import Path
from io import BytesIO
import cairosvg
from PIL import Image

shapes = {
    'home': '<path d="M3 11 12 3 21 11 M6 10 V21 H18 V10 M10 21 V15 H14 V21"/>',
    'person': '<circle cx="12" cy="8" r="4"/><path d="M4 22 V20 A8 8 0 0 1 20 20 V22"/>',
    'search': '<circle cx="10.5" cy="10.5" r="6.5"/><path d="M15.5 15.5 21 21"/>',
    'back': '<path d="M15 5 8 12 15 19"/>',
    'next': '<path d="M9 5 16 12 9 19"/>',
    'refresh': '<path d="M20 9 A8 8 0 1 0 20 16 M20 3 V9 H14"/>',
    'bookmark': '<path d="M6 3 H18 V22 L12 18 6 22 Z"/>',
    'bookmark-filled': '<path d="M6 3 H18 V22 L12 18 6 22 Z" fill="currentColor"/>',
    'clock': '<circle cx="12" cy="12" r="9"/><path d="M12 6 V12 L16 15"/>',
    'play': '<path d="M8 4 20 12 8 20 Z" fill="currentColor" stroke="none"/>',
    'pause': '<path d="M8 5 V19 M16 5 V19" stroke-width="4"/>',
    'check': '<path d="M4 12 9 17 20 6"/>',
    'delete': '<path d="M4 6 H20 M9 6 V3 H15 V6 M6 6 7 21 H17 L18 6 M10 10 V17 M14 10 V17"/>',
}
dest = Path(__file__).resolve().parents[1] / 'assets/ui'
dest.mkdir(parents=True, exist_ok=True)
palette = {name:('24292f','ffffff') for name in ('home','person','search','back','next','refresh','bookmark','bookmark-filled','clock','play','pause')}
expected = set()
for name, colors in palette.items():
    for color in colors:
        shape = shapes[name]
        svg = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" color="#{color}"><g fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">{shape}</g></svg>'
        for size in (48,96):
            expected.add(f'{name}-{color}-{size}.png')
            png = cairosvg.svg2png(bytestring=svg.encode(), output_width=size*4, output_height=size*4)
            Image.open(BytesIO(png)).convert('RGBA').resize((size, size), Image.Resampling.LANCZOS).save(dest/f'{name}-{color}-{size}.png', optimize=True)
for path in dest.glob('*.png'):
    if path.stem.rsplit('-', 2)[0] in shapes and path.name not in expected:
        path.unlink()
print('Rendered original Bilibili UI icons')
