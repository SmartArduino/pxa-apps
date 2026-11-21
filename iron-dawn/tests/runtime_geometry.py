# SPDX-License-Identifier: MIT
"""Touch coordinates for official simulator journeys."""
import math
from types import SimpleNamespace


def column_center(width, index, count):
    space = width - 10
    left = 8 + space * index // count
    right = 2 + space * (index + 1) // count
    return (left + right) / 2


def pause_save_center(width, height, physical_width, physical_height, menu_row=32, dpi=160):
    if physical_width >= 400 and dpi < 200:
        return (height - 248) // 2 + 216

    density = dpi / 160
    text_scale = max(1, density * width / physical_width) if dpi >= 200 else 1

    def text_height(size):
        pixels = math.floor(size * physical_width / width + .5)
        strike = max(1, (math.floor(pixels * text_scale + .5) + 6) // 12)
        return math.ceil(strike * 12 * height / physical_height)

    title = min(text_height(26), height // 6)
    caption = min(text_height(13), height // 9)
    target = menu_row if dpi >= 200 else min(math.ceil(32 * height / physical_height), height // 6)
    row = min(target, max(24, (height - title - caption - 96) // 4))
    modal_height = title + caption + 4 * row + 80
    return (height + modal_height) // 2 - 20 - row / 2


def compact_layout(width, height, physical_height, dpi=160):
    density = dpi / 160
    readable = density >= 1.25

    def logical(pixels):
        return math.ceil(pixels * height / physical_height)

    glyph = 12 * max(1, int(12 * density + 6) // 12)
    line = min(max(logical(glyph + 4 if readable else 14), 20), max(20, height // 11))
    row = min(max(line + 8, logical(max(36, 30 * density))), height // 5)
    top = height - row - 10
    status = min(max(2 * line + 8, 50), height // 4) if readable else 50
    header = line + 8
    budget = top - status - 18
    tactic_row = min(max(24, logical(max(32, 24 * density))), max(24, min(row, (budget - header - 18) // 2)))
    picture = min(max(24, logical(26 * density if readable else 22)), max(24, budget - header - 24 - 2 * line))
    card = 12 + picture + 2 * line
    return SimpleNamespace(
        line=line, row=row, tactic_row=tactic_row, card=card, header=header,
        top=top, status=status, production=top-header-12-card,
        tactics=top-header-18-2*tactic_row,
        map_size=min(max(min(width, height)//5, 80), 112),
    )


def wide_layout(width, height, world_width, top, bottom, dp):
    line = 12 * max(1, int(12 * dp + 6) // 12) + 4
    row = height - bottom - 10
    side = width - world_width
    map_budget = height - top - 2 * row - 56 - 3 * (2 * line + 4)
    map_width = side - max(68, 3*(line-4)+12) - 32
    map_size = min(160, max(32, min(max(height//5, round(96*dp)), map_budget, map_width)))
    tabs = top + max(map_size, row) + 20
    grid = tabs + row + 8
    footer = height - row - 8
    card = min((footer-8-grid-12)//3, 2*line + min(max(height//14, 24), 56) + 12)
    return SimpleNamespace(row=row, side=side, map_size=map_size, tabs=tabs, grid=grid, footer=footer, card=card)
