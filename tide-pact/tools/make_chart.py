"""Build the game's bathymetry-style chart and water-only passage graph."""
import argparse
import heapq
import io
import json
import math
import struct
import zipfile
from pathlib import Path

import shapefile
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
W, H = 1540, 1136
WEST, NORTH = -125, 84
SX, SY = 7, 8
DIRS = ((1, 0), (0, 1), (-1, 0), (0, -1), (1, 1), (-1, 1), (-1, -1), (1, -1))


def project(lon, lat):
    return round((lon - WEST) * SX), round((NORTH - lat) * SY)


def mask_from_land(path):
    mask = Image.new("L", (W, H))
    draw = ImageDraw.Draw(mask)
    with zipfile.ZipFile(path) as archive:
        shp = next(name for name in archive.namelist() if name.endswith(".shp"))
        reader = shapefile.Reader(shp=io.BytesIO(archive.read(shp)))
        for land in reader.iterShapes():
            lo, bot, hi, top = land.bbox
            if hi < WEST or lo > WEST + W / SX or bot > NORTH or top < NORTH - H / SY:
                continue
            starts = [*land.parts, len(land.points)]
            for i, j in zip(starts, starts[1:]):
                points = land.points[i:j]
                clockwise = sum(x * v - y * u for (x, y), (u, v) in zip(points, points[1:])) < 0
                draw.polygon([project(lon, lat) for lon, lat in points], fill=255 if clockwise else 0)
    # One atlas pixel is close to the width of the narrowest part of Gibraltar.
    # Retain its navigable channel where rasterization joins the two banks.
    draw.line([project(lon, lat) for lon, lat in [
        (-6.02, 35.96), (-5.79, 35.94), (-5.61, 35.96), (-5.43, 36.01),
        (-5.22, 36.02)]], fill=0, width=1)
    return mask


def sea(mask, point):
    x, y = point
    return 0 <= x < W and 0 <= y < H and mask[x, y] == 0


def segment(a, b):
    steps = max(abs(a[0] - b[0]), abs(a[1] - b[1]), 1)
    return ((round(a[0] + (b[0]-a[0])*k / steps),
             round(a[1] + (b[1]-a[1])*k / steps)) for k in range(steps + 1))


def water_path(mask, start, end, step):
    if all(sea(mask, p) for p in segment(start, end)):
        return [start, end]
    queue = [(math.dist(start, end), 0, start)]
    cost = {start: 0}
    parent = {}
    while queue:
        _, distance, a = heapq.heappop(queue)
        if distance != cost[a]:
            continue
        if math.dist(a, end) <= step * 2 and all(sea(mask, p) for p in segment(a, end)):
            points = [end, a]
            while a != start:
                a = parent[a]
                points.append(a)
            points.reverse()
            reduced = [start]
            index = 0
            while index < len(points) - 1:
                candidate = len(points) - 1
                while not all(sea(mask, p) for p in segment(points[index], points[candidate])):
                    candidate -= 1
                reduced.append(points[candidate])
                index = candidate
            return reduced
        for dx, dy in DIRS:
            b = a[0] + dx * step, a[1] + dy * step
            if not sea(mask, b) or not all(sea(mask, p) for p in segment(a, b)):
                continue
            price = distance + step * (1.41421356 if dx and dy else 1)
            if price >= cost.get(b, math.inf):
                continue
            cost[b] = price
            parent[b] = a
            heapq.heappush(queue, (price + math.dist(b, end), price, b))
    return None


def nautical(a, b):
    lon_a, lon_b = (WEST + point[0] / SX for point in (a, b))
    lat_a, lat_b = (math.radians(NORTH - point[1] / SY) for point in (a, b))
    sinlat = math.sin((lat_b - lat_a) / 2)
    sinlon = math.sin(math.radians(lon_b - lon_a) / 2)
    return 6880.13 * math.asin(min(1, math.sqrt(sinlat**2 + math.cos(lat_a)*math.cos(lat_b)*sinlon**2)))


def chart(mask, destination):
    # Terrain uses two bits per pixel. Grain and the graticule are drawn at runtime.
    bases = [(21,59,67), (48,91,91), (184,163,113), (144,132,95)]
    grid = [(53,104,108), (66,109,105), (164,146,104), (134,123,88)]
    colors = []
    for base, line in zip(bases, grid):
        colors.extend(tuple(v+shade for v in base) for shade in (-2, 2, 5))
        colors.append(line)
    rgb565 = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3) for r, g, b in colors]
    shelf = mask.filter(ImageFilter.MaxFilter(9)).load()
    inland = mask.filter(ImageFilter.MinFilter(3)).load()
    binary = mask.load()
    packed = bytearray()
    assert W % 4 == 0
    for y in range(H):
        for left in range(0, W, 4):
            tile = 0
            for i in range(4):
                x = left + i
                terrain = (2 if inland[x, y] else 3) if binary[x, y] else int(bool(shelf[x, y]))
                tile |= terrain << (2*i)
            packed.append(tile)
    (destination / "chart.pxi").write_bytes(
        struct.pack("<HH256H", W | 0x8000, H, *rgb565, *([0] * (256-len(rgb565)))) + packed)


