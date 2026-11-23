"""Pack the three side-view ship portraits used by the shipyard."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, write_texture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Transparent three-column sheet in HullNames order")
    args = parser.parse_args()
    source = Image.open(args.source).convert("RGBA")
    width, height = 112, 80
    atlas = Image.new("RGBA", (3 * width, height))
    for index, size in enumerate(((91, 66), (103, 73), (108, 77))):
        cell = source.crop((index * source.width // 3, 0,
                            (index + 1) * source.width // 3, source.height))
        alpha = cell.getchannel("A")
        bounds = alpha.point(lambda value: 255 if value >= 128 else 0).getbbox()
        if bounds is None or alpha.getextrema()[0] != 0:
            raise ValueError(f"Ship {index + 1} needs a silhouette on a transparent background")
        ship = ImageOps.contain(cell.crop(bounds), size, Image.Resampling.LANCZOS)
        atlas.paste(ship, (index * width + (width - ship.width) // 2, height - ship.height - 2))
    atlas.save(ASSETS / "ship_portraits.png", optimize=True)
    write_texture(atlas, "ship_portraits")


if __name__ == "__main__":
    main()
