#include "dungeon.h"

#include <stddef.h>

/* File-scope scratch: the Guest stack is small, so map-sized temporaries live
 * here instead of in a frame. */
static uint8_t g_room_id[PD_MAP_TILES];
static uint16_t g_bfs_parent[PD_MAP_TILES];
static uint16_t g_bfs_queue[PD_MAP_TILES];

static void zero_bytes(uint8_t *bytes, int count) {
    for (int index = 0; index < count; ++index) bytes[index] = 0;
}

static int room_center_x(const pd_room_t *room) {
    return room->x + room->w / 2;
}

static int room_center_y(const pd_room_t *room) {
    return room->y + room->h / 2;
}

static int rooms_overlap(const pd_room_t *left, const pd_room_t *right) {
    return left->x - 2 < right->x + right->w && right->x - 2 < left->x + left->w &&
           left->y - 2 < right->y + right->h && right->y - 2 < left->y + left->h;
}

static void carve_rect(pd_level_t *level, int x, int y, int w, int h,
                       uint8_t tile) {
    for (int row = 0; row < h; ++row) {
        for (int column = 0; column < w; ++column) {
            const int px = x + column;
            const int py = y + row;
            if (pd_in_bounds(px, py)) level->tiles[py * PD_MAP_W + px] = tile;
        }
    }
}

/* Carves an L-shaped corridor and records which room each new tile belongs to. */
static void carve_corridor(pd_level_t *level, int x0, int y0, int x1, int y1) {
    int x = x0;
    int y = y0;
    const int horizontal_first = ((x0 + y0 + x1 + y1) & 1) == 0;
    if (horizontal_first) {
        while (x != x1) {
            x += x1 > x ? 1 : -1;
            if (pd_in_bounds(x, y) && level->tiles[y * PD_MAP_W + x] == PD_TILE_VOID)
                level->tiles[y * PD_MAP_W + x] = level->floor_tile;
        }
    }
    while (y != y1) {
        y += y1 > y ? 1 : -1;
        if (pd_in_bounds(x, y) && level->tiles[y * PD_MAP_W + x] == PD_TILE_VOID)
            level->tiles[y * PD_MAP_W + x] = level->floor_tile;
    }
    if (!horizontal_first) {
        while (x != x1) {
            x += x1 > x ? 1 : -1;
            if (pd_in_bounds(x, y) && level->tiles[y * PD_MAP_W + x] == PD_TILE_VOID)
                level->tiles[y * PD_MAP_W + x] = level->floor_tile;
        }
    }
}

static int room_distance(const pd_room_t *left, const pd_room_t *right) {
    const int dx = room_center_x(left) - room_center_x(right);
    const int dy = room_center_y(left) - room_center_y(right);
    return dx * dx + dy * dy;
}

static void connect_rooms(pd_level_t *level, int left, int right) {
    carve_corridor(level, room_center_x(&level->rooms[left]),
                   room_center_y(&level->rooms[left]),
                   room_center_x(&level->rooms[right]),
                   room_center_y(&level->rooms[right]));
}

static int room_angle_before(const pd_level_t *level, int left, int right,
                             int center_x, int center_y) {
    const int left_x = room_center_x(&level->rooms[left]) * level->room_count -
                       center_x;
    const int left_y = room_center_y(&level->rooms[left]) * level->room_count -
                       center_y;
    const int right_x = room_center_x(&level->rooms[right]) * level->room_count -
                        center_x;
    const int right_y = room_center_y(&level->rooms[right]) * level->room_count -
                        center_y;
    const int left_half = left_y < 0 || (left_y == 0 && left_x >= 0);
    const int right_half = right_y < 0 || (right_y == 0 && right_x >= 0);
    if (left_half != right_half) return left_half;
    return left_x * right_y - left_y * right_x > 0;
}

