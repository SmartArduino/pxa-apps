#ifndef PD_WALL_TILES_H
#define PD_WALL_TILES_H

#include "dungeon.h"

static inline int pd_wall_stitchable(uint8_t tile) {
    return pd_tile_wall(tile) || PD_TILE_KIND(tile) == PD_TILEK_VOID;
}

static inline int pd_wall_exposed(const pd_level_t *level, int x, int y) {
    for (int offset_y = -1; offset_y <= 1; ++offset_y) {
        for (int offset_x = -1; offset_x <= 1; ++offset_x) {
            const int adjacent_x = x + offset_x;
            const int adjacent_y = y + offset_y;
            uint8_t adjacent;
            if (!pd_in_bounds(adjacent_x, adjacent_y) ||
                !level->explored[adjacent_y * PD_MAP_W + adjacent_x]) continue;
            adjacent = pd_tile_at(level, adjacent_x, adjacent_y);
            if (!pd_tile_wall(adjacent) &&
                PD_TILE_KIND(adjacent) != PD_TILEK_VOID) return 1;
        }
    }
    return 0;
}

static inline int pd_wall_face(const pd_level_t *level, int x, int y) {
    const uint8_t tile = pd_tile_at(level, x, y);
    uint8_t below;
    int base;
    int variant;
    if (!pd_tile_wall(tile) || y + 1 >= PD_MAP_H) return -1;
    below = pd_tile_at(level, x, y + 1);
    if (pd_wall_stitchable(below)) return -1;
    if (pd_tile_door(below)) base = PD_WALLK_FACE_DOOR;
    else if (PD_TILE_KIND(tile) == PD_TILEK_WALL_DECO)
        base = PD_WALLK_FACE_DECO;
    else {
        const uint32_t hash = (uint32_t)x * UINT32_C(73856093) ^
                              (uint32_t)y * UINT32_C(19349663) ^
                              (uint32_t)level->depth * UINT32_C(83492791);
        base = (hash >> 16) & 1u ? PD_WALLK_FACE_ALT : PD_WALLK_FACE;
    }
    variant = !pd_wall_stitchable(pd_tile_at(level, x + 1, y)) +
              2 * !pd_wall_stitchable(pd_tile_at(level, x - 1, y));
    return PD_WALL(level->theme, base + variant);
}

static inline int pd_wall_upper(const pd_level_t *level, int x, int y) {
    const uint8_t tile = pd_tile_at(level, x, y);
    const uint8_t below = pd_tile_at(level, x, y + 1);
    int variant;
    int base;
    if (y + 1 >= PD_MAP_H || PD_TILE_KIND(tile) == PD_TILEK_VOID)
        return -1;
    if (pd_tile_wall(tile)) {
        if (pd_wall_stitchable(below)) {
            variant = !pd_wall_stitchable(pd_tile_at(level, x + 1, y)) +
                      2 * !pd_wall_stitchable(pd_tile_at(level, x + 1, y + 1)) +
                      4 * !pd_wall_stitchable(pd_tile_at(level, x - 1, y + 1)) +
                      8 * !pd_wall_stitchable(pd_tile_at(level, x - 1, y));
            return PD_WALL(level->theme, PD_WALLK_INTERNAL + variant);
        }
        return PD_TILE_KIND(below) == PD_TILEK_DOOR ?
               PD_WALL(level->theme, PD_WALLK_DOOR_SIDEWAYS) : -1;
    }
    if (pd_tile_wall(below)) {
        variant = !pd_wall_stitchable(pd_tile_at(level, x + 1, y + 1)) +
                  2 * !pd_wall_stitchable(pd_tile_at(level, x - 1, y + 1));
        base = PD_WALLK_OVERHANG;
        if (PD_TILE_KIND(tile) == PD_TILEK_DOOR)
            base = PD_WALLK_SIDE_OVERHANG_CLOSED;
        if (PD_TILE_KIND(tile) == PD_TILEK_DOOR_OPEN)
            base = PD_WALLK_SIDE_OVERHANG_OPEN;
        return PD_WALL(level->theme, base + variant);
    }
    if (PD_TILE_KIND(below) == PD_TILEK_DOOR)
        return PD_WALL(level->theme, PD_WALLK_DOOR_OVERHANG);
    if (PD_TILE_KIND(below) == PD_TILEK_DOOR_OPEN)
        return PD_WALL(level->theme, PD_WALLK_DOOR_OVERHANG_OPEN);
    return -1;
}

#endif
