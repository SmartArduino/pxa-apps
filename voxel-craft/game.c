#include "game.h"

#include <stddef.h>

#include "rc_math.h"

chunk_t *g_chunk_grid[GRID_W][GRID_W];
int16_t g_chunk_origin_cx;
int16_t g_chunk_origin_cz;
uint32_t g_world_seed = 1u;

particle_t g_particles[MAX_PARTICLES];
mob_t g_mobs[MAX_MOBS];

item_stack_t g_inventory[INV_SLOTS];
item_stack_t g_craft[CRAFT_SLOTS];
item_stack_t g_craft_result;
item_stack_t g_table_craft[TABLE_CRAFT_SLOTS];
item_stack_t g_table_result;

static chunk_t g_chunk_pool[GRID_COUNT];
static uint8_t g_chunk_pending[GRID_COUNT];
static uint8_t g_grid_ready;
static edit_t g_edits[MAX_EDITS];
static int g_edit_count;
static uint32_t g_chunk_revision = 1u;

static void touch_chunk_revision(chunk_t *chunk) {
    if (chunk == NULL) return;
    ++g_chunk_revision;
    if (g_chunk_revision == 0) ++g_chunk_revision;
    chunk->revision = g_chunk_revision;
}

static const char *const kBlockNames[BLOCK_TYPE_COUNT] = {
    "AIR",    "GRASS", "DIRT",  "STONE", "SAND",  "WOOD",   "LEAVES",
    "WATER",  "PLANK", "BRICK", "GLASS", "COBBLE", "TABLE", "SNOW",
    "GRAVEL", "CACTUS", "BUSH", "FLOWER", "WOOL", "BEDROCK",
};

/* Seconds a continuous mining action needs per block type. */
static const float kHardness[BLOCK_TYPE_COUNT] = {
    0.0F, 0.35F, 0.35F, 1.10F, 0.40F, 0.75F, 0.25F,
    0.0F, 0.70F, 1.10F, 0.55F, 1.05F, 0.80F, 0.25F,
    0.60F, 0.45F, 0.15F, 0.10F, 0.40F, -1.0F,
};

static uint32_t g_rand_state = 0x12345678u;

static uint32_t game_rand(void) {
    uint32_t value = g_rand_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    g_rand_state = value;
    return value;
}

static float rand_unit(void) {
    return (float)(game_rand() & 1023u) / 511.5F - 1.0F;
}

int game_block(int x, int y, int z) { return game_block_fast(x, y, z); }

#define WATER_FLOW_MAX 256
#define WATER_FLOW_SPREAD 4

typedef struct {
    int32_t x;
    int16_t y;
    int32_t z;
    uint8_t distance;
} water_cell_t;

static water_cell_t g_water_queue[WATER_FLOW_MAX];
static uint8_t g_water_flow_active;

static int water_flow_neighbor(const water_cell_t *cell, int dx, int dy,
                               int dz, water_cell_t *out) {
    int32_t x = cell->x + dx;
    int32_t z = cell->z + dz;
    int y = (int)cell->y + dy;
    uint8_t distance;
    if ((unsigned)y >= (unsigned)CHUNK_HEIGHT) {
        return 0;
    }
    if (game_block(x, y, z) != BLOCK_AIR) {
        return 0;
    }
    if (dy > 0) {
        return 0; /* Water never flows upwards. */
    }
    if (dy == 0) {
        if (cell->distance >= WATER_FLOW_SPREAD) {
            return 0;
        }
        distance = (uint8_t)(cell->distance + 1);
    } else {
        distance = cell->distance;
    }
    out->x = x;
    out->y = (int16_t)y;
    out->z = z;
    out->distance = distance;
    return 1;
}

/* After a block is removed, let adjacent water flow into the gap: down first,
 * then sideways for a few cells. The flow is written through the edit log so
 * it survives save and reload. */
static void water_flow(int x, int y, int z) {
    static const int8_t kDx[6] = {0, 0, -1, 1, 0, 0};
    static const int8_t kDy[6] = {-1, 1, 0, 0, 0, 0};
    static const int8_t kDz[6] = {0, 0, 0, 0, -1, 1};
    int head = 0;
    int tail = 0;
    int index;
    int has_water = 0;
    if (g_water_flow_active || game_block(x, y, z) != BLOCK_AIR) {
        return;
    }
    for (index = 0; index < 6; ++index) {
        if (game_block(x + kDx[index], y + kDy[index],
                       z + kDz[index]) == BLOCK_WATER) {
            has_water = 1;
            break;
        }
    }
    if (!has_water) {
        return;
    }
    g_water_flow_active = 1;
    g_water_queue[tail].x = x;
    g_water_queue[tail].y = (int16_t)y;
    g_water_queue[tail].z = z;
    g_water_queue[tail].distance = 0;
    ++tail;
    while (head < tail) {
        const water_cell_t cell = g_water_queue[head++];
        game_set_block(cell.x, cell.y, cell.z, BLOCK_WATER);
        for (index = 0; index < 6; ++index) {
            water_cell_t next;
            if (!water_flow_neighbor(&cell, kDx[index], kDy[index],
                                     kDz[index], &next)) {
                continue;
            }
            if (tail < WATER_FLOW_MAX) {
                g_water_queue[tail++] = next;
            }
        }
    }
    g_water_flow_active = 0;
}

void game_set_block(int x, int y, int z, int block) {
    int i;
    int j;
    int index;
    chunk_t *chunk;
    if ((unsigned)y >= (unsigned)CHUNK_HEIGHT) {
        return;
    }
    i = (x >> CHUNK_BITS) - g_chunk_origin_cx;
    j = (z >> CHUNK_BITS) - g_chunk_origin_cz;
    if ((unsigned)i >= (unsigned)GRID_W || (unsigned)j >= (unsigned)GRID_W) {
        return;
    }
    chunk = g_chunk_grid[j][i];
    index = (y << 8) | ((z & CHUNK_MASK) << CHUNK_BITS) | (x & CHUNK_MASK);
    if (chunk->blocks[index] == (uint8_t)block) {
        return;
    }
    chunk->blocks[index] = (uint8_t)block;
    touch_chunk_revision(chunk);
    if ((x & CHUNK_MASK) == 0 && i > 0)
        touch_chunk_revision(g_chunk_grid[j][i - 1]);
    if ((x & CHUNK_MASK) == CHUNK_MASK && i + 1 < GRID_W)
        touch_chunk_revision(g_chunk_grid[j][i + 1]);
    if ((z & CHUNK_MASK) == 0 && j > 0)
        touch_chunk_revision(g_chunk_grid[j - 1][i]);
    if ((z & CHUNK_MASK) == CHUNK_MASK && j + 1 < GRID_W)
        touch_chunk_revision(g_chunk_grid[j + 1][i]);
    if (g_edit_count < MAX_EDITS) {
        g_edits[g_edit_count].x = x;
        g_edits[g_edit_count].y = (int16_t)y;
        g_edits[g_edit_count].z = z;
        g_edits[g_edit_count].block = (uint8_t)block;
        ++g_edit_count;
    }
    if (block == BLOCK_AIR && !g_water_flow_active) {
        water_flow(x, y, z);
    }
}

int game_solid(int x, int y, int z) {
    const int block = game_block_fast(x, y, z);
    return block != BLOCK_AIR && block != BLOCK_WATER &&
           block != BLOCK_BUSH && block != BLOCK_FLOWER;
}

const char *game_block_name(int block) {
    if ((unsigned)block >= (unsigned)BLOCK_TYPE_COUNT) {
        return kBlockNames[BLOCK_AIR];
    }
    return kBlockNames[block];
}

float game_block_hardness(int block) {
    if ((unsigned)block >= (unsigned)BLOCK_TYPE_COUNT) {
        return 0.0F;
    }
    return kHardness[block];
}

static const char *const kToolNames[] = {
    "W.PICK", "W.AXE", "W.SHOVEL", "W.SWORD",
    "S.PICK", "S.AXE", "S.SHOVEL", "S.SWORD",
};

int game_item_is_block(int item) {
    return item > BLOCK_AIR && item < BLOCK_TYPE_COUNT;
}

int game_item_is_tool(int item) {
    return item >= ITEM_FIRST_TOOL && item <= ITEM_LAST_TOOL;
}

const char *game_item_name(int item) {
    if (game_item_is_tool(item)) {
        return kToolNames[item - ITEM_FIRST_TOOL];
    }
    if (game_item_is_block(item)) {
        return game_block_name(item);
    }
    return "EMPTY";
}

