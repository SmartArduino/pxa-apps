"""Build 4-bit glyph atlases from Noto Sans CJK SC."""
import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]


def rasterize(face, code, cell):
    scratch = Image.new("L", (3 * cell, 3 * cell))
    char = chr(code)
    ImageDraw.Draw(scratch).text((cell, 2 * cell - 2), char, font=face, fill=255, anchor="ls")
    image = Image.new("L", (cell + cell % 2, cell))
    advance = max(3, min(cell, round(face.getlength(char))))
    bounds = scratch.point(lambda alpha: alpha & 0xf0).getbbox()
    if bounds:
        glyph = scratch.crop(bounds)
        glyph.thumbnail((cell, cell), Image.Resampling.LANCZOS)
        x = max(0, min(bounds[0] - cell, cell - glyph.width))
        y = max(0, min(bounds[1] - cell, cell - glyph.height))
        image.paste(glyph, (x, y))
        advance = max(advance, x + glyph.width)
    return image, advance


def write_font(path, codes, cell, name):
    face = ImageFont.truetype(str(path), cell, index=2)
    data = bytearray(struct.pack("<H", len(codes)))
    for code in codes:
        image, advance = rasterize(face, code, cell)
        data.extend(struct.pack("<HB", code, advance))
        pixels = image.tobytes()
        data.extend((a & 0xf0) | (b >> 4) for a, b in zip(pixels[::2], pixels[1::2]))
        ink = image.point(lambda alpha: alpha & 0xf0).getbbox()
        data.extend(ink or (0, 0, 0, 0))
    (ROOT / "art" / name).write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ttc", type=Path)
    parser.add_argument("--compact-ttc", type=Path, help="font face for the 12px atlas, defaults to ttc")
    args = parser.parse_args()
    text = "".join(p.read_text() for p in ROOT.glob("*.?pp"))
    codes = sorted(set(range(32, 127)) | {ord(c) for c in text if 127 < ord(c) <= 0xffff})
    write_font(args.ttc, codes, 14, "glyphs.pxi")
    write_font(args.compact_ttc or args.ttc, codes, 12, "glyphs_compact.pxi")
    write_font(args.compact_ttc or args.ttc, codes, 24, "glyphs_native.pxi")


if __name__ == "__main__":
    main()
