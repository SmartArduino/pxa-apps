"""Prepare the transparent voyage atlas and launcher icon from their raster sources."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, write_texture


def sprite(sheet, col, row, columns, rows):
    left, right = col * sheet.width // columns, (col + 1) * sheet.width // columns
    top, bottom = row * sheet.height // rows, (row + 1) * sheet.height // rows
    cell = sheet.crop((left, top, right, bottom)).convert("RGBA")
    shape = cell.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0)
    bounds = shape.getbbox()
    if bounds is None:
        raise ValueError(f"empty sprite at ({col}, {row})")
    return ImageOps.contain(cell.crop(bounds), (30, 30), Image.Resampling.LANCZOS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("atlas", "instruments", "launcher"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    atlas = Image.open(args.atlas).convert("RGBA")
    instruments = Image.open(args.instruments).convert("RGBA")
    tiles = [sprite(atlas, i % 3, i // 3, 3, 3) for i in range(9)]
    tiles[3:5] = [sprite(instruments, i, 0, 2, 1) for i in range(2)]
    output = Image.new("RGBA", (96, 96))
    for index, tile in enumerate(tiles):
        x = index % 3 * 32 + (32 - tile.width) // 2
        y = index // 3 * 32 + (32 - tile.height) // 2
        output.paste(tile, (x, y))
    output.save(ASSETS / "voyage-icons.png", optimize=True)
    write_texture(output, "voyage_icons")
    launcher = ImageOps.fit(Image.open(args.launcher).convert("RGB"), (128, 128),
                            Image.Resampling.LANCZOS)
    launcher.save(ASSETS.parent / "assets/icon.png", optimize=True)


if __name__ == "__main__":
    main()
