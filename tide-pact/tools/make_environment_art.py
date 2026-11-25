"""Build indexed exploration illustrations and weather animation frames."""
import argparse
from pathlib import Path

from PIL import Image

from make_art import ASSETS, pack_scene, write_texture


SCENES = ("estuary", "forest", "wadi", "vardo", "vardo_thaw", "malabar", "malabar_dry", "sabaki", "arguin")


def pack_weather(path):
    original = Image.open(path)
    if "A" not in original.getbands():
        raise ValueError("Weather sheet needs a transparent background")
    atlas = Image.new("RGBA", (128, 84))
    for row in range(3):
        for column in range(4):
            x1, x2 = [round(original.width * x / 4) for x in (column, column + 1)]
            y1, y2 = [round(original.height * y / 3) for y in (row, row + 1)]
            cell = original.crop((x1, y1, x2, y2))
            margin = min(cell.size) // 24
            cell = cell.crop((margin, margin, cell.width - margin, cell.height - margin))
            cell = cell.resize((32, 28), Image.Resampling.LANCZOS)
            cell.putalpha(cell.getchannel("A").point(lambda a: min(255, 2 * a)))
            if cell.getchannel("A").point(lambda a: 255 if a >= 100 else 0).getbbox() is None:
                raise ValueError(f"No visible weather marks in cell {column + 1}, {row + 1}")
            atlas.paste(cell, (column * 32, row * 28))
    atlas.save(ASSETS / "weather_motion.png", optimize=True)
    write_texture(atlas, "weather_motion")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (*SCENES, "weather"):
        parser.add_argument("--" + name.replace("_", "-"), type=Path)
    args = parser.parse_args()
    output = ASSETS.parent / "assets" / "expeditions"
    output.mkdir(parents=True, exist_ok=True)
    for name in SCENES:
        source = getattr(args, name)
        if source:
            size = (320, 160) if name == "arguin" else (384, 228)
            texture_path = output / (name.replace("_", "-") + ".pxi")
            pack_scene(source, f"expedition_{name}", size, texture_path=texture_path)
    if args.weather:
        pack_weather(args.weather)


if __name__ == "__main__":
    main()