/* Wooden tools are 1.8x, stone tools 2.6x on their matching blocks. */
float game_mining_speed(int item, int block) {
    const int wooden = item == ITEM_WOOD_PICK || item == ITEM_WOOD_AXE ||
                       item == ITEM_WOOD_SHOVEL;
    const int stone = item == ITEM_STONE_PICK || item == ITEM_STONE_AXE ||
                      item == ITEM_STONE_SHOVEL;
    float speed = 1.0F;
    if (!wooden && !stone) {
        if (item == ITEM_WOOD_SWORD || item == ITEM_STONE_SWORD) {
            return block == BLOCK_LEAVES ? 1.6F : 1.0F;
        }
        return 1.0F;
    }
    speed = stone ? 2.6F : 1.8F;
    if (item == ITEM_WOOD_PICK || item == ITEM_STONE_PICK) {
        if (block == BLOCK_STONE || block == BLOCK_COBBLE ||
            block == BLOCK_BRICK || block == BLOCK_GLASS ||
            block == BLOCK_TABLE) {
            return speed;
        }
    } else if (item == ITEM_WOOD_AXE || item == ITEM_STONE_AXE) {
        if (block == BLOCK_WOOD || block == BLOCK_PLANK ||
            block == BLOCK_LEAVES || block == BLOCK_TABLE) {
            return speed;
        }
    } else if (block == BLOCK_DIRT || block == BLOCK_GRASS ||
               block == BLOCK_SAND) {
        return speed;
    }
    return 1.0F;
}

int game_attack_damage(int item) {
    if (item == ITEM_WOOD_SWORD) {
        return 2;
    }
    if (item == ITEM_STONE_SWORD) {
        return 3;
    }
    if (item == ITEM_WOOD_AXE || item == ITEM_WOOD_PICK ||
        item == ITEM_WOOD_SHOVEL) {
        return 1;
    }
    if (item == ITEM_STONE_AXE || item == ITEM_STONE_PICK ||
        item == ITEM_STONE_SHOVEL) {
        return 2;
    }
    return 1;
}

/* --- terrain generation ------------------------------------------------- */

static uint32_t hash2(int x, int z, uint32_t seed) {
    uint32_t value = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u +
                     seed * 2246822519u;
    value = (value ^ (value >> 13)) * 1274126177u;
    return value ^ (value >> 16);
}

static float lattice(int x, int z, uint32_t seed) {
    return (float)(hash2(x, z, seed) & 1023u) * (2.0F / 1023.0F) - 1.0F;
}

static float value_noise(float x, float z, uint32_t seed) {
    const int ix = rc_floor_int(x);
    const int iz = rc_floor_int(z);
    const float fx = x - (float)ix;
    const float fz = z - (float)iz;
    const float sx = fx * fx * (3.0F - 2.0F * fx);
    const float sz = fz * fz * (3.0F - 2.0F * fz);
    const float n00 = lattice(ix, iz, seed);
    const float n10 = lattice(ix + 1, iz, seed);
    const float n01 = lattice(ix, iz + 1, seed);
    const float n11 = lattice(ix + 1, iz + 1, seed);
    const float a = n00 + (n10 - n00) * sx;
    const float b = n01 + (n11 - n01) * sx;
    return a + (b - a) * sz;
}

static int terrain_height(int x, int z) {
    float noise = value_noise((float)x * 0.085F, (float)z * 0.085F,
                              g_world_seed);
    int height;
    noise += value_noise((float)x * 0.17F, (float)z * 0.17F,
                         g_world_seed + 101u) *
             0.5F;
    noise += value_noise((float)x * 0.34F, (float)z * 0.34F,
                         g_world_seed + 202u) *
             0.25F;
    noise *= 1.0F / 1.75F;
    height = 9 + (int)(noise * 4.5F + (noise >= 0.0F ? 0.5F : -0.5F));
    if (height < 3) {
        height = 3;
    } else if (height > CHUNK_HEIGHT - 8) {
        height = CHUNK_HEIGHT - 8;
    }
    return height;
}

static int biome_at(int x, int z) {
    const float value = value_noise((float)x * 0.02F, (float)z * 0.02F,
                                    g_world_seed + 777u);
    if (value < -0.35F) {
        return BIOME_SNOW;
    }
    if (value > 0.45F) {
        return BIOME_DESERT;
    }
    if (value > 0.1F) {
        return BIOME_FOREST;
    }
    return BIOME_PLAINS;
}

static void chunk_set(chunk_t *chunk, int base_x, int base_z, int x, int y,
                      int z, int block) {
    const int lx = x - base_x;
    const int lz = z - base_z;
    if ((unsigned)lx >= (unsigned)CHUNK_SIZE ||
        (unsigned)lz >= (unsigned)CHUNK_SIZE ||
        (unsigned)y >= (unsigned)CHUNK_HEIGHT) {
        return;
    }
    chunk->blocks[(y << 8) | (lz << CHUNK_BITS) | lx] = (uint8_t)block;
}

static int chunk_get(chunk_t *chunk, int base_x, int base_z, int x, int y,
                     int z) {
    const int lx = x - base_x;
    const int lz = z - base_z;
    if ((unsigned)lx >= (unsigned)CHUNK_SIZE ||
        (unsigned)lz >= (unsigned)CHUNK_SIZE ||
        (unsigned)y >= (unsigned)CHUNK_HEIGHT) {
        return BLOCK_AIR;
    }
    return chunk->blocks[(y << 8) | (lz << CHUNK_BITS) | lx];
}

static void tree_leaf(chunk_t *chunk, int base_x, int base_z, int x, int y,
                      int z) {
    if (chunk_get(chunk, base_x, base_z, x, y, z) == BLOCK_AIR) {
        chunk_set(chunk, base_x, base_z, x, y, z, BLOCK_LEAVES);
    }
}

/* Broad oak: a thick crown around the trunk top. */
static void plant_oak(chunk_t *chunk, int base_x, int base_z, int x, int top,
                      int z) {
    const uint32_t shape = hash2(x, z, 0x5bd1u);
    const int trunk = 4 + (int)(shape % 3u);
    const int crown = top + trunk;
    int dy;
    if (crown + 2 >= CHUNK_HEIGHT) {
        return;
    }
    for (dy = 1; dy <= trunk; ++dy) {
        chunk_set(chunk, base_x, base_z, x, top + dy, z, BLOCK_WOOD);
    }
    for (dy = -1; dy <= 1; ++dy) {
        const int radius = dy == 1 ? 1 : 2;
        int dx;
        int dz;
        for (dx = -radius; dx <= radius; ++dx) {
            for (dz = -radius; dz <= radius; ++dz) {
                if (radius == 2 && (dx == 2 || dx == -2) &&
                    (dz == 2 || dz == -2)) {
                    continue;
                }
                tree_leaf(chunk, base_x, base_z, x + dx, crown + dy, z + dz);
            }
        }
    }
    tree_leaf(chunk, base_x, base_z, x, crown + 2, z);
}

/* Tall birch: a slim trunk and a small round crown. */
static void plant_birch(chunk_t *chunk, int base_x, int base_z, int x, int top,
                        int z) {
    const uint32_t shape = hash2(x, z, 0x71abu);
    const int trunk = 6 + (int)(shape % 3u);
    const int crown = top + trunk;
    int dy;
    if (crown + 2 >= CHUNK_HEIGHT) {
        return;
    }
    for (dy = 1; dy <= trunk; ++dy) {
        chunk_set(chunk, base_x, base_z, x, top + dy, z, BLOCK_WOOD);
    }
    for (dy = 0; dy <= 2; ++dy) {
        const int radius = dy == 0 ? 2 : 1;
        int dx;
        int dz;
        for (dx = -radius; dx <= radius; ++dx) {
            for (dz = -radius; dz <= radius; ++dz) {
                if (radius == 2 && (dx == 2 || dx == -2) &&
                    (dz == 2 || dz == -2) && ((dx ^ dz) & 1) != 0) {
                    continue;
                }
                tree_leaf(chunk, base_x, base_z, x + dx, crown + dy, z + dz);
            }
        }
    }
    tree_leaf(chunk, base_x, base_z, x, crown + 3, z);
}

/* Spruce: a conical stack of leaf rings. */
static void plant_spruce(chunk_t *chunk, int base_x, int base_z, int x,
                         int top, int z) {
    const uint32_t shape = hash2(x, z, 0x2f9u);
    const int trunk = 6 + (int)(shape % 3u);
    const int crown = top + trunk;
    int dy;
    if (crown + 2 >= CHUNK_HEIGHT) {
        return;
    }
    for (dy = 1; dy <= trunk; ++dy) {
        chunk_set(chunk, base_x, base_z, x, top + dy, z, BLOCK_WOOD);
    }
    for (dy = 2; dy <= trunk; ++dy) {
        const int from_top = trunk - dy;
        const int radius = from_top < 2 ? 0 : (from_top < 4 ? 1 : 2);
        int dx;
        int dz;
        for (dx = -radius; dx <= radius; ++dx) {
            for (dz = -radius; dz <= radius; ++dz) {
                if (radius == 2 && (dx == 2 || dx == -2) &&
                    (dz == 2 || dz == -2) && ((dx ^ dz) & 1) != 0) {
                    continue;
                }
                tree_leaf(chunk, base_x, base_z, x + dx, top + dy, z + dz);
            }
        }
    }
    tree_leaf(chunk, base_x, base_z, x, crown + 1, z);
}