static void build_room_loop(pd_level_t *level) {
    uint8_t perimeter[PD_ROOMS_MAX] = {0};
    uint8_t order[PD_ROOMS_MAX];
    int center_x = 0;
    int center_y = 0;
    const int main_count = level->room_count > 6 ? level->room_count - 2 :
                           level->room_count > 3 ? level->room_count - 1 :
                           level->room_count;
    for (int index = 0; index < level->room_count; ++index) {
        center_x += room_center_x(&level->rooms[index]);
        center_y += room_center_y(&level->rooms[index]);
    }
    for (int position = 0; position < main_count; ++position) {
        int most_distant = -1;
        int best_distance = -1;
        for (int index = 0; index < level->room_count; ++index) {
            if (perimeter[index]) continue;
            const int delta_x = room_center_x(&level->rooms[index]) *
                                    level->room_count - center_x;
            const int delta_y = room_center_y(&level->rooms[index]) *
                                    level->room_count - center_y;
            const int distance = delta_x * delta_x + delta_y * delta_y;
            if (distance <= best_distance) continue;
            best_distance = distance;
            most_distant = index;
        }
        perimeter[most_distant] = 1;
        order[position] = (uint8_t)most_distant;
    }
    for (int index = 1; index < main_count; ++index) {
        const uint8_t room = order[index];
        int position = index;
        while (position > 0 &&
               room_angle_before(level, room, order[position - 1],
                                 center_x, center_y)) {
            order[position] = order[position - 1];
            --position;
        }
        order[position] = room;
    }
    for (int index = 1; index < main_count; ++index)
        connect_rooms(level, order[index - 1], order[index]);
    if (main_count > 2) connect_rooms(level, order[main_count - 1], order[0]);
    for (int index = 0; index < level->room_count; ++index) {
        if (perimeter[index]) continue;
        int nearest = order[0];
        int best_distance = room_distance(&level->rooms[index],
                                          &level->rooms[nearest]);
        for (int position = 1; position < main_count; ++position) {
            const int distance = room_distance(&level->rooms[index],
                                               &level->rooms[order[position]]);
            if (distance >= best_distance) continue;
            nearest = order[position];
            best_distance = distance;
        }
        connect_rooms(level, index, nearest);
    }
}

static void place_stairs(pd_level_t *level) {
    const pd_room_t *first = &level->rooms[0];
    level->entrance_x = (uint8_t)room_center_x(first);
    level->entrance_y = (uint8_t)room_center_y(first);
    level->tiles[level->entrance_y * PD_MAP_W + level->entrance_x] =
        PD_TILE(level->theme, PD_TILEK_STAIRS_UP);
    /* The exit sits in the room farthest from the entrance. */
    {
        int best = 1;
        int best_distance = -1;
        for (int index = 1; index < level->room_count; ++index) {
            const pd_room_t *room = &level->rooms[index];
            const int dx = room_center_x(room) - level->entrance_x;
            const int dy = room_center_y(room) - level->entrance_y;
            const int distance = dx * dx + dy * dy;
            if (distance > best_distance) {
                best_distance = distance;
                best = index;
            }
        }
        level->exit_x = (uint8_t)room_center_x(&level->rooms[best]);
        level->exit_y = (uint8_t)room_center_y(&level->rooms[best]);
        /* The final floor is the end of the descent: no stairs down. */
        if (level->depth < 25)
            level->tiles[level->exit_y * PD_MAP_W + level->exit_x] =
                PD_TILE(level->theme, PD_TILEK_STAIRS_DOWN);
    }
}

static void paint_floor_patches(pd_level_t *level, uint32_t *rng,
                                const pd_room_t *room, int room_index) {
    const int patches = pd_rng_range(rng, 1, 2);
    for (int patch = 0; patch < patches; ++patch) {
        const int center_x = room->x + (int)pd_rng_below(rng, room->w);
        const int center_y = room->y + (int)pd_rng_below(rng, room->h);
        const int radius_x = pd_rng_range(rng, 1, 3);
        const int radius_y = pd_rng_range(rng, 1, 2);
        const uint32_t style = pd_rng_below(rng, 100);
        uint8_t kind;
        if (level->theme <= 1)
            kind = style < 48 ? PD_TILEK_WATER_A :
                   style < 77 ? PD_TILEK_GRASS : PD_TILEK_HIGH_GRASS;
        else if (level->theme == 2)
            kind = style < 36 ? PD_TILEK_WATER_A :
                   style < 73 ? PD_TILEK_GRASS : PD_TILEK_EMBERS_A;
        else
            kind = style < 25 ? PD_TILEK_WATER_A :
                   style < 65 ? PD_TILEK_EMBERS_A : PD_TILEK_DECO_ALT;
        for (int y = center_y - radius_y; y <= center_y + radius_y; ++y) {
            for (int x = center_x - radius_x; x <= center_x + radius_x; ++x) {
                const int delta_x = x - center_x;
                const int delta_y = y - center_y;
                if (!pd_in_bounds(x, y) ||
                    delta_x * delta_x * radius_y * radius_y +
                    delta_y * delta_y * radius_x * radius_x >
                        radius_x * radius_x * radius_y * radius_y)
                    continue;
                const int at = y * PD_MAP_W + x;
                if (g_room_id[at] == room_index + 1 &&
                    level->tiles[at] == level->floor_tile)
                    level->tiles[at] = PD_TILE(level->theme, kind);
            }
        }
    }
}

