"""Prepare the pixel atlases and game-specific CJK font. Requires Pillow."""
import argparse
from pathlib import Path
import struct

from PIL import Image, ImageFont, ImageOps, ImageStat

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
PAPER = (26, 49, 49, 255)
WORLD_PALETTE = None


def write_indexed(image, name, colors=None, swatches=()):
    alpha = image.getchannel("A")
    rgb = image.convert("RGB")
    if colors is None and WORLD_PALETTE is not None and name != "title":
        indexed = rgb.quantize(palette=WORLD_PALETTE, dither=Image.Dither.NONE)
    else:
        indexed = rgb.quantize(colors or 255, dither=Image.Dither.NONE)
    palette = indexed.getpalette()
    data = bytearray(struct.pack("<HH", *image.size))
    for red, green, blue in zip(palette[::3], palette[1::3], palette[2::3]):
        data.extend(struct.pack("<H", (red >> 3) << 11 | (green >> 2) << 5 | blue >> 3))
    data.extend(bytes(516 - len(data)))
    data.extend(color if opacity >= 120 else 255
                for color, opacity in zip(indexed.tobytes(), alpha.tobytes()))
    for index, (x,y) in enumerate(swatches, 255-len(swatches)):
        red, green, blue = rgb.getpixel((x,y))
        struct.pack_into("<H", data, 4+2*index, (red >> 3) << 11 | (green >> 2) << 5 | blue >> 3)
        data[516+y*image.width+x] = index
    (ROOT / "art" / (name + ".pxi")).write_bytes(data)


