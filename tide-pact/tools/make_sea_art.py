"""Pack three moving hull sprites and the story panels for asset streaming."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, pack_icons, texture_bytes, write_texture

FRAME_WIDTH, FRAME_HEIGHT = 40, 30
HEAVE = (0, -1, -1, 0, 0, 1, 1, 0)
ROLL = (0, 2, 4, 2, 0, -2, -4, -2)
SCENE_SIZE = (160, 104)
ENCOUNTERS = ("castaways", "false-beacon", "rigging", "water-boat",
              "fog-bank", "grounded", "reversed-flag")


def vessels(source):
    atlas = Image.new("RGBA", (len(HEAVE) * FRAME_WIDTH, 3 * FRAME_HEIGHT))
    for row, (name, size) in enumerate(zip(("cutter", "caravel", "carrack"),
                                           ((30, 20), (37, 25), (40, 27)))):
        with Image.open(source / f"{name}.png") as original:
            sprite = original.convert("RGBA")
        alpha = sprite.getchannel("A")
        bounds = alpha.point(lambda value: 255 if value >= 160 else 0).getbbox()
        if not bounds or alpha.getextrema()[0] != 0:
            raise ValueError(f"{name} must have a visible ship and a transparent background")
        hull = ImageOps.contain(sprite.crop(bounds), size, Image.Resampling.LANCZOS)
        left = (FRAME_WIDTH - hull.width) // 2
        for frame, (heave, roll) in enumerate(zip(HEAVE, ROLL)):
            tile = Image.new("RGBA", (FRAME_WIDTH, FRAME_HEIGHT))
            tile.paste(hull, (left, FRAME_HEIGHT - 2 - hull.height + heave))
            if roll:
                tile = tile.rotate(roll, Image.Resampling.BICUBIC,
                                   center=(FRAME_WIDTH // 2, FRAME_HEIGHT - 3))
            atlas.paste(tile, (frame * FRAME_WIDTH, row * FRAME_HEIGHT))
    atlas.save(ASSETS / "vessels.png", optimize=True)
    write_texture(atlas, "vessels")


def write_scene(destination, name, source):
    tile = ImageOps.fit(source.convert("RGB"), SCENE_SIZE, Image.Resampling.LANCZOS)
    (destination / f"{name}.pxi").write_bytes(texture_bytes(tile))
    return tile


def scenes(source):
    destination = ASSETS.parent / "assets" / "sea"
    destination.mkdir(parents=True, exist_ok=True)
    width, height = SCENE_SIZE
    preview = Image.new("RGB", (len(ENCOUNTERS) * width, height))
    for index, name in enumerate(ENCOUNTERS):
        with Image.open(source / f"{name}.png") as original:
            tile = write_scene(destination, name, original)
        preview.paste(tile, (index * width, 0))
    preview.save(ASSETS / "encounters.jpg", quality=94, subsampling=0)
    with Image.open(ASSETS / "sea-stories.jpg") as existing:
        for index, name in enumerate(("shoal-lantern", "doldrums")):
            write_scene(destination, name, existing.crop((index * width, 0, (index + 1) * width, height)))
    with Image.open(ASSETS / "aden-ship.jpg") as existing:
        write_scene(destination, "aden-ship", existing)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("generated", type=Path, help="directory containing the generated PNG sources")
    args = parser.parse_args()
    vessels(args.generated)
    scenes(args.generated)
    effects=args.generated / 'skirmish-effects.png'
    if effects.is_file():
        pack_icons(effects,'skirmish',4,2,26)


if __name__ == "__main__":
    main()
