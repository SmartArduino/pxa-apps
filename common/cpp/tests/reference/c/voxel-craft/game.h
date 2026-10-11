#ifndef VOXEL_CRAFT_GAME_H
#define VOXEL_CRAFT_GAME_H

#include <stdint.h>

/* The world is an unbounded grid of 16 x 24 x 16 chunks generated on demand.
 * A (GRID_W x GRID_W) window of chunks is kept resident and re-centred when the
 * player crosses a chunk boundary; terrain, trees and edits are deterministic
 * per chunk coordinate so evicted chunks regenerate identically. */

#define CHUNK_BITS 4
#define CHUNK_SIZE 16
#define CHUNK_MASK (CHUNK_SIZE - 1)
#define CHUNK_HEIGHT 24
#define CHUNK_VOLUME (CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT)
#define GRID_W 7
#define GRID_COUNT (GRID_W * GRID_W)
#define WORLD_Y CHUNK_HEIGHT

#define WATER_LEVEL 8
#define MAX_EDITS 2048

#define EYE_HEIGHT 1.62F
#define PLAYER_HALF 0.30F
#define PLAYER_HEIGHT 1.80F
#define REACH_DISTANCE 6.0F
#define ATTACK_DISTANCE 3.6F

#define HOTBAR_SLOTS 9
#define INV_MAIN_SLOTS 27
#define INV_SLOTS (HOTBAR_SLOTS + INV_MAIN_SLOTS)
#define CRAFT_SLOTS 4
#define TABLE_CRAFT_SLOTS 9
#define ITEM_MAX_STACK 64
#define MAX_PARTICLES 96
#define MAX_MOBS 8

#define MOB_HALF 0.35F
#define MOB_HEIGHT 0.90F

enum {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_SAND,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_WATER,
    BLOCK_PLANK,
    BLOCK_BRICK,
    BLOCK_GLASS,
    BLOCK_COBBLE,
    BLOCK_TABLE,
    BLOCK_SNOW,
    BLOCK_GRAVEL,
    BLOCK_CACTUS,
    BLOCK_BUSH,
    BLOCK_FLOWER,
    BLOCK_WOOL,
    BLOCK_BEDROCK,
    BLOCK_TYPE_COUNT
};

enum {
    BIOME_PLAINS = 0,
    BIOME_FOREST,
    BIOME_SNOW,
    BIOME_DESERT,
};

enum {
    MOB_SLIME = 0,
    MOB_SHEEP,
    MOB_PIG,
    MOB_COW,
    MOB_KIND_COUNT,
};

/* Tools are non-block items crafted at the crafting table. */
#define ITEM_WOOD_PICK 20
#define ITEM_WOOD_AXE 21
#define ITEM_WOOD_SHOVEL 22
#define ITEM_WOOD_SWORD 23
#define ITEM_STONE_PICK 24
#define ITEM_STONE_AXE 25
#define ITEM_STONE_SHOVEL 26
#define ITEM_STONE_SWORD 27
#define ITEM_FIRST_TOOL ITEM_WOOD_PICK
#define ITEM_LAST_TOOL ITEM_STONE_SWORD

typedef struct {
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;
    float yaw;
    float pitch;
    uint8_t on_ground;
    uint8_t flying;
    uint8_t in_water;
} player_t;

typedef struct {
    uint8_t hit;
    uint8_t face;
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t place_x;
    int16_t place_y;
    int16_t place_z;
} ray_hit_t;

typedef struct {
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;
    float life;
    uint8_t block;
} particle_t;

typedef struct {
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;
    float yaw;
    float speed;
    float health;
    float hurt;
    float knock;
    float wander;
    float respawn;
    uint8_t kind;
    uint8_t alive;
    uint8_t on_ground;
} mob_t;

typedef struct {
    int16_t cx;
    int16_t cz;
    uint32_t revision;
    uint8_t loaded;
    uint8_t blocks[CHUNK_VOLUME];
} chunk_t;

typedef struct {
    int32_t x;
    int16_t y;
    int32_t z;
    uint8_t block;
} edit_t;

typedef struct {
    uint8_t item;
    uint8_t count;
} item_stack_t;

