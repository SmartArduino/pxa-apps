#ifndef TOMB_CHARACTER_H
#define TOMB_CHARACTER_H

#include <stdint.h>

#include "mesh.h"

/* Low-polygon explorer: eleven textured boxes in a rigid hierarchy (pelvis,
 * torso, head, two arms and two legs of two segments each) animated
 * procedurally. Ported from micropixel game/character.cpp. */

#define TOMB_CHARACTER_PARTS 11u
#define TOMB_CHARACTER_HEIGHT 1.7f
#define TOMB_CHARACTER_RADIUS 0.25f

typedef struct {
    float walk_phase;  /* radians, advances with distance walked */
    float walk_weight; /* 0 standing, 1 full stride */
    float crouch;      /* 0..1, bends knees (jump take-off and landing) */
    float airborne;    /* 0..1, arms up and legs tucked */
} tomb_pose_t;

void tomb_character_init(void);
/* Queues the character standing at `position` (feet) facing `yaw`, lit at
 * `brightness` (room light, 0..255), into `group` with `scissor`. Returns the
 * number of parts submitted. */
uint32_t tomb_character_submit(tomb_renderer_t *renderer, tomb_vec3_t position,
                               float yaw, const tomb_pose_t *pose,
                               uint8_t brightness, uint8_t group,
                               tomb_rect_t scissor);

#endif
