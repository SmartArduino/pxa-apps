"""Pack a 2×2 portrait painting in story order for the game."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, write_texture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("portraits", type=Path)
    args = parser.parse_args()
    painting = Image.open(args.portraits).convert("RGB")
    width, height = painting.width // 2, painting.height // 2
    sheet = Image.new("RGB", (256, 64))
    for index in range(4):
        x, y = index % 2 * width, index // 2 * height
        portrait = painting.crop((x + 5, y + 5, x + width - 5, y + height - 5))
        sheet.paste(ImageOps.fit(portrait, (64, 64), Image.Resampling.LANCZOS), (64 * index, 0))
    sheet.save(ASSETS / "port-people.jpg", quality=94, optimize=True)
    write_texture(sheet, "port_people")


if __name__ == "__main__":
    main()
