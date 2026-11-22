"""Pack six illustrated natural history entries into the game atlas."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps
from make_art import write_texture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for entry in ("laurel", "pigeon", "saltwort", "flamingo", "dolphin", "turtle"):
        parser.add_argument(entry, type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    width, height = 144, 96
    entries = (args.laurel, args.pigeon, args.saltwort, args.flamingo, args.dolphin, args.turtle)
    atlas = Image.new("RGB", (2 * width, 3 * height))
    for index, entry in enumerate(entries):
        with Image.open(entry) as original:
            plate = ImageOps.fit(original.convert("RGB"), (width, height), Image.Resampling.LANCZOS)
        atlas.paste(plate, ((index % 2) * width, (index // 2) * height))
    if args.preview:
        atlas.save(args.preview, quality=95, subsampling=0)
    write_texture(atlas, "nature")


if __name__ == "__main__":
    main()
