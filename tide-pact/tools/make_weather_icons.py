"""Pack the transparent weather sprites in game enum order."""
import argparse
from pathlib import Path

from PIL import Image

from make_art import ASSETS, write_texture
from make_voyage_icons import sprite


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sheet", type=Path)
    args = parser.parse_args()
    source = Image.open(args.sheet)
    if "A" not in source.getbands():
        parser.error("weather sheet must have transparency")
    atlas = Image.new("RGBA", (128, 32))
    for i in range(4):
        icon = sprite(source, i % 2, i // 2, 2, 2)
        atlas.paste(icon, (32 * i + (32 - icon.width) // 2, (32 - icon.height) // 2))
    atlas.save(ASSETS / "weather-icons.png", optimize=True)
    write_texture(atlas, "weather_icons")


if __name__ == "__main__":
    main()
