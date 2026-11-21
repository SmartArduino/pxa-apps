"""Pack four port maps, six facility icons and nine captain frames for RGB565."""
import argparse
from pathlib import Path

from PIL import Image

from make_art import ASSETS, write_texture


def build_maps(paths):
    width, height = 288, 136
    atlas = Image.new("RGB", (width, height * len(paths)))
    for index, path in enumerate(paths):
        scene = Image.open(path).convert("RGB").resize((width, height), Image.Resampling.LANCZOS)
        atlas.paste(scene, (0, index * height))
    atlas.save(ASSETS / "towns.jpg", quality=91, optimize=True)
    write_texture(atlas, "towns")


def build_sprites(path, rows, size, name, feet=False):
    source = Image.open(path).convert("RGBA")
    width, height = size
    atlas = Image.new("RGBA", (3 * width, rows * height))
    for y in range(rows):
        for x in range(3):
            tile = source.crop((x * source.width // 3, y * source.height // rows,
                                (x + 1) * source.width // 3, (y + 1) * source.height // rows))
            bounds = tile.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0).getbbox()
            if bounds is None or bounds == (0, 0, tile.width, tile.height):
                raise ValueError(f"{name} needs a separate sprite with transparent margins in cell {x}, {y}")
            tile = tile.crop(bounds)
            tile.thumbnail((width - 2, height - 3), Image.Resampling.LANCZOS)
            dx = x * width + (width - tile.width) // 2
            dy = y * height + (height - tile.height - 2 if feet else (height - tile.height) // 2)
            atlas.paste(tile, (dx, dy))
    mask = atlas.getchannel("A").point(lambda alpha: 255 if alpha >= 100 else 0)
    atlas = Image.composite(atlas, Image.new("RGBA", atlas.size), mask)
    atlas.save(ASSETS / f"{name.replace('_', '-')}.png", optimize=True)
    write_texture(atlas, name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for asset in ("lisbon", "madeira", "canaries", "cadiz", "icons", "captain"):
        parser.add_argument(asset, type=Path)
    args = parser.parse_args()
    build_maps((args.lisbon, args.madeira, args.canaries, args.cadiz))
    build_sprites(args.icons, 2, (32, 32), "town_icons")
    build_sprites(args.captain, 3, (24, 32), "town_walk", feet=True)


if __name__ == "__main__":
    main()
