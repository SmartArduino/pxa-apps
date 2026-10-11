/* Dungeon level: tiles, procedural generation, field of view and pathfinding.
 *
 * Generation is deterministic from (seed, depth) so a saved run can rebuild
 * the current floor and only the entity state has to be stored. */
#ifndef PD_DUNGEON_H
#define PD_DUNGEON_H

#include <stdint.h>

#include "assets.h"
#include "rng.h"

#define PD_MAP_W 40
#define PD_MAP_H 32
#define PD_MAP_TILES (PD_MAP_W * PD_MAP_H)
#define PD_ROOMS_MAX 10
#define PD_FOV_RADIUS 7

typedef struct {
    uint8_t x, y, w, h;
} pd_room_t;

typedef struct {
    uint8_t tiles[PD_MAP_TILES];
    uint8_t explored[PD_MAP_TILES];
    uint8_t visible[PD_MAP_TILES];
    uint8_t known[PD_MAP_TILES]; /* trap revealed by searching or triggering */
    uint8_t room_count;
    pd_room_t rooms[PD_ROOMS_MAX];
    uint8_t theme;
    uint8_t floor_tile;
    uint8_t wall_tile;
    uint8_t entrance_x, entrance_y;
    uint8_t exit_x, exit_y;
    uint8_t depth;
} pd_level_t;

/* Tile predicates -------------------------------------------------------
 * A tile value is PD_TILE(theme, kind); predicates work on the kind so they
 * stay independent of which region tileset the floor uses. */
static inline int pd_tile_wall(uint8_t tile) {
    const uint8_t kind = PD_TILE_KIND(tile);
    return kind == PD_TILEK_WALL || kind == PD_TILEK_WALL_DECO;
}

static inline int pd_tile_blocking(uint8_t tile) {
    const uint8_t kind = PD_TILE_KIND(tile);
    return pd_tile_wall(tile) || kind == PD_TILEK_VOID ||
           kind == PD_TILEK_CHEST || kind == PD_TILEK_ALCHEMY ||
           kind == PD_TILEK_STATUE;
}

/* Doors block sight until they are opened. */
static inline int pd_tile_opaque(uint8_t tile) {
    return pd_tile_blocking(tile) || PD_TILE_KIND(tile) == PD_TILEK_DOOR;
}

static inline int pd_tile_walkable(uint8_t tile) {
    return !pd_tile_blocking(tile);
}

static inline int pd_tile_stairs_down(uint8_t tile) {
    return PD_TILE_KIND(tile) == PD_TILEK_STAIRS_DOWN;
}

static inline int pd_tile_stairs_up(uint8_t tile) {
    return PD_TILE_KIND(tile) == PD_TILEK_STAIRS_UP;
}

static inline int pd_tile_door(uint8_t tile) {
    const uint8_t kind = PD_TILE_KIND(tile);
    return kind == PD_TILEK_DOOR || kind == PD_TILEK_DOOR_OPEN;
}

static inline int pd_tile_trap(uint8_t tile) {
    return PD_TILE_KIND(tile) == PD_TILEK_TRAP;
}

static inline int pd_trap_variant(uint32_t seed, uint8_t depth, int x, int y) {
    return (int)(pd_rng_mix(seed, (uint32_t)depth * 1031u +
                             (uint32_t)x * 163u + (uint32_t)y * 479u) & 1u);
}

static inline int pd_tile_water(uint8_t tile) {
    const uint8_t kind = PD_TILE_KIND(tile);
    return kind >= PD_TILEK_WATER_A && kind <= PD_TILEK_WATER_D;
}

static inline int pd_tile_embers(uint8_t tile) {
    const uint8_t kind = PD_TILE_KIND(tile);
    return kind == PD_TILEK_EMBERS_A || kind == PD_TILEK_EMBERS_B;
}

static inline int pd_in_bounds(int x, int y) {
    return x >= 0 && y >= 0 && x < PD_MAP_W && y < PD_MAP_H;
}

static inline uint8_t pd_tile_at(const pd_level_t *level, int x, int y) {
    return pd_in_bounds(x, y) ? level->tiles[y * PD_MAP_W + x] : PD_TILE_VOID;
}

static inline uint8_t pd_water_shore(const pd_level_t *level, int x, int y) {
    const int offsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    uint8_t result = 0;
    for (int direction = 0; direction < 4; ++direction) {
        const uint8_t adjacent = pd_tile_at(level, x + offsets[direction][0],
                                            y + offsets[direction][1]);
        if (!pd_tile_water(adjacent) && !pd_tile_wall(adjacent) &&
            PD_TILE_KIND(adjacent) != PD_TILEK_VOID)
            result |= (uint8_t)(1u << direction);
    }
    return result;
}

void pd_level_generate(pd_level_t *level, uint32_t seed, uint8_t depth);
void pd_level_generate_legacy(pd_level_t *level, uint32_t seed, uint8_t depth);

/* Recomputes `visible` and grows `explored`. */
void pd_level_update_fov(pd_level_t *level, int origin_x, int origin_y);

/* Reveals the whole floor plan (Scroll of Magic Mapping). */
void pd_level_map_all(pd_level_t *level);

/* Breadth-first path over walkable tiles, ignoring entities. Writes up to
 * `limit` steps (excluding the origin) into `out_x`/`out_y` and returns the
 * step count, or 0 when no route exists. */
int pd_level_path(const pd_level_t *level, int from_x, int from_y, int to_x,
                  int to_y, uint8_t *out_x, uint8_t *out_y, int limit);

/* Nearest walkable tile to (x, y) within `radius`, searching by distance.
 * Returns 1 and writes the result when one exists. */
int pd_level_nearest_open(const pd_level_t *level, int x, int y, int radius,
                          int *out_x, int *out_y);

#endif
