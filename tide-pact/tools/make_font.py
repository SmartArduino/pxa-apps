"""Build a compact 4-bit glyph atlas from Noto Sans CJK SC and app strings."""
import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CELL = 14


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ttc", type=Path)
    args = parser.parse_args()
    face = ImageFont.truetype(str(args.ttc), CELL, index=2)
    text = "".join(p.read_text() for p in ROOT.glob("*.?pp"))
    codes = sorted(set(range(32, 127)) | {ord(c) for c in text if 127 < ord(c) <= 0xffff})
    data = bytearray(struct.pack("<H", len(codes)))
    for code in codes:
        char = chr(code)
        image = Image.new("L", (CELL, CELL))
        ImageDraw.Draw(image).text((0, 12), char, font=face, fill=255, anchor="ls")
        data.extend(struct.pack("<HB", code, max(3, min(CELL, round(face.getlength(char))))))
        pixels = image.tobytes()
        data.extend((a & 0xf0) | (b >> 4) for a, b in zip(pixels[::2], pixels[1::2]))
    (ROOT / "art/glyphs.pxi").write_bytes(data)


if __name__ == "__main__":
    main()
