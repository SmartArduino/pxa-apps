#!/usr/bin/env python3
"""Check that imported whole-body poses contain no neighbouring elbow fragments."""
from collections import deque
from pathlib import Path
import unittest
import importlib.util

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
FRAMES = ('neutral', 'speaking', 'happy', 'sad', 'thinking', 'surprised', 'angry', 'blink')


def opaque_components(image):
    width, height = image.size
    remaining = {i for i, alpha in enumerate(image.getchannel('A').tobytes())
                 if alpha >= 16}
    components = []
    while remaining:
        start = remaining.pop()
        queue = deque([start])
        count, bottom = 0, 0
        while queue:
            at = queue.popleft()
            x, y = at % width, at // width
            count += 1
            bottom = max(bottom, y)
            neighbours = [at - width, at + width]
            if x:
                neighbours.append(at - 1)
            if x + 1 < width:
                neighbours.append(at + 1)
            for neighbour in neighbours:
                if neighbour in remaining:
                    remaining.remove(neighbour)
                    queue.append(neighbour)
        components.append((count, bottom))
    return sorted(components, reverse=True)


class CharacterImportTest(unittest.TestCase):
    @staticmethod
    def importer():
        spec = importlib.util.spec_from_file_location('prepare_art', ROOT / 'tools/prepare_art.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_common_scale_and_anchor(self):
        importer = self.importer()
        atlas = Image.new('RGBA', importer.CHARACTER_ATLAS_SIZE)
        draw = ImageDraw.Draw(atlas)
        for i, head in enumerate(importer.CHARACTER_HEAD_CENTERS):
            bottom = importer.CHARACTER_Y_EDGES[i // 4 + 1]
            draw.rectangle((head - 20, bottom - 300, head + 19, bottom - 261),
                           fill=(255, 255, 255, 255))
        # Identical source landmarks must have the same dimensions and position
        # after import, even though the eight gutter crops have different sizes.
        bounds = [importer.aligned_character(atlas, i).resize(
            (176, 220), Image.Resampling.NEAREST).getbbox() for i in range(8)]
        self.assertTrue(all(box == bounds[0] for box in bounds))

    def test_alignment_retains_each_whole_body(self):
        importer = self.importer()
        with Image.open(ROOT / 'art/girl-gestures-atlas.png') as atlas:
            for i, name in enumerate(FRAMES):
                with self.subTest(pose=name):
                    before = opaque_components(importer.character_tile(atlas, i))[0][0]
                    after = opaque_components(importer.aligned_character(atlas, i))[0][0]
                    self.assertEqual(before, after, 'Alignment clipped the character')

    def test_no_separate_lower_body_fragments(self):
        for name in FRAMES:
            with self.subTest(pose=name), Image.open(ROOT / f'art/characters/{name}.png') as png:
                image = png.convert('RGBA')
                components = opaque_components(image)
                self.assertGreater(components[0][0], image.width * image.height // 2)
                # Separate emotion marks may float beside the head. A separate
                # lower-half component was a neighbouring pose's cut-off elbow.
                for count, bottom in components[1:]:
                    if count >= 8:
                        self.assertLess(bottom, image.height // 2)

    def test_premultiplied_assets(self):
        for name in FRAMES:
            with self.subTest(pose=name), Image.open(ROOT / f'art/characters/{name}.png') as png:
                data = (ROOT / f'art/characters/{name}.bgra').read_bytes()
                self.assertEqual(len(data), png.width * png.height * 4)
                for at in range(0, len(data), 4):
                    b, g, r, alpha = data[at:at + 4]
                    self.assertLessEqual(max(b, g, r), alpha)


if __name__ == '__main__':
    unittest.main()
