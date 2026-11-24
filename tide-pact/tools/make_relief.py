"""Build a compact, shaded land layer from Natural Earth 1:50m Gray Earth."""
import argparse
import io
import math
import zipfile
from pathlib import Path

from PIL import Image

from make_chart import ROOT, W, H, WEST, NORTH, SX, SY

STRIDE = 8


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gray_earth", type=Path)
    args = parser.parse_args()
    with zipfile.ZipFile(args.gray_earth) as archive:
        filename = next(name for name in archive.namelist() if name.endswith(".tif"))
        with Image.open(io.BytesIO(archive.read(filename))) as original:
            gray = original.convert("L")
    width, height = gray.size
    # Repeat the Pacific edge so the equirectangular source straddles our seam.
    full = Image.new("L", (width * 2, height))
    full.paste(gray, (0, 0))
    full.paste(gray, (width, 0))
    grid_w, grid_h = math.ceil(W / STRIDE), math.ceil(H / STRIDE)
    extent = ((WEST + 180) * width / 360, (90 - NORTH) * height / 180,
              (WEST + grid_w * STRIDE / SX + 180) * width / 360,
              (90 - NORTH + grid_h * STRIDE / SY) * height / 180)
    # The source follows the same plate carrée grid. Only the game coastline
    # decides which samples are rendered as land.
    shade = full.transform((grid_w, grid_h), Image.Transform.EXTENT, extent, Image.Resampling.BILINEAR)
    levels = [min(15, max(0, (value - 110) * 15 // 140)) for value in shade.tobytes()]
    if len(levels) % 2:
        levels.append(0)
    packed = bytes(left | (right << 4) for left, right in zip(levels[::2], levels[1::2]))
    (ROOT / "art" / "relief.pxi").write_bytes(packed)


if __name__ == "__main__":
    main()