static void decorate(pd_level_t *level, uint32_t *rng, int legacy) {
    /* Wall torches, shelves and altars, one or two features per room. */
    for (int index = 0; index < level->room_count; ++index) {
        const pd_room_t *room = &level->rooms[index];
        const int features = pd_rng_range(rng, 1, 2);
        for (int feature = 0; feature < features; ++feature) {
            const int side = (int)pd_rng_below(rng, 4);
            int x = room->x;
            int y = room->y;
            if (side == 0) {
                x = room->x + (int)pd_rng_below(rng, (uint32_t)room->w);
                y = room->y - 1;
            } else if (side == 1) {
                x = room->x + (int)pd_rng_below(rng, (uint32_t)room->w);
                y = room->y + room->h;
            } else if (side == 2) {
                x = room->x - 1;
                y = room->y + (int)pd_rng_below(rng, (uint32_t)room->h);
            } else {
                x = room->x + room->w;
                y = room->y + (int)pd_rng_below(rng, (uint32_t)room->h);
            }
            if (!pd_in_bounds(x, y) || level->tiles[y * PD_MAP_W + x] !=
                                            level->wall_tile) {
                continue;
            }
            {
                const uint32_t roll = pd_rng_below(rng, 100);
                uint8_t tile;
                if (roll < 55) {
                    tile = PD_TILE(level->theme, PD_TILEK_WALL_DECO);
                } else {
                    continue;
                }
                level->tiles[y * PD_MAP_W + x] = tile;
            }
        }
        if (!legacy) {
            paint_floor_patches(level, rng, room, index);
            continue;
        }
        /* Floor clutter: grass, rubble or embers depending on the theme. */
        for (int clutter = 0; clutter < 3; ++clutter) {
            if (pd_rng_below(rng, 100) >= 45) continue;
            {
                const int x = room->x + (int)pd_rng_below(rng, (uint32_t)room->w);
                const int y = room->y + (int)pd_rng_below(rng, (uint32_t)room->h);
                const int at = y * PD_MAP_W + x;
                uint8_t tile;
                const uint32_t roll = pd_rng_below(rng, 100);
                if (level->theme <= 1) {
                    tile = roll < 40 ? PD_TILE(level->theme, PD_TILEK_GRASS)
                                     : (roll < 55
                                            ? PD_TILE(level->theme,
                                                      PD_TILEK_HIGH_GRASS)
                                            : (roll < 75
                                                   ? PD_TILE(level->theme,
                                                             PD_TILEK_WATER_A)
                                                   : PD_TILE(level->theme,
                                                             PD_TILEK_DECO)));
                } else if (level->theme == 2) {
                    tile = roll < 40 ? PD_TILE(level->theme, PD_TILEK_GRASS)
                                     : (roll < 60
                                            ? PD_TILE(level->theme,
                                                      PD_TILEK_EMBERS_A)
                                            : PD_TILE(level->theme,
                                                      PD_TILEK_DECO));
                } else {
                    tile = roll < 45 ? PD_TILE(level->theme, PD_TILEK_EMBERS_A)
                                     : (roll < 70
                                            ? PD_TILE(level->theme,
                                                      PD_TILEK_DECO_ALT)
                                            : PD_TILE(level->theme,
                                                      PD_TILEK_DECO));
                }
                if (level->tiles[at] == level->floor_tile) level->tiles[at] = tile;
            }
        }
    }
}

