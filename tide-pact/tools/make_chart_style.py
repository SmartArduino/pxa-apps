"""Pack generated materials, overhead ships and brass controls for the helm chart."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, pack_icons, texture_bytes

TILE_SIZE = 32
SHIP_CELL = 24


def quadrants(image, columns, rows):
    for i in range(columns * rows):
        col, row = i % columns, i // columns
        yield image.crop((image.width * col // columns, image.height * row // rows,
                          image.width * (col + 1) // columns, image.height * (row + 1) // rows))


def materials(path):
    source = Image.open(path).convert("L")
    shades = bytearray()
    for cell in quadrants(source, 2, 2):
        inset_x, inset_y = cell.width // 12, cell.height // 12
        sample = cell.crop((inset_x, inset_y, cell.width - inset_x, cell.height - inset_y))
        levels = sample.resize((TILE_SIZE, TILE_SIZE), Image.Resampling.LANCZOS).tobytes()
        ranked = sorted(levels)
        low, high = ranked[len(ranked) // 9], ranked[len(ranked) * 8 // 9]
        span = max(10, high - low)
        shades.extend(max(0, min(15, (value - low) * 15 // span)) for value in levels)
    packed = bytes(a | b << 4 for a, b in zip(shades[::2], shades[1::2]))
    (ASSETS / "chart_materials.pxi").write_bytes(packed)


def fleet(path):
    source = Image.open(path).convert("RGBA")
    sheet = Image.new("RGBA", (160, 104))
    sizes = ((12, 18), (15, 20), (17, 21))
    for hull, cell in enumerate(quadrants(source, 3, 1)):
        alpha = cell.getchannel("A")
        bounds = alpha.point(lambda value: 255 if value >= 150 else 0).getbbox()
        if not bounds or alpha.getextrema()[0] > 100:
            raise ValueError(f"Hull {hull + 1} requires a visible ship surrounded by transparency")
        sprite = ImageOps.contain(cell.crop(bounds), sizes[hull], Image.Resampling.LANCZOS)
        frame = Image.new("RGBA", (SHIP_CELL, SHIP_CELL))
        frame.paste(sprite, ((SHIP_CELL - sprite.width) // 2, (SHIP_CELL - sprite.height) // 2))
        for heading in range(8):
            i = hull * 8 + heading
            pose = frame.rotate(-45 * heading, Image.Resampling.BICUBIC)
            sheet.paste(pose, (i % 6 * SHIP_CELL, i // 6 * SHIP_CELL))
    (ASSETS.parent / "assets" / "sea" / "chart-fleet.pxi").write_bytes(texture_bytes(sheet))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("materials", type=Path)
    parser.add_argument("fleet", type=Path)
    parser.add_argument("controls", type=Path)
    args = parser.parse_args()
    materials(args.materials)
    fleet(args.fleet)
    pack_icons(args.controls, "helm_icons", 4, 2, 22, alpha_cutoff=126)


if __name__ == "__main__":
    main()
