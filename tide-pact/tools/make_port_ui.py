"""Build the harbor plaques, dialogue paper and brass trim from four swatches."""
import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

from make_art import ASSETS, write_texture


WIDTH, HEIGHT = 64, 40


def outline(draw, inset, cut, color):
    x, y = WIDTH - 1 - inset, HEIGHT - 1 - inset
    draw.polygon(((inset + cut, inset), (x - cut, inset), (x, inset + cut), (x, y - cut),
                  (x - cut, y), (inset + cut, y), (inset, y - cut), (inset, inset + cut)),
                 outline=color)


def panel(surface, metal, bright=False):
    tile = Image.new("RGBA", (WIDTH, HEIGHT))
    draw = ImageDraw.Draw(tile)
    outline(draw, 0, 4, "#291e15")
    outline(draw, 1, 3, "#c99c55" if bright else "#987141")
    outline(draw, 2, 2, "#e5bb74" if bright else "#6e4d30")
    outline(draw, 3, 1, "#6c492d")
    if surface is not None:
        tile.paste(surface, (4, 4))
    for x in (1, WIDTH - 6):
        for y in (1, HEIGHT - 6):
            draw.arc((x, y, x + 4, y + 4), 0, 359, fill="#3f2821")
            draw.line((x + 2, y + 1, x + 2, y + 3), fill="#edc784" if bright else "#cba16a")
            draw.point((x + 1, y + 2), fill=metal.getpixel((x + 1, y + 2)))
    return tile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("swatches", type=Path)
    args = parser.parse_args()
    source = Image.open(args.swatches).convert("RGB")
    w, h = source.width // 2, source.height // 2
    tiles = []
    for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
        cell = source.crop((x*w+5, y*h+5, (x+1)*w-5, (y+1)*h-5))
        tiles.append(ImageOps.fit(cell, (WIDTH-8, HEIGHT-8), Image.Resampling.LANCZOS))
    teal, wood, parchment, metal = tiles
    metal = metal.resize((WIDTH, HEIGHT), Image.Resampling.NEAREST)
    parchment = Image.blend(parchment, Image.new("RGB", parchment.size, "#e8c68e"), .22)
    atlas = Image.new("RGBA", (WIDTH*5, HEIGHT))
    for index, (surface, bright) in enumerate(((teal, True), (wood, False), (wood, True),
                                                (parchment, True), (None, True))):
        atlas.paste(panel(surface, metal, bright), (WIDTH*index, 0))
    atlas.save(ASSETS / "port-ui.png", optimize=True)
    write_texture(atlas, "port_ui")


if __name__ == "__main__":
    main()
