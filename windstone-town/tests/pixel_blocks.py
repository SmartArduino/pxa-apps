"""Check that a PXA simulator capture expands a surface in whole pixel blocks."""
import argparse

from PIL import Image


def verify(path, scale):
    with Image.open(path) as capture:
        image = capture.convert("RGB")
    width, height = image.width // scale, image.height // scale
    assert width > 0 and height > 0
    x = (image.width - width * scale) // 2
    y = (image.height - height * scale) // 2
    surface = image.crop((x, y, x + width * scale, y + height * scale))
    logical = surface.resize((width, height), Image.Resampling.NEAREST)
    expanded = logical.resize(surface.size, Image.Resampling.NEAREST)
    assert len(logical.getcolors(width * height)) >= 16, "the capture must contain the game"
    assert surface.tobytes() == expanded.tobytes(), f"found mixed pixels within a {scale}×{scale} block"
    print(f"{image.width}×{image.height} capture contains exact {scale}×{scale} pixels")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture")
    parser.add_argument("--scale", type=int, choices=(2, 4), default=2)
    args = parser.parse_args()
    verify(args.capture, args.scale)
