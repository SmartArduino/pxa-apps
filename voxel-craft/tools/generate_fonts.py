#!/usr/bin/env python3
"""Generate the Voxel Craft antialiased HUD glyph atlas.

The raster HUD used to draw a 3x5 cut-out font scaled up by the UI scale, which
is unreadable on large panels. This tool rasterises a real typeface into three
coverage tiers instead; each tier is drawn at its native pixel size, so the
glyphs never depend on the Host scaling a tiny bitmap:

    small   cell 6x9    hotbar counts, status line, toasts, hints
    medium  cell 10x14  labels and menu buttons
    large   cell 16x22  screen titles

A texel is 8-bit coverage: 0 is transparent, 1..255 blends the glyph ink with
whatever is behind it through PXA_RASTER_SPRITE_TEXEL_ALPHA. The same coverage
also feeds a cut-out fallback (any texel above the floor) for Hosts without the
alpha sprite path.

Usage:
    python3 tools/generate_fonts.py            # writes voxel_font_data.h
    python3 tools/generate_fonts.py --check    # verifies the file is current
"""

from __future__ import annotations

import argparse
import pathlib
import sys

try:
    from PIL import Image, ImageDraw, ImageFont  # noqa: F401
except ImportError:  # pragma: no cover - developer tooling only
    # The invoked interpreter may lack Pillow (for example an ESP-IDF virtual
    # environment); hand over to a system interpreter that has it.
    import os
    import subprocess

    for candidate in ("/usr/bin/python3", "/usr/local/bin/python3"):
        if candidate == sys.executable or not os.path.exists(candidate):
            continue
        if subprocess.run([candidate, "-c", "import PIL"],
                          capture_output=True).returncode == 0:
            os.execv(candidate, [candidate, __file__, *sys.argv[1:]])
    raise SystemExit("Pillow is required: python3 -m pip install pillow "
                     "(or run with /usr/bin/python3)")

APP_DIR = pathlib.Path(__file__).resolve().parent.parent
OUTPUT = APP_DIR / "voxel_font_data.h"

GLYPHS = "0123456789.:-/+ABCDEFGHIJKLMNOPQRSTUVWXYZ "

# tier name, cell (width, height), point size used to rasterise the cell
TIERS = (
    ("small", (6, 9), 13),
    ("medium", (10, 14), 20),
    ("large", (16, 22), 32),
)

FONT_CANDIDATES = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
]

SUPERSAMPLE = 8
# Coverage below this fraction stays transparent. It trims the faintest halo
# texels, which keeps the blend path from smearing ink over neighbouring cells
# and gives the cut-out fallback a clean silhouette.
AA_FLOOR = 10

MAX_TEXTURE_DIMENSION = 256


def pick_font(size: int) -> ImageFont.FreeTypeFont:
    for path in FONT_CANDIDATES:
        if pathlib.Path(path).exists():
            try:
                return ImageFont.truetype(path, size=size)
            except OSError:
                continue
    raise SystemExit("no usable TrueType font found for the glyph atlas")


def render_cell(character: str, cell: tuple[int, int],
                font: ImageFont.FreeTypeFont) -> list[list[int]]:
    """Rasterises one character into a coverage bitmap of ``cell`` pixels."""
    width, height = cell
    scale = SUPERSAMPLE
    image = Image.new("L", (width * scale, height * scale), 0)
    draw = ImageDraw.Draw(image)
    box = draw.textbbox((0, 0), character, font=font)
    ink_w = box[2] - box[0]
    ink_h = box[3] - box[1]
    x = (width * scale - ink_w) // 2 - box[0]
    y = (height * scale - ink_h) // 2 - box[1]
    draw.text((x, y), character, fill=255, font=font)
    pixels = image.load()
    rows: list[list[int]] = []
    samples = scale * scale
    for row in range(height):
        values = []
        for column in range(width):
            covered = 0
            for dy in range(scale):
                for dx in range(scale):
                    if pixels[column * scale + dx, row * scale + dy] > 127:
                        covered += 1
            level = (covered * 255 + samples // 2) // samples
            values.append(0 if level < AA_FLOOR else level)
        rows.append(values)
    return rows


def atlas_columns(glyph_count: int, cell_width: int) -> int:
    columns = MAX_TEXTURE_DIMENSION // cell_width
    if columns < 1:
        columns = 1
    if columns > glyph_count:
        columns = glyph_count
    return columns


def render_atlas(cell: tuple[int, int],
                 size: int) -> tuple[list[list[int]], int]:
    font = pick_font(size * SUPERSAMPLE)
    cell_width, cell_height = cell
    columns = atlas_columns(len(GLYPHS), cell_width)
    texture_rows = (len(GLYPHS) + columns - 1) // columns
    rows = [[0] * (cell_width * columns)
            for _ in range(cell_height * texture_rows)]
    for position, character in enumerate(GLYPHS):
        glyph = render_cell(character, cell, font)
        origin_x = (position % columns) * cell_width
        origin_y = (position // columns) * cell_height
        for row in range(cell_height):
            for column in range(cell_width):
                rows[origin_y + row][origin_x + column] = glyph[row][column]
    return rows, columns


def emit(lines: list[str], name: str, cell: tuple[int, int],
         rows: list[list[int]], columns: int) -> None:
    width = cell[0] * columns
    lines.append(
        f"/* {name}: cell {cell[0]}x{cell[1]}, {len(GLYPHS)} glyphs in "
        f"{columns} columns, {width}x{len(rows)} texels. */")
    lines.append(f"static const uint8_t voxel_font_{name}_pixels"
                 f"[{width}u * {len(rows)}u] = {{")
    for row in rows:
        lines.append("    " + ",".join(str(value) for value in row) + ",")
    lines.append("};")
    lines.append("")


def build_header() -> str:
    lines = [
        "/* Generated by tools/generate_fonts.py - do not edit by hand. */",
        "#ifndef VOXEL_FONT_DATA_H",
        "#define VOXEL_FONT_DATA_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define VOXEL_FONT_GLYPHS \"{GLYPHS}\"",
        f"#define VOXEL_FONT_GLYPH_COUNT {len(GLYPHS)}u",
        f"#define VOXEL_FONT_TIERS {len(TIERS)}u",
        "",
    ]
    for name, cell, size in TIERS:
        rows, columns = render_atlas(cell, size)
        lines.append(f"#define VOXEL_FONT_{name.upper()}_WIDTH {cell[0]}u")
        lines.append(f"#define VOXEL_FONT_{name.upper()}_HEIGHT {cell[1]}u")
        lines.append(f"#define VOXEL_FONT_{name.upper()}_COLUMNS {columns}u")
        lines.append(
            f"#define VOXEL_FONT_{name.upper()}_ROWS "
            f"{len(rows) // cell[1]}u")
        emit(lines, name, cell, rows, columns)
    lines.append("#endif")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verify the committed header is current")
    args = parser.parse_args()
    header = build_header()
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != header:
            print(f"{OUTPUT} is out of date; run tools/generate_fonts.py")
            return 1
        print(f"{OUTPUT} is current")
        return 0
    OUTPUT.write_text(header)
    print(f"wrote {OUTPUT} ({len(header)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
