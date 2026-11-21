"""Typeset the Chinese title from a Noto Serif CJK bold font collection."""
import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from make_art import build_logo


def lettering(text, path, size, spacing, color):
    font = ImageFont.truetype(str(path), size, index=2)
    image = Image.new("RGBA", (size * len(text) + spacing * (len(text) - 1) + 8, size + 18))
    draw = ImageDraw.Draw(image)
    for index, char in enumerate(text):
        x = 4 + index * (size + spacing)
        draw.text((x, 8), char, font=font, anchor="lt", fill=color,
                  stroke_width=2, stroke_fill="#24261b")
    return image.crop(image.getbbox())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ttc", type=Path)
    args = parser.parse_args()
    series = lettering("大航路时代", args.ttc, 35, 5, "#d9bf87")
    title = lettering("潮汐之约", args.ttc, 69, 7, "#f5deaa")
    height = series.height + 10 + title.height
    image = Image.new("RGBA", (title.width, height))
    image.paste(series, ((title.width - series.width) // 2, 0))
    image.paste(title, (0, series.height + 10))
    build_logo(image)


if __name__ == "__main__":
    main()