def quote(value):
    return json.dumps(value, ensure_ascii=False)


def world(data, mask):
    locations = []
    points = []
    ids = {}
    port_bias = []
    port_zone = []
    for entry in data["ports"] + data["sites"]:
        ident, name, *values = entry
        is_port = len(values) == 8
        region = values.pop(0) if is_port else "测绘点"
        lon, lat, anch_lon, anch_lat, *info = values
        if is_port:
            zone, bias, note = info
            port_bias.append(bias)
            port_zone.append(zone)
        else:
            note, = info
        x, y = project(lon, lat)
        anchor = project(anch_lon, anch_lat)
        near = [(p, q) for p in range(anchor[0]-7, anchor[0]+8)
                for q in range(anchor[1]-7, anchor[1]+8) if sea(mask, (p, q))]
        if not near:
            raise ValueError(f"No sea near {ident}: {anchor}")
        anchor = min(near, key=lambda i: math.dist(i, anchor))
        locations.append(f'    {{{quote(name)}, {quote(region)}, {quote(note)}, '
                         f'{{{x},{y}}}, {round(lon*100)}, {round(lat*100)}}},')
        ids[ident] = len(points)
        points.append(anchor)
    links = []
    waypoints = []
    for left, right, *via in data["links"]:
        a, b = ids[left], ids[right]
        intermediate = [project(lon, lat) for lon, lat in via[0]] if via else []
        course = [points[a], *intermediate, points[b]]
        step = 1 if "gibraltar" in (left, right) or "mandeb" in (left, right) else 3
        path = [course[0]]
        for start, end in zip(course, course[1:]):
            if not sea(mask, end):
                raise ValueError(f"Waypoint is on land: {left} to {right} at {end}")
            segment_path = water_path(mask, start, end, step)
            if segment_path is None and step != 1:
                segment_path = water_path(mask, start, end, 1)
            if segment_path is None:
                raise ValueError(f"No water path: {left} to {right} between {start} and {end}")
            path += segment_path[1:]
        nm = round(sum(nautical(p, q) for p, q in zip(path, path[1:])))
        print(f"{left} → {right}: {nm} nmi, {len(path)} points")
        links.append(f"    {{{a}, {b}, {len(waypoints)}, {len(path)}, {nm}}},")
        waypoints += path
    assert {i for left, right, *_ in data["links"] for i in (left, right)} == set(ids)
    blocks = [
        f"inline constexpr int ChartWidth = {W}, ChartHeight = {H}, ChartNorth = {NORTH}, ChartWest = {WEST};\n"
        f"inline constexpr int ChartLongitudeScale = {SX}, ChartLatitudeScale = {SY};",
        f"inline constexpr int PortCount = {len(data['ports'])};",
        "enum PlaceId {\n    " + ", ".join(ids) + "\n};",
        "inline constexpr Location Places[] = {\n" + "\n".join(locations) + "\n};",
        "inline constexpr Point Anchor[] = {\n    " + ", ".join("{%d,%d}" % p for p in points) + "\n};",
        "inline constexpr int Zone[] = {" + ", ".join(map(str, port_zone)) + "};",
        "inline constexpr int Bias[][8] = {\n    " + ",\n    ".join(
            "{" + ", ".join(map(str, b)) + "}" for b in port_bias) + "\n};",
        "inline constexpr Point Waypoints[] = {\n    " + ",\n    ".join(
            ", ".join("{%d,%d}" % p for p in waypoints[i:i+8])
            for i in range(0, len(waypoints), 8)) + "\n};",
        "inline constexpr Leg Legs[] = {\n" + "\n".join(links) + "\n};",
    ]
    (ROOT / "world_data.hpp").write_text("// Generated by tools/make_chart.py from world.json.\n" +
                                         "\n\n".join(blocks) + "\n")


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("natural_earth_land_zip", type=Path)
    args = cli.parse_args()
    data = json.loads((ROOT / "world.json").read_text())
    mask = mask_from_land(args.natural_earth_land_zip)
    world(data, mask.load())
    chart(mask, ROOT / "art")


if __name__ == "__main__":
    main()
