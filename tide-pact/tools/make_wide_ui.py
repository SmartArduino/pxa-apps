"""Pack native-pixel controls and panels from the original UI artwork."""
import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

from make_art import ASSETS, write_texture


WIDTH, HEIGHT, EDGE = 128, 56, 10
BAR_HEIGHT = 20


def tileable(image, size):
    tile = ImageOps.fit(image, size, Image.Resampling.LANCZOS)
    # Feather opposite edges together to keep the grain continuous when tiled.
    for axis in (0, 1):
        if axis:
            tile = tile.transpose(Image.Transpose.TRANSPOSE)
        for i in range(4):
            left = tile.crop((i, 0, i + 1, tile.height))
            right = tile.crop((tile.width - 1 - i, 0, tile.width - i, tile.height))
            blend = (4 - i) / 8
            tile.paste(Image.blend(left, right, blend), (i, 0))
            tile.paste(Image.blend(right, left, blend), (tile.width - 1 - i, 0))
        if axis:
            tile = tile.transpose(Image.Transpose.TRANSPOSE)
    return tile


def shade(image, color, amount):
    return Image.blend(image, Image.new("RGB", image.size, color), amount)


def panel(surface, bright=False):
    w, h = WIDTH, HEIGHT
    tile = Image.new("RGBA", (w, h))
    if surface is not None:
        tile.paste(surface, (0, 0))
    mask = Image.new("L", (w, h))
    ImageDraw.Draw(mask).polygon([(0, 5), (5, 0), (w-6, 0), (w-1, 5),
                                 (w-1, h-6), (w-6, h-1), (5, h-1), (0, h-6)], fill=255)
    if surface is not None:
        tile.putalpha(mask)
    draw = ImageDraw.Draw(tile)
    highlights = ("#2a2018", "#947149", "#caa46b" if bright else "#a78354", "#664930")
    shadows = ("#211912", "#503a27", "#85633e", "#34261b")
    for inset, (light, dark) in enumerate(zip(highlights, shadows)):
        x, y = w - 1 - inset, h - 1 - inset
        cut = 5 - inset
        draw.line([(inset, y-cut), (inset, 5), (5, inset), (x-cut, inset), (x, 5)], fill=light)
        draw.line([(x, 5), (x, y-cut), (x-cut, y), (5, y), (inset, y-cut)], fill=dark)
    for x in (7, w - 8):
        for y in (7, h - 8):
            draw.ellipse((x-2, y-2, x+2, y+2), fill="#503725")
            draw.point((x, y), fill="#cba470" if bright else "#af8c5c")
            draw.point((x, y+1), fill="#83603d")
    return tile


def control(source, index):
    w, h = source.width // 2, source.height // 2
    tile = source.crop((index*w, 0, (index+1)*w, h))
    bounds = tile.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0).getbbox()
    if bounds is None:
        raise ValueError(f"Control {index} is empty")
    tile = tile.crop(bounds)
    out = Image.new("RGBA", (WIDTH, HEIGHT))
    x = (0, tile.width//12, tile.width-tile.width//12, tile.width)
    y = (0, tile.height//4, tile.height-tile.height//4, tile.height)
    rim = 8
    target_x = (0, rim, WIDTH-rim, WIDTH)
    target_y = (0, rim, HEIGHT-rim, HEIGHT)
    for row in range(3):
        for col in range(3):
            piece = tile.crop((x[col], y[row], x[col+1], y[row+1]))
            size = (target_x[col+1]-target_x[col], target_y[row+1]-target_y[row])
            piece = tileable(piece.convert("RGB"), size) if row == col == 1 else piece.resize(size, Image.Resampling.LANCZOS)
            out.paste(piece, (target_x[col], target_y[row]))
    return out


def enamel(texture, rgb):
    fill = ImageOps.colorize(texture.convert("L").point(lambda value: min(255, value*3)),
                             tuple(channel//3 for channel in rgb), rgb)
    for y in range(fill.height):
        amount = abs(4-y)/(fill.height+4)
        row = fill.crop((0, y, fill.width, y+1))
        fill.paste(shade(row, "#132824", amount*.46), (0, y))
    return fill


def build(controls, materials):
    width, height = materials.width//2, materials.height//2
    teal, wood, paper, brass = [
        tileable(materials.crop((col*width+8, row*height+8, (col+1)*width-8, (row+1)*height-8)),
                 (WIDTH, HEIGHT))
        for row in range(2) for col in range(2)
    ]
    inset = (EDGE, EDGE, WIDTH-EDGE, HEIGHT-EDGE)
    panels = [
        panel(shade(teal, "#061b20", .55)),
        panel(shade(teal, "#426459", .42), bright=True),
        control(controls, 0),
        control(controls, 1),
        panel(shade(teal, "#0b2728", .28), bright=True),
        panel(shade(wood, "#291f1a", .35)),
        panel(shade(wood, "#5f452e", .32), bright=True),
        panel(shade(paper, "#ebd1a3", .22), bright=True),
        panel(None, bright=True),
    ]
    atlas = Image.new("RGBA", (WIDTH*len(panels), HEIGHT+BAR_HEIGHT))
    for i, piece in enumerate(panels):
        if i not in (2, 3, 8):
            piece.paste(tileable(piece.crop(inset).convert("RGB"), (WIDTH-2*EDGE, HEIGHT-2*EDGE)),
                        (EDGE, EDGE))
        atlas.paste(piece, (i*WIDTH, 0))
    rail = Image.new("RGB", (WIDTH, BAR_HEIGHT), "#102023")
    rim = ImageDraw.Draw(rail)
    for i, color in enumerate(("#241c16", "#a88354", "#62482e", "#232820")):
        rim.rectangle((i, i, WIDTH-1-i, BAR_HEIGHT-1-i), outline=color)
    atlas.paste(rail, (0, HEIGHT))
    for i, rgb in enumerate(((79, 187, 142), (218, 167, 83)), 1):
        glaze = enamel(tileable(brass, (WIDTH, BAR_HEIGHT)), rgb)
        atlas.paste(glaze, (i*WIDTH, HEIGHT))
    return atlas


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("controls", type=Path)
    parser.add_argument("materials", type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    atlas = build(Image.open(args.controls).convert("RGBA"), Image.open(args.materials).convert("RGB"))
    # Use only the visible RGB when choosing the 565 palette.
    clean = Image.composite(atlas, Image.new("RGBA", atlas.size), atlas.getchannel("A").point(
                            lambda alpha: 255 if alpha >= 100 else 0))
    write_texture(clean, "ui_wide")
    if args.preview:
        clean.save(args.preview)


if __name__ == "__main__":
    main()
