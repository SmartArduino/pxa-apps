"""Prepare the duel backdrops and generate the three timed strike sequences."""

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

from make_art import ASSETS, sheet, write_texture


def clash_frame(move, frame):
    scale = 3
    image = Image.new("RGBA", (48 * scale, 36 * scale))
    draw = ImageDraw.Draw(image)
    progress = frame / 5
    alpha = (55, 160, 255, 222, 160, 70)[frame]
    stroke = ((235, 188, 101), (145, 224, 209), (167, 193, 225))[move]
    if move == 0:
        x = 13 + 21 * progress
        draw.polygon([(int(a * scale), int(b * scale)) for a, b in (
            (x - 11, 23), (x + 8, 11), (x + 1, 19), (x - 3, 23))],
            fill=(*stroke, alpha))
        center = (x + 4, 15)
    else:
        box = (7 * scale, 3 * scale, 41 * scale, 33 * scale)
        start = (201 + frame * 12) if move == 1 else (-25 + frame * 22)
        draw.arc(box, start, start + 83, fill=(*stroke, alpha), width=(4 if frame in (2, 3) else 2) * scale)
        angle = math.radians(start + 82)
        center = (24 + 17 * math.cos(angle), 18 + 14 * math.sin(angle))
    cx, cy = center
    if 1 <= frame <= 4:
        for direction in range(6):
            angle = direction * math.tau / 6 + frame * .15
            inner, outer = 2 + frame, 5 + 2 * frame
            draw.line([(round((cx + radius * math.cos(angle)) * scale),
                        round((cy + radius * math.sin(angle)) * scale))
                       for radius in (inner, outer)],
                      fill=(*stroke, alpha), width=scale)
        draw.ellipse((int((cx - 2) * scale), int((cy - 2) * scale),
                      int((cx + 2) * scale), int((cy + 2) * scale)),
                     fill=(255, 241, 193, alpha))
    return image.resize((48, 36), Image.Resampling.LANCZOS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("boarding", type=Path)
    parser.add_argument("cadiz", type=Path)
    args = parser.parse_args()
    for source, name in ((args.boarding, "boarding"), (args.cadiz, "cadiz_duel")):
        art = ImageOps.fit(Image.open(source).convert("RGB"), (256, 160),
                           Image.Resampling.LANCZOS)
        art.save(ASSETS / f"{name}.jpg", quality=92, optimize=True)
        write_texture(art, name)
    sheet([clash_frame(move, frame) for move in range(3) for frame in range(6)], "duel_fx")


if __name__ == "__main__":
    main()
