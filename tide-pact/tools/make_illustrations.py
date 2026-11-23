"""Export detailed title, Lisbon streets and sea-story artwork from the source paintings."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, texture_bytes


ENCOUNTERS = ("castaways", "false-beacon", "fog-bank", "grounded", "reversed-flag", "rigging", "water-boat")
LEGENDS = ("doldrums", "shoal-lantern")


def write(path, dest, size, *, preserve_positions=False):
    image = Image.open(path).convert("RGB")
    if preserve_positions:
        image = image.resize(size, Image.Resampling.LANCZOS)
    else:
        image = ImageOps.fit(image, size, Image.Resampling.LANCZOS)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(texture_bytes(image))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("title_wide", type=Path)
    parser.add_argument("title_compact", type=Path)
    parser.add_argument("lisbon", type=Path)
    parser.add_argument("encounters", type=Path)
    parser.add_argument("legends", type=Path)
    args = parser.parse_args()
    assets = ASSETS.parent / "assets"
    write(args.title_wide, ASSETS / "harbor.pxi", (854, 480))
    write(args.title_compact, ASSETS / "title_compact.pxi", (360, 360))
    # The town uses normalized positions for facility signs and walking paths.
    write(args.lisbon, assets / "ports/lisbon-streets.pxi", (576, 272), preserve_positions=True)
    for source, names in ((args.encounters, ENCOUNTERS), (args.legends, LEGENDS)):
        for stem in names:
            write(source / f"{stem}.png", assets / f"sea/{stem}.pxi", (320, 208))


if __name__ == "__main__":
    main()
