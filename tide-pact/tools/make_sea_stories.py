"""Pack the shoal and doldrums illustrations for the encounter screen."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, write_texture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shoal_lantern", type=Path)
    parser.add_argument("doldrums", type=Path)
    args = parser.parse_args()
    atlas = Image.new("RGB", (320, 104))
    for i, path in enumerate((args.shoal_lantern, args.doldrums)):
        sheet = Image.open(path).convert("RGB")
        tile = ImageOps.fit(sheet, (160, 104), Image.Resampling.LANCZOS)
        atlas.paste(tile, (160 * i, 0))
    atlas.save(ASSETS / "sea-stories.jpg", quality=93, optimize=True)
    write_texture(atlas, "sea_stories")


if __name__ == "__main__":
    main()
