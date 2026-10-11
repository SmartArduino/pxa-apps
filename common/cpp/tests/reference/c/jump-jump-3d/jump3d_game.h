#ifndef JUMP3D_GAME_H
#define JUMP3D_GAME_H

#include <stdint.h>

/* World units follow the original game divided by ten: one block is 1.0 wide
 * and 0.55 tall, the little man is 0.57 tall and the gaps between block edges
 * run from 0.10 to 1.70. Gravity, launch speeds and timings keep the original
 * ratios (weapp-jump: gravity 720, block 10 x 5.5, vz 70/s, vy 135 + 15/s). */
#define J3_BLOCK_SIZE 1.0F
#define J3_BLOCK_HEIGHT 0.55F
#define J3_BLOCK_HALF_HEIGHT (J3_BLOCK_HEIGHT * 0.5F)
#define J3_BODY_RADIUS 0.12F

#define J3_BLOCK_MAX 6
#define J3_WAVE_MAX 4

enum {
    J3_KIND_BOX = 1,
    J3_KIND_ROUND,
    J3_KIND_MUSIC,
    J3_KIND_STORE,
    J3_KIND_WELL,
    J3_KIND_RUBIK
};

enum {
    J3_STATE_READY = 0,
    J3_STATE_CHARGING,
    J3_STATE_FLYING,
    J3_STATE_TIP,
    J3_STATE_FALL,
    J3_STATE_OVER
};

enum {
    J3_LAND_OK = 0,
    J3_LAND_PERFECT,
    J3_LAND_EDGE,
    J3_LAND_BACK,
    J3_LAND_MISS
};

typedef struct {
    uint8_t kind;
    uint8_t color;         /* J3_HUE_BLOCK variant */
    uint8_t bonus_done;    /* the bonus of this block was collected */
    uint8_t center_dot;    /* white aim dot, awarded by a centre landing */
    float x, z;            /* world centre; the block spans y = 0..height */
    float radius;          /* half width for boxes, radius for rounds */
    float squash;          /* 1 = full height, 0.5 = fully charged */
    float appear;          /* 0..1 drop-in animation */
    float bonus_timer;     /* counts down while the player stays on it */
} j3_block_t;

typedef struct {
    uint8_t active;
    uint8_t power; /* 1..4 rings awarded by consecutive centre hits */
    float t;
} j3_wave_t;

typedef struct {
    uint8_t state;
    uint32_t score;
    uint32_t best;
    uint32_t rng;
    float time;

    /* Difficulty, mirroring the original's progressive tightening. */
    float min_radius_scale;
    float max_radius_scale;
    float max_distance;
    uint16_t jump_count;
    uint16_t bonus_interval;
    uint16_t last_bonus_jump;
    uint8_t last_bonus_kind;

    /* Charging. */
    float charge;
    float body_scale;

    /* Player transform. */
    float px, py, pz;
    float vx, vy, vz;
    float dir_x, dir_z;
    float spin;       /* flip angle, radians about the flip axis */
    float tilt;       /* failure tip-over angle */
    float land_scale; /* squash after landing, 1 = idle */
    float root_yaw;   /* heading, radians about +Y */

    /* Analytic landing solved on release. */
    float land_x, land_z, land_time;
    uint8_t land_result;

    /* Feedback. */
    float popup_timer;
    uint16_t popup_points;
    uint8_t popup_kind; /* 0 none, 1 score, 2 perfect, 3 quick, 4 bonus */
    float bonus_popup_timer;
    uint16_t bonus_points;
    uint8_t bonus_kind;    /* block kind that paid the last stay bonus */
    uint16_t combo;
    float quick_window; /* time left for a quick jump bonus */

    /* Camera follow point (world). */
    float cam_x, cam_z;

    j3_block_t blocks[J3_BLOCK_MAX];
    uint8_t block_count;
    j3_wave_t waves[J3_WAVE_MAX];
} j3_game_t;

void j3_game_reset(j3_game_t *game, uint32_t seed);

/* Press/release while ready or charging. Ignored in other states except for
 * the restart tap while the result panel is up. */
void j3_game_press(j3_game_t *game);
void j3_game_release(j3_game_t *game);

/* Advances by `dt` seconds. */
void j3_game_tick(j3_game_t *game, float dt);

const j3_block_t *j3_game_current(const j3_game_t *game);
const j3_block_t *j3_game_next(const j3_game_t *game);

/* Height of a block's top surface including squash and drop-in offset. */
float j3_block_top(const j3_block_t *block);
/* Drop-in animation offset added to the block's base. */
float j3_block_offset(const j3_block_t *block);
/* 1 when (x, z) is inside the block footprint. */
int j3_block_contains(const j3_block_t *block, float x, float z);
/* Points awarded when the block's stay bonus triggers (0 for plain blocks). */
uint16_t j3_block_bonus(const j3_block_t *block);

#endif