def grid(path, columns, rows, inset=0):
    image = Image.open(path).convert("RGBA")
    for row in range(rows):
        for col in range(columns):
            yield image.crop((col * image.width // columns + inset, row * image.height // rows + inset,
                              (col + 1) * image.width // columns - inset,
                              (row + 1) * image.height // rows - inset))


def cutouts(cells, threshold=138):
    for index, item in enumerate(cells):
        item.putalpha(item.getchannel("A").point(lambda a: 255 if a >= threshold else 0))
        bounds = item.getbbox()
        if bounds is None:
            raise ValueError(f"Empty artwork cell {index}")
        yield item.crop(bounds)


def isolated(path, columns, rows, threshold=138, inset=0):
    return cutouts(grid(path, columns, rows, inset), threshold)


def sized(item, bounds):
    item = item.copy()
    item.thumbnail(bounds, Image.Resampling.BOX)
    return item


def sheet(name, items, cell, columns, center=False, stretch=False, colors=None):
    items = list(items)
    w, h = cell
    image = Image.new("RGBA", (columns * w, ((len(items) + columns - 1) // columns) * h))
    for i, item in enumerate(items):
        if stretch:
            item = item.resize(cell, Image.Resampling.BOX)
        x = i % columns * w + (w - item.width) // 2
        y = i // columns * h + ((h - item.height) // 2 if center else h - item.height)
        image.paste(item, (x, y))
    write_indexed(image, name, colors)


def aligned_frames(paths, rows, size, threshold=140, columns=4):
    for path in paths:
        sources = list(grid(path, columns, rows))
        for row in range(rows):
            items = sources[row*columns:(row+1)*columns]
            bounds = []
            normalized = []
            w, h = items[0].size
            for item in items:
                item = item.resize((w, h), Image.Resampling.NEAREST)
                item.putalpha(item.getchannel("A").point(lambda a: 255 if a >= threshold else 0))
                bounds.append(item.getbbox())
                normalized.append(item)
            if any(box is None for box in bounds):
                raise ValueError(f"Missing animation frame in row {row} of {path}")
            x0, y0 = (min(box[i] for box in bounds) for i in (0, 1))
            x1, y1 = (max(box[i] for box in bounds) for i in (2, 3))
            for item in normalized:
                item = item.crop((x0, y0, x1, y1))
                yield sized(item, size)


def walking_frames(steps, opposite):
    for base, alternate in zip(steps, opposite, strict=True):
        first = list(aligned_frames([base], 4, (19, 22), 207))
        second = list(aligned_frames([alternate], 4, (19, 22), 207, columns=2))
        for row in range(4):
            yield from first[row*4:row*4+2]
            yield from second[row*2:row*2+2]


def pack_native_atlases(directory, atlases):
    images = {}
    for name, size in atlases.items():
        source = directory / (name + ".png")
        with Image.open(source) as image:
            rgba = image.convert("RGBA")
        if rgba.size != size:
            raise ValueError(f"{source} must be {size[0]} × {size[1]} pixels")
        if any(rgba.getchannel("A").histogram()[1:255]):
            raise ValueError(f"{source} must use fully opaque or fully transparent pixels")
        images[name] = rgba
    for name, image in images.items():
        write_indexed(image, name, colors=255)


# One map cell spans two 18 × 10 axes, with a 16-pixel story height.
EAST, SOUTH, LIFT = (18, 10), (-18, 10), 16


def project(item, across, along, flip=False):
    """Bake an aligned isometric patch. The renderer only copies these pixels."""
    ux, uy = across
    vx, vy = along
    xs, ys = (0, ux, vx, ux + vx), (0, uy, vy, uy + vy)
    left, top = min(xs), min(ys)
    result = Image.new("RGBA", (max(xs)-left, max(ys)-top))
    det = ux*vy - uy*vx
    side = -1 if det < 0 else 1
    length = 2*det*side
    src, dst = item.load(), result.load()
    for y in range(result.height):
        dy = 2*(top+y) + 1
        for x in range(result.width):
            dx = 2*(left+x) + 1
            u, v = (dx*vy-dy*vx)*side, (ux*dy-uy*dx)*side
            if 0 <= u < length and 0 <= v < length:
                sx, sy = u*item.width//length, v*item.height//length
                dst[x, y] = src[item.width-1-sx if flip else sx, sy]
    return result


def rectangle(item, size):
    return ImageOps.fit(item, size, method=Image.Resampling.BOX).convert("RGBA")


def pack_buildings(path, landmarks):
    items = list(isolated(path, 4, 2, inset=8))
    cave, inn, forge, guild, market = landmarks[3:]
    for index, item in zip((6, 1, 2, 0, 5), (cave, inn, forge, guild, market), strict=True):
        items[index] = item
    world_sizes = (46, 42, 42, 42, 42, 35, 46, 42)
    image = Image.new("RGBA", (384, 104))
    for i, item in enumerate(items):
        for top, slot, size in ((0, 48, world_sizes[i]), (48, 22, 21), (70, 34, 32)):
            sprite = sized(item, (size, size))
            image.paste(sprite, (i*48+(48-sprite.width)//2, top+slot-sprite.height))
    write_indexed(image, "sprites", colors=160)


def pack_plants(path, edges, village, flora, landmarks):
    items = list(isolated(path, 4, 4))
    items.extend(isolated(edges, 4, 2))
    items.extend(isolated(village, 4, 2))
    flora_cells = list(grid(flora, 4, 3))
    # The wide old oak extends into the otherwise empty right side of the
    # adjacent daisy cell on the generated sheet.
    source = Image.open(flora).convert("RGBA")
    start, cut = source.width // 2, source.width * 73 // 100
    top = source.height * 2 // 3
    flora_cells[-2:] = [source.crop((start, top, cut, source.height)),
                       source.crop((cut, top, source.width, source.height))]
    details = list(cutouts(flora_cells, threshold=200))
    items.append(details[8])
    for index, item in zip((0, 1, 2, 3, 4, 5, 6, 7, 28, 29, 30),
                           details[:8] + details[9:], strict=True):
        items[index] = item
    for index, item in zip((10, 25, 13), landmarks[:3], strict=True):
        items[index] = item
    sizes = [(32,34),(32,34),(28,32),(29,32),(18,19),(24,26),(13,14),(15,17),
             (16,18),(22,24),(41,34),(31,33),(28,30),(31,33),(29,31),(20,22),
             (18,14),(18,14),(19,14),(19,14),(22,20),(15,19),(13,16),(17,19),
             (21,23),(18,20),(24,26),(18,20),(17,19),(18,20),(33,36),(29,31),(32,36)]
    sheet("plants", (sized(item, size) for item, size in zip(items, sizes, strict=True)),
          (42, 36), 4, colors=160)


def pack_ui(path, book, folio, tabs):
    atlas = Image.new("RGBA", (160, 128))
    cells = list(grid(path, 4, 3))
    # The generated middle row has an extra highlight in its upper gutter.
    for i in range(4, 8):
        w, h = cells[i].size
        cells[i] = cells[i].crop((w*11//100, h*28//100, w*90//100, h*67//100))
    parts = list(cutouts(cells, threshold=190))
    cover, _, _, _, chosen, focus_row, rule, rail = isolated(book, 4, 2, threshold=190)
    for i, item in ((0, cover), (6, chosen), (7, focus_row), (8, rail)):
        parts[i] = item
    for i, item in enumerate(parts):
        size = (40, 18 if i==11 else 30)
        atlas.paste(item.resize(size, Image.Resampling.BOX), (i%4*40, i//4*30))
    frame = next(isolated(folio, 1, 1, threshold=192)).resize((72, 52), Image.Resampling.BOX)
    write_indexed(frame, "folio", colors=144)
    paper = frame.getpixel((36, 26))
    for i, item in enumerate(isolated(tabs, 2, 2, threshold=192)):
        selected = i % 2 == 0
        # The shorter inactive tab shares its lower row with the page rim.
        inset = 0 if selected else 2
        tab = item.resize((78, 19-inset), Image.Resampling.BOX)
        if selected:
            r, g, b, alpha = tab.split()
            color = tab.getpixel((39, tab.height-1))
            channels = [plane.point(lambda value, shift=target-seen: max(0,min(255,value+shift)))
                        for plane, target, seen in zip((r,g,b), paper[:3], color[:3], strict=True)]
            tab = Image.merge("RGBA", (*channels, alpha))
            if i == 0:
                rim = [tab.getpixel((x,tab.height-1)) for x in range(2)]
            for j, color in enumerate(rim):
                tab.putpixel((j,tab.height-1),color)
                tab.putpixel((tab.width-1-j,tab.height-1),color)
        else:
            for x in range(tab.width):
                tab.putpixel((x,tab.height-1),rim[0])
        atlas.paste(tab, (i%2*78, 90+i//2*19+inset))
    texture = rule.resize((72, 48), Image.Resampling.BOX)
    samples = [texture.getpixel((x, y)) for y in range(48) for x in range(72)]
    ink = sorted((sample[:3] for sample in samples if sample[3] >= 190), key=sum)
    # The dark fibers provide a slightly deeper paper tone for the rules.
    rule_color = ink[len(ink)//100]
    atlas.putpixel((156,106), tuple(round(a*0.35+b*0.65) for a,b in zip(paper[:3],rule_color)) + (255,))
    write_indexed(atlas, "ui", colors=168, swatches=((156,106),))


def pack_markers(path):
    raw = list(isolated(path, 4, 3))
    marks = [project(item.resize((40,30), Image.Resampling.BOX), EAST, SOUTH) for item in raw[6:8]]
    sheet("markers", marks, (36, 20), 2)


def pack_expedition(path):
    items = list(isolated(path, 4, 2, threshold=190))
    atlas = Image.new("RGBA", (4 * 42, 66))
    for i, bounds in enumerate(((30, 30), (23, 18), (20, 23), (22, 16))):
        item = sized(items[i], bounds)
        atlas.paste(item, (i * 42 + (42 - item.width) // 2, 36 - item.height))
    for i, image in enumerate(items[4:]):
        item = sized(image, (20, 20))
        atlas.paste(item, (i * 42 + (42 - item.width) // 2, 36 + (20 - item.height) // 2))
    medicine = sized(items[-1], (10, 10))
    atlas.paste(medicine, (40 + (10 - medicine.width) // 2, 56 + (10 - medicine.height) // 2))
    write_indexed(atlas, "expedition", colors=142)


def pack_icons(path):
    raw = list(isolated(path, 5, 3, threshold=190))
    atlas = Image.new("RGBA", (15*18, 10+12+14+18))
    top = 0
    for size in (10, 12, 14, 18):
        for i, item in enumerate(raw):
            icon = sized(item, (size, size))
            atlas.paste(icon, (i*18+(size-icon.width)//2, top+(size-icon.height)//2))
        top += size
    write_indexed(atlas, "icons", colors=160)


def pack_effects(path):
    result = []
    for i, frame in enumerate(aligned_frames([path], 4, (23, 25), 148)):
        bounds = ((23,25),(16,21),(15,15),(16,23))[i//4]
        result.append(sized(frame, bounds))
    sheet("effects", result, (24, 26), 4)


def pack_waterfall(path):
    # Match the generated curtain's two lip corners and foot to the map edge.
    # Pixel (6, 4) in a frame is the screen-left corner of the upper river lip.
    lip_x, lip_y, far_x, far_y = 130, 300, 425, 233
    left, top = 6, 4
    sx, sy = (far_x - lip_x) / EAST[0], 220 / LIFT
    shear = (far_y - lip_y - EAST[1] * sy) / EAST[0]
    transform = (sx, 0, lip_x - left * sx, shear, sy, lip_y - left * shear - top * sy)
    frames = [item.transform((30, 34), Image.Transform.AFFINE, transform, Image.Resampling.NEAREST)
              for item in grid(path, 4, 1)]
    sheet("waterfall", frames, (30, 34), 4)


def terrain(path, materials):
    textures = []
    for index, item in enumerate(grid(path, 4, 4, 5)):
        item = item.resize((24, 24), Image.Resampling.LANCZOS)
        if index in (0, 1, 2, 3, 11):
            mean = tuple(round(ch) for ch in ImageStat.Stat(item).mean)
            item = Image.blend(item, Image.new("RGBA", item.size, mean), 0.47)
        if index == 14:
            item = item.resize((12, 12), Image.Resampling.LANCZOS).resize((24, 24), Image.Resampling.NEAREST)
        textures.append(item)
    # Each flat source is projected into the same native masks as the map.
    # Resampling here leaves the runtime free to copy whole pixels.
    textures.append(None)
    for index, cell in zip((0, 2, 1, 6, 4, 14, 7, 15, 12, 13, 5, 16),
                           grid(materials, 4, 3, 2), strict=True):
        item = cell.resize((32, 32), Image.Resampling.BOX)
        if index in (0, 2, 6, 7):
            mean = tuple(round(ch) for ch in ImageStat.Stat(item).mean)
            item = Image.blend(item, Image.new("RGBA", item.size, mean), .22 if index in (6, 7) else .32)
        textures[index] = item
    textures[10] = textures[4]
    # Mirrored neighbors share the same texture edge when the river turns.
    surface = textures[:16] + [ImageOps.flip(textures[i]) for i in (6, 7)]
    sheet("terrain", (project(item, EAST, SOUTH, flip)
                      for item in surface for flip in (False, True)), (36, 20), 2, colors=120)

    walls = []
    for index in (5, 12, 13, 16):
        for across in (SOUTH, EAST):
            walls.extend(project(textures[index], across, (0, LIFT), flip) for flip in (False, True))
    sheet("walls", walls, (18, 26), 4, colors=120)
    step_vectors = (((10,-5),(-18,6)), ((10,-5),(18,26)),
                    ((10,5),(18,6)), ((10,5),(-18,26)))
    sheet("stairs", (project(textures[9], width, slope) for width, slope in step_vectors), (28, 31), 4)
    forest = textures[1].resize((36,36), Image.Resampling.BOX)
    forest = Image.blend(forest, Image.new("RGBA", forest.size, (55, 83, 64, 255)), .52)
    sheet("backdrop", [forest] + [textures[i].resize((36,36), Image.Resampling.BOX) for i in (3,11)],
          (36,36), 3, colors=120)

    tools = []
    width, length = (10,5),(-10,5)
    for item in (textures[4], textures[9], textures[2], textures[2]):
        patch = Image.new("RGBA", (22, 17))
        is_earthwork = len(tools)>=2
        top = 0 if is_earthwork else 4
        if is_earthwork:
            patch.paste(project(textures[5],width,(0,6)),(1,5))
            patch.paste(project(textures[16],length,(0,6)),(11,5))
        patch.alpha_composite(project(item,width,length),(1,top))
        tools.append(patch)
    sheet("tools", tools, (22,17), 4)


def title(cover):
    source = Image.open(cover).convert("RGBA")
    image = Image.new("RGBA", (320, 500), PAPER)
    image.alpha_composite(ImageOps.fit(source, (320, 200), Image.Resampling.NEAREST), (0, 0))
    portrait = ImageOps.fit(source, (240, 300), Image.Resampling.NEAREST, centering=(.58, .55))
    image.alpha_composite(portrait, (0, 200))
    write_indexed(image, "title")
    icon = image.crop((47, 101, 132, 186)).resize((64, 64), Image.Resampling.BOX)
    icon.save(ASSETS / "icon.png", optimize=True)


def font(path, index):
    import re
    text = "".join(p.read_text() for p in ROOT.glob("*.?pp") if p.name != "pixel_assets.cpp")
    strings = "".join(re.findall(r'"(?:\\.|[^"\\])*"', text))
    codes = sorted(set(range(32, 127)) | {ord(c) for c in strings if 127 < ord(c) <= 0xffff})
    data = bytearray(16)
    for slot, size in enumerate((10, 12, 16, 24)):
        chars = codes if size!=24 else sorted(set(range(32, 127)) | {ord(c) for c in "秘宝冒险镇"})
        face = ImageFont.truetype(str(path), size, index=index)
        ascent, descent = face.getmetrics()
        baseline = (size + ascent - descent) // 2
        struct.pack_into("<I", data, slot * 4, len(data))
        data.extend(struct.pack("<H", len(chars)))
        entries, pixels = bytearray(), bytearray()
        for code in chars:
            char = chr(code)
            mask, (left, top) = face.getmask2(char, mode="L", anchor="ls")
            bitmap = Image.frombytes("L", mask.size, bytes(mask)) if mask.size[0] and mask.size[1] else None
            bounds = bitmap.getbbox() if bitmap else None
            width, height = 0, 0
            alpha = b""
            if bounds:
                bitmap = bitmap.crop(bounds)
                width, height = bitmap.size
                left += bounds[0]
                top += bounds[1]
                alpha = bitmap.tobytes()
            advance = max(3, round(face.getlength(char))) + 1
            entries.extend(struct.pack("<HBbbBBI", code, advance, left, top+baseline,
                                       width, height, len(pixels)))
            for y in range(height):
                row = alpha[y*width:(y+1)*width]
                # Light strokes remain visible when reduced to four bits.
                levels = [max(1, round(15*(value/255)**.85)) if value else 0 for value in row]
                levels += [0] * (width % 2)
                pixels.extend((a << 4) | b for a,b in zip(levels[::2], levels[1::2]))
        data.extend(entries)
        data.extend(pixels)
    (ROOT / "art" / "font.pxi").write_bytes(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cover", type=Path)
    parser.add_argument("--launcher-icon", type=Path,
                        help="Use a separately authored square launcher icon")
    parser.add_argument("--buildings", type=Path)
    parser.add_argument("--terrain", type=Path)
    parser.add_argument("--materials", type=Path)
    parser.add_argument("--plants", type=Path)
    parser.add_argument("--edges", type=Path)
    parser.add_argument("--village-props", type=Path)
    parser.add_argument("--flora", type=Path)
    parser.add_argument("--landmarks", type=Path)
    parser.add_argument("--palette", type=Path)
    parser.add_argument("--actors", type=Path, nargs=4)
    parser.add_argument("--walk-frames", type=Path, nargs=4)
    parser.add_argument("--opposite-steps", type=Path, nargs=4)
    parser.add_argument("--combat-frames", type=Path, nargs=2)
    parser.add_argument("--native-adventurers", type=Path,
                        help="Import finished actors.png, walking.png and combat.png atlases at 1:1")
    parser.add_argument("--native-scenes", type=Path,
                        help="Import finished monsters.png, waterfall.png and glints.png atlases at 1:1")
    parser.add_argument("--native-items", type=Path,
                        help="Import items.png from a directory as 16 native 18 × 18 icons")
    parser.add_argument("--native-outfits", type=Path,
                        help="Import the modular body, held weapon and worn gear atlases at 1:1")
    parser.add_argument("--monsters", type=Path, nargs=2)
    parser.add_argument("--ui", type=Path)
    parser.add_argument("--book-ui", type=Path)
    parser.add_argument("--folio", type=Path)
    parser.add_argument("--catalog-tabs", type=Path)
    parser.add_argument("--icons", type=Path)
    parser.add_argument("--markers", type=Path, help="Original UI sheet containing the two ground markers")
    parser.add_argument("--expedition", type=Path)
    parser.add_argument("--effects", type=Path)
    parser.add_argument("--waterfall", type=Path)
    parser.add_argument("--river-glints", type=Path)
    parser.add_argument("--relics", type=Path)
    parser.add_argument("--caves", type=Path)
    parser.add_argument("--font", type=Path, default=Path("/usr/share/fonts/noto-cjk/NotoSansCJK-Medium.ttc"))
    parser.add_argument("--face", type=int, default=2)
    args = parser.parse_args()
    if args.native_adventurers and (args.actors or args.walk_frames or args.opposite_steps or args.combat_frames):
        parser.error("--native-adventurers cannot be combined with generated-sheet animation inputs")
    if args.native_scenes and (args.monsters or args.waterfall or args.river_glints):
        parser.error("--native-scenes cannot be combined with generated-sheet inputs for the same scenes")
    if args.palette:
        keyframe = Image.open(args.palette).convert("RGB")
        keyframe.thumbnail((384, 256), Image.Resampling.LANCZOS)
        WORLD_PALETTE = keyframe.quantize(colors=118, method=Image.Quantize.MEDIANCUT,
                                          dither=Image.Dither.NONE)
        palette = WORLD_PALETTE.getpalette()[:118*3]
        WORLD_PALETTE.putpalette(palette + palette[-3:] * (256-118))
    if args.cover:
        title(args.cover)
    if args.launcher_icon:
        with Image.open(args.launcher_icon) as icon:
            if icon.size != (64, 64):
                parser.error("--launcher-icon requires a 64 × 64 pixel PNG")
            icon.convert("RGBA").save(ASSETS / "icon.png", optimize=True)
    if (args.buildings or args.plants) and not args.landmarks:
        parser.error("--buildings and --plants require --landmarks")
    landmarks = list(isolated(args.landmarks, 4, 2)) if args.landmarks else []
    if args.buildings:
        pack_buildings(args.buildings, landmarks)
    if args.terrain:
        if not args.materials:
            parser.error("--terrain requires --materials")
        terrain(args.terrain, args.materials)
    if args.plants:
        if not args.edges or not args.village_props or not args.flora:
            parser.error("--plants requires --edges, --village-props and --flora")
        pack_plants(args.plants, args.edges, args.village_props, args.flora, landmarks)
    if args.actors:
        frames = list(aligned_frames(args.actors, 4, (17, 20)))
        sheet("actors", frames[::4], (20, 22), 1)
    if args.walk_frames:
        if not args.opposite_steps:
            parser.error("--walk-frames requires --opposite-steps")
        sheet("walking", walking_frames(args.walk_frames,args.opposite_steps), (20, 22), 4, colors=148)
    if args.combat_frames:
        sheet("combat", aligned_frames(args.combat_frames, 4, (25, 24), 214), (26, 24), 4, colors=148)
    if args.native_adventurers:
        pack_native_atlases(args.native_adventurers, {
            "actors": (20, 16 * 22),
            "walking": (4 * 20, 16 * 22),
            "combat": (4 * 26, 8 * 24),
        })
    if args.monsters:
        frames = list(aligned_frames(args.monsters, 3, (53, 54), 145))
        order = (0, 3, 1, 2, 3)
        sheet("monsters", (frames[row+pose] for row in range(0,len(frames),4) for pose in order), (54, 54), 5)
    if args.native_scenes:
        pack_native_atlases(args.native_scenes, {
            "monsters": (5 * 54, 6 * 54),
            "waterfall": (4 * 30, 34),
            "glints": (4 * 22, 10),
        })
    if args.native_items:
        pack_native_atlases(args.native_items, {"items": (4 * 18, 4 * 18)})
    if args.native_outfits:
        pack_native_atlases(args.native_outfits, {
            "combat_body": (4 * 26, 8 * 24),
            "held_weapons": (4 * 16, 4 * 16),
            "worn_gear": (4 * 8, 3 * 7 + 10),
        })
    if args.ui:
        if not args.book_ui or not args.folio or not args.catalog_tabs:
            parser.error("--ui requires --book-ui, --folio and --catalog-tabs")
        pack_ui(args.ui, args.book_ui, args.folio, args.catalog_tabs)
    if args.icons:
        pack_icons(args.icons)
    if args.markers:
        pack_markers(args.markers)
    if args.expedition:
        pack_expedition(args.expedition)
    if args.effects:
        pack_effects(args.effects)
    if args.waterfall:
        pack_waterfall(args.waterfall)
    if args.river_glints:
        sheet("glints", aligned_frames([args.river_glints], 1, (20, 7), 185),
              (22, 10), 4, center=True, colors=48)
    if args.relics:
        sheet("relics", (sized(i, (26, 26)) for i in isolated(args.relics, 3, 2)), (28, 28), 3, center=True)
    if args.caves:
        sheet("caves", (rectangle(i, (320, 116)) for i in grid(args.caves, 2, 3, 4)), (320, 116), 2)
    font(args.font, args.face)
