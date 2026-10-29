#!/usr/bin/env python3
"""Import generated atlases as whole alpha sprites and independent room scenes.

Import uses atlas slicing, nearest-neighbor scaling, palette encoding and pixel
format conversion. Alpha is retained; no face replacement, drawing, retouching
or precompositing is performed.
"""
import json
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
    people = ROOT / 'art/characters'
    rooms = ROOT / 'art/rooms'
    people.mkdir(parents=True, exist_ok=True)
    rooms.mkdir(parents=True, exist_ok=True)
    with Image.open(ROOT / 'art/girl-gestures-atlas.png') as atlas:
        assert atlas.mode == 'RGBA' and atlas.getchannel('A').getextrema()[0] == 0
        for i, name in enumerate(FRAMES):
            image = aligned_character(atlas, i).resize((176, 220), Image.Resampling.NEAREST)
            # The complete body stays in every frame. A 256-color palette
            # encodes the pixel art compactly; its alpha remains transparent.
            indexed = image.quantize(colors=256, method=Image.Quantize.FASTOCTREE,
                                     dither=Image.Dither.NONE)
            indexed.save(people / f'{name}.png', optimize=True)
            # PXR encoding 8 matches the renderer's premultiplied BGRA format.
            # This conversion also removes meaningless RGB in alpha-zero pixels.
            rgba = indexed.convert('RGBA').tobytes()
            bgra = bytearray(len(rgba))
            for at in range(0, len(rgba), 4):
                r, g, b, a = rgba[at:at+4]
                bgra[at:at+4] = bytes(((b*a+127)//255, (g*a+127)//255,
                                     (r*a+127)//255, a))
            (people / f'{name}.bgra').write_bytes(bgra)
    with Image.open(ROOT / 'art/room-atlas.png') as atlas:
        for i, name in enumerate(SCENES):
            image = tile(atlas, i, 2, 2).resize((320, 240), Image.Resampling.NEAREST)
            image.convert('RGB').save(rooms / f'{name}.png', optimize=True)
    assets = [{'kind': 'image', 'source': f'art/rooms/{name}.png',
               'path': f'assets/companion/room-{name}.pxr', 'encoding': 'rgb565'}
              for name in SCENES]
    assets += [{'kind': 'image', 'source': f'art/characters/{name}.bgra',
                'path': f'assets/companion/frames/{name}.pxr',
                'encoding': 'bgra8888-premultiplied', 'width': 176, 'height': 220}
               for name in FRAMES]
    (ROOT / 'resources.json').write_text(json.dumps({'assets': assets}, indent=2) + '\n')


if __name__ == '__main__':
    main()
