"""Pack generated trade icons, land units and port views for the PXA game."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, pack_icons, texture_bytes, write_texture


def split_image(path, columns, rows):
    source = Image.open(path)
    for y in range(rows):
        for x in range(columns):
            yield source.crop((round(x * source.width / columns), round(y * source.height / rows),
                               round((x + 1) * source.width / columns), round((y + 1) * source.height / rows)))


def pack_troops(path):
    width, height = 24, 27
    sheet = Image.new("RGBA", (4 * width, 3 * height))
    for index, cell in enumerate(split_image(path, 4, 3)):
        cell = cell.convert("RGBA")
        bounds = cell.getchannel("A").point(lambda alpha: 255 if alpha >= 116 else 0).getbbox()
        if bounds is None:
            raise ValueError(f"Missing troop pose {index}")
        sprite = cell.crop(bounds)
        sprite.thumbnail((width - 1, height - 1), Image.Resampling.LANCZOS)
        x, y = index % 4 * width, index // 4 * height
        sheet.paste(sprite, (x + (width - sprite.width) // 2, y + height - sprite.height))
    sheet.save(ASSETS / "shore_troops.png", optimize=True)
    write_texture(sheet, "shore_troops")


def pack_views(harbors, towns):
    destination = ASSETS.parent / "assets" / "ports"
    destination.mkdir(exist_ok=True)
    named = zip(("lisbon", "venice", "kilwa", "calicut"), split_image(harbors, 2, 2), strict=True)
    fallback = zip(("atlantic", "mediterranean", "islands", "indian"), split_image(ASSETS / "port-scenes.jpg", 4, 1), strict=True)
    for name, source in [*named, *fallback]:
        view = source.convert("RGB").resize((160, 104), Image.Resampling.LANCZOS)
        (destination / f"{name}.pxi").write_bytes(texture_bytes(view))
    for name, source in zip(("venice-streets", "kilwa-streets"), split_image(towns, 2, 1), strict=True):
        view = ImageOps.crop(source.convert("RGB"), (2, int(source.height * .06), 2, int(source.height * .025)))
        view = view.resize((160, 104), Image.Resampling.LANCZOS)
        (destination / f"{name}.pxi").write_bytes(texture_bytes(view))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("troops", "icons", "harbors", "towns"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    pack_troops(args.troops)
    pack_icons(args.icons, "specialty_icons", 5, 1, 24, 116)
    pack_views(args.harbors, args.towns)


if __name__ == "__main__":
    main()
