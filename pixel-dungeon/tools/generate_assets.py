#!/usr/bin/env python3
"""Generate the Pixel Dungeon INDEX8 atlases and palette from Shattered Pixel
Dungeon's original art.

The upstream assets are not vendored here. Fetch them first with
`tools/fetch_spd_assets.sh`, then run:

    python3 tools/generate_assets.py --spd-assets /path/to/shattered-pixel-dungeon

Outputs (checked in):
    assets.h   tile/sprite ids and atlas geometry
    assets.c   terrain, stitched-wall, sprite and UI atlases with RGB565 palette

Every tile is drawn from one of SPD's five region tilesets, so the theme a
floor uses follows the original game's sewers -> prison -> caves -> city ->
halls progression. Remembered terrain is darkened at runtime with a fixed-alpha
painter quad instead of a second texture, which keeps the full palette for the
art itself.
"""
import argparse
import os
import sys
from pathlib import Path

from PIL import Image

APP = Path(__file__).resolve().parents[1]
CELL = 16
CELLS_PER_ROW = 16
ATLAS_W = CELL * CELLS_PER_ROW  # 256
ATLAS_H = 128                   # 8 rows of cells
WALL_ATLAS_H = 256
TILE_ATLAS_H = 256
SPRITE_ATLAS_H = 256

TILE_COLORS = 191
SPRITE_COLORS = 62
TRANSPARENT_INDEX = 0
SEARCH_INDEX = 254
FOG_INDEX = 255
SEARCH_COLOR = (85, 170, 255)
FOG_COLOR = (12, 12, 22)

THEMES = ['sewers', 'prison', 'caves', 'city', 'halls']

# Visual cells from SPD's DungeonTileSheet.java, not Terrain.java identifiers.
T_VOID = 24
T_FLOOR = 0
T_FLOOR_ALT = 6
T_WALL = 80
T_WALL_DECO = 84
T_WALL_DOOR = 88
T_DOOR = 112
T_DOOR_OPEN = 113
T_STAIRS_UP = 16
T_STAIRS_DOWN = 17
T_GRASS = 2
T_HIGH_GRASS = 122
T_EMBERS = 3
T_DECO = 1
T_DECO_ALT = 7
T_ALCHEMY = 64
T_STATUE = 72
T_TRAP = 3

# Items sheet cells (SPD ItemSpriteSheet, xy() is 1-based).
ITEM_CELLS = {
    'GOLD': (2, 1),
    'IRON_KEY': (7, 3),
    'CHEST': (4, 2),
    'AMULET': (13, 3),
    'DAGGER': (4, 6),
    'SHORT_SWORD': (8, 6),
    'SWORD': (0, 7),
    'GREAT_SWORD': (0, 8),
    'CLOTH': (0, 11),
    'LEATHER': (1, 11),
    'MAIL': (2, 11),
    'PLATE': (4, 11),
    'SCROLL_UPGRADE': (0, 19),
    'SCROLL_MAP': (1, 19),
    'POTION_HEAL': (0, 22),
    'POTION_STRENGTH': (3, 22),
    'RATION': (5, 27),
}

# Film frame dimensions and indices from SPD's HeroSprite and MobSprite
# subclasses. Every frame is normalized into a 16x16 atlas cell.
MOB_LAYOUT = {
    'RAT': (16, 15, (0, 1, 6, 7, 2, 11, 13)),
    'GNOLL': (12, 15, (0, 1, 4, 5, 2, 8, 10)),
    'CRAB': (16, 16, (0, 1, 3, 4, 7, 10, 13)),
    'SKELETON': (12, 15, (0, 1, 4, 5, 14, 10, 13)),
    'BAT': (15, 15, (0, 1, 0, 1, 2, 4, 6)),
    'SNAKE': (12, 11, (0, 1, 4, 5, 8, 11, 13)),
    'SPIDER': (16, 16, (0, 1, 2, 3, 4, 6, 9)),
    'SLIME': (14, 12, (0, 1, 2, 3, 4, 5, 7)),
    'GOLEM': (17, 19, (0, 1, 2, 3, 6, 9, 13)),
    'YOG': (20, 19, (0, 1, 0, 1, 0, 7, 9)),
}
MOBS = [
    ('RAT', 'rat'),
    ('GNOLL', 'gnoll'),
    ('CRAB', 'crab'),
    ('SKELETON', 'skeleton'),
    ('BAT', 'bat'),
    ('SNAKE', 'snake'),
    ('SPIDER', 'spinner'),
    ('SLIME', 'slime'),
    ('GOLEM', 'golem'),
    ('YOG', 'yog'),
]
HERO_FRAMES = (0, 1, 2, 4, 6)
HERO_TIERS = 5
HERO_CLASSES = (('WARRIOR', 'warrior'), ('ROGUE', 'rogue'), ('MAGE', 'mage'))

