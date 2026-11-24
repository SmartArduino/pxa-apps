"""Pack expedition plates into streamed RGB565 indexed art."""
import argparse
import json
from pathlib import Path

from PIL import Image, ImageOps

from make_art import ASSETS, texture_bytes, write_texture

LOCATIONS = ("newfoundland", "orinoco", "greenland", "shetlands", "malacca", "naha", "kimberley", "galapagos", "quanzhou")


def pack_study(name, image):
    sheet = Image.open(image).convert("RGB")
    sheet.thumbnail((480, 780), Image.Resampling.LANCZOS)
    sheet.save(ASSETS / f"world-{name}.jpg", quality=93, optimize=True)
    output = ASSETS.parent / "assets" / "world"
    output.mkdir(exist_ok=True)
    for index, key in enumerate(("shore", "animal", "plant")):
        top, bottom = (round(sheet.height * i / 3) for i in (index, index + 1))
        strip = sheet.crop((0, top + 2, sheet.width, bottom - 2))
        panel = ImageOps.fit(strip, (352, 168), Image.Resampling.LANCZOS)
        (output / f"{name}-{key}.pxi").write_bytes(texture_bytes(panel))


def pack_animals(image):
    sheet = Image.open(image)
    if "A" not in sheet.getbands():
        raise ValueError("Animal frames need a transparent background")
    atlas = Image.new("RGBA", (96, 40))
    for row in range(2):
        for column in range(4):
            x1, x2 = (round(sheet.width * x / 4) for x in (column, column + 1))
            y1, y2 = (round(sheet.height * y / 2) for y in (row, row + 1))
            cell = sheet.crop((x1, y1, x2, y2))
            bounds = cell.getchannel("A").point(lambda a: 255 if a >= 100 else 0).getbbox()
            if not bounds:
                raise ValueError(f"Empty animal frame at {column}, {row}")
            frame = cell.crop(bounds)
            frame.thumbnail((22, 18), Image.Resampling.LANCZOS)
            atlas.paste(frame, (24 * column + (24 - frame.width) // 2, 20 * row + 19 - frame.height))
    atlas.save(ASSETS / "field_animals.png", optimize=True)
    write_texture(atlas, "field_animals")


GROUNDS = ("boreal", "mangrove", "ice", "tropical", "reef", "sandstone", "lava")
LANDMARKS = ("camp", "animal", "plant", "quarry")
SCENE_SIZE = (480, 270)


def split_sheet(source, columns, rows):
    image = Image.open(source)
    for y in range(rows):
        for x in range(columns):
            bounds = (round(image.width * x / columns), round(image.height * y / rows),
                      round(image.width * (x + 1) / columns), round(image.height * (y + 1) / rows))
            yield image.crop(bounds)


def localized_sprites(source, columns, rows, size, name):
    atlas = Image.new("RGBA", (size[0] * columns, size[1] * rows))
    for i, cell in enumerate(split_sheet(source, columns, rows)):
        if "A" not in cell.getbands():
            raise ValueError(f"Transparency required in {source}")
        crop = cell.getchannel("A").point(lambda a: 255 if a >= 100 else 0).getbbox()
        if not crop:
            raise ValueError(f"Empty sprite {i} in {source}")
        sprite = cell.crop(crop)
        sprite.thumbnail((size[0]-2, size[1]-2), Image.Resampling.LANCZOS)
        x = i % columns * size[0] + (size[0]-sprite.width)//2
        y = i // columns * size[1] + (size[1]-sprite.height if name=="field_traveler" else
                                      (size[1]-sprite.height)//2)
        atlas.paste(sprite, (x,y))
    atlas.save(ASSETS / f"{name}.png", optimize=True)
    write_texture(atlas, name)
    return atlas


def read_layout():
    layout = json.loads((ASSETS.parent / "field_layout.json").read_text())
    if tuple(layout) != GROUNDS:
        raise ValueError("Field layouts must follow the game biome order")
    for name, scene in layout.items():
        rows = scene["rows"]
        if len(rows) != 11 or any(len(row) != 22 or set(row) - set("wr.") for row in rows):
            raise ValueError(f"Invalid terrain layout for {name}")
        land = {(x, y) for y in range(1, 10) for x in range(1, 21) if rows[y][x] == "."}
        camp = tuple(scene["camp"])
        pending = [camp] if camp in land else []
        reachable = set(pending)
        while pending:
            x, y = pending.pop()
            for point in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if point in land and point not in reachable:
                    reachable.add(point)
                    pending.append(point)
        for key in LANDMARKS:
            if tuple(scene[key]) not in reachable:
                raise ValueError(f"{name}: {key} cannot be reached from camp")
        if land != reachable:
            raise ValueError(f"{name}: disconnected walkable ground")
    return layout


def write_layout(layout):
    blocks = []
    landmarks = []
    for kind, scene in layout.items():
        lines = ''.join(f'        "{row}",\n' for row in scene["rows"])
        blocks.append(f"    {{ // {kind}\n{lines}    }},")
        points = ", ".join(f"{{{x},{y}}}" for x, y in (scene[key] for key in LANDMARKS))
        landmarks.append(f"    {{{points}}}, // {kind}")
    (ASSETS.parent / "field_layout.hpp").write_text(
        "// Generated by tools/make_world_field.py from field_layout.json.\n"
        "inline constexpr char FieldLayout[][FieldRows][FieldColumns+1]{\n"
        + "\n".join(blocks) + "\n};\n"
        "inline constexpr FieldSceneLandmarks FieldLandmarks[]{\n"
        + "\n".join(landmarks) + "\n};\n")


def pack_scenes():
    output = ASSETS.parent / "assets" / "world"
    output.mkdir(exist_ok=True)
    for name in GROUNDS:
        with Image.open(ASSETS / f"field-scene-{name}.jpg") as painting:
            if painting.size != SCENE_SIZE:
                raise ValueError(f"{name}: scene must be {SCENE_SIZE} to match the walkable terrain")
            (output / f"ground-{name}.pxi").write_bytes(texture_bytes(painting.convert("RGB")))


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    for name in LOCATIONS + ("animals",):
        cli.add_argument(f"--{name}", type=Path)
    cli.add_argument("--traveler",type=Path)
    cli.add_argument("--marks",type=Path)
    cli.add_argument("--scenes", action="store_true", help="pack the paintings in art/field-scene-*.jpg")
    options = cli.parse_args()
    for name in LOCATIONS:
        source = getattr(options, name)
        if source:
            pack_study(name, source)
    if options.animals:
        pack_animals(options.animals)
    if options.traveler:
        localized_sprites(options.traveler,4,3,(24,32),"field_traveler")
    if options.marks:
        localized_sprites(options.marks,4,2,(24,24),"field_marks")
    write_layout(read_layout())
    if options.scenes:
        pack_scenes()


if __name__ == "__main__":
    main()
