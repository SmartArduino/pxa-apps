"""Pack the transparent banner sprites and the three shore illustrations."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, write_texture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("merchant", "portuguese", "castilian", "mine", "porcelain", "ledger"):
        parser.add_argument(name, type=Path)
    assets = parser.parse_args()
    width, height = 96, 56
    flags = Image.new("RGBA", (3 * width, height))
    for index, path in enumerate((assets.merchant, assets.portuguese, assets.castilian)):
        with Image.open(path) as source:
            sprite = source.convert("RGBA")
            alpha = sprite.getchannel("A")
            mask = alpha.point(lambda amount: 255 if amount >= 160 else 0)
            bounds = mask.getbbox()
            if not bounds or mask.getextrema()[0] != 0:
                raise ValueError(f"{path} needs a transparent border around the flag")
            sprite.putalpha(alpha.point(lambda amount: amount if amount >= 128 else 0))
            tile = ImageOps.contain(sprite.crop(bounds), (width - 4, height - 2), Image.Resampling.LANCZOS)
            flags.paste(tile, (index * width + (width - tile.width) // 2, (height - tile.height) // 2))
    flags.save(ASSETS / "flags.png", optimize=True)
    write_texture(flags, "flags")

    width, height = 160, 104
    scenes = Image.new("RGB", (3 * width, height))
    for index, path in enumerate((assets.mine, assets.porcelain, assets.ledger)):
        with Image.open(path) as source:
            tile = ImageOps.fit(source.convert("RGB"), (width, height), Image.Resampling.LANCZOS)
        scenes.paste(tile, (index * width, 0))
    scenes.save(ASSETS / "shore-tasks.jpg", quality=95, subsampling=0)
    write_texture(scenes, "shore_tasks")


if __name__ == "__main__":
    main()