static void generate_level(pd_level_t *level, uint32_t seed, uint8_t depth,
                           int legacy) {
    uint32_t rng;
    int rooms_wanted;
    int attempt;

    pd_rng_seed(&rng, pd_rng_mix(seed, (uint32_t)depth * UINT32_C(2654435761)));
    level->depth = depth;
    /* SPD's five regions, one per five floors. */
    level->theme = (uint8_t)(depth <= 5 ? 0 : (depth - 1) / 5);
    if (level->theme >= PD_THEME_COUNT) level->theme = PD_THEME_COUNT - 1;
    level->floor_tile = PD_TILE(level->theme, PD_TILEK_FLOOR);
    level->wall_tile = PD_TILE(level->theme, PD_TILEK_WALL);
    zero_bytes(level->tiles, PD_MAP_TILES);
    zero_bytes(level->explored, PD_MAP_TILES);
    zero_bytes(level->visible, PD_MAP_TILES);
    zero_bytes(level->known, PD_MAP_TILES);
    zero_bytes(g_room_id, PD_MAP_TILES);
    level->room_count = 0;

    rooms_wanted = pd_rng_range(&rng, legacy ? 5 : 6, legacy ? 8 : 9);
    for (attempt = 0; attempt < (legacy ? 90 : 160) &&
                      level->room_count < rooms_wanted;
         ++attempt) {
        pd_room_t candidate;
        int fits = 1;
        candidate.w = (uint8_t)pd_rng_range(&rng, 4, legacy ? 8 : 9);
        candidate.h = (uint8_t)pd_rng_range(&rng, legacy ? 3 : 4,
                                           legacy ? 6 : 8);
        candidate.x =
            (uint8_t)pd_rng_range(&rng, 1, PD_MAP_W - candidate.w - 2);
        candidate.y =
            (uint8_t)pd_rng_range(&rng, 1, PD_MAP_H - candidate.h - 2);
        for (int index = 0; index < level->room_count; ++index) {
            if (rooms_overlap(&candidate, &level->rooms[index])) {
                fits = 0;
                break;
            }
        }
        if (!fits) continue;
        level->rooms[level->room_count] = candidate;
        for (int row = 0; row < candidate.h; ++row) {
            for (int column = 0; column < candidate.w; ++column) {
                const int x = candidate.x + column;
                const int y = candidate.y + row;
                level->tiles[y * PD_MAP_W + x] = level->floor_tile;
                g_room_id[y * PD_MAP_W + x] = (uint8_t)(level->room_count + 1);
            }
        }
        ++level->room_count;
    }
    if (level->room_count == 0) {
        /* Degenerate paranoia: a single guaranteed room. */
        pd_room_t fallback;
        fallback.x = 2;
        fallback.y = 2;
        fallback.w = 8;
        fallback.h = 6;
        level->rooms[0] = fallback;
        carve_rect(level, 2, 2, 8, 6, level->floor_tile);
        level->room_count = 1;
        for (int row = 0; row < 6; ++row)
            for (int column = 0; column < 8; ++column)
                g_room_id[(2 + row) * PD_MAP_W + 2 + column] = 1;
    }

    if (legacy) {
        for (int index = 1; index < level->room_count; ++index)
            connect_rooms(level, index - 1, index);
        for (int extra = 0; extra < 2 && level->room_count > 2; ++extra) {
            const int left = (int)pd_rng_below(&rng, (uint32_t)level->room_count);
            const int right = (int)pd_rng_below(&rng, (uint32_t)level->room_count);
            if (left != right) connect_rooms(level, left, right);
        }
    } else {
        build_room_loop(level);
    }

    /* Doorways where a corridor passes straight through a room wall. */
    for (int y = 1; y < PD_MAP_H - 1; ++y) {
        for (int x = 1; x < PD_MAP_W - 1; ++x) {
            const int at = y * PD_MAP_W + x;
            if (level->tiles[at] != level->floor_tile || g_room_id[at] != 0)
                continue;
            {
                static const int8_t kOffsets[4][2] = {
                    {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (int direction = 0; direction < 4; ++direction) {
                    const int nx = x + kOffsets[direction][0];
                    const int ny = y + kOffsets[direction][1];
                    const int neighbor = ny * PD_MAP_W + nx;
                    if (g_room_id[neighbor] == 0) continue;
                    if (level->tiles[neighbor] != level->floor_tile) continue;
                    if (!legacy) {
                        int nearby = 0;
                        for (int row = y - 1; row <= y + 1; ++row)
                            for (int column = x - 1; column <= x + 1; ++column)
                                if (PD_TILE_KIND(pd_tile_at(level, column, row)) ==
                                    PD_TILEK_DOOR) nearby = 1;
                        if (nearby) continue;
                    }
                    /* The opposite side must also be open floor. */
                    if (level->tiles[(y - kOffsets[direction][1]) * PD_MAP_W +
                                     (x - kOffsets[direction][0])] !=
                        level->floor_tile)
                        continue;
                    level->tiles[at] = PD_TILE(level->theme, PD_TILEK_DOOR);
                    break;
                }
            }
        }
    }

    /* Everything adjacent to open space becomes themed wall. */
    for (int y = 0; y < PD_MAP_H; ++y) {
        for (int x = 0; x < PD_MAP_W; ++x) {
            const int at = y * PD_MAP_W + x;
            if (level->tiles[at] != PD_TILE_VOID) continue;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if ((dx == 0 && dy == 0) || !pd_in_bounds(nx, ny)) continue;
                    const uint8_t neighbor = level->tiles[ny * PD_MAP_W + nx];
                    if (neighbor != PD_TILE_VOID && !pd_tile_wall(neighbor)) {
                        level->tiles[at] = level->wall_tile;
                        dy = 2;
                        break;
                    }
                }
            }
        }
    }

    place_stairs(level);
    decorate(level, &rng, legacy);
    /* A statue or an alchemy pot anchors some rooms, like the original. Only
     * interior tiles are used: a tile with four open neighbours can never be a
     * chokepoint, so the placement cannot seal a corridor off. */
    for (int index = 0; index < level->room_count; ++index) {
        const pd_room_t *room = &level->rooms[index];
        int x;
        int y;
        int at;
        if (room->w < 4 || room->h < 4) continue;
        if (pd_rng_below(&rng, 100) >= 45) continue;
        x = room->x + 1 + (int)pd_rng_below(&rng, room->w - 2);
        y = room->y + 1 + (int)pd_rng_below(&rng, room->h - 2);
        at = y * PD_MAP_W + x;
        if (level->tiles[at] != level->floor_tile) continue;
        if (level->tiles[at - 1] != level->floor_tile ||
            level->tiles[at + 1] != level->floor_tile ||
            level->tiles[at - PD_MAP_W] != level->floor_tile ||
            level->tiles[at + PD_MAP_W] != level->floor_tile)
            continue;
        level->tiles[at] = PD_TILE(level->theme, pd_rng_below(&rng, 2) == 0
                                                    ? PD_TILEK_ALCHEMY
                                                    : PD_TILEK_STATUE);
    }
}

