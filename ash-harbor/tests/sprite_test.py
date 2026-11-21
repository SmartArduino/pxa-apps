"""Regress the scarf spilling from the active kick into its anticipation frame."""
import json, sys
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from sprite_frames import fighter_frames

frames, offsets = fighter_frames(Image.open(root / 'art/source/jin.png'))
assert frames[12].width < 80, 'Anticipation frame includes the next kick scarf'
assert offsets[13][0] < 0 and frames[13].width >= 100, 'Full extended scarf was clipped'
assert offsets[28][0] < 0, 'Rushing pose was clipped to its nominal cell'
layout = json.loads((root / 'art/layout.json').read_text())
resources = json.loads((root / 'resources.json').read_text())['assets']
for actor in ('jin', 'lan', 'zhen', 'ling'):
    for index in range(32):
        page, x, y, width, height = layout[f'{actor}_0_{index}']
        resource = resources[page]
        raw = (root / resource['source']).read_bytes()
        stride = resource['width']
        assert x > 0 and y > 0 and x + width < stride and y + height < resource['height']
        assert not any(raw[(y-1)*stride+x-1:(y-1)*stride+x+width+1])
        assert not any(raw[(y+height)*stride+x-1:(y+height)*stride+x+width+1])
        assert not any(raw[row*stride+x-1] or raw[row*stride+x+width] for row in range(y, y+height))
print('128 complete fighter silhouettes; kick scarf isolated and retained; mirrored tile gutters clear.')
