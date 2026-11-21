"""Prepare the walkable Lisbon riverside backdrop."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, write_texture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scene", type=Path)
    args = parser.parse_args()
    source = Image.open(args.scene).convert("RGB")
    scene = ImageOps.fit(source, (288, 136), Image.Resampling.LANCZOS)
    scene.save(ASSETS / "outskirts.jpg", quality=93, optimize=True)
    write_texture(scene, "outskirts")


if __name__ == "__main__":
    main()
