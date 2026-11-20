"""Pack a 2×2 discovery art source into four 160×80 journal tiles."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, write_texture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("art", type=Path)
    args = parser.parse_args()
    source = Image.open(args.art).convert("RGB")
    atlas = Image.new("RGB", (640, 80))
    for index in range(4):
        column, row = index % 2, index // 2
        crop = source.crop((column * source.width // 2 + 2, row * source.height // 2 + 2,
                            (column + 1) * source.width // 2 - 2,
                            (row + 1) * source.height // 2 - 2))
        tile = ImageOps.fit(crop, (160, 80), Image.Resampling.LANCZOS)
        atlas.paste(tile, (index * 160, 0))
    atlas.save(ASSETS / "discoveries.jpg", quality=93, optimize=True)
    write_texture(atlas, "discoveries")


if __name__ == "__main__":
    main()
