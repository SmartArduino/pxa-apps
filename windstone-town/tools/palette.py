"""Color grade the generated atlases at build time, preserving native pixel masks."""
from colorsys import hls_to_rgb, rgb_to_hls
from functools import cache
import struct


RAMPS = {
    "foliage": ((20, 0x1E3731), (65, 0x355947), (100, 0x547352), (145, 0x789655),
                (178, 0xA3B76C), (213, 0xCDD893), (246, 0xEEF1BF)),
    "forest": ((20, 0x1D342E), (75, 0x355345), (105, 0x47614D), (170, 0x6C8562),
               (240, 0xA3B58A)),
    "stone": ((25, 0x343640), (75, 0x645E63), (117, 0x877D78), (146, 0xA19586),
              (178, 0xC1B095), (217, 0xDECAA8), (246, 0xF4E4C7)),
    "stone shade": ((25, 0x30323D), (75, 0x514F60), (115, 0x686879), (150, 0x827E8B),
                    (185, 0xA4A0A2), (246, 0xD8D2C8)),
    "paving": ((25, 0x3E3F36), (90, 0x746A55), (145, 0xA39878), (180, 0xC3B28D),
               (215, 0xDECDA7), (246, 0xF4E6C7)),
    "water": ((25, 0x193A47), (60, 0x245569), (95, 0x317385), (130, 0x3F8795),
              (165, 0x66ADAE), (206, 0xB1DCD1), (245, 0xE6F3E5)),
    "autumn": ((35, 0x4F4A3A), (90, 0x8A6648), (135, 0xAB845D), (175, 0xC5A56D),
               (220, 0xDBC392)),
    "frost": ((35, 0x405668), (90, 0x6A8290), (125, 0x9CB5BF), (165, 0xB8CDD0),
              (205, 0xD4E4E2), (245, 0xEFF4ED)),
}

# Blocks follow the atlas layout in make_art.py. Left and right rock faces keep
# their own ramps so a shaded face remains distinct in all four seasons.
REGIONS = {
    "terrain": (72, 20, ("foliage", "forest", "foliage", "frost", "paving", "stone",
                        "water", "water", "scene", "stone", "paving", "autumn",
                        "stone", "stone shade", "paving", "bank", "water", "water")),
    "walls": (72, 26, ("stone", "stone", "stone shade", "stone shade")),
    "backdrop": (36, 36, ("forest", "frost", "autumn")),
    "tools": (22, 17, ("paving", "stone", "scene", "scene")),
}


def unpack_rgb(color):
    return color >> 16, (color >> 8) & 255, color & 255


def to_rgb565(rgb):
    r, g, b = rgb
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def from_rgb565(color):
    r, g, b = color >> 11, (color >> 5) & 63, color & 31
    return (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)


def blend(a, b, fraction):
    return tuple(round(x + (y - x) * fraction) for x, y in zip(a, b))


def ramp(name, luma):
    stops = RAMPS[name]
    for (lo, a), (hi, b) in zip(stops, stops[1:]):
        if luma <= hi:
            return blend(unpack_rgb(a), unpack_rgb(b), max(0, (luma - lo) / (hi - lo)))
    return unpack_rgb(stops[-1][1])


@cache
def grade(color, role):
    rgb = from_rgb565(color)
    h, light, saturation = rgb_to_hls(*(ch / 255 for ch in rgb))
    hue = h * 360
    luma = sum(ch * weight for ch, weight in zip(rgb, (.213, .715, .072)))
    foliage = 48 <= hue < 168 and saturation > .20
    if role in ("stone", "stone shade", "bank") and foliage:
        return to_rgb565(ramp("forest" if role == "stone shade" else "foliage", luma))
    if role == "bank":
        return to_rgb565(ramp("stone", luma))
    if role == "water":
        # Preserve differences between ripples and the river's base hue.
        return to_rgb565(blend(ramp(role, luma), rgb, .30))
    if role in ("stone", "stone shade"):
        return to_rgb565(blend(ramp(role, luma), rgb, .25))
    if role in RAMPS:
        return to_rgb565(ramp(role, luma))
    if foliage:
        return to_rgb565(ramp("foliage", luma))
    if role == "plants" and 168 <= hue < 200 and luma < 130:
        return to_rgb565(blend(rgb, ramp("foliage", luma), .36))
    if 165 <= hue <= 214 and saturation > .22:
        return to_rgb565(blend(rgb, ramp("water", luma), .35))
    saturation *= .80 if 16 <= hue < 48 else .94
    return to_rgb565(tuple(round(ch * 255) for ch in hls_to_rgb(h, light, saturation)))


def grade_atlas(name, data):
    if name in ("font", "ui", "icons", "folio"):
        return data
    w, h = struct.unpack_from("<HH", data)
    palette = struct.unpack_from("<256H", data, 4)
    default_role = {"stairs": "stone", "waterfall": "water", "glints": "water"}.get(name, name)
    bw, bh, roles = REGIONS.get(name, (w, h, (default_role,)))
    columns = w // bw
    output = bytearray(data)
    colors = {}
    for block, role in enumerate(roles):
        col, row = block % columns * bw, block // columns * bh
        lookup = [255] * 256
        for y in range(row, row + bh):
            start = 516 + y * w + col
            for x in range(start, start + bw):
                index = data[x]
                if index == 255:
                    continue
                if lookup[index] == 255:
                    color = grade(palette[index], role)
                    lookup[index] = colors.setdefault(color, len(colors))
                    if lookup[index] >= 255:
                        raise ValueError(f"Too many colors after grading {name}")
                output[x] = lookup[index]
    struct.pack_into(f"<{len(colors)}H", output, 4, *colors)
    return output
