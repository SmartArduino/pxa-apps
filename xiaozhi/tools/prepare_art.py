#!/usr/bin/env python3
"""Import generated atlases as whole alpha sprites and independent room scenes.

Import uses atlas slicing, nearest-neighbor scaling, lossless PNG encoding and pixel
format conversion. Alpha is retained; no face replacement, drawing, retouching
or precompositing is performed.
"""
import json
import argparse
import subprocess
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FRAMES = ('neutral', 'speaking', 'happy', 'sad', 'thinking', 'surprised', 'angry', 'blink')
SCENES = ('morning', 'day', 'evening', 'night')
# The generated atlas is not a uniform grid: sleeves extend across its nominal
# quarter-width boundaries. Slice at the transparent gutters between poses so
# each sprite retains its complete arms without importing a neighbour's elbow.
CHARACTER_ATLAS_SIZE = (1587, 991)
CHARACTER_X_EDGES = (0, 420, 813, 1202, 1587)
CHARACTER_Y_EDGES = (0, 501, 991)
# All poses share one source-pixel scale and head anchor. Resizing each gutter
# crop separately stretches wider gestures and makes the face change size.
CHARACTER_CANVAS = (400, 500)
CHARACTER_HEAD_CENTERS = (219, 616, 1008, 1395, 209, 616, 1008, 1395)


def tile(atlas, index, columns, rows):
    # Image generators can return dimensions that do not divide by the grid.
    x, y = index % columns, index // columns
    return atlas.crop((round(x * atlas.width / columns),
                       round(y * atlas.height / rows),
                       round((x + 1) * atlas.width / columns),
                       round((y + 1) * atlas.height / rows)))


def character_tile(atlas, index):
    if atlas.size != CHARACTER_ATLAS_SIZE:
        raise ValueError('Review character gutters after replacing the atlas')
    x, y = index % 4, index // 4
    return atlas.crop((CHARACTER_X_EDGES[x], CHARACTER_Y_EDGES[y],
                       CHARACTER_X_EDGES[x + 1], CHARACTER_Y_EDGES[y + 1]))


def aligned_character(atlas, index):
    pose = character_tile(atlas, index)
    column = index % 4
    head_x = CHARACTER_HEAD_CENTERS[index] - CHARACTER_X_EDGES[column]
    canvas = Image.new('RGBA', CHARACTER_CANVAS)
    # The common 400x500 canvas maps to 176x220 with the same 0.44 scale on
    # both axes. Align the head horizontally and the seated-body baseline.
    canvas.paste(pose, (CHARACTER_CANVAS[0] // 2 - head_x,
                        CHARACTER_CANVAS[1] - pose.height))
    return canvas


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--oxipng', type=Path, help='optional host-only lossless PNG optimizer')
    args = parser.parse_args()
    people = ROOT / 'art/characters'
    rooms = ROOT / 'art/rooms'
    people.mkdir(parents=True, exist_ok=True)
    rooms.mkdir(parents=True, exist_ok=True)
    with Image.open(ROOT / 'art/girl-gestures-atlas.png') as atlas:
        assert atlas.mode == 'RGBA' and atlas.getchannel('A').getextrema()[0] == 0
        for i, name in enumerate(FRAMES):
            image = aligned_character(atlas, i).resize((176, 220), Image.Resampling.NEAREST)
            # PNG compression is lossless. Preserve every source color and
            # alpha value: global 256-color quantization bands the skin tones.
            output = people / f'{name}.png'
            image.save(output, optimize=True)
            if args.oxipng:
                # Do not enable alpha optimization: even hidden RGB values
                # remain identical to the source artwork.
                subprocess.run([str(args.oxipng.resolve()), '-o', 'max', '--fast',
                                '-z', '--timeout', '15', '--strip', 'safe', str(output)], check=True)
                with Image.open(output) as compressed:
                    if compressed.convert('RGBA').tobytes() != image.tobytes():
                        raise ValueError(f'Optimizer changed pixels: {output}')
    with Image.open(ROOT / 'art/room-atlas.png') as atlas:
        for i, name in enumerate(SCENES):
            image = tile(atlas, i, 2, 2).resize((320, 240), Image.Resampling.NEAREST)
            image.convert('RGB').save(rooms / f'{name}.png', optimize=True)
    assets = [{'kind': 'image', 'source': f'art/rooms/{name}.png',
               'path': f'assets/companion/room-{name}.pxr', 'encoding': 'png-rgb565'}
              for name in SCENES]
    # PNG poses stay compressed on the device; Assets 2.4 decodes and caches
    # native premultiplied pixels. Backgrounds store PNG-compressed RGB565 pixels.
    assets += [{'kind': 'image', 'source': f'art/characters/{name}.png',
                'path': f'assets/companion/frames/{name}.png', 'encoding': 'png'}
               for name in FRAMES]
    (ROOT / 'resources.json').write_text(json.dumps({'assets': assets}, indent=2) + '\n')


if __name__ == '__main__':
    main()
