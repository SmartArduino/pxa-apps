"""Pack the painted waterfronts as streamed indexed game textures."""
from PIL import Image

from make_art import ASSETS, texture_bytes, write_texture

STORY_PORTS = ("malacca", "naha", "ningbo", "sakai")
PORTS = ("ragusa", "candia", "diu", "gresik", "quanzhou", "hakata", "kochi", "busanpo", *STORY_PORTS)
MAP_SIZE = (480, 270)


def main():
    with Image.open(ASSETS / "port_boats.png") as frames:
        if frames.size != (288, 288) or "A" not in frames.getbands():
            raise ValueError("Expected a transparent 3-frame atlas for 4 vessels")
        write_texture(frames.convert("RGBA"), "port_boats")
    scenes = [(f"town-{port}.jpg", f"{port}-streets.pxi") for port in PORTS]
    scenes += [(f"story-{port}.jpg", f"{port}-story.pxi") for port in STORY_PORTS]
    for source, packed in scenes:
        with Image.open(ASSETS / source) as painting:
            if painting.size != MAP_SIZE:
                raise ValueError(f"{source}: expected {MAP_SIZE}")
            path = ASSETS.parent / "assets" / "ports" / packed
            path.write_bytes(texture_bytes(painting.convert("RGB")))


if __name__ == "__main__":
    main()