void pd_level_generate(pd_level_t *level, uint32_t seed, uint8_t depth) {
    generate_level(level, seed, depth, 0);
}

void pd_level_generate_legacy(pd_level_t *level, uint32_t seed, uint8_t depth) {
    generate_level(level, seed, depth, 1);
}

static int line_of_sight(const pd_level_t *level, int x0, int y0, int x1,
                         int y1) {
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const int steps = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)
                          ? (dx < 0 ? -dx : dx)
                          : (dy < 0 ? -dy : dy);
    if (steps <= 1) return 1;
    for (int step = 1; step < steps; ++step) {
        for (int sub = 0; sub < 4; ++sub) {
            const int numerator = step * 4 + sub;
            const int x = x0 + (int)(((long)dx * numerator) / (steps * 4));
            const int y = y0 + (int)(((long)dy * numerator) / (steps * 4));
            if (!pd_in_bounds(x, y)) return 0;
            if (pd_tile_opaque(pd_tile_at(level, x, y))) return 0;
        }
    }
    return 1;
}

void pd_level_update_fov(pd_level_t *level, int origin_x, int origin_y) {
    zero_bytes(level->visible, PD_MAP_TILES);
    for (int dy = -PD_FOV_RADIUS; dy <= PD_FOV_RADIUS; ++dy) {
        for (int dx = -PD_FOV_RADIUS; dx <= PD_FOV_RADIUS; ++dx) {
            const int x = origin_x + dx;
            const int y = origin_y + dy;
            if (dx * dx + dy * dy > PD_FOV_RADIUS * PD_FOV_RADIUS) continue;
            if (!pd_in_bounds(x, y)) continue;
            if (!line_of_sight(level, origin_x, origin_y, x, y)) continue;
            level->visible[y * PD_MAP_W + x] = 1;
            level->explored[y * PD_MAP_W + x] = 1;
        }
    }
    /* Open tiles reveal their surrounding walls so rooms read as boxed in. */
    for (int y = 0; y < PD_MAP_H; ++y) {
        for (int x = 0; x < PD_MAP_W; ++x) {
            if (!level->visible[y * PD_MAP_W + x]) continue;
            if (pd_tile_opaque(pd_tile_at(level, x, y))) continue;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (!pd_in_bounds(nx, ny)) continue;
                    level->visible[ny * PD_MAP_W + nx] = 1;
                    level->explored[ny * PD_MAP_W + nx] = 1;
                }
            }
        }
    }
}

