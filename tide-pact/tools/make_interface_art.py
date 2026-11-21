"""Prepare the title, four nine-slice control plates, and five navigation icons."""
import argparse
from pathlib import Path

from PIL import Image

from make_art import ASSETS, build_logo, write_texture
from make_voyage_icons import sprite


def save_atlas(image, name):
    # Clear the hidden RGB so the palette is spent on visible artwork.
    mask = image.getchannel("A").point(lambda alpha: 255 if alpha >= 100 else 0)
    clean = Image.composite(image, Image.new("RGBA", image.size), mask)
    clean.save(ASSETS / f"{name}.png", optimize=True)
    write_texture(clean, name)


def build_plates(source):
    tile_width, tile_height = 96, 32
    atlas = Image.new("RGBA", (tile_width * 4, tile_height))
    for index in range(4):
        x, y = index % 2, index // 2
        tile = source.crop((x * source.width // 2, y * source.height // 2,
                            (x + 1) * source.width // 2, (y + 1) * source.height // 2))
        mask = tile.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0)
        bounds = mask.getbbox()
        if not bounds or bounds == (0, 0, tile.width, tile.height):
            raise ValueError(f"Plate {index} needs transparent space around its rim")
        tile = tile.crop(bounds)
        # Pre-filter the bevel to its final four pixels on all four edges.
        # Scaling from the artwork at runtime could skip the bright rim.
        sx = (0, tile.width // 12, tile.width - tile.width // 12, tile.width)
        sy = (0, tile.height // 4, tile.height - tile.height // 4, tile.height)
        dx = (0, 4, tile_width - 4, tile_width)
        dy = (0, 4, tile_height - 4, tile_height)
        for row in range(3):
            for col in range(3):
                piece = tile.crop((sx[col], sy[row], sx[col + 1], sy[row + 1]))
                size = (dx[col + 1] - dx[col], dy[row + 1] - dy[row])
                piece = piece.resize(size, Image.Resampling.LANCZOS)
                atlas.paste(piece, (index * tile_width + dx[col], dy[row]))
    save_atlas(atlas, "controls")


def build_tabs(source):
    atlas = Image.new("RGBA", (32 * 5, 32))
    for index in range(5):
        icon = sprite(source, index % 3, index // 3, 3, 2)
        atlas.paste(icon, (32 * index + (32 - icon.width) // 2, (32 - icon.height) // 2))
    save_atlas(atlas, "tabs")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("title", "plates", "tabs"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    build_logo(Image.open(args.title))
    build_plates(Image.open(args.plates).convert("RGBA"))
    build_tabs(Image.open(args.tabs).convert("RGBA"))


if __name__ == "__main__":
    main()