/* Desert cactus: a bare green column. */
static void plant_cactus(chunk_t *chunk, int base_x, int base_z, int x,
                         int top, int z) {
    const uint32_t shape = hash2(x, z, 0x51du);
    const int height = 1 + (int)(shape % 3u);
    int dy;
    for (dy = 1; dy <= height && top + dy < CHUNK_HEIGHT; ++dy) {
        chunk_set(chunk, base_x, base_z, x, top + dy, z, BLOCK_CACTUS);
    }
}

static int surface_block(int biome, int height) {
    if (height <= WATER_LEVEL + 1) {
        return BLOCK_SAND;
    }
    if (height >= 16) {
        return BLOCK_SNOW;
    }
    if (height >= 15) {
        return BLOCK_STONE;
    }
    if (biome == BIOME_DESERT) {
        return BLOCK_SAND;
    }
    if (biome == BIOME_SNOW) {
        return BLOCK_SNOW;
    }
    return BLOCK_GRASS;
}

static void generate_chunk(chunk_t *chunk, int cx, int cz) {
    const int base_x = cx * CHUNK_SIZE;
    const int base_z = cz * CHUNK_SIZE;
    int lx;
    int lz;
    int index;
    for (index = 0; index < CHUNK_VOLUME; ++index) {
        chunk->blocks[index] = BLOCK_AIR;
    }
    for (lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (lz = 0; lz < CHUNK_SIZE; ++lz) {
            const int x = base_x + lx;
            const int z = base_z + lz;
            const int height = terrain_height(x, z);
            const int biome = biome_at(x, z);
            int surface = surface_block(biome, height);
            const int filler =
                biome == BIOME_DESERT ? BLOCK_SAND : BLOCK_DIRT;
            int y;
            if (height >= 13 && height < 15 &&
                (hash2(x, z, g_world_seed ^ 0x99u) % 100u) < 20u) {
                surface = BLOCK_GRAVEL;
            }
            for (y = 0; y <= height; ++y) {
                int block;
                if (y == 0) {
                    block = BLOCK_BEDROCK;
                } else if (y < height - 3) {
                    block = BLOCK_STONE;
                } else if (y < height) {
                    block = filler;
                } else {
                    block = surface;
                }
                if (height >= 15 && y >= height - 2) {
                    block = BLOCK_STONE;
                }
                if (height >= 16 && y == height) {
                    block = BLOCK_SNOW;
                }
                chunk_set(chunk, base_x, base_z, x, y, z, block);
            }
            for (y = height + 1; y <= WATER_LEVEL; ++y) {
                chunk_set(chunk, base_x, base_z, x, y, z, BLOCK_WATER);
            }
        }
    }
    /* Trees are chosen per world column, so a two-block margin reproduces the
     * parts of neighbouring trees that cross into this chunk. */
    for (lx = -2; lx < CHUNK_SIZE + 2; ++lx) {
        for (lz = -2; lz < CHUNK_SIZE + 2; ++lz) {
            const int x = base_x + lx;
            const int z = base_z + lz;
            const int height = terrain_height(x, z);
            const int biome = biome_at(x, z);
            uint32_t roll;
            if (height <= WATER_LEVEL + 1 || height >= 16) {
                continue;
            }
            if (x > -4 && x < 4 && z > -4 && z < 4) {
                continue; /* keep a clearing at the origin */
            }
            roll = hash2(x, z, g_world_seed ^ 0x77u) % 1000u;
            if (biome == BIOME_FOREST) {
                if (roll < 50u) {
                    if ((roll & 1u) != 0u) {
                        plant_birch(chunk, base_x, base_z, x, height, z);
                    } else {
                        plant_oak(chunk, base_x, base_z, x, height, z);
                    }
                }
            } else if (biome == BIOME_SNOW) {
                if (roll < 30u) {
                    plant_spruce(chunk, base_x, base_z, x, height, z);
                }
            } else if (biome == BIOME_DESERT) {
                if (roll < 25u) {
                    plant_cactus(chunk, base_x, base_z, x, height, z);
                }
            } else if (roll < 18u) {
                plant_oak(chunk, base_x, base_z, x, height, z);
            }
        }
    }
    /* Shrubs and flowers on open grass. */
    for (lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (lz = 0; lz < CHUNK_SIZE; ++lz) {
            const int x = base_x + lx;
            const int z = base_z + lz;
            const int height = terrain_height(x, z);
            uint32_t roll;
            if (height <= WATER_LEVEL + 1 || height >= 15) {
                continue;
            }
            if (chunk_get(chunk, base_x, base_z, x, height, z) !=
                BLOCK_GRASS) {
                continue;
            }
            if (chunk_get(chunk, base_x, base_z, x, height + 1, z) !=
                BLOCK_AIR) {
                continue;
            }
            roll = hash2(x, z, g_world_seed ^ 0x1234u) % 1000u;
            if (roll < 25u) {
                chunk_set(chunk, base_x, base_z, x, height + 1, z,
                          BLOCK_BUSH);
            } else if (roll < 45u) {
                chunk_set(chunk, base_x, base_z, x, height + 1, z,
                          BLOCK_FLOWER);
            }
        }
    }
    for (index = 0; index < g_edit_count; ++index) {
        const edit_t *edit = &g_edits[index];
        if ((edit->x >> CHUNK_BITS) == cx &&
            (edit->z >> CHUNK_BITS) == cz) {
            chunk_set(chunk, base_x, base_z, edit->x, edit->y, edit->z,
                      edit->block);
        }
    }
    touch_chunk_revision(chunk);
}

static void ensure_at(int px, int pz, int generate_now) {
    const int pcx = px >> CHUNK_BITS;
    const int pcz = pz >> CHUNK_BITS;
    const int new_cx = pcx - GRID_W / 2;
    const int new_cz = pcz - GRID_W / 2;
    chunk_t *new_grid[GRID_W][GRID_W];
    uint8_t changed[GRID_W][GRID_W];
    uint8_t used[GRID_COUNT];
    int i;
    int j;
    int k;
    if (g_grid_ready && new_cx == g_chunk_origin_cx &&
        new_cz == g_chunk_origin_cz) {
        return;
    }
    for (k = 0; k < GRID_COUNT; ++k) {
        used[k] = 0;
    }
    for (j = 0; j < GRID_W; ++j) {
        for (i = 0; i < GRID_W; ++i) {
            const int cx = new_cx + i;
            const int cz = new_cz + j;
            chunk_t *found = (chunk_t *)0;
            for (k = 0; k < GRID_COUNT; ++k) {
                if ((g_chunk_pool[k].loaded || g_chunk_pending[k]) &&
                    g_chunk_pool[k].cx == cx && g_chunk_pool[k].cz == cz) {
                    found = &g_chunk_pool[k];
                    used[k] = 1;
                    break;
                }
            }
            new_grid[j][i] = found;
            changed[j][i] = 0;
        }
    }
    for (j = 0; j < GRID_W; ++j) {
        for (i = 0; i < GRID_W; ++i) {
            chunk_t *chunk;
            const int cx = new_cx + i;
            const int cz = new_cz + j;
            if (new_grid[j][i] != (chunk_t *)0) {
                continue;
            }
            for (k = 0; k < GRID_COUNT; ++k) {
                if (!used[k]) {
                    break;
                }
            }
            used[k] = 1;
            chunk = &g_chunk_pool[k];
            chunk->cx = (int16_t)cx;
            chunk->cz = (int16_t)cz;
            chunk->loaded = 0;
            g_chunk_pending[k] = 1;
            if (generate_now) {
                generate_chunk(chunk, cx, cz);
                chunk->loaded = 1;
                g_chunk_pending[k] = 0;
            }
            new_grid[j][i] = chunk;
            changed[j][i] = 1;
        }
    }
    for (j = 0; j < GRID_W; ++j) {
        for (i = 0; i < GRID_W; ++i) {
            g_chunk_grid[j][i] = new_grid[j][i];
        }
    }
    g_chunk_origin_cx = (int16_t)new_cx;
    g_chunk_origin_cz = (int16_t)new_cz;
    g_grid_ready = 1;
    /* Rebuild old border meshes when a newly assigned neighbor appears. A
     * pending chunk first exposes their border; loading it invalidates them
     * again so hidden and transparent-material faces are updated. */
    for (j = 0; j < GRID_W; ++j) {
        for (i = 0; i < GRID_W; ++i) {
            static const int8_t dx[4] = {-1, 1, 0, 0};
            static const int8_t dz[4] = {0, 0, -1, 1};
            int direction;
            if (!changed[j][i]) continue;
            for (direction = 0; direction < 4; ++direction) {
                const int nx = i + dx[direction];
                const int nz = j + dz[direction];
                chunk_t *neighbor;
                if ((unsigned)nx >= GRID_W || (unsigned)nz >= GRID_W)
                    continue;
                neighbor = g_chunk_grid[nz][nx];
                if (neighbor != NULL && neighbor->loaded)
                    touch_chunk_revision(neighbor);
            }
        }
    }
}

int game_pending_chunk_count(void) {
    int count = 0;
    int index;
    for (index = 0; index < GRID_COUNT; ++index) {
        if (g_chunk_pending[index]) ++count;
    }
    return count;
}