extern chunk_t *g_chunk_grid[GRID_W][GRID_W];
extern int16_t g_chunk_origin_cx;
extern int16_t g_chunk_origin_cz;
extern uint32_t g_world_seed;
extern item_stack_t g_inventory[INV_SLOTS];
extern item_stack_t g_craft[CRAFT_SLOTS];
extern item_stack_t g_craft_result;
extern item_stack_t g_table_craft[TABLE_CRAFT_SLOTS];
extern item_stack_t g_table_result;
extern particle_t g_particles[MAX_PARTICLES];
extern mob_t g_mobs[MAX_MOBS];

/* Hot-path accessor used by the ray caster. Coordinates are floor()ed voxel
 * indices; anything outside the resident window is air. */
static inline int game_block_fast(int x, int y, int z) {
    int i;
    int j;
    const chunk_t *chunk;
    if ((unsigned)y >= (unsigned)CHUNK_HEIGHT) {
        return 0;
    }
    i = (x >> CHUNK_BITS) - g_chunk_origin_cx;
    j = (z >> CHUNK_BITS) - g_chunk_origin_cz;
    if ((unsigned)i >= (unsigned)GRID_W || (unsigned)j >= (unsigned)GRID_W) {
        return 0;
    }
    chunk = g_chunk_grid[j][i];
    if (chunk == (const chunk_t *)0 || !chunk->loaded) {
        return 0;
    }
    return chunk->blocks[((y << 8) |
                          ((z & CHUNK_MASK) << CHUNK_BITS) |
                          (x & CHUNK_MASK))];
}

/* Highest visible block in the chunk holding (x, z), or -1 outside. */
static inline int game_in_grid(int x, int z) {
    const int i = (x >> CHUNK_BITS) - g_chunk_origin_cx;
    const int j = (z >> CHUNK_BITS) - g_chunk_origin_cz;
    return (unsigned)i < (unsigned)GRID_W && (unsigned)j < (unsigned)GRID_W;
}

int game_block(int x, int y, int z);
void game_set_block(int x, int y, int z, int block);
int game_solid(int x, int y, int z);
const char *game_block_name(int block);
const char *game_item_name(int item);
float game_block_hardness(int block);
int game_item_is_block(int item);
int game_item_is_tool(int item);
float game_mining_speed(int item, int block);
int game_attack_damage(int item);

void game_generate(uint32_t seed);
uint32_t game_seed(void);
/* Generates at most `limit` deferred edge chunks. Called once per display
 * frame so crossing a chunk boundary cannot monopolize one simulation tick. */
int game_stream_chunks(int limit);
int game_pending_chunk_count(void);
/* Serializes the world seed, player pose and the edit log. Returns the byte
 * count, or 0 when the buffer is too small. */
int game_serialize(const player_t *player, uint8_t *out, int capacity);
/* Restores a serialized save and points the chunk grid at the player. */
int game_deserialize(const uint8_t *data, int length, player_t *player);
void game_ensure_chunks(const player_t *player);
void game_spawn(player_t *player);
void game_step(player_t *player, float dt, float move_x, float move_z,
               int jump, int ascend, int descend);
void game_raycast(float origin_x, float origin_y, float origin_z, float dir_x,
                  float dir_y, float dir_z, float max_distance,
                  ray_hit_t *hit);
int game_target_block(const player_t *player, ray_hit_t *hit);
int game_break_block(player_t *player);
int game_place_block(player_t *player, int block);
int game_point_inside_player(float x, float y, float z, const player_t *player);

void game_inventory_init(void);
int game_inventory_add(int item, int count);
int game_inventory_remove(int slot, int count);
void game_craft_update(void);
int game_craft_take(uint8_t *out_item, uint8_t *out_count);
void game_table_craft_update(void);
int game_table_craft_take(uint8_t *out_item, uint8_t *out_count);

void game_spawn_particles(float x, float y, float z, int block, int count);
void game_update_particles(float dt);
void game_spawn_mobs(uint32_t seed, const player_t *player);
void game_update_mobs(float dt, const player_t *player);
int game_attack(const player_t *player, int item);
/* 1 when a mob is within attack reach in front of the player. */
int game_attack_target(const player_t *player);

#endif