TILE_ORDER = [
    'VOID', 'FLOOR', 'FLOOR_ALT', 'WALL', 'WALL_DECO', 'DOOR', 'DOOR_OPEN',
    'STAIRS_UP', 'STAIRS_DOWN', 'GRASS', 'HIGH_GRASS',
    'EMBERS_A', 'EMBERS_B', 'WATER_A', 'WATER_B', 'WATER_C', 'WATER_D',
    'DECO', 'DECO_ALT', 'ALCHEMY', 'STATUE', 'TRAP', 'CHEST',
    'WALL_DOOR',
] + [
    'WATER_SHORE_%d' % index for index in range(16)
] + [
    'GRASS_ALT', 'HIGH_GRASS_ALT',
    'HIGH_GRASS_OVERHANG', 'HIGH_GRASS_OVERHANG_ALT',
]

WALL_GROUPS = [
    ('FACE', 80, 4), ('FACE_DECO', 84, 4),
    ('FACE_DOOR', 88, 4), ('INTERNAL', 144, 16),
    ('OVERHANG', 192, 4), ('SIDE_OVERHANG_OPEN', 208, 4),
    ('SIDE_OVERHANG_CLOSED', 212, 4),
    ('DOOR_OVERHANG', 224, 1), ('DOOR_OVERHANG_OPEN', 225, 1),
    ('DOOR_SIDEWAYS', 227, 1), ('DOOR_FLOOR_SIDEWAYS', 116, 1),
    ('FACE_ALT', 96, 4),
]
WALLS_PER_THEME = sum(count for _, _, count in WALL_GROUPS)


def cell(image, x, y):
    return image.crop((x * CELL, y * CELL, x * CELL + CELL, y * CELL + CELL))


