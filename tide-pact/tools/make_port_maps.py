"""Pack the painted waterfronts as streamed indexed game textures."""
from PIL import Image

from make_art import ASSETS, texture_bytes, write_texture

PORTS = ("ragusa", "candia", "diu", "gresik", "quanzhou", "hakata", "kochi", "busanpo")
MAP_SIZE = (480, 270)


def main():
    with Image.open(ASSETS / "port_boats.png") as frames:
        if frames.size != (288, 288) or "A" not in frames.getbands():
            raise ValueError("Expected a transparent 3-frame atlas for 4 vessels")
        write_texture(frames.convert("RGBA"), "port_boats")
    for port in PORTS:
        with Image.open(ASSETS / f"town-{port}.jpg") as painting:
            if painting.size != MAP_SIZE:
                raise ValueError(f"{port}: expected {MAP_SIZE} to match the marked streets")
            path = ASSETS.parent / "assets" / "ports" / f"{port}-streets.pxi"
            path.write_bytes(texture_bytes(painting.convert("RGB")))


if __name__ == "__main__":
    main()
