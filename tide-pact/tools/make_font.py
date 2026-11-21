"""Build a compact 4-bit glyph atlas from Noto Sans CJK SC and app strings."""
import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]


def write_font(path, codes, cell, name):
    face = ImageFont.truetype(str(path), cell, index=2)
    stride = cell + cell % 2
    data = bytearray(struct.pack("<H", len(codes)))
    for code in codes:
        char = chr(code)
        image = Image.new("L", (stride, cell))
        ImageDraw.Draw(image).text((0, cell - 2), char, font=face, fill=255, anchor="ls")
        data.extend(struct.pack("<HB", code, max(3, min(cell, round(face.getlength(char))))))
        pixels = image.tobytes()
        data.extend((a & 0xf0) | (b >> 4) for a, b in zip(pixels[::2], pixels[1::2]))
    (ROOT / "art" / name).write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ttc", type=Path)
    args = parser.parse_args()
    text = "".join(p.read_text() for p in ROOT.glob("*.?pp"))
    codes = sorted(set(range(32, 127)) | {ord(c) for c in text if 127 < ord(c) <= 0xffff})
    write_font(args.ttc, codes, 14, "glyphs.pxi")
    write_font(args.ttc, codes, 11, "glyphs_compact.pxi")


if __name__ == "__main__":
    main()