void pd_level_map_all(pd_level_t *level) {
    for (int index = 0; index < PD_MAP_TILES; ++index)
        level->explored[index] = 1;
}

int pd_level_path(const pd_level_t *level, int from_x, int from_y, int to_x,
                  int to_y, uint8_t *out_x, uint8_t *out_y, int limit) {
    int head = 0;
    int tail = 0;
    int target;

    if (limit <= 0 || !pd_in_bounds(to_x, to_y)) return 0;
    if (!pd_tile_walkable(pd_tile_at(level, to_x, to_y))) {
        int found = 0;
        for (int radius = 1; radius <= 3 && !found; ++radius) {
            for (int dy = -radius; dy <= radius && !found; ++dy) {
                for (int dx = -radius; dx <= radius && !found; ++dx) {
                    if (dx * dx + dy * dy != radius * radius) continue;
                    if (pd_tile_walkable(
                            pd_tile_at(level, to_x + dx, to_y + dy))) {
                        to_x += dx;
                        to_y += dy;
                        found = 1;
                    }
                }
            }
        }
        if (!found) return 0;
    }
    if (from_x == to_x && from_y == to_y) return 0;

    for (int index = 0; index < PD_MAP_TILES; ++index)
        g_bfs_parent[index] = UINT16_MAX;
    target = to_y * PD_MAP_W + to_x;
    g_bfs_parent[from_y * PD_MAP_W + from_x] = (uint16_t)(from_y * PD_MAP_W + from_x);
    g_bfs_queue[tail++] = (uint16_t)(from_y * PD_MAP_W + from_x);
    while (head < tail) {
        const int at = g_bfs_queue[head++];
        const int x = at % PD_MAP_W;
        const int y = at / PD_MAP_W;
        if (at == target) break;
        {
            static const int8_t kSteps[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
            for (int step = 0; step < 4; ++step) {
                const int nx = x + kSteps[step][0];
                const int ny = y + kSteps[step][1];
                const int next = ny * PD_MAP_W + nx;
                if (!pd_in_bounds(nx, ny) || g_bfs_parent[next] != UINT16_MAX)
                    continue;
                if (!pd_tile_walkable(pd_tile_at(level, nx, ny))) continue;
                g_bfs_parent[next] = (uint16_t)at;
                g_bfs_queue[tail++] = (uint16_t)next;
            }
        }
    }
    if (g_bfs_parent[target] == UINT16_MAX) return 0;

    {
        /* reversed[] holds the route from the target back to the source, so the
         * first step of the walk is its last entry. */
        static uint8_t reversed_x[PD_MAP_TILES];
        static uint8_t reversed_y[PD_MAP_TILES];
        int count = 0;
        int at = target;
        while (at != from_y * PD_MAP_W + from_x &&
               count < (int)sizeof(reversed_x)) {
            reversed_x[count] = (uint8_t)(at % PD_MAP_W);
            reversed_y[count] = (uint8_t)(at / PD_MAP_W);
            ++count;
            at = g_bfs_parent[at];
        }
        if (count == 0) return 0;
        if (limit > count) limit = count;
        for (int index = 0; index < limit; ++index) {
            out_x[index] = reversed_x[count - 1 - index];
            out_y[index] = reversed_y[count - 1 - index];
        }
        return limit;
    }
}

int pd_level_nearest_open(const pd_level_t *level, int x, int y, int radius,
                          int *out_x, int *out_y) {
    for (int ring = 0; ring <= radius; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (ring > 0 && (dx > -ring && dx < ring) &&
                    (dy > -ring && dy < ring))
                    continue;
                if (!pd_in_bounds(x + dx, y + dy)) continue;
                if (!pd_tile_walkable(pd_tile_at(level, x + dx, y + dy)))
                    continue;
                *out_x = x + dx;
                *out_y = y + dy;
                return 1;
            }
        }
    }
    return 0;
}
