"""Pack the aligned sail poses of a ship into a streaming 160×104 texture."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps

from make_art import texture_bytes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", nargs="+", type=Path,
                        help="one three-pose strip, or three separate pose images")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    if len(args.images) == 1:
        strip = Image.open(args.images[0]).convert("RGBA")
        width = strip.width // 3
        if width * 3 != strip.width:
            parser.error("the strip must have three equal cells")
        poses = [strip.crop((i * width, 0, (i + 1) * width, strip.height)) for i in range(3)]
    elif len(args.images) == 3:
        poses = [Image.open(path).convert("RGBA") for path in args.images]
    else:
        parser.error("provide one strip or three images")

    sheet = Image.new("RGBA", (160, 104))
    for index, pose in enumerate(poses):
        if not pose.getchannel("A").getbbox():
            parser.error(f"pose {index + 1} has no visible ship")
        frame = ImageOps.contain(pose, (80, 52), Image.Resampling.LANCZOS)
        sheet.paste(frame, (index % 2 * 80 + (80 - frame.width) // 2,
                            index // 2 * 52 + 52 - frame.height))
    args.output.write_bytes(texture_bytes(sheet))


if __name__ == "__main__":
    main()
