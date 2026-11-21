"""Build 4-bit glyph atlases from Noto Sans CJK SC and Fusion Pixel 12px."""
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


def read_bitmaps(path, codes, cell):
    glyphs = {}
    for block in path.read_text(encoding="utf-8").split("\nSTARTCHAR ")[1:]:
        metrics, _, rows = block.partition("\nBITMAP\n")
        values = dict(line.split(" ", 1) for line in metrics.splitlines()[1:])
        code = int(values["ENCODING"])
        if code not in codes:
            continue
        width, height, x, below = map(int, values["BBX"].split())
        advance = int(values["DWIDTH"].split()[0])
        y = cell - 2 - below - height
        rows = rows.partition("ENDCHAR")[0].splitlines()
        if not (0 <= x <= advance - width and advance <= cell and
                0 <= y <= cell - height and len(rows) == height):
            raise ValueError(f"U+{code:04X} does not fit the {cell}px bitmap cell")
        image = Image.new("L", (cell + cell % 2, cell))
        if width and height:
            bitmap = Image.frombytes("1", (width, height), bytes.fromhex(" ".join(rows)))
            image.paste(bitmap.convert("L"), (x, y))
        glyphs[code] = image, advance
    return glyphs


def write_font(path, codes, cell, name, bitmaps=None):
    face = ImageFont.truetype(str(path), cell, index=2)
    bitmaps = {} if bitmaps is None else bitmaps
    data = bytearray(struct.pack("<H", len(codes)))
    for code in codes:
        image, advance = bitmaps[code] if code in bitmaps else rasterize(face, code, cell)
        data.extend(struct.pack("<HB", code, advance))
        pixels = image.tobytes()
        data.extend((a & 0xf0) | (b >> 4) for a, b in zip(pixels[::2], pixels[1::2]))
        ink = image.point(lambda alpha: alpha & 0xf0).getbbox()
        data.extend(ink or (0, 0, 0, 0))
    (ROOT / "art" / name).write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ttc", type=Path)
    parser.add_argument("--compact-bdf", required=True, type=Path)
    args = parser.parse_args()
    text = "".join(p.read_text() for p in ROOT.glob("*.?pp"))
    codes = sorted(set(range(32, 127)) | {ord(c) for c in text if 127 < ord(c) <= 0xffff})
    write_font(args.ttc, codes, 14, "glyphs.pxi")
    bitmaps = read_bitmaps(args.compact_bdf, set(codes), 12)
    write_font(args.ttc, codes, 12, "glyphs_compact.pxi", bitmaps)


if __name__ == "__main__":
    main()
