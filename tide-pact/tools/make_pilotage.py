"""Compile nearshore charts from GeoTIFFs and documented shoreline corrections."""
import argparse
import json
import math
from collections import deque
from pathlib import Path

from PIL import Image, ImageDraw
from pilot_berths import edge_water, neighbours, settlement_mark, shoreline_berth

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT, FRACTION = 256, 128, 16


def relief(elevation):
    if elevation < -65:
        return 0
    if elevation < -19:
        return 1
    if elevation < -7:
        return 2
    if elevation < 0:
        return 3
    if elevation < 3:
        return 4
    return min(15, 5 + int(math.sqrt(elevation) / 4.5))


def roadstead_terrain(heights, region, project):
    # Copernicus gives surface heights, with the sea flattened to zero.
    # Reconstruct the lost inlet and rocky point before deriving depth classes.
    coast = Image.new("L", (WIDTH, HEIGHT))
    coast.putdata([int(value > .25) for value in heights])
    draw = ImageDraw.Draw(coast)
    for key, material in (("rocky_point", 1), ("lagoon", 2)):
        draw.polygon([tuple(value / FRACTION - .5 for value in project(*point))
                      for point in region[key]], fill=material)
    surface = list(coast.get_flattened_data())
    distance = [0 if value == 1 else WIDTH + HEIGHT for value in surface]
    coast_cells = deque(index for index, value in enumerate(surface) if value == 1)
    while coast_cells:
        index = coast_cells.popleft()
        depth = distance[index] + 1
        if depth > 30:
            continue
        for x, y in neighbours(index % WIDTH, index // WIDTH):
            if 0 <= x < WIDTH and 0 <= y < HEIGHT:
                cell = y * WIDTH + x
                if distance[cell] > depth:
                    distance[cell] = depth
                    coast_cells.append(cell)
    return [relief(max(3, height)) if material == 1 else
            3 if material == 2 or steps <= 2 else 2 if steps <= 30 else 1
            for height, material, steps in zip(heights, surface, distance)]


def compile_area(region, directory):
    with Image.open(directory / (region["tile"] + ".tif")) as tile:
        sx, sy, _ = tile.tag_v2[33550]
        _, _, _, lon, lat, _ = tile.tag_v2[33922]
        west, north, east, south = region["bounds"]
        extent = ((west - lon) / sx, (lat - north) / sy, (east - lon) / sx, (lat - south) / sy)
        if not (0 <= extent[0] < extent[2] <= tile.width and 0 <= extent[1] < extent[3] <= tile.height):
            raise ValueError(f"Region {region['key']} exceeds its ETOPO tile")
        sample = tile.transform((WIDTH, HEIGHT), Image.Transform.EXTENT, extent, Image.Resampling.BILINEAR)
        heights = list(sample.get_flattened_data())
    def locate(x, y):
        return (round((x - west) / (east - west) * WIDTH * FRACTION),
                round((north - y) / (north - south) * HEIGHT * FRACTION))

    roadstead = region.get("roadstead", False)
    terrain = roadstead_terrain(heights, region, locate) if roadstead else [relief(height) for height in heights]
    data = bytes(a | (b << 4) for a, b in zip(terrain[::2], terrain[1::2]))
    asset = "pilot-" + region["key"] + ".bin"

    def wet(x, y):
        return terrain[y * WIDTH + x] < 4

    def navigable(x, y):
        return terrain[y * WIDTH + x] < (3 if roadstead else 4)

    connected = edge_water((0, 0, WIDTH, HEIGHT), navigable)

    def aspect(lat):
        return (east - west) * HEIGHT * math.cos(math.radians(lat)) / ((north - south) * WIDTH)

    def safe_berth(lon, lat):
        x, y = locate(lon, lat)
        if roadstead:
            i, j = x // FRACTION, y // FRACTION
            if (i, j) not in connected:
                raise ValueError(f"Roadstead at {lon}, {lat} is outside navigable water")
            return i * FRACTION + 8, j * FRACTION + 8
        i, j = shoreline_berth(x / FRACTION, y / FRACTION, 14, (WIDTH, HEIGHT), wet, connected,
                               aspect(lat), lambda u, v: heights[v * WIDTH + u] < -1)
        return i * FRACTION + 8, j * FRACTION + 8

    def quoted(value):
        return json.dumps(value, ensure_ascii=False)

    sites = []
    for name, x, y, kind, note, *dock in region["sites"]:
        mark_x, mark_y = locate(x, y)
        if not (0 <= mark_x < WIDTH * FRACTION and 0 <= mark_y < HEIGHT * FRACTION):
            raise ValueError(f"Site {name} is outside its region")
        if kind != "point":
            u, v = settlement_mark(mark_x / FRACTION, mark_y / FRACTION, (WIDTH, HEIGHT), wet, aspect(y))
            mark_x, mark_y = round(u * FRACTION), round(v * FRACTION)
        bx, by = safe_berth(*dock[0]) if dock else (-1, -1)
        sites.append(f"    {{{quoted(name)}, {quoted(note)}, {{{mark_x},{mark_y}}}, {{{bx},{by}}}, Pilot{kind.title()}}},")
    edges = ", ".join(f"{value:.5f}f" for value in region["bounds"])
    (ROOT / "assets" / "sea" / asset).write_bytes(b"PCH1" + data)
    return asset, edges, sites


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tiles", type=Path, help="directory containing N45W015.tif and the other named tiles")
    args = parser.parse_args()
    regions = json.loads((ROOT / "pilotage.json").read_text())
    region_rows, site_rows = [], []
    for region in regions:
        asset, bounds, sites = compile_area(region, args.tiles)
        region_rows.append(
            f'    {{"{region["name"]}", "assets/sea/{asset}", {region["port"]}, '
            f'{len(site_rows)}, {len(sites)}, {bounds}'
            + (", true" if region.get("roadstead") else "") + "},")
        site_rows.extend(sites)
    if len(site_rows) > 32:
        raise ValueError("The pilotage discovery bitset has 32 places")
    entries = "\n".join(region_rows)
    sites = "\n".join(site_rows)
    (ROOT / "pilotage_data.hpp").write_text(
        "// Generated by tools/make_pilotage.py.\n"
        f"inline constexpr PilotRegion PilotRegions[]{{\n{entries}\n}};\n"
        f"inline constexpr PilotSite PilotSites[]{{\n{sites}\n}};\n")


if __name__ == "__main__":
    main()
