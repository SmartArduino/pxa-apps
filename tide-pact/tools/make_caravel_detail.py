"""Pack three aligned transparent caravel poses for the 2D chart close-up."""
import argparse
from pathlib import Path

from PIL import Image

from make_art import texture_bytes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("full", type=Path)
    parser.add_argument("half", type=Path)
    parser.add_argument("reefed", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    sheet = Image.new("RGBA", (160, 104))
    for index, path in enumerate((args.full, args.half, args.reefed)):
        pose = Image.open(path).convert("RGBA")
        if not pose.getchannel("A").getbbox():
            raise ValueError(f"No visible ship in {path}")
        # All poses share the same canvas and waterline.
        pose = pose.resize((80, 52), Image.Resampling.LANCZOS)
        sheet.paste(pose, (index % 2 * 80, index // 2 * 52))
    args.output.write_bytes(texture_bytes(sheet))


if __name__ == "__main__":
    main()