int game_stream_chunks(int limit) {
    int generated = 0;
    while (generated < limit) {
        int best_i = -1;
        int best_j = -1;
        int best_distance = 0x7fffffff;
        int i;
        int j;
        for (j = 0; j < GRID_W; ++j) {
            for (i = 0; i < GRID_W; ++i) {
                chunk_t *chunk = g_chunk_grid[j][i];
                int pool_index;
                int dx;
                int dz;
                int distance;
                if (chunk == NULL) continue;
                pool_index = (int)(chunk - g_chunk_pool);
                if ((unsigned)pool_index >= (unsigned)GRID_COUNT ||
                    !g_chunk_pending[pool_index]) {
                    continue;
                }
                dx = i - GRID_W / 2;
                dz = j - GRID_W / 2;
                distance = dx * dx + dz * dz;
                if (distance < best_distance) {
                    best_distance = distance;
                    best_i = i;
                    best_j = j;
                }
            }
        }
        if (best_i < 0) break;
        {
            chunk_t *chunk = g_chunk_grid[best_j][best_i];
            const int pool_index = (int)(chunk - g_chunk_pool);
            generate_chunk(chunk, chunk->cx, chunk->cz);
            chunk->loaded = 1;
            g_chunk_pending[pool_index] = 0;
            {
                static const int8_t dx[4] = {-1, 1, 0, 0};
                static const int8_t dz[4] = {0, 0, -1, 1};
                int direction;
                for (direction = 0; direction < 4; ++direction) {
                    const int nx = best_i + dx[direction];
                    const int nz = best_j + dz[direction];
                    chunk_t *neighbor;
                    if ((unsigned)nx >= GRID_W || (unsigned)nz >= GRID_W)
                        continue;
                    neighbor = g_chunk_grid[nz][nx];
                    if (neighbor != NULL && neighbor->loaded)
                        touch_chunk_revision(neighbor);
                }
            }
        }
        ++generated;
    }
    return generated;
}

static void regenerate_chunks_at(int px, int pz);

uint32_t game_seed(void) { return g_world_seed; }

