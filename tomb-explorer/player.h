#ifndef TOMB_PLAYER_H
#define TOMB_PLAYER_H

#include <stdint.h>

#include "character.h"
#include "level.h"
#include "mesh.h"
#include "world.h"

/* Per-frame input in camera-relative terms. */
typedef struct {
    float forward; /* -1..1, away from the camera */
    float strafe;  /* -1..1, camera right */
    float orbit;   /* radians to turn the camera around the player this frame */
    float tilt;    /* radians to pitch the camera this frame */
    int jump;
} tomb_controls_t;

/* The explorer: walks on the sector floors with step-up, drop and jump
 * handling, blocked by walls, solids and low ceilings; moves between rooms
 * through portals. Also owns the third-person follow camera. Ported from
 * micropixel game/player.cpp. */

#define TOMB_PLAYER_STEP_UP 0.36f
#define TOMB_PLAYER_WALK_SPEED 2.4f
#define TOMB_PLAYER_GRAVITY 11.0f
#define TOMB_PLAYER_JUMP_SPEED 4.4f

typedef struct {
    tomb_vec3_t position;
    float yaw;
    float speed;
    float vertical_speed;
    int grounded;
    uint8_t room;
    tomb_pose_t pose;
    float landing;

    float camera_yaw;
    float camera_pitch;
    float camera_distance;
    tomb_vec3_t camera_position;
    uint8_t camera_room;
} tomb_player_t;

void tomb_player_reset(tomb_player_t *player, const tomb_level_t *level);
void tomb_player_update(tomb_player_t *player, const tomb_level_t *level,
                        const tomb_controls_t *controls, float dt);
/* Camera for this frame: orbits the player, pulled in when a wall is in the
 * way. */
void tomb_player_camera(const tomb_player_t *player, float focal_length,
                        tomb_camera_t *camera_out);

#endif
