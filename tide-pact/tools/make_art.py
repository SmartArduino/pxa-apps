"""Prepare indexed illustrations and build the sailing and cannon sprite sheets."""
import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

ASSETS = Path(__file__).resolve().parents[1] / "art"


def texture_bytes(image):
    rgb = image.convert("RGB").quantize(colors=255, dither=Image.Dither.NONE)
    colors = rgb.getpalette()[:768]
    colors += [0] * (768 - len(colors))
    palette = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
               for r, g, b in zip(colors[::3], colors[1::3], colors[2::3])]
    data = bytearray(rgb.tobytes())
    if "A" in image.getbands():
        for i, alpha in enumerate(image.getchannel("A").tobytes()):
            if alpha < 100:
                data[i] = 255
    return struct.pack("<HH256H", *image.size, *palette) + data


def write_texture(image, name):
    (ASSETS / (name + ".pxi")).write_bytes(texture_bytes(image))


def cannon_frame(index):
    size = 32
    frame = Image.new("RGBA", (size, size))
    draw = ImageDraw.Draw(frame)
    if index < 3:
        radius = 3 + index * 3
        draw.polygon([(16 + (radius if i % 2 == 0 else radius * .4) * math.cos(i * math.pi / 7),
                       16 + (radius if i % 2 == 0 else radius * .4) * math.sin(i * math.pi / 7))
                      for i in range(14)], fill="#ec9a54")
        draw.ellipse((13-index, 13-index, 19+index, 19+index), fill="#fff3c1")
    else:
        spread = index - 1
        for x, y, radius in [(16,16,4), (10,12,2), (20,11,3), (24,18,2), (11,21,2)]:
            radius += spread // 2
            x = x + (x - 16) * spread // 5
            y = y + (y - 16) * spread // 5
            draw.ellipse((x-radius, y-radius, x+radius, y+radius),
                         fill=(153 + index*4, 154+index*3, 137+index*5, 230-(index-3)*31))
    return frame


def sheet(frames, name):
    width, height = frames[0].size
    texture = Image.new("RGBA", (width * len(frames), height))
    for i, frame in enumerate(frames):
        texture.paste(frame, (i * width, 0))
    texture.save(ASSETS / f"{name}.png", optimize=True)
    write_texture(texture, name)


def build_skin(path):
    source = Image.open(path).convert("RGB")
    w, h = source.width // 2, source.height // 2
    skin = Image.new("RGB", (240, 72))
    for column in range(2):
        x = column * w
        panel = source.crop((x+2, 2, x+w-2, h-2))
        skin.paste(panel.resize((72,72), Image.Resampling.LANCZOS), (72*column, 0))
        texture = source.crop((x+w//7, h+h//4, x+w-w//7, h+h//2))
        skin.paste(texture.resize((48,12), Image.Resampling.LANCZOS), (144+48*column, 0))
    skin.save(ASSETS / "ui-skin.png", optimize=True)
    write_texture(skin, "skin")


def build_logo(image):
    source = image.convert("RGBA")
    mask = source.getchannel("A").point(lambda a: 255 if a >= 120 else 0)
    bbox = mask.getbbox()
    if bbox is None:
        raise ValueError("The wordmark needs opaque type on a transparent layer")
    source = source.crop(bbox)
    source.thumbnail((460,200), Image.Resampling.LANCZOS)
    source.save(ASSETS / "wordmark.png", optimize=True)
    write_texture(source, "wordmark")


def pack_scene(path, name, size, *, texture_path=None):
    image = ImageOps.fit(Image.open(path).convert("RGB"), size, Image.Resampling.LANCZOS)
    image.save(ASSETS / f"{name}.jpg", quality=89, optimize=True)
    if texture_path is None:
        write_texture(image, name)
    else:
        texture_path.write_bytes(texture_bytes(image))


def pack_icons(path, name, cols, rows, size, alpha_cutoff=100):
    original = Image.open(path).convert("RGBA")
    sheet = Image.new("RGBA", (size * cols, size * rows))
    for row in range(rows):
        for col in range(cols):
            rect = (round(original.width*col/cols), round(original.height*row/rows),
                    round(original.width*(col+1)/cols), round(original.height*(row+1)/rows))
            cell = original.crop(rect)
            alpha = cell.getchannel("A")
            bounds = alpha.point(lambda a: 255 if a >= alpha_cutoff else 0).getbbox()
            if bounds is None:
                raise ValueError(f"Empty icon in {name} at ({row}, {col})")
            if alpha_cutoff>100:
                cell.putalpha(alpha.point(lambda a: a if a >= alpha_cutoff else 0))
            icon = cell.crop(bounds)
            icon.thumbnail((size-2,size-2), Image.Resampling.LANCZOS)
            sheet.paste(icon,(col*size+(size-icon.width)//2,row*size+(size-icon.height)//2))
    sheet.save(ASSETS / f"{name}.png", optimize=True)
    write_texture(sheet, name)


def pack_ports(path):
    original = Image.open(path).convert("RGB")
    sheet = Image.new("RGB", (4*188,105))
    w, h = original.width // 2, original.height // 2
    for i in range(4):
        x, y = w*(i%2), h*(i//2)
        view = original.crop((x+2,y+2,x+w-2,y+h-2))
        tile = ImageOps.fit(view,(188,105),Image.Resampling.LANCZOS,centering=(.5,.53))
        sheet.paste(tile,(i*188,0))
    sheet.save(ASSETS / "port-scenes.jpg", quality=89, optimize=True)
    write_texture(sheet,"ports")


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("harbor", type=Path)
    cli.add_argument("portraits", type=Path)
    cli.add_argument("storm", type=Path)
    cli.add_argument("ui", type=Path)
    cli.add_argument("wordmark", type=Path)
    cli.add_argument("goods", type=Path)
    cli.add_argument("navigation", type=Path)
    cli.add_argument("ports", type=Path)
    cli.add_argument("--title-compact", type=Path)
    args = cli.parse_args()
    pack_scene(args.harbor, "harbor", (384,216))
    pack_scene(args.storm, "storm", (372,248))
    if args.title_compact:
        pack_scene(args.title_compact, "title_compact", (256,256))
    source = Image.open(args.portraits).convert("RGB")
    frame = Image.new("RGB", (6*86, 86))
    width, height = source.width // 3, source.height // 2
    for i in range(6):
        x, y = i % 3 * width, i // 3 * height
        tile = source.crop((x+3, y+3, x+width-3, y+height-3))
        frame.paste(ImageOps.fit(tile, (86,86)), (i*86,0))
    frame.save(ASSETS / "portraits.jpg", quality=89, optimize=True)
    write_texture(frame, "portraits")
    sheet([cannon_frame(i) for i in range(8)], "broadside")
    build_skin(args.ui)
    build_logo(Image.open(args.wordmark))
    pack_icons(args.goods, "goods", 4, 2, 48)
    pack_icons(args.navigation, "navigation", 4, 3, 42)
    pack_ports(args.ports)


if __name__ == "__main__":
    main()
