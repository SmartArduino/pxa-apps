"""Pack the painted waterfronts as streamed indexed game textures."""
from PIL import Image

from make_art import ASSETS, texture_bytes

PORTS = ("ragusa", "candia", "diu", "gresik")
MAP_SIZE = (480, 270)


def main():
    for port in PORTS:
        with Image.open(ASSETS / f"town-{port}.jpg") as painting:
            if painting.size != MAP_SIZE:
                raise ValueError(f"{port}: expected {MAP_SIZE} to match the marked streets")
            path = ASSETS.parent / "assets" / "ports" / f"{port}-streets.pxi"
            path.write_bytes(texture_bytes(painting.convert("RGB")))


if __name__ == "__main__":
    main()
