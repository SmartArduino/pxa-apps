"""Prepare the game portraits from a horizontal three-panel painting."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, write_texture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("portrait_strip", type=Path)
    args = parser.parse_args()
    original = Image.open(args.portrait_strip).convert("RGB")
    sheet = Image.new("RGB", (288, 96))
    for index in range(3):
        face = original.crop((index*original.width//3+2, 2,
                              (index+1)*original.width//3-2, original.height-2))
        sheet.paste(ImageOps.fit(face, (96, 96), Image.Resampling.LANCZOS), (index*96, 0))
    sheet.save(ASSETS / "officers.jpg", quality=94, optimize=True)
    write_texture(sheet, "officers")


if __name__ == "__main__":
    main()