static void put_u32_at(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32_at(const uint8_t *value) {
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

#define SAVE_HEADER_BYTES 28
#define SAVE_EDIT_BYTES 11

int game_serialize(const player_t *player, uint8_t *out, int capacity) {
    const int need = SAVE_HEADER_BYTES + g_edit_count * SAVE_EDIT_BYTES;
    int index;
    if (out == NULL || player == NULL || capacity < need) {
        return 0;
    }
    put_u32_at(out, g_world_seed);
    put_u32_at(out + 4, (uint32_t)(int32_t)(player->x * 100.0F));
    put_u32_at(out + 8, (uint32_t)(int32_t)(player->y * 100.0F));
    put_u32_at(out + 12, (uint32_t)(int32_t)(player->z * 100.0F));
    put_u32_at(out + 16, (uint32_t)(int32_t)(player->yaw * 1000.0F));
    put_u32_at(out + 20, (uint32_t)(int32_t)(player->pitch * 1000.0F));
    out[24] = (uint8_t)g_edit_count;
    out[25] = (uint8_t)(g_edit_count >> 8);
    out[26] = 0;
    out[27] = 0;
    for (index = 0; index < g_edit_count; ++index) {
        uint8_t *entry = out + SAVE_HEADER_BYTES + index * SAVE_EDIT_BYTES;
        put_u32_at(entry, (uint32_t)g_edits[index].x);
        entry[4] = (uint8_t)g_edits[index].y;
        entry[5] = (uint8_t)((uint16_t)g_edits[index].y >> 8);
        put_u32_at(entry + 6, (uint32_t)g_edits[index].z);
        entry[10] = g_edits[index].block;
    }
    return need;
}

int game_deserialize(const uint8_t *data, int length, player_t *player) {
    uint32_t seed;
    int count;
    int index;
    if (data == NULL || length < SAVE_HEADER_BYTES) {
        return 0;
    }
    seed = get_u32_at(data);
    count = data[24] | ((int)data[25] << 8);
    if (count > MAX_EDITS || length < SAVE_HEADER_BYTES + count * SAVE_EDIT_BYTES) {
        return 0;
    }
    game_generate(seed);
    g_edit_count = 0;
    for (index = 0; index < count; ++index) {
        const uint8_t *entry =
            data + SAVE_HEADER_BYTES + index * SAVE_EDIT_BYTES;
        edit_t *edit = &g_edits[g_edit_count++];
        edit->x = (int32_t)get_u32_at(entry);
        edit->y = (int16_t)((uint16_t)entry[4] | ((uint16_t)entry[5] << 8));
        edit->z = (int32_t)get_u32_at(entry + 6);
        edit->block = entry[10];
    }
    if (player != NULL) {
        player->x = (float)(int32_t)get_u32_at(data + 4) / 100.0F;
        player->y = (float)(int32_t)get_u32_at(data + 8) / 100.0F;
        player->z = (float)(int32_t)get_u32_at(data + 12) / 100.0F;
        player->yaw = (float)(int32_t)get_u32_at(data + 16) / 1000.0F;
        player->pitch = (float)(int32_t)get_u32_at(data + 20) / 1000.0F;
        player->vx = 0.0F;
        player->vy = 0.0F;
        player->vz = 0.0F;
        player->on_ground = 0;
        player->flying = 0;
        player->in_water = 0;
        regenerate_chunks_at(rc_floor_int(player->x),
                             rc_floor_int(player->z));
    }
    return 1;
}

void game_ensure_chunks(const player_t *player) {
    ensure_at(rc_floor_int(player->x), rc_floor_int(player->z), 1);
    while (game_stream_chunks(GRID_COUNT) != 0) {
    }
}

/* Drops every resident chunk and regenerates around a position. Used after a
 * load so the replayed edit log is applied to fresh chunks. */
static void regenerate_chunks_at(int px, int pz) {
    int index;
    for (index = 0; index < GRID_COUNT; ++index) {
        g_chunk_pool[index].loaded = 0;
        g_chunk_pending[index] = 0;
    }
    g_grid_ready = 0;
    ensure_at(px, pz, 1);
}

void game_generate(uint32_t seed) {
    int index;
    g_world_seed = seed;
    g_edit_count = 0;
    g_grid_ready = 0;
    for (index = 0; index < GRID_COUNT; ++index) {
        g_chunk_pool[index].loaded = 0;
        g_chunk_pending[index] = 0;
    }
    ensure_at(0, 0, 1);
}

void game_spawn(player_t *player) {
    int best_x = 0;
    int best_z = 0;
    int best_h = terrain_height(0, 0);
    int fallback_x = 0;
    int fallback_z = 0;
    int fallback_h = 0;
    int has_fallback = 0;
    int radius;
    if (best_h <= WATER_LEVEL) {
        int found = 0;
        for (radius = 1; radius < 24 && !found; ++radius) {
            int dx;
            int dz;
            for (dz = -radius; dz <= radius && !found; ++dz) {
                for (dx = -radius; dx <= radius && !found; ++dx) {
                    int h;
                    if (dx != -radius && dx != radius && dz != -radius &&
                        dz != radius) {
                        continue;
                    }
                    h = terrain_height(dx, dz);
                    if (h <= WATER_LEVEL) {
                        continue;
                    }
                    if (!has_fallback) {
                        fallback_x = dx;
                        fallback_z = dz;
                        fallback_h = h;
                        has_fallback = 1;
                    }
                    if (biome_at(dx, dz) != BIOME_DESERT) {
                        best_x = dx;
                        best_z = dz;
                        best_h = h;
                        found = 1;
                    }
                }
            }
        }
        if (!found && has_fallback) {
            best_x = fallback_x;
            best_z = fallback_z;
            best_h = fallback_h;
        }
    }
    player->x = (float)best_x + 0.5F;
    player->y = (float)(best_h + 1);
    player->z = (float)best_z + 0.5F;
    player->vx = 0.0F;
    player->vy = 0.0F;
    player->vz = 0.0F;
    player->yaw = 0.0F;
    player->pitch = 0.0F;
    player->on_ground = 0;
    player->flying = 0;
    player->in_water = 0;
    ensure_at(best_x, best_z, 1);
}

static int box_blocked_sized(float x, float y, float z, float half,
                             float height) {
    const float min_x = x - half;
    const float max_x = x + half - 0.001F;
    const float min_y = y;
    const float max_y = y + height - 0.001F;
    const float min_z = z - half;
    const float max_z = z + half - 0.001F;
    int bx;
    int by;
    int bz;
    for (bx = rc_floor_int(min_x); bx <= rc_floor_int(max_x); ++bx) {
        for (by = rc_floor_int(min_y); by <= rc_floor_int(max_y); ++by) {
            for (bz = rc_floor_int(min_z); bz <= rc_floor_int(max_z); ++bz) {
                if (game_solid(bx, by, bz)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int box_blocked(float x, float y, float z) {
    return box_blocked_sized(x, y, z, PLAYER_HALF, PLAYER_HEIGHT);
}

static int water_at(float x, float y, float z) {
    return game_block_fast(rc_floor_int(x), rc_floor_int(y),
                           rc_floor_int(z)) == BLOCK_WATER;
}

void game_step(player_t *player, float dt, float move_x, float move_z,
               int jump, int ascend, int descend) {
    const float cy = rc_cos(player->yaw);
    const float sy = rc_sin(player->yaw);
    const float length = rc_sqrt(move_x * move_x + move_z * move_z);
    float speed;
    if (length > 1.0F) {
        move_x /= length;
        move_z /= length;
    }
    player->in_water = water_at(player->x, player->y + 0.2F, player->z);
    speed = player->flying ? 9.0F : (player->in_water ? 2.8F : 4.3F);
    player->vx = (sy * move_z + cy * move_x) * speed;
    player->vz = (cy * move_z - sy * move_x) * speed;
    if (player->flying) {
        player->vy = (ascend ? 6.0F : 0.0F) - (descend ? 6.0F : 0.0F);
        player->on_ground = 0;
    } else if (player->in_water) {
        if (ascend) {
            player->vy = 3.0F;
        } else {
            player->vy -= 9.0F * dt;
        }
        if (player->vy < -2.6F) {
            player->vy = -2.6F;
        }
    } else {
        player->vy -= 26.0F * dt;
        if (player->vy < -45.0F) {
            player->vy = -45.0F;
        }
        if (jump && player->on_ground) {
            player->vy = 8.2F;
            player->on_ground = 0;
        }
    }

    {
        const float next_x = player->x + player->vx * dt;
        if (!box_blocked(next_x, player->y, player->z)) {
            player->x = next_x;
        } else if (player->on_ground &&
                   !box_blocked(next_x, player->y + 1.0F, player->z)) {
            player->x = next_x;
            player->y += 1.0F;
        } else {
            player->vx = 0.0F;
        }
    }
    {
        const float next_z = player->z + player->vz * dt;
        if (!box_blocked(player->x, player->y, next_z)) {
            player->z = next_z;
        } else if (player->on_ground &&
                   !box_blocked(player->x, player->y + 1.0F, next_z)) {
            player->z = next_z;
            player->y += 1.0F;
        } else {
            player->vz = 0.0F;
        }
    }
    {
        const float next_y = player->y + player->vy * dt;
        if (!box_blocked(player->x, next_y, player->z)) {
            player->y = next_y;
            if (player->vy < 0.0F) {
                player->on_ground = 0;
            }
        } else {
            if (player->vy < 0.0F) {
                player->on_ground = 1;
            }
            player->vy = 0.0F;
        }
    }

    if (player->x > 500000.0F) {
        player->x = 500000.0F;
    } else if (player->x < -500000.0F) {
        player->x = -500000.0F;
    }
    if (player->z > 500000.0F) {
        player->z = 500000.0F;
    } else if (player->z < -500000.0F) {
        player->z = -500000.0F;
    }
    if (player->y > (float)CHUNK_HEIGHT - 2.2F) {
        player->y = (float)CHUNK_HEIGHT - 2.2F;
        if (player->vy > 0.0F) {
            player->vy = 0.0F;
        }
    }
    if (player->y < -8.0F) {
        game_spawn(player);
    }
    ensure_at(rc_floor_int(player->x), rc_floor_int(player->z), 0);
}

void game_raycast(float origin_x, float origin_y, float origin_z, float dir_x,
                  float dir_y, float dir_z, float max_distance,
                  ray_hit_t *hit) {
    int map_x = rc_floor_int(origin_x);
    int map_y = rc_floor_int(origin_y);
    int map_z = rc_floor_int(origin_z);
    const int step_x = dir_x > 0.0F ? 1 : -1;
    const int step_y = dir_y > 0.0F ? 1 : -1;
    const int step_z = dir_z > 0.0F ? 1 : -1;
    const float delta_x = dir_x != 0.0F ? rc_fabs(1.0F / dir_x) : 1.0e30F;
    const float delta_y = dir_y != 0.0F ? rc_fabs(1.0F / dir_y) : 1.0e30F;
    const float delta_z = dir_z != 0.0F ? rc_fabs(1.0F / dir_z) : 1.0e30F;
    float side_x = dir_x > 0.0F ? ((float)map_x + 1.0F - origin_x) * delta_x
                                : (origin_x - (float)map_x) * delta_x;
    float side_y = dir_y > 0.0F ? ((float)map_y + 1.0F - origin_y) * delta_y
                                : (origin_y - (float)map_y) * delta_y;
    float side_z = dir_z > 0.0F ? ((float)map_z + 1.0F - origin_z) * delta_z
                                : (origin_z - (float)map_z) * delta_z;
    int face = 0;
    int sign = 1;
    int step;
    hit->hit = 0;
    for (step = 0; step < 160; ++step) {
        float distance;
        int block;
        if (side_x <= side_y && side_x <= side_z) {
            map_x += step_x;
            distance = side_x;
            side_x += delta_x;
            face = 0;
            sign = step_x;
        } else if (side_y <= side_z) {
            map_y += step_y;
            distance = side_y;
            side_y += delta_y;
            face = 1;
            sign = step_y;
        } else {
            map_z += step_z;
            distance = side_z;
            side_z += delta_z;
            face = 2;
            sign = step_z;
        }
        if (distance > max_distance) {
            return;
        }
        if ((unsigned)map_y >= (unsigned)CHUNK_HEIGHT ||
            !game_in_grid(map_x, map_z)) {
            return;
        }
        block = game_block_fast(map_x, map_y, map_z);
        if (block == BLOCK_WATER) {
            /* Targeting ignores water so submerged blocks stay mineable. */
            continue;
        }
        if (block != BLOCK_AIR) {
            hit->hit = 1;
            hit->face = (uint8_t)face;
            hit->x = (int16_t)map_x;
            hit->y = (int16_t)map_y;
            hit->z = (int16_t)map_z;
            hit->place_x = (int16_t)(map_x - (face == 0 ? sign : 0));
            hit->place_y = (int16_t)(map_y - (face == 1 ? sign : 0));
            hit->place_z = (int16_t)(map_z - (face == 2 ? sign : 0));
            return;
        }
    }
}

int game_target_block(const player_t *player, ray_hit_t *hit) {
    const float cy = rc_cos(player->yaw);
    const float sy = rc_sin(player->yaw);
    const float cp = rc_cos(player->pitch);
    const float sp = rc_sin(player->pitch);
    game_raycast(player->x, player->y + EYE_HEIGHT, player->z, sy * cp, sp,
                 cy * cp, REACH_DISTANCE, hit);
    return hit->hit;
}

int game_break_block(player_t *player) {
    ray_hit_t hit;
    if (!game_target_block(player, &hit)) {
        return 0;
    }
    if (game_block(hit.x, hit.y, hit.z) == BLOCK_BEDROCK) {
        return 0;
    }
    game_set_block(hit.x, hit.y, hit.z, BLOCK_AIR);
    return 1;
}

int game_point_inside_player(float x, float y, float z,
                             const player_t *player) {
    return x + 1.0F > player->x - PLAYER_HALF &&
           x < player->x + PLAYER_HALF &&
           y + 1.0F > player->y &&
           y < player->y + PLAYER_HEIGHT &&
           z + 1.0F > player->z - PLAYER_HALF &&
           z < player->z + PLAYER_HALF;
}

int game_place_block(player_t *player, int block) {
    ray_hit_t hit;
    int existing;
    if (block == BLOCK_AIR || block >= BLOCK_TYPE_COUNT) {
        return 0;
    }
    if (!game_target_block(player, &hit)) {
        return 0;
    }
    existing = game_block(hit.place_x, hit.place_y, hit.place_z);
    if (existing != BLOCK_AIR && existing != BLOCK_WATER) {
        return 0;
    }
    if (game_point_inside_player((float)hit.place_x + 0.5F,
                                 (float)hit.place_y + 0.5F,
                                 (float)hit.place_z + 0.5F, player)) {
        return 0;
    }
    game_set_block(hit.place_x, hit.place_y, hit.place_z, block);
    return 1;
}

/* --- inventory and crafting --------------------------------------------- */

void game_inventory_init(void) {
    int index;
    for (index = 0; index < INV_SLOTS; ++index) {
        g_inventory[index].item = BLOCK_AIR;
        g_inventory[index].count = 0;
    }
    for (index = 0; index < CRAFT_SLOTS; ++index) {
        g_craft[index].item = BLOCK_AIR;
        g_craft[index].count = 0;
    }
    g_craft_result.item = BLOCK_AIR;
    g_craft_result.count = 0;
    for (index = 0; index < TABLE_CRAFT_SLOTS; ++index) {
        g_table_craft[index].item = BLOCK_AIR;
        g_table_craft[index].count = 0;
    }
    g_table_result.item = BLOCK_AIR;
    g_table_result.count = 0;
    /* A small starter kit so building is possible before the first mine. */
    (void)game_inventory_add(BLOCK_PLANK, 16);
    (void)game_inventory_add(BLOCK_GLASS, 8);
    (void)game_inventory_add(BLOCK_STONE, 8);
    (void)game_inventory_add(BLOCK_BRICK, 4);
    (void)game_inventory_add(ITEM_WOOD_PICK, 1);
    (void)game_inventory_add(ITEM_WOOD_AXE, 1);
}

int game_inventory_add(int item, int count) {
    int index;
    if (item <= BLOCK_AIR || item >= BLOCK_TYPE_COUNT || count <= 0) {
        return 0;
    }
    for (index = 0; index < INV_SLOTS && count > 0; ++index) {
        item_stack_t *slot = &g_inventory[index];
        if (slot->item == item && slot->count < ITEM_MAX_STACK) {
            const int room = ITEM_MAX_STACK - slot->count;
            const int take = count < room ? count : room;
            slot->count = (uint8_t)(slot->count + take);
            count -= take;
        }
    }
    for (index = 0; index < INV_SLOTS && count > 0; ++index) {
        item_stack_t *slot = &g_inventory[index];
        if (slot->item == BLOCK_AIR) {
            const int take =
                count < ITEM_MAX_STACK ? count : ITEM_MAX_STACK;
            slot->item = (uint8_t)item;
            slot->count = (uint8_t)take;
            count -= take;
        }
    }
    return count == 0;
}

int game_inventory_remove(int slot, int count) {
    item_stack_t *value;
    if (slot < 0 || slot >= INV_SLOTS || count <= 0) {
        return 0;
    }
    value = &g_inventory[slot];
    if (value->count < count) {
        return 0;
    }
    value->count = (uint8_t)(value->count - count);
    if (value->count == 0) {
        value->item = BLOCK_AIR;
    }
    return 1;
}

typedef struct {
    uint8_t item;
    uint8_t count;
    uint8_t result;
    uint8_t result_count;
    uint8_t needs_table;
} recipe_t;

/* Shape-insensitive recipes: the grid only needs to contain the counts. */
static const recipe_t kRecipes[] = {
    {BLOCK_PLANK, 4, BLOCK_TABLE, 1, 0},
    {BLOCK_WOOD, 1, BLOCK_PLANK, 4, 0},
    {BLOCK_SAND, 4, BLOCK_GLASS, 1, 0},
    {BLOCK_COBBLE, 4, BLOCK_STONE, 4, 0},
    {BLOCK_LEAVES, 4, BLOCK_DIRT, 1, 0},
    {BLOCK_DIRT, 4, BLOCK_GRASS, 1, 0},
    {BLOCK_COBBLE, 9, BLOCK_BEDROCK, 1, 1},
};

#define RECIPE_COUNT ((int)(sizeof(kRecipes) / sizeof(kRecipes[0])))

/* Shaped 3x3 recipes, matched with the pattern's bounding box so the shape can
 * sit anywhere in the grid. Used by the crafting table only. */
typedef struct {
    uint8_t pattern[9];
    uint8_t result;
} shaped_recipe_t;

#define S_P  BLOCK_PLANK
#define S_C  BLOCK_COBBLE
#define S_0  BLOCK_AIR

static const shaped_recipe_t kShapedRecipes[] = {
    /* Wooden tools. */
    {{S_P, S_P, S_P, S_0, S_P, S_0, S_0, S_P, S_0}, ITEM_WOOD_PICK},
    {{S_0, S_P, S_P, S_0, S_P, S_P, S_0, S_P, S_0}, ITEM_WOOD_AXE},
    {{S_0, S_P, S_0, S_0, S_P, S_0, S_0, S_P, S_0}, ITEM_WOOD_SHOVEL},
    {{S_0, S_0, S_P, S_0, S_P, S_0, S_P, S_0, S_0}, ITEM_WOOD_SWORD},
    /* Stone tools. */
    {{S_C, S_C, S_C, S_0, S_P, S_0, S_0, S_P, S_0}, ITEM_STONE_PICK},
    {{S_0, S_C, S_C, S_0, S_C, S_C, S_0, S_P, S_0}, ITEM_STONE_AXE},
    {{S_0, S_C, S_0, S_0, S_P, S_0, S_0, S_P, S_0}, ITEM_STONE_SHOVEL},
    {{S_0, S_0, S_C, S_0, S_C, S_0, S_P, S_0, S_0}, ITEM_STONE_SWORD},
};

#define SHAPED_RECIPE_COUNT \
    ((int)(sizeof(kShapedRecipes) / sizeof(kShapedRecipes[0])))

static int shape_bbox(const uint8_t *cells, int *row0, int *row1, int *col0,
                      int *col1) {
    int row;
    int column;
    int found = 0;
    *row0 = 3;
    *row1 = -1;
    *col0 = 3;
    *col1 = -1;
    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column) {
            if (cells[row * 3 + column] == BLOCK_AIR) {
                continue;
            }
            found = 1;
            if (row < *row0) {
                *row0 = row;
            }
            if (row > *row1) {
                *row1 = row;
            }
            if (column < *col0) {
                *col0 = column;
            }
            if (column > *col1) {
                *col1 = column;
            }
        }
    }
    return found;
}

static int shaped_match(const item_stack_t *slots, int *out_recipe) {
    uint8_t cells[9];
    int grid_r0;
    int grid_r1;
    int grid_c0;
    int grid_c1;
    int index;
    int cell;
    for (cell = 0; cell < 9; ++cell) {
        cells[cell] = game_item_is_block(slots[cell].item)
                          ? (uint8_t)slots[cell].item
                          : BLOCK_AIR;
    }
    if (!shape_bbox(cells, &grid_r0, &grid_r1, &grid_c0, &grid_c1)) {
        return 0;
    }
    for (index = 0; index < SHAPED_RECIPE_COUNT; ++index) {
        const shaped_recipe_t *recipe = &kShapedRecipes[index];
        int recipe_r0;
        int recipe_r1;
        int recipe_c0;
        int recipe_c1;
        int row;
        int column;
        int ok = 1;
        if (!shape_bbox(recipe->pattern, &recipe_r0, &recipe_r1, &recipe_c0,
                        &recipe_c1)) {
            continue;
        }
        if (grid_r1 - grid_r0 != recipe_r1 - recipe_r0 ||
            grid_c1 - grid_c0 != recipe_c1 - recipe_c0) {
            continue;
        }
        for (row = 0; row <= recipe_r1 - recipe_r0 && ok; ++row) {
            for (column = 0; column <= recipe_c1 - recipe_c0; ++column) {
                const int grid_cell =
                    cells[(grid_r0 + row) * 3 + grid_c0 + column];
                const int pattern_cell =
                    recipe->pattern[(recipe_r0 + row) * 3 + recipe_c0 +
                                    column];
                if (grid_cell != pattern_cell) {
                    ok = 0;
                    break;
                }
            }
        }
        if (ok) {
            *out_recipe = index;
            return 1;
        }
    }
    return 0;
}

static int shaped_take(item_stack_t *slots, int *out_item) {
    uint8_t cells[9];
    int grid_r0;
    int grid_r1;
    int grid_c0;
    int grid_c1;
    int index;
    int cell;
    for (cell = 0; cell < 9; ++cell) {
        cells[cell] = game_item_is_block(slots[cell].item)
                          ? (uint8_t)slots[cell].item
                          : BLOCK_AIR;
    }
    if (!shape_bbox(cells, &grid_r0, &grid_r1, &grid_c0, &grid_c1)) {
        return 0;
    }
    for (index = 0; index < SHAPED_RECIPE_COUNT; ++index) {
        const shaped_recipe_t *recipe = &kShapedRecipes[index];
        int recipe_r0;
        int recipe_r1;
        int recipe_c0;
        int recipe_c1;
        int row;
        int column;
        int ok = 1;
        if (!shape_bbox(recipe->pattern, &recipe_r0, &recipe_r1, &recipe_c0,
                        &recipe_c1)) {
            continue;
        }
        if (grid_r1 - grid_r0 != recipe_r1 - recipe_r0 ||
            grid_c1 - grid_c0 != recipe_c1 - recipe_c0) {
            continue;
        }
        for (row = 0; row <= recipe_r1 - recipe_r0 && ok; ++row) {
            for (column = 0; column <= recipe_c1 - recipe_c0; ++column) {
                if (cells[(grid_r0 + row) * 3 + grid_c0 + column] !=
                    recipe->pattern[(recipe_r0 + row) * 3 + recipe_c0 +
                                    column]) {
                    ok = 0;
                    break;
                }
            }
        }
        if (!ok) {
            continue;
        }
        /* Consume one item from every cell covered by the shape. */
        for (row = 0; row <= recipe_r1 - recipe_r0; ++row) {
            for (column = 0; column <= recipe_c1 - recipe_c0; ++column) {
                if (recipe->pattern[(recipe_r0 + row) * 3 + recipe_c0 +
                                    column] == BLOCK_AIR) {
                    continue;
                }
                {
                    item_stack_t *slot =
                        &slots[(grid_r0 + row) * 3 + grid_c0 + column];
                    if (slot->count > 0) {
                        --slot->count;
                        if (slot->count == 0) {
                            slot->item = BLOCK_AIR;
                        }
                    }
                }
            }
        }
        *out_item = recipe->result;
        return 1;
    }
    return 0;
}

static int craft_match(const int *counts, int table) {
    int best = -1;
    int index;
    for (index = 0; index < RECIPE_COUNT; ++index) {
        if (kRecipes[index].needs_table && !table) {
            continue;
        }
        if (counts[kRecipes[index].item] < kRecipes[index].count) {
            continue;
        }
        /* Prefer the most specific recipe so 9 cobble becomes bedrock while
         * 4 cobble becomes stone. */
        if (best < 0 || kRecipes[index].count > kRecipes[best].count) {
            best = index;
        }
    }
    return best;
}

static void craft_counts(const item_stack_t *slots, int slot_count,
                         int *counts) {
    int index;
    for (index = 0; index < BLOCK_TYPE_COUNT; ++index) {
        counts[index] = 0;
    }
    for (index = 0; index < slot_count; ++index) {
        if (slots[index].item < BLOCK_TYPE_COUNT) {
            counts[slots[index].item] += slots[index].count;
        }
    }
}

static int craft_update_slots(const item_stack_t *slots, int slot_count,
                              item_stack_t *result, int table) {
    int counts[BLOCK_TYPE_COUNT];
    int match;
    if (table && slot_count == TABLE_CRAFT_SLOTS) {
        int shaped;
        if (shaped_match(slots, &shaped)) {
            result->item = kShapedRecipes[shaped].result;
            result->count = 1;
            return 1;
        }
    }
    craft_counts(slots, slot_count, counts);
    match = craft_match(counts, table);
    if (match < 0) {
        result->item = BLOCK_AIR;
        result->count = 0;
        return 0;
    }
    result->item = kRecipes[match].result;
    result->count = kRecipes[match].result_count;
    return 1;
}

static int craft_take_slots(item_stack_t *slots, int slot_count,
                            item_stack_t *result, int table, uint8_t *out_item,
                            uint8_t *out_count) {
    int counts[BLOCK_TYPE_COUNT];
    int match;
    int index;
    if (table && slot_count == TABLE_CRAFT_SLOTS) {
        int shaped_item;
        if (shaped_take(slots, &shaped_item)) {
            *out_item = (uint8_t)shaped_item;
            *out_count = 1;
            craft_update_slots(slots, slot_count, result, table);
            return 1;
        }
    }
    craft_counts(slots, slot_count, counts);
    match = craft_match(counts, table);
    if (match < 0) {
        result->item = BLOCK_AIR;
        result->count = 0;
        return 0;
    }
    {
        int remaining = kRecipes[match].count;
        const int item = kRecipes[match].item;
        for (index = 0; index < slot_count && remaining > 0; ++index) {
            item_stack_t *slot = &slots[index];
            if (slot->item != item || slot->count == 0) {
                continue;
            }
            if (slot->count <= remaining) {
                remaining -= slot->count;
                slot->count = 0;
                slot->item = BLOCK_AIR;
            } else {
                slot->count = (uint8_t)(slot->count - remaining);
                remaining = 0;
            }
        }
    }
    *out_item = kRecipes[match].result;
    *out_count = kRecipes[match].result_count;
    craft_update_slots(slots, slot_count, result, table);
    return 1;
}

void game_craft_update(void) {
    (void)craft_update_slots(g_craft, CRAFT_SLOTS, &g_craft_result, 0);
}

int game_craft_take(uint8_t *out_item, uint8_t *out_count) {
    return craft_take_slots(g_craft, CRAFT_SLOTS, &g_craft_result, 0,
                            out_item, out_count);
}

void game_table_craft_update(void) {
    (void)craft_update_slots(g_table_craft, TABLE_CRAFT_SLOTS,
                             &g_table_result, 1);
}

int game_table_craft_take(uint8_t *out_item, uint8_t *out_count) {
    return craft_take_slots(g_table_craft, TABLE_CRAFT_SLOTS,
                            &g_table_result, 1, out_item, out_count);
}

/* --- particles ---------------------------------------------------------- */

void game_spawn_particles(float x, float y, float z, int block, int count) {
    int index;
    for (index = 0; index < count; ++index) {
        int slot;
        for (slot = 0; slot < MAX_PARTICLES; ++slot) {
            if (g_particles[slot].life <= 0.0F) {
                break;
            }
        }
        if (slot == MAX_PARTICLES) {
            return;
        }
        g_particles[slot].x = x + rand_unit() * 0.35F;
        g_particles[slot].y = y + rand_unit() * 0.35F;
        g_particles[slot].z = z + rand_unit() * 0.35F;
        g_particles[slot].vx = rand_unit() * 3.0F;
        g_particles[slot].vy = 1.5F + rand_unit() * 2.5F;
        g_particles[slot].vz = rand_unit() * 3.0F;
        g_particles[slot].life = 0.45F + (float)(game_rand() & 31u) * 0.012F;
        g_particles[slot].block = (uint8_t)block;
    }
}

void game_update_particles(float dt) {
    int index;
    for (index = 0; index < MAX_PARTICLES; ++index) {
        particle_t *particle = &g_particles[index];
        float next_x;
        float next_y;
        float next_z;
        if (particle->life <= 0.0F) {
            continue;
        }
        particle->life -= dt;
        if (particle->life <= 0.0F) {
            continue;
        }
        particle->vy -= 20.0F * dt;
        next_x = particle->x + particle->vx * dt;
        next_y = particle->y + particle->vy * dt;
        next_z = particle->z + particle->vz * dt;
        if (game_solid(rc_floor_int(next_x), rc_floor_int(particle->y),
                       rc_floor_int(particle->z))) {
            particle->vx = -particle->vx * 0.3F;
        } else {
            particle->x = next_x;
        }
        if (game_solid(rc_floor_int(particle->x), rc_floor_int(next_y),
                       rc_floor_int(particle->z))) {
            particle->vy = -particle->vy * 0.25F;
            particle->vx *= 0.7F;
            particle->vz *= 0.7F;
        } else {
            particle->y = next_y;
        }
        if (game_solid(rc_floor_int(particle->x),
                       rc_floor_int(particle->y),
                       rc_floor_int(next_z))) {
            particle->vz = -particle->vz * 0.3F;
        } else {
            particle->z = next_z;
        }
    }
}

/* --- mobs --------------------------------------------------------------- */

static int mob_drop_block(uint8_t kind) {
    switch (kind) {
        case MOB_SHEEP:
            return BLOCK_WOOL;
        case MOB_PIG:
        case MOB_COW:
            return BLOCK_DIRT;
        default:
            return BLOCK_LEAVES;
    }
}

static void mob_place(mob_t *mob, float x, float y, float z) {
    mob->x = x;
    mob->y = y;
    mob->z = z;
    mob->vx = 0.0F;
    mob->vy = 0.0F;
    mob->vz = 0.0F;
    mob->yaw = 0.0F;
    mob->speed = 0.0F;
    mob->health = 3.0F;
    mob->hurt = 0.0F;
    mob->knock = 0.0F;
    mob->wander = 0.5F + (float)(game_rand() & 127u) * 0.02F;
    mob->respawn = 0.0F;
    mob->alive = 1;
    mob->on_ground = 0;
}

static int find_grass_spot(uint32_t seed_offset, int center_x, int center_z,
                           int min_radius, int max_radius, float *out_x,
                           float *out_y, float *out_z) {
    const int span = max_radius - min_radius + 1;
    int attempt;
    if (span <= 0) {
        return 0;
    }
    for (attempt = 0; attempt < 64; ++attempt) {
        const uint32_t hx = hash2((int)seed_offset, attempt, 0xa53u);
        const uint32_t hz = hash2(attempt, (int)seed_offset, 0x1c9u);
        int dx = min_radius + (int)((hx >> 8) % (uint32_t)span);
        int dz = min_radius + (int)((hz >> 8) % (uint32_t)span);
        int x;
        int z;
        int height;
        if ((hx & 1u) != 0u) {
            dx = -dx;
        }
        if ((hz & 1u) != 0u) {
            dz = -dz;
        }
        x = center_x + dx;
        z = center_z + dz;
        height = terrain_height(x, z);
        if (height > WATER_LEVEL) {
            *out_x = (float)x + 0.5F;
            *out_y = (float)(height + 1);
            *out_z = (float)z + 0.5F;
            return 1;
        }
    }
    return 0;
}

void game_spawn_mobs(uint32_t seed, const player_t *player) {
    const int center_x = rc_floor_int(player->x);
    const int center_z = rc_floor_int(player->z);
    int index;
    for (index = 0; index < MAX_MOBS; ++index) {
        float x;
        float y;
        float z;
        if (!find_grass_spot(seed + (uint32_t)index * 17u, center_x,
                             center_z, 10, 34, &x, &y, &z)) {
            g_mobs[index].alive = 0;
            g_mobs[index].respawn = 1.0F;
            continue;
        }
        mob_place(&g_mobs[index], x, y, z);
        g_mobs[index].kind =
            (uint8_t)((index + (seed & 3u)) % MOB_KIND_COUNT);
        g_mobs[index].yaw = (float)(game_rand() % 628u) * 0.01F;
    }
}

static int mob_blocked(const mob_t *mob, float x, float y, float z) {
    return box_blocked_sized(x, y, z, MOB_HALF, MOB_HEIGHT);
}

static void mob_try_respawn(mob_t *mob, int index, const player_t *player) {
    float x;
    float y;
    float z;
    if (find_grass_spot((uint32_t)(index + 1) * 91u +
                            (uint32_t)(game_rand() & 255u),
                        rc_floor_int(player->x), rc_floor_int(player->z), 10,
                        34, &x, &y, &z)) {
        mob_place(mob, x, y, z);
        mob->kind = (uint8_t)(game_rand() % MOB_KIND_COUNT);
        mob->yaw = (float)(game_rand() % 628u) * 0.01F;
    } else {
        mob->respawn = 2.0F;
    }
}

void game_update_mobs(float dt, const player_t *player) {
    int index;
    for (index = 0; index < MAX_MOBS; ++index) {
        mob_t *mob = &g_mobs[index];
        if (!mob->alive) {
            mob->respawn -= dt;
            if (mob->respawn <= 0.0F) {
                mob_try_respawn(mob, index, player);
            }
            continue;
        }
        if (mob->x - player->x > 70.0F || player->x - mob->x > 70.0F ||
            mob->z - player->z > 70.0F || player->z - mob->z > 70.0F) {
            mob->alive = 0;
            mob->respawn = 0.0F;
            continue;
        }
        if (mob->hurt > 0.0F) {
            mob->hurt -= dt;
        }
        if (mob->knock > 0.0F) {
            mob->knock -= dt;
            mob->vx *= 0.88F;
            mob->vz *= 0.88F;
        } else {
            mob->wander -= dt;
            if (mob->wander <= 0.0F) {
                mob->wander = 1.4F + (float)(game_rand() % 260u) * 0.01F;
                if ((game_rand() & 3u) == 0u) {
                    mob->speed = 0.0F;
                } else {
                    mob->yaw = (float)(game_rand() % 628u) * 0.01F;
                    mob->speed = 1.05F;
                }
            }
            if (mob->speed > 0.0F) {
                mob->vx = rc_sin(mob->yaw) * mob->speed;
                mob->vz = rc_cos(mob->yaw) * mob->speed;
            } else {
                mob->vx = 0.0F;
                mob->vz = 0.0F;
            }
        }
        mob->vy -= 26.0F * dt;
        if (mob->vy < -40.0F) {
            mob->vy = -40.0F;
        }
        {
            const float next_x = mob->x + mob->vx * dt;
            if (!mob_blocked(mob, next_x, mob->y, mob->z)) {
                mob->x = next_x;
            } else if (mob->on_ground &&
                       !mob_blocked(mob, next_x, mob->y + 1.0F, mob->z)) {
                mob->x = next_x;
                mob->y += 1.0F;
            } else {
                if (mob->on_ground) {
                    mob->vy = 6.4F;
                }
                mob->vx = 0.0F;
            }
        }
        {
            const float next_z = mob->z + mob->vz * dt;
            if (!mob_blocked(mob, mob->x, mob->y, next_z)) {
                mob->z = next_z;
            } else if (mob->on_ground &&
                       !mob_blocked(mob, mob->x, mob->y + 1.0F, next_z)) {
                mob->z = next_z;
                mob->y += 1.0F;
            } else {
                if (mob->on_ground) {
                    mob->vy = 6.4F;
                }
                mob->vz = 0.0F;
            }
        }
        {
            const float next_y = mob->y + mob->vy * dt;
            if (!mob_blocked(mob, mob->x, next_y, mob->z)) {
                mob->y = next_y;
                if (mob->vy < 0.0F) {
                    mob->on_ground = 0;
                }
            } else {
                if (mob->vy < 0.0F) {
                    mob->on_ground = 1;
                }
                mob->vy = 0.0F;
            }
        }
        if (mob->y < -8.0F) {
            mob->alive = 0;
            mob->respawn = 0.0F;
        }
    }
}

static float ray_box(float origin_x, float origin_y, float origin_z,
                     float dir_x, float dir_y, float dir_z, float min_x,
                     float min_y, float min_z, float max_x, float max_y,
                     float max_z, int *out_face, int *out_sign) {
    float t_min = -1.0e30F;
    float t_max = 1.0e30F;
    int face = 0;
    int sign = 1;
    if (dir_x != 0.0F) {
        const float inv = 1.0F / dir_x;
        float t1 = (min_x - origin_x) * inv;
        float t2 = (max_x - origin_x) * inv;
        if (t1 > t2) {
            const float swap = t1;
            t1 = t2;
            t2 = swap;
        }
        if (t1 > t_min) {
            t_min = t1;
            face = 0;
            sign = dir_x > 0.0F ? -1 : 1;
        }
        if (t2 < t_max) {
            t_max = t2;
        }
    } else if (origin_x < min_x || origin_x > max_x) {
        return -1.0F;
    }
    if (dir_y != 0.0F) {
        const float inv = 1.0F / dir_y;
        float t1 = (min_y - origin_y) * inv;
        float t2 = (max_y - origin_y) * inv;
        if (t1 > t2) {
            const float swap = t1;
            t1 = t2;
            t2 = swap;
        }
        if (t1 > t_min) {
            t_min = t1;
            face = 1;
            sign = dir_y > 0.0F ? -1 : 1;
        }
        if (t2 < t_max) {
            t_max = t2;
        }
    } else if (origin_y < min_y || origin_y > max_y) {
        return -1.0F;
    }
    if (dir_z != 0.0F) {
        const float inv = 1.0F / dir_z;
        float t1 = (min_z - origin_z) * inv;
        float t2 = (max_z - origin_z) * inv;
        if (t1 > t2) {
            const float swap = t1;
            t1 = t2;
            t2 = swap;
        }
        if (t1 > t_min) {
            t_min = t1;
            face = 2;
            sign = dir_z > 0.0F ? -1 : 1;
        }
        if (t2 < t_max) {
            t_max = t2;
        }
    } else if (origin_z < min_z || origin_z > max_z) {
        return -1.0F;
    }
    if (t_max < t_min || t_max < 0.0F) {
        return -1.0F;
    }
    if (t_min < 0.0F) {
        t_min = 0.0F;
    }
    *out_face = face;
    *out_sign = sign;
    return t_min;
}

static int find_attack_target(const player_t *player) {
    const float cy = rc_cos(player->yaw);
    const float sy = rc_sin(player->yaw);
    const float cp = rc_cos(player->pitch);
    const float sp = rc_sin(player->pitch);
    const float dir_x = sy * cp;
    const float dir_y = sp;
    const float dir_z = cy * cp;
    const float eye_y = player->y + EYE_HEIGHT;
    float best = ATTACK_DISTANCE;
    int best_index = -1;
    int index;
    for (index = 0; index < MAX_MOBS; ++index) {
        const mob_t *mob = &g_mobs[index];
        int face;
        int sign;
        float distance;
        if (!mob->alive) {
            continue;
        }
        distance = ray_box(player->x, eye_y, player->z, dir_x, dir_y, dir_z,
                           mob->x - MOB_HALF, mob->y, mob->z - MOB_HALF,
                           mob->x + MOB_HALF, mob->y + MOB_HEIGHT,
                           mob->z + MOB_HALF, &face, &sign);
        if (distance >= 0.0F && distance < best) {
            ray_hit_t block_hit;
            game_raycast(player->x, eye_y, player->z, dir_x, dir_y, dir_z,
                         distance - 0.05F, &block_hit);
            if (!block_hit.hit) {
                best = distance;
                best_index = index;
            }
        }
    }
    return best_index;
}

int game_attack_target(const player_t *player) {
    return find_attack_target(player) >= 0;
}

int game_attack(const player_t *player, int item) {
    const float cy = rc_cos(player->yaw);
    const float sy = rc_sin(player->yaw);
    const float cp = rc_cos(player->pitch);
    const float dir_x = sy * cp;
    const float dir_z = cy * cp;
    const int best_index = find_attack_target(player);
    if (best_index < 0) {
        return 0;
    }
    {
        mob_t *mob = &g_mobs[best_index];
        mob->health -= (float)game_attack_damage(item);
        mob->hurt = 0.35F;
        mob->knock = 0.4F;
        mob->vx += dir_x * 5.0F;
        mob->vz += dir_z * 5.0F;
        mob->vy = 3.2F;
        if (mob->health <= 0.0F) {
            mob->alive = 0;
            mob->respawn = 12.0F;
            game_spawn_particles(mob->x, mob->y + MOB_HEIGHT * 0.5F, mob->z,
                                 mob_drop_block(mob->kind), 14);
            (void)game_inventory_add(mob_drop_block(mob->kind), 1);
        } else {
            game_spawn_particles(mob->x, mob->y + MOB_HEIGHT * 0.5F, mob->z,
                                 mob_drop_block(mob->kind), 5);
        }
    }
    return 1;
}
