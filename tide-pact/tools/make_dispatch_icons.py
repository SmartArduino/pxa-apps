"""Pack the mail, freight and passenger icons from a transparent strip."""
import argparse
from pathlib import Path

from PIL import Image

from make_art import ASSETS, write_texture
from make_voyage_icons import sprite


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("icons", type=Path)
    args = parser.parse_args()
    source = Image.open(args.icons)
    if "A" not in source.getbands():
        parser.error("icon strip must have transparency")
    atlas = Image.new("RGBA", (96, 32))
    for i in range(3):
        icon = sprite(source, i, 0, 3, 1)
        atlas.paste(icon, (32 * i + (32 - icon.width) // 2, (32 - icon.height) // 2))
    atlas.save(ASSETS / "dispatch-icons.png", optimize=True)
    write_texture(atlas, "dispatch_icons")


if __name__ == "__main__":
    main()
