#!/usr/bin/env python3
"""Generate the Jump Jump 3D INDEX8 glyph atlases.

Three resolution tiers are emitted so the interface stays crisp at every render
scale the app can pick (the GameRender context may be halved when the Host
raster is slow, and larger panels are supported as well). Each tier is designed
for its own target height and is drawn at scale 1, so no atlas is ever upscaled
by a fractional factor:

    tier 0  compact   target height <= 170   (e.g. 148x120 half resolution)
    tier 1  standard  target height <= 330   (e.g. 296x240 native)
    tier 2  large     larger panels          (e.g. 412x412)

Three faces per tier:

* ``big``   - score digits and the floating "+N" popups;
* ``small`` - latin labels and the best-score badge number;
* ``cjk``   - Simplified Chinese labels ("本次得分 最高分 重新开始 很好 好快").

A texel is 1 where ink is present and 0 elsewhere; the raster path draws each
glyph as a TRANSPARENT_INDEX0 sprite with a solid colour, so the atlas only
carries coverage.

Usage:
    python3 tools/generate_fonts.py            # writes jump3d_font_data.h
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
OUTPUT = APP_DIR / "jump3d_font_data.h"

BIG_GLYPHS = "0123456789+-"
SMALL_GLYPHS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:.-+/%"
CJK_GLYPHS = "本次得分最高重新开始很好快"

# tier name, big cell, small cell, cjk cell
TIERS = (
    ("compact", (10, 15), (5, 7), (12, 12)),
    ("standard", (20, 30), (8, 11), (16, 16)),
    ("large", (27, 41), (11, 15), (22, 22)),
)

FONT_CANDIDATES = [
    "/usr/share/fonts/adobe-source-han-sans/SourceHanSansCN-Regular.otf",
    "/usr/share/fonts/adobe-source-han-sans/SourceHanSansCN-Medium.otf",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
]

BOLD_CANDIDATES = [
    "/usr/share/fonts/adobe-source-han-sans/SourceHanSansCN-Bold.otf",
    "/usr/share/fonts/adobe-source-han-sans/SourceHanSansCN-Heavy.otf",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
]

SUPERSAMPLE = 6
# Coverage below this fraction is dropped: it keeps the crisp cut-out fallback
# from looking bold while the antialiasing paths still keep 30+ levels.
AA_FLOOR = 24


def pick_font(candidates: list[str], size: int, index: int = 0) -> ImageFont.FreeTypeFont:
    for path in candidates:
        if pathlib.Path(path).exists():
            try:
                return ImageFont.truetype(path, size=size, index=index)
            except OSError:
                continue
    raise SystemExit("no usable TrueType font found for the glyph atlas")


def render_cell(character: str, cell: tuple[int, int],
                font: ImageFont.FreeTypeFont) -> list[list[int]]:
    """Rasterise one character into a coverage bitmap of ``cell`` pixels."""
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
        bits = []
        for column in range(width):
            covered = 0
            for dy in range(scale):
                for dx in range(scale):
                    if pixels[column * scale + dx, row * scale + dy] > 127:
                        covered += 1
            # 8-bit coverage keeps the antialiasing the supersampled render
            # already carries: PXA_RASTER_SPRITE_TEXEL_ALPHA blends with it and
            # PXA_RASTER_SPRITE_PALETTE_RAMP quantises it to 32 levels. Levels
            # below the floor are dropped so a Host without either path still
            # cuts out a clean glyph.
            level = (covered * 255 + samples // 2) // samples
            bits.append(0 if level < AA_FLOOR else level)
        rows.append(bits)
    return rows


MAX_TEXTURE_DIMENSION = 256


def atlas_columns(glyph_count: int, cell_width: int) -> int:
    columns = MAX_TEXTURE_DIMENSION // cell_width
    if columns < 1:
        columns = 1
    if columns > glyph_count:
        columns = glyph_count
    return columns


def render_atlas(glyphs: str, cell: tuple[int, int], size: int,
                 bold: bool) -> tuple[list[list[int]], int]:
    """Renders the glyphs into a column-major-free grid of cells. Returns the
    pixel rows and the number of glyph columns per atlas row."""
    paths = BOLD_CANDIDATES if bold else FONT_CANDIDATES
    font = pick_font(paths, size=max(5, size) * SUPERSAMPLE)
    cell_width, cell_height = cell
    columns = atlas_columns(len(glyphs), cell_width)
    texture_rows = (len(glyphs) + columns - 1) // columns
    rows = [[0] * (cell_width * columns)
            for _ in range(cell_height * texture_rows)]
    for position, character in enumerate(glyphs):
        glyph = render_cell(character, cell, font)
        origin_x = (position % columns) * cell_width
        origin_y = (position // columns) * cell_height
        for row in range(cell_height):
            for column in range(cell_width):
                rows[origin_y + row][origin_x + column] = glyph[row][column]
    return rows, columns


def emit_atlas(symbol: str, glyphs: str, cell: tuple[int, int],
               rows: list[list[int]], columns: int) -> list[str]:
    width = cell[0] * columns
    lines = [
        f"/* {symbol}: {len(glyphs)} glyphs of {cell[0]}x{cell[1]} in "
        f"{columns} columns. */",
        f"#define J3_FONT_{symbol.upper()}_GLYPHS \"{glyphs}\"",
        f"static const uint8_t j3_font_{symbol}_pixels[{width}u * {len(rows)}u] = {{",
    ]
    for row in rows:
        lines.append("    " + ",".join(str(value) for value in row) + ",")
    lines.append("};")
    lines.append("")
    return lines


def build_header() -> str:
    lines = [
        "/* Generated by tools/generate_fonts.py - do not edit by hand. */",
        "#ifndef JUMP3D_FONT_DATA_H",
        "#define JUMP3D_FONT_DATA_H",
        "",
        "#include <stdint.h>",
        "",
        "#include \"jump3d_font.h\"",
        "",
        f"#define J3_FONT_TIERS {len(TIERS)}u",
        "",
    ]
    faces: dict[str, list[tuple[str, int, int]]] = {
        "big": [], "small": [], "cjk": []}
    for tier_index, (name, big_cell, small_cell, cjk_cell) in enumerate(TIERS):
        lines.append(f"/* ---- tier {tier_index}: {name} ---- */")
        lines.append("")
        for face, glyphs, cell, bold in (
            ("big", BIG_GLYPHS, big_cell, True),
            ("small", SMALL_GLYPHS, small_cell, True),
            ("cjk", CJK_GLYPHS, cjk_cell, False),
        ):
            symbol = f"{face}_t{tier_index}"
            # The em size that fills the cell with a small margin.
            size = int(round(cell[1] * (1.32 if bold else 1.05)))
            rows, columns = render_atlas(glyphs, cell, size, bold)
            lines += emit_atlas(symbol, glyphs, cell, rows, columns)
            faces[face].append((symbol, cell[0] * columns, len(rows), columns))
        lines.append("")
    for face in ("big", "small", "cjk"):
        cells = {"big": BIG_GLYPHS, "small": SMALL_GLYPHS, "cjk": CJK_GLYPHS}[face]
        lines.append(f"static const j3_font_face_t j3_font_{face}_faces[J3_FONT_TIERS] = {{")
        for symbol, atlas_width, atlas_height, columns in faces[face]:
            cell_width = atlas_width // columns
            cell_height = atlas_height // ((len(cells) + columns - 1) // columns)
            lines.append(
                f"    {{j3_font_{symbol}_pixels, J3_FONT_{symbol.upper()}_GLYPHS, "
                f"{cell_width}u, {cell_height}u, {columns}u, {atlas_width}u, {atlas_height}u}},")
        lines.append("};")
        lines.append("")
    largest = max(atlas_width * atlas_height
                  for face in faces.values()
                  for _, atlas_width, atlas_height, _ in face)
    lines.append(f"#define J3_FONT_MAX_ATLAS_BYTES_DATA {largest}u")
    lines.append("")
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="exit non-zero when the generated header is stale")
    arguments = parser.parse_args()
    content = build_header()
    if arguments.check:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != content:
            print(f"stale: {OUTPUT}", file=sys.stderr)
            return 1
        print(f"up to date: {OUTPUT}")
        return 0
    OUTPUT.write_text(content, encoding="utf-8")
    print(f"wrote {OUTPUT} ({len(content)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
