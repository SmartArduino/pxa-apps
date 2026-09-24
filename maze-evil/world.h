#ifndef MAZE_EVIL_WORLD_H
#define MAZE_EVIL_WORLD_H

#include <stdint.h>

#include "raycast.h"

#define MAP_WIDTH 32
#define MAP_HEIGHT 32
#define MAX_IMPS 24
#define MAX_FIREBALLS 16
#define MAX_ITEMS 16
#define MAX_DECORATIONS 24
#define MAX_DOORS 24
#define MAX_THINGS (MAX_IMPS + MAX_FIREBALLS + MAX_ITEMS + MAX_DECORATIONS)
#define MAX_PENDING_SOUNDS 16

/* Sound ids, matching maze-evil audio/sound_ids.hpp. */
enum {
    SND_SHOTGUN = 0,
    SND_EMPTY_CLICK,
    SND_IMP_ALERT,
    SND_IMP_FIREBALL,
    SND_IMP_MELEE,
    SND_IMP_PAIN,
    SND_IMP_DEATH,
    SND_FIREBALL_EXPLODE,
    SND_PLAYER_PAIN,
    SND_PICKUP_HEALTH,
    SND_PICKUP_AMMO,
    SND_DOOR_OPEN,
    SND_DOOR_CLOSE,
    SND_EXIT_SEALED,
    SND_WIN,
    SND_DIE,
    SND_COUNT,
};

typedef struct {
    float forward; /* -1..1 */
    float strafe;  /* -1..1, positive = right */
    float turn;    /* radians for this frame, positive = clockwise */
    int fire;
} controls_t;

typedef struct {
    float x;
    float y;
    float angle;
    float dir_x;
    float dir_y;
    float plane_x;
    float plane_y;
    int health;
    int ammo;
    float fire_cooldown;
    float recoil;       /* 0..1, decays after a shot */
    float bob_phase;    /* walking bob */
    float damage_flash; /* seconds of red tint left */
    float speed;        /* current planar speed for bobbing */
} player_t;

enum {
    IMP_IDLE = 0,
    IMP_CHASE,
    IMP_ATTACK,
    IMP_PAIN,
    IMP_DEAD,
};

typedef struct {
    float x;
    float y;
    int health;
    uint8_t state;
    float timer;
    float anim;
    float attack_cooldown;
    int fired;
} imp_t;

typedef struct {
    int alive;
    float x;
    float y;
    float vx;
    float vy;
    float age;
} fireball_t;

enum {
    ITEM_MEDKIT = 0,
    ITEM_AMMO,
};

typedef struct {
    int alive;
    uint8_t kind;
    float x;
    float y;
} item_t;

typedef struct {
    uint8_t sprite;
    int solid;
    int animated;
    float x;
    float y;
} decoration_t;

enum {
    DOOR_CLOSED = 0,
    DOOR_OPENING,
    DOOR_OPEN,
    DOOR_CLOSING,
};

typedef struct {
    int x;
    int y;
    float open; /* 0 closed .. 1 fully open */
    uint8_t state;
    float timer;
} door_t;

enum {
    PHASE_PLAYING = 0,
    PHASE_DEAD,
    PHASE_WON,
};

typedef struct {
    int sprite; /* sprite id */
    float x;
    float y;
    float height; /* fraction of wall height */
    float lift;   /* fraction of wall height above the floor */
} thing_t;

typedef struct {
    uint8_t id;
    uint8_t gain;
} sound_event_t;

typedef struct {
    ray_cell_t cells[MAP_HEIGHT][MAP_WIDTH];
    uint8_t tiles[MAP_HEIGHT][MAP_WIDTH];
    player_t player;
    imp_t imps[MAX_IMPS];
    int imp_count;
    fireball_t fireballs[MAX_FIREBALLS];
    item_t items[MAX_ITEMS];
    int item_count;
    decoration_t decorations[MAX_DECORATIONS];
    int decoration_count;
    door_t doors[MAX_DOORS];
    int door_count;
    int kills;
    uint8_t phase;
    float time;
    const char *message;
    float message_timer;
    int fire_was_down;
    uint32_t rng;
    sound_event_t pending_sounds[MAX_PENDING_SOUNDS];
    int pending_sound_count;
} world_t;

void world_reset(world_t *world);
void world_update(world_t *world, float dt, const controls_t *controls);
int world_collect_things(const world_t *world, thing_t *out, int capacity);
int world_take_sounds(world_t *world, sound_event_t *out, int capacity);

static inline int world_muzzle_flash(const world_t *world) {
    return world->player.recoil > 0.75F;
}

static inline const char *world_message(const world_t *world) {
    return world->message_timer > 0.0F ? world->message : 0;
}

#endif
