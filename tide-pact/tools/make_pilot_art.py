"""Pack separately generated atlas sprites for the nearshore chart."""
import argparse
import struct
from pathlib import Path

from PIL import Image

from make_art import ASSETS, pack_icons


def ships(path):
    size = 20
    original = Image.open(path).convert("RGBA")
    sheet = Image.new("RGBA", (8 * size, 6 * size))
    for hull, bounds in enumerate(((8, 17), (10, 18), (13, 19))):
        frames = []
        for reefed in range(2):
            x = original.width // 3
            y = original.height // 2
            sprite = original.crop((hull * x, reefed * y, (hull + 1) * x, (reefed + 1) * y))
            alpha = sprite.getchannel("A")
            crop = alpha.point(lambda a: 255 if a >= 132 else 0).getbbox()
            if crop is None or alpha.getextrema()[0] >= 132:
                raise ValueError("Expected a transparent ship in each of the six atlas cells")
            frames.append(sprite.crop(crop))
        width = max(sprite.width for sprite in frames)
        height = max(sprite.height for sprite in frames)
        for reefed, sprite in enumerate(frames):
            # Use the same hull scale in both sail states.
            dimensions = (round(bounds[0] * sprite.width / width), round(bounds[1] * sprite.height / height))
            sprite = sprite.resize(dimensions, Image.Resampling.LANCZOS)
            tile = Image.new("RGBA", (size, size))
            tile.paste(sprite, ((size - sprite.width) // 2, (size - sprite.height) // 2))
            for heading in range(8):
                sprite_frame = tile.rotate(-45 * heading, Image.Resampling.BICUBIC)
                sheet.paste(sprite_frame, (heading * size, (hull * 2 + reefed) * size))
    quantized = sheet.convert("RGB").quantize(colors=15, dither=Image.Dither.NONE)
    colors = (quantized.getpalette() + [0] * 48)[:48]
    rgb565 = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
              for r, g, b in zip(colors[::3], colors[1::3], colors[2::3])]
    values = bytes(index if alpha >= 112 else 15 for index, alpha in
                   zip(quantized.tobytes(), sheet.getchannel("A").tobytes()))
    packed = bytes(a | b << 4 for a, b in zip(values[::2], values[1::2]))
    (ASSETS / "pilot_ships.pxi").write_bytes(struct.pack("<16H", *rgb565) + packed)
    sheet.save(ASSETS / "pilot-ships.png", optimize=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ships", type=Path)
    parser.add_argument("marks", type=Path, nargs="?")
    args = parser.parse_args()
    ships(args.ships)
    if args.marks:
        pack_icons(args.marks, "pilot_marks", 3, 2, 22, alpha_cutoff=136)


if __name__ == "__main__":
    main()
