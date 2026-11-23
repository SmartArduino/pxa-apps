"""Pack journal badges and the illustrated merchant rooms."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, pack_icons, texture_bytes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("icons", type=Path, help="Transparent four-column by two-row icon sheet")
    parser.add_argument("guilds", type=Path, help="Two guild interiors, Lisbon to the left of Calicut")
    args = parser.parse_args()
    pack_icons(args.icons, "journal_icons", 4, 2, 32)

    source = Image.open(args.guilds).convert("RGB")
    dest = ASSETS.parent / "assets" / "ports"
    for index, port in enumerate(("lisbon", "calicut")):
        begin = index * source.width // 2
        end = (index + 1) * source.width // 2
        room = source.crop((begin, 0, end, source.height))
        image = ImageOps.fit(room, (352, 232), Image.Resampling.LANCZOS)
        (dest / (port + "-guild.pxi")).write_bytes(texture_bytes(image))


if __name__ == "__main__":
    main()