def film_frame(image, index, width, height):
    columns = image.width // width
    frame = image.crop(((index % columns) * width,
                        (index // columns) * height,
                        (index % columns + 1) * width,
                        (index // columns + 1) * height))
    if width > CELL or height > CELL:
        scale = min(CELL / width, CELL / height)
        frame = frame.resize((round(width * scale), round(height * scale)),
                             Image.Resampling.NEAREST)
    normalized = Image.new('RGBA', (CELL, CELL))
    normalized.alpha_composite(frame,
                               ((CELL - frame.width) // 2, CELL - frame.height))
    return normalized


def cell_id(image, tile_id):
    return cell(image, tile_id % 16, tile_id // 16)


def over(base, overlay):
    out = base.copy()
    out.alpha_composite(overlay)
    return out


def load_tiles(spd, theme, stage):
    tileset = Image.open(spd / ('environment/tiles_%s.png' % theme)).convert('RGBA')
    water = Image.open(spd / ('environment/water%d.png' % stage)).convert('RGBA')
    floor = cell_id(tileset, T_FLOOR)
    tiles = {
        'VOID': cell_id(tileset, T_VOID),
        'FLOOR': floor,
        'FLOOR_ALT': cell_id(tileset, T_FLOOR_ALT),
        'WALL': cell_id(tileset, T_WALL),
        'WALL_DECO': cell_id(tileset, T_WALL_DECO),
        'WALL_DOOR': cell_id(tileset, T_WALL_DOOR),
        'DOOR': cell_id(tileset, T_DOOR),
        'DOOR_OPEN': cell_id(tileset, T_DOOR_OPEN),
        'STAIRS_UP': cell_id(tileset, T_STAIRS_UP),
        'STAIRS_DOWN': cell_id(tileset, T_STAIRS_DOWN),
        'GRASS': cell_id(tileset, T_GRASS),
        'GRASS_ALT': cell_id(tileset, 8),
        'HIGH_GRASS': over(floor, cell_id(tileset, T_HIGH_GRASS)),
        'HIGH_GRASS_ALT': over(floor, cell_id(tileset, 125)),
        'HIGH_GRASS_OVERHANG': cell_id(tileset, 234),
        'HIGH_GRASS_OVERHANG_ALT': cell_id(tileset, 237),
        'EMBERS_A': cell_id(tileset, T_EMBERS),
        'EMBERS_B': cell_id(tileset, T_EMBERS),
        'DECO': cell_id(tileset, T_DECO),
        'DECO_ALT': cell_id(tileset, T_DECO_ALT),
        'ALCHEMY': over(floor, cell_id(tileset, T_ALCHEMY)),
        'STATUE': over(floor, cell_id(tileset, T_STATUE)),
        'TRAP': cell_id(tileset, T_TRAP),
        'CHEST': None,  # filled in once the item sheet is loaded
    }
    for index, suffix in enumerate(('A', 'B', 'C', 'D')):
        tiles['WATER_' + suffix] = over(
            floor, cell(water, index % 2, index // 2))
    for index in range(16):
        tiles['WATER_SHORE_%d' % index] = cell_id(tileset, 32 + index)
    return tiles


UI_ATLAS_W = 256
UI_ATLAS_H = 192
UI_ATLAS_ROW = 64
# Slices of Shattered Pixel Dungeon's own interface sheets, packed into one
# 256x32 strip: (name, source file, x, y, width, height).
UI_SLICES = [
    ('STATUS_PANE', 'interfaces/status_pane.png', 0, 0, 82, 38),
    ('HP_BAR', 'interfaces/status_pane.png', 0, 40, 50, 4),
    ('EXP_BAR', 'interfaces/status_pane.png', 0, 48, 17, 4),
    ('TOOLBAR_SLOT', 'interfaces/toolbar.png', 0, 0, 24, 26),
    ('TOOLBAR_BAG', 'interfaces/toolbar.png', 160, 0, 16, 16),
    ('TOOLBAR_WAIT', 'interfaces/toolbar.png', 176, 0, 16, 16),
    ('TOOLBAR_SEARCH', 'interfaces/toolbar.png', 192, 0, 16, 16),
    ('AVATAR_WARRIOR', 'sprites/avatars.png', 0, 0, 24, 32),
    ('AVATAR_ROGUE', 'sprites/avatars.png', 48, 0, 24, 32),
    ('AVATAR_MAGE', 'sprites/avatars.png', 24, 0, 24, 32),
    ('WINDOW', 'interfaces/chrome.png', 0, 0, 20, 20),
    ('BUTTON', 'interfaces/chrome.png', 20, 9, 9, 9),
    ('PREFS', 'interfaces/icons.png', 102, 0, 14, 14),
    ('BANNER_GAME_OVER', 'interfaces/banners.png', 128, 157, 128, 35),
    ('BANNER_TITLE', 'interfaces/banners.png', 0, 100, 240, 57),
    ('MENU_PANE', 'interfaces/menu_pane.png', 1, 0, 31, 21),
    ('MENU_BUTTON', 'interfaces/menu_button.png', 17, 2, 12, 11),
    ('MENU_JOURNAL', 'interfaces/menu_button.png', 2, 2, 13, 11),
    ('MENU_JOURNAL_ICON', 'interfaces/menu_button.png', 31, 0, 11, 6),
    ('RED_BUTTON', 'interfaces/chrome.png', 38, 0, 6, 6),
    ('STAIRS_ICON', 'interfaces/icons.png', 0, 64, 15, 16),
    ('DISPLAY_ICON', 'interfaces/icons.png', 16, 16, 12, 16),
    ('DEPTH_ICON', 'interfaces/icons.png', 32, 80, 6, 7),
    ('EXIT', 'interfaces/icons.png', 0, 16, 15, 11),
    ('CLOSE', 'interfaces/icons.png', 80, 32, 11, 11),
    ('TITLE_ENTER', 'interfaces/icons.png', 0, 0, 16, 16),
    ('TITLE_RANKINGS', 'interfaces/icons.png', 34, 0, 17, 16),
    ('TITLE_JOURNAL', 'interfaces/icons.png', 136, 0, 17, 15),
]


def build_ui_atlas(spd, lookup):
    """Packs the interface slices into one INDEX8 strip."""
    atlas = bytearray(UI_ATLAS_W * UI_ATLAS_H)
    offsets = {}
    cursor = 0
    row = 0
    shelf_y = 0
    cache = {}
    for name, source, x, y, width, height in UI_SLICES:
        if name == 'MENU_PANE':
            cursor = 0
            row = 0
            shelf_y = 40
        if source not in cache:
            cache[source] = Image.open(spd / source).convert('RGBA')
        image = cache[source].crop((x, y, x + width, y + height))
        if cursor + width > UI_ATLAS_W:
            row += 1
            cursor = 0
        if row * UI_ATLAS_ROW + shelf_y + height > UI_ATLAS_H:
            raise SystemExit('UI atlas overflow at %s' % name)
        offsets[name] = (cursor, row * UI_ATLAS_ROW + shelf_y, width, height)
        for y in range(height):
            for column in range(width):
                r, g, b, a = image.getpixel((column, y))
                value = 0 if a < 128 else nearest(lookup, (r, g, b))
                atlas[(row * UI_ATLAS_ROW + shelf_y + y) * UI_ATLAS_W + cursor + column] = value
        cursor += width
    return atlas, offsets


def build_title_background(spd, lookup):
    arches = Image.open(spd / 'splashes/title/archs.png').convert('RGBA')
    image = Image.new('RGBA', (256, 128), (12, 10, 8, 255))
    image.alpha_composite(arches.crop((0, 0, 740, 200)).resize(
        (256, 128), Image.Resampling.BILINEAR))
    atlas = bytearray(256 * 128)
    for offset, (red, green, blue, _) in enumerate(image.get_flattened_data()):
        atlas[offset] = nearest(lookup, (red, green, blue))
    return atlas


def build_title_fire(spd, lookup):
    image = Image.open(spd / 'effects/fireball-short.png').convert('RGBA')
    atlas = bytearray(192 * 72)
    for frame in range(24):
        source = image.crop(((frame % 5) * 47, (frame // 5) * 47,
                             (frame % 5 + 1) * 47, (frame // 5 + 1) * 47))
        source = source.resize((24, 24), Image.Resampling.NEAREST)
        left = frame % 8 * 24
        top = frame // 8 * 24
        for row in range(24):
            for column in range(24):
                red, green, blue, alpha = source.getpixel((column, row))
                if alpha >= 80:
                    atlas[(top + row) * 192 + left + column] = nearest(
                        lookup, (red, green, blue))
    return atlas


def load_sprites(spd):
    """Returns an ordered list of (name, RGBA image)."""
    out = []
    for label, sheet in HERO_CLASSES:
        hero = Image.open(spd / ('sprites/%s.png' % sheet)).convert('RGBA')
        for tier in range(HERO_TIERS):
            for index in HERO_FRAMES:
                out.append(('HERO_' + label,
                            film_frame(hero, tier * (hero.width // 12) + index,
                                       12, 15)))
            for index in HERO_FRAMES:
                out.append(('HERO_' + label + '_LEFT',
                            film_frame(hero, tier * (hero.width // 12) + index,
                                       12, 15)
                            .transpose(Image.Transpose.FLIP_LEFT_RIGHT)))
    for label, sheet in MOBS:
        image = Image.open(spd / ('sprites/%s.png' % sheet)).convert('RGBA')
        width, height, frames = MOB_LAYOUT[label]
        for position, index in enumerate(frames):
            animation = ('IDLE', 'IDLE', 'RUN', 'RUN', 'ATTACK', 'DIE', 'DIE')[position]
            out.append(('%s_%s' % (label, animation),
                        film_frame(image, index, width, height)))
    items = Image.open(spd / 'sprites/items.png').convert('RGBA')
    for name, (x, y) in ITEM_CELLS.items():
        out.append(('ITEM_' + name, cell(items, x, y)))
    return out


def build_palette(tile_images, sprite_images):
    """Quantizes tiles and sprites into one 256-entry palette. Index 0 is the
    transparency key and stays black, tiles own 1..191, sprites own 192..253."""
    def flatten(images):
        width = CELL * len(images)
        sheet = Image.new('RGB', (width, CELL), (0, 0, 0))
        for index, image in enumerate(images):
            sheet.paste(image, (index * CELL, 0), image)
        return sheet

    tiles = flatten(tile_images)
    sprites = flatten(sprite_images)
    tile_pal = tiles.quantize(colors=TILE_COLORS, method=Image.MEDIANCUT,
                              dither=Image.NONE)
    sprite_pal = sprites.quantize(colors=SPRITE_COLORS,
                                  method=Image.MEDIANCUT, dither=Image.NONE)
    palette = [(0, 0, 0)]
    tile_lookup = {}
    for index in range(TILE_COLORS):
        color = tuple(tile_pal.getpalette()[index * 3:index * 3 + 3])
        palette.append(color)
        tile_lookup[color] = 1 + index
    sprite_lookup = {}
    for index in range(SPRITE_COLORS):
        color = tuple(sprite_pal.getpalette()[index * 3:index * 3 + 3])
        palette.append(color)
        sprite_lookup[color] = 1 + TILE_COLORS + index
    palette.append(SEARCH_COLOR)
    palette.append(FOG_COLOR)
    assert len(palette) == 256
    return palette, tile_lookup, sprite_lookup


def to_index8(image, lookup, fallback, tile_mode):
    """Maps an RGBA cell onto palette indices; index 0 stays transparent."""
    pixels = image.load()
    out = bytearray(CELL * CELL)
    for y in range(CELL):
        for x in range(CELL):
            r, g, b, a = pixels[x, y]
            if a < 128:
                out[y * CELL + x] = 0 if not tile_mode else fallback
                continue
            index = lookup.get((r, g, b))
            if index is None:
                index = nearest(lookup, (r, g, b))
            out[y * CELL + x] = index if index != 0 else fallback
    return out


def nearest(lookup, color):
    best = 0
    best_distance = None
    for candidate, index in lookup.items():
        distance = (candidate[0] - color[0]) ** 2 + (candidate[1] - color[1]) ** 2 + \
                   (candidate[2] - color[2]) ** 2
        if best_distance is None or distance < best_distance:
            best_distance = distance
            best = index
    return best


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def build_atlas(entries, lookup, fallback, tile_mode, height=ATLAS_H):
    atlas = bytearray(ATLAS_W * height)
    for slot, (_, image) in enumerate(entries):
        if slot >= CELLS_PER_ROW * (height // CELL):
            raise SystemExit('atlas overflow at %s' % entries[slot][0])
        u = (slot % CELLS_PER_ROW) * CELL
        v = (slot // CELLS_PER_ROW) * CELL
        cell_bytes = to_index8(image, lookup, fallback, tile_mode)
        for y in range(CELL):
            row = (v + y) * ATLAS_W + u
            atlas[row:row + CELL] = cell_bytes[y * CELL:(y + 1) * CELL]
    return atlas


HEADER = """/* Generated by tools/generate_assets.py from Shattered Pixel Dungeon art.
 * Shattered Pixel Dungeon (C) 2014-2026 Evan Debenham, GPL-3.0. */
#ifndef PD_ASSETS_H
#define PD_ASSETS_H

#include <stdint.h>

#define PD_ATLAS_WIDTH {atlas_w}
#define PD_ATLAS_HEIGHT {atlas_h}
#define PD_TILE_ATLAS_HEIGHT {tile_atlas_h}
#define PD_SPRITE_ATLAS_HEIGHT {sprite_atlas_h}
#define PD_CELL {cell}
#define PD_CELLS_PER_ROW {cells_per_row}
#define PD_THEME_COUNT {themes}
#define PD_TILES_PER_THEME {tiles_per_theme}
#define PD_WALL_ATLAS_HEIGHT {wall_atlas_h}
#define PD_WALLS_PER_THEME {walls_per_theme}
#define PD_TILE(theme, kind) ((uint8_t)((theme) * PD_TILES_PER_THEME + (kind)))
#define PD_TILE_KIND(tile) ((uint8_t)((tile) % PD_TILES_PER_THEME))
#define PD_WALL(theme, kind) ((uint8_t)((theme) * PD_WALLS_PER_THEME + (kind)))

/* Remembered terrain is the same tile with this palette colour blended over it
 * by a fixed-alpha painter quad. */
#define PD_SEARCH_INDEX {search_index}
#define PD_FOG_INDEX {fog_index}

/* The chasm tile is identical in every region, so one alias is enough. */
#define PD_TILE_VOID PD_TILE(0, PD_TILEK_VOID)

/* Sprite groups. Frames are ordered so the game can index them by direction,
 * mob type or animation without another table. */
#define PD_SPRITE_HERO_FRAMES {hero_frames}
#define PD_SPRITE_HERO_TIERS {hero_tiers}
#define PD_SPRITE_HERO(hero_class, tier, frame) \
    (PD_SPRITE_HERO_WARRIOR_0 + ((hero_class) * PD_SPRITE_HERO_TIERS + (tier)) * PD_SPRITE_HERO_FRAMES * 2 + (frame))
#define PD_SPRITE_HERO_FACING(hero_class, tier, left, frame) \
    (PD_SPRITE_HERO(hero_class, tier, frame) + ((left) ? PD_SPRITE_HERO_FRAMES : 0))
#define PD_SPRITE_MOB_FRAMES 7
#define PD_SPRITE_MOB(type, frame) \
    (PD_SPRITE_RAT_IDLE_{first_mob_slot} + (type) * PD_SPRITE_MOB_FRAMES + (frame))
#define PD_MOB_FRAME_IDLE 0
#define PD_MOB_FRAME_RUN 2
#define PD_MOB_FRAME_ATTACK 4
#define PD_MOB_FRAME_DIE 5

enum {{
{tile_enum}
}};

enum {{
{wall_enum}
}};

enum {{
{sprite_enum}
}};

#define PD_UI_ATLAS_WIDTH {ui_atlas_w}
#define PD_UI_ATLAS_HEIGHT {ui_atlas_h}
#define PD_TITLE_ATLAS_WIDTH 256
#define PD_TITLE_ATLAS_HEIGHT 128
#define PD_FIRE_ATLAS_WIDTH 192
#define PD_FIRE_ATLAS_HEIGHT 72
#define PD_UI_SLICE_COUNT {ui_count}

/* Interface slices packed into the UI strip: source x, y, width, height. */
enum {{
{ui_enum}
}};

extern const uint8_t pd_tile_atlas[PD_ATLAS_WIDTH * PD_TILE_ATLAS_HEIGHT];
extern const uint8_t pd_wall_atlas[PD_ATLAS_WIDTH * PD_WALL_ATLAS_HEIGHT];
extern const uint8_t pd_sprite_atlas[PD_ATLAS_WIDTH * PD_SPRITE_ATLAS_HEIGHT];
extern const uint8_t pd_ui_atlas[PD_UI_ATLAS_WIDTH * PD_UI_ATLAS_HEIGHT];
extern const uint8_t pd_title_atlas[PD_TITLE_ATLAS_WIDTH * PD_TITLE_ATLAS_HEIGHT];
extern const uint8_t pd_fire_atlas[PD_FIRE_ATLAS_WIDTH * PD_FIRE_ATLAS_HEIGHT];
extern const uint16_t pd_ui_rect[PD_UI_SLICE_COUNT][4];
extern const uint16_t pd_palette[256];

#endif
"""


def emit_header(tile_count, sprite_count):
    tile_enum = '\n'.join(
        '    PD_TILEK_%s = %d,' % (name, slot)
        for slot, name in enumerate(TILE_ORDER))
    sprite_enum = []
    wall_enum = []
    offset = 0
    for name, _, count in WALL_GROUPS:
        wall_enum.append('    PD_WALLK_%s = %d,' % (name, offset))
        offset += count
    for slot, (name, _) in enumerate(SPRITES):
        if slot == 0 or SPRITES[slot - 1][0] != name:
            sprite_enum.append('    /* %s */' % name)
        if name.startswith('ITEM_'):
            sprite_enum.append('    PD_SPRITE_%s = %d,' % (name, slot))
        else:
            sprite_enum.append('    PD_SPRITE_%s_%d = %d,' % (name, slot, slot))
    return HEADER.format(
        atlas_w=ATLAS_W, atlas_h=ATLAS_H, cell=CELL,
        tile_atlas_h=TILE_ATLAS_H, sprite_atlas_h=SPRITE_ATLAS_H,
        hero_tiers=HERO_TIERS, hero_frames=len(HERO_FRAMES),
        first_mob_slot=len(HERO_CLASSES) * HERO_TIERS * len(HERO_FRAMES) * 2,
        cells_per_row=CELLS_PER_ROW, themes=len(THEMES),
        tiles_per_theme=len(TILE_ORDER), tile_enum=tile_enum,
        wall_atlas_h=WALL_ATLAS_H, walls_per_theme=WALLS_PER_THEME,
        wall_enum='\n'.join(wall_enum),
        search_index=SEARCH_INDEX, fog_index=FOG_INDEX,
        sprite_enum='\n'.join(sprite_enum),
        ui_atlas_w=UI_ATLAS_W, ui_atlas_h=UI_ATLAS_H,
        ui_count=len(UI_SLICES),
        ui_enum='\n'.join('    PD_UI_%s = %d,' % (name, index)
                          for index, (name, _, _, _, _, _) in
                          enumerate(UI_SLICES)))


def emit_source(tile_atlas, wall_atlas, sprite_atlas, ui_atlas, title_atlas,
                fire_atlas, palette):
    def array(name, data, width, height):
        lines = []
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            lines.append('    ' + ', '.join(str(value) for value in chunk) + ',')
        return 'const uint8_t %s[%s * %s] = {\n%s\n};\n' % (
            name, width, height, '\n'.join(lines))

    body = ['/* Generated by tools/generate_assets.py from Shattered Pixel Dungeon art.',
            ' * Shattered Pixel Dungeon (C) 2014-2026 Evan Debenham, GPL-3.0. */',
            '#include "assets.h"', '']
    body.append(array('pd_tile_atlas', tile_atlas, 'PD_ATLAS_WIDTH', 'PD_TILE_ATLAS_HEIGHT'))
    body.append(array('pd_wall_atlas', wall_atlas, 'PD_ATLAS_WIDTH', 'PD_WALL_ATLAS_HEIGHT'))
    body.append(array('pd_sprite_atlas', sprite_atlas, 'PD_ATLAS_WIDTH', 'PD_SPRITE_ATLAS_HEIGHT'))
    body.append(array('pd_ui_atlas', ui_atlas, 'PD_UI_ATLAS_WIDTH', 'PD_UI_ATLAS_HEIGHT'))
    body.append(array('pd_title_atlas', title_atlas, 'PD_TITLE_ATLAS_WIDTH', 'PD_TITLE_ATLAS_HEIGHT'))
    body.append(array('pd_fire_atlas', fire_atlas, 'PD_FIRE_ATLAS_WIDTH', 'PD_FIRE_ATLAS_HEIGHT'))
    body.append('const uint16_t pd_ui_rect[PD_UI_SLICE_COUNT][4] = {')
    for name, _, _, _, width, height in UI_SLICES:
        x, y, _, _ = ui_offsets[name]
        body.append('    {%d, %d, %d, %d}, /* %s */' % (x, y, width, height, name))
    body.append('};')
    body.append('const uint16_t pd_palette[256] = {')
    for offset in range(0, 256, 8):
        body.append('    ' + ', '.join(
            str(rgb565(*palette[index])) for index in range(offset, offset + 8)) + ',')
    body.append('};')
    return '\n'.join(body) + '\n'


def preview(tile_entries, sprite_entries, path):
    pal = palette_global
    levels = [build_atlas(tile_entries, tile_lookup_global, 1, True)]
    sprites = build_atlas(sprite_entries, sprite_lookup_global, 1, False)
    image = Image.new('RGB', (ATLAS_W, ATLAS_H * 2 + 4), (20, 20, 24))
    for name, atlas in (('tiles', levels[0]), ('sprites', sprites)):
        pixels = image.load()
        base = 0 if name == 'tiles' else ATLAS_H + 4
        for y in range(ATLAS_H):
            for x in range(ATLAS_W):
                value = pal[atlas[y * ATLAS_W + x]]
                pixels[x, base + y] = value
    image.resize((ATLAS_W * 3, (ATLAS_H * 2 + 4) * 3), Image.NEAREST).save(path)
    print('wrote %s' % path)


palette_global = []
tile_lookup_global = {}
sprite_lookup_global = {}
SPRITES = []


def main():
    global palette_global, tile_lookup_global, sprite_lookup_global, SPRITES
    parser = argparse.ArgumentParser()
    parser.add_argument('--spd-assets', default=os.environ.get('PXA_SPD_ASSETS'))
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--preview')
    args = parser.parse_args()
    if not args.spd_assets:
        raise SystemExit(
            'Shattered Pixel Dungeon assets are required. Run\n'
            '  tools/fetch_spd_assets.sh\n'
            'or pass --spd-assets /path/to/shattered-pixel-dungeon')
    spd = Path(args.spd_assets) / 'core/src/main/assets'
    if not (spd / 'environment/tiles_sewers.png').exists():
        raise SystemExit('not a Shattered Pixel Dungeon checkout: %s' % spd)

    item_sheet = Image.open(spd / 'sprites/items.png').convert('RGBA')
    chest = cell(item_sheet, *ITEM_CELLS['CHEST'])

    tiles = []
    walls = []
    for stage, theme in enumerate(THEMES):
        built = load_tiles(spd, theme, stage)
        tileset = Image.open(spd / ('environment/tiles_%s.png' % theme)).convert('RGBA')
        for name, first, count in WALL_GROUPS:
            for variant in range(count):
                walls.append(('%s_%s_%d' % (theme, name, variant),
                              cell_id(tileset, first + variant)))
        built['CHEST'] = over(built['FLOOR'], chest)
        for name in TILE_ORDER:
            overlay = name.startswith('WATER_SHORE_') or name.startswith('HIGH_GRASS_OVERHANG')
            tiles.append(('%s_%s' % (theme, name),
                          built[name] if overlay else over(built['FLOOR'], built[name])))

    SPRITES = load_sprites(spd)
    palette_global, tile_lookup_global, sprite_lookup_global = build_palette(
        [image for _, image in tiles + walls],
        [image for _, image in SPRITES])

    tile_atlas = build_atlas(tiles, tile_lookup_global, 1, False,
                             TILE_ATLAS_H)
    wall_atlas = build_atlas(walls, tile_lookup_global, 1, False,
                             WALL_ATLAS_H)
    sprite_atlas = build_atlas(SPRITES, sprite_lookup_global, 1, False,
                               SPRITE_ATLAS_H)
    artwork_lookup = {**tile_lookup_global, **sprite_lookup_global}
    ui_atlas, ui_offsets = build_ui_atlas(spd, artwork_lookup)
    title_atlas = build_title_background(spd, artwork_lookup)
    fire_atlas = build_title_fire(spd, artwork_lookup)
    globals()['ui_offsets'] = ui_offsets

    if args.preview:
        preview(tiles, SPRITES, args.preview)
        return

    header = emit_header(len(TILE_ORDER), len(SPRITES))
    source = emit_source(tile_atlas, wall_atlas, sprite_atlas, ui_atlas,
                         title_atlas, fire_atlas, palette_global)
    for name, content in (('assets.h', header), ('assets.c', source)):
        target = APP / name
        if args.check:
            if not target.exists() or target.read_text() != content:
                raise SystemExit('%s is out of date' % name)
            print('%s ok' % name)
        else:
            target.write_text(content)
            print('wrote %s' % target)
    print('tiles=%d walls=%d sprites=%d' % (len(tiles), len(walls), len(SPRITES)))


if __name__ == '__main__':
    main()
