#include "player.h"

#include "tomb_math.h"

#define TOMB_HEADROOM 1.75f
#define TOMB_CAMERA_HEIGHT 1.35f /* look-at point above the feet */
#define TOMB_CAMERA_MIN_DISTANCE 0.6f
#define TOMB_CAMERA_FOLLOW_RATE 2.2f /* radians per second the orbit drifts behind */
#define TOMB_MAX_SLOPE_SNAP 0.5f     /* floor rise per frame that counts as a slope */

/* True when a body of radius TOMB_CHARACTER_RADIUS can stand at (x, z) with
 * its feet at `y` (floor no higher than y + step-up, headroom under the
 * ceiling). */
static int can_stand(const tomb_level_t *level, float x, float z, float y,
                     uint8_t room_hint, float *floor_out) {
    /* The centre and four points on the body's radius must all be on open
     * floor no more than a step above the feet, with headroom. */
    const float offsets[5][2] = {{0.0f, 0.0f},
                                 {TOMB_CHARACTER_RADIUS, 0.0f},
                                 {-TOMB_CHARACTER_RADIUS, 0.0f},
                                 {0.0f, TOMB_CHARACTER_RADIUS},
                                 {0.0f, -TOMB_CHARACTER_RADIUS}};
    float highest_floor = -1e9f;
    uint32_t index;
    for (index = 0u; index < 5u; ++index) {
        const float px = x + offsets[index][0];
        const float pz = z + offsets[index][1];
        const uint8_t room = tomb_world_room_at(level, px, pz, room_hint);
        float floor;
        float ceiling;
        if (room == TOMB_NO_ROOM ||
            !tomb_world_heights_at(level, room, px, pz, &floor, &ceiling))
            return 0;
        if (floor > y + TOMB_PLAYER_STEP_UP) return 0;
        if (ceiling - (floor > y ? floor : y) < TOMB_HEADROOM) return 0;
        if (floor > highest_floor) highest_floor = floor;
    }
    *floor_out = highest_floor;
    return 1;
}

static void move_horizontal(tomb_player_t *player, const tomb_level_t *level,
                            float dx, float dz) {
    float floor;
    if (!can_stand(level, player->position.x + dx, player->position.z + dz,
                   player->position.y, player->room, &floor)) {
        /* Slide along the wall: keep whichever axis is free. */
        if (!can_stand(level, player->position.x + dx, player->position.z,
                       player->position.y, player->room, &floor)) {
            if (!can_stand(level, player->position.x, player->position.z + dz,
                           player->position.y, player->room, &floor))
                return;
            player->position.z += dz;
        } else {
            player->position.x += dx;
        }
    } else {
        player->position.x += dx;
        player->position.z += dz;
    }
    player->room = tomb_world_room_at(level, player->position.x,
                                      player->position.z, player->room);
}

static void move_vertical(tomb_player_t *player, const tomb_level_t *level,
                          float dt, int jump) {
    float floor;
    float ceiling;
    if (!tomb_world_heights_at(level, player->room, player->position.x,
                               player->position.z, &floor, &ceiling)) {
        /* Should not happen after a successful horizontal move; stay put. */
        return;
    }
    if (player->grounded && jump) {
        player->vertical_speed = TOMB_PLAYER_JUMP_SPEED;
        player->grounded = 0;
    }
    if (player->grounded) {
        /* Follow slopes and steps; a floor well below the feet starts a fall. */
        if (floor >= player->position.y - TOMB_MAX_SLOPE_SNAP) {
            player->position.y = floor;
            player->vertical_speed = 0.0f;
        } else {
            player->grounded = 0;
        }
    }
    if (!player->grounded) {
        player->vertical_speed -= TOMB_PLAYER_GRAVITY * dt;
        player->position.y += player->vertical_speed * dt;
        if (player->position.y + TOMB_HEADROOM > ceiling &&
            player->vertical_speed > 0.0f) {
            player->position.y = ceiling - TOMB_HEADROOM;
            player->vertical_speed = 0.0f;
        }
        if (player->position.y <= floor) {
            player->position.y = floor;
            player->grounded = 1;
            player->landing = player->vertical_speed < -3.0f ? 0.25f : 0.1f;
            player->vertical_speed = 0.0f;
        }
    }
}

static void place_camera(tomb_player_t *player, const tomb_level_t *level) {
    tomb_vec3_t target;
    tomb_vec3_t back;
    tomb_vec3_t best;
    uint8_t best_room;
    float cos_pitch;
    int step;
    const int steps = 14;
    target = player->position;
    target.y += TOMB_CAMERA_HEIGHT;
    cos_pitch = tomb_cos(player->camera_pitch);
    tomb_vec3_set(&back, -tomb_sin(player->camera_yaw) * cos_pitch,
                  -tomb_sin(player->camera_pitch),
                  -tomb_cos(player->camera_yaw) * cos_pitch);
    /* March out from the look-at point and stop before leaving open space. */
    best = tomb_vec3_add(target, tomb_vec3_scale(back, TOMB_CAMERA_MIN_DISTANCE));
    best_room = tomb_world_room_at(level, best.x, best.z, player->room);
    if (best_room == TOMB_NO_ROOM) best_room = player->room;
    for (step = 1; step <= steps; ++step) {
        const float d = TOMB_CAMERA_MIN_DISTANCE +
                        (player->camera_distance - TOMB_CAMERA_MIN_DISTANCE) *
                            (float)step / (float)steps;
        const tomb_vec3_t candidate = tomb_vec3_add(target, tomb_vec3_scale(back, d));
        const uint8_t room =
            tomb_world_room_at(level, candidate.x, candidate.z, best_room);
        float floor;
        float ceiling;
        if (room == TOMB_NO_ROOM ||
            !tomb_world_heights_at(level, room, candidate.x, candidate.z, &floor,
                                   &ceiling) ||
            candidate.y < floor + 0.15f || candidate.y > ceiling - 0.1f) {
            break;
        }
        best = candidate;
        best_room = room;
    }
    player->camera_position = best;
    player->camera_room = best_room;
}

void tomb_player_reset(tomb_player_t *player, const tomb_level_t *level) {
    player->position = level->start;
    player->yaw = level->start_yaw;
    player->room = level->start_room;
    player->speed = 0.0f;
    player->vertical_speed = 0.0f;
    player->grounded = 1;
    player->pose = (tomb_pose_t){0};
    player->landing = 0.0f;
    player->camera_yaw = player->yaw;
    player->camera_pitch = -0.22f;
    player->camera_distance = 2.8f;
    place_camera(player, level);
}

void tomb_player_update(tomb_player_t *player, const tomb_level_t *level,
                        const tomb_controls_t *controls, float dt) {
    float magnitude_sq;
    float target_speed = 0.0f;
    player->camera_yaw =
        tomb_wrap_angle(player->camera_yaw + controls->orbit);
    player->camera_pitch =
        tomb_clamp(player->camera_pitch + controls->tilt, -0.9f, 0.35f);

    /* Stick direction is relative to the camera; the character turns to face it. */
    magnitude_sq = controls->forward * controls->forward +
                   controls->strafe * controls->strafe;
    if (magnitude_sq > 0.01f) {
        float magnitude = tomb_sqrt(magnitude_sq);
        float heading;
        if (magnitude > 1.0f) magnitude = 1.0f;
        /* A heading of 0 walks along +z; stick right (+strafe) turns towards
         * +x, measured from the camera's own yaw. */
        heading = tomb_wrap_angle(player->camera_yaw +
                                  tomb_atan2(controls->strafe, controls->forward));
        player->yaw = tomb_approach_angle(player->yaw, heading, 9.0f * dt);
        target_speed = TOMB_PLAYER_WALK_SPEED * magnitude;
        /* Accelerate quickly, and move along the current facing so turns feel
         * weighty. */
        player->speed += (target_speed - player->speed) *
                         tomb_clamp(dt * 10.0f, 0.0f, 1.0f);
    } else {
        player->speed += (0.0f - player->speed) *
                         tomb_clamp(dt * 12.0f, 0.0f, 1.0f);
        if (player->speed < 0.05f) player->speed = 0.0f;
    }
    if (player->speed > 0.0f) {
        move_horizontal(player, level,
                        tomb_sin(player->yaw) * player->speed * dt,
                        tomb_cos(player->yaw) * player->speed * dt);
    }
    move_vertical(player, level, dt, controls->jump);

    /* Pose: stride phase from distance walked, weights eased towards targets. */
    player->pose.walk_phase =
        tomb_wrap_angle(player->pose.walk_phase + player->speed * dt * 5.5f);
    {
        const float walk_target =
            player->grounded
                ? tomb_clamp(player->speed / TOMB_PLAYER_WALK_SPEED, 0.0f, 1.0f)
                : 0.0f;
        player->pose.walk_weight +=
            (walk_target - player->pose.walk_weight) *
            tomb_clamp(dt * 8.0f, 0.0f, 1.0f);
    }
    {
        const float air_target = player->grounded ? 0.0f : 1.0f;
        player->pose.airborne += (air_target - player->pose.airborne) *
                                 tomb_clamp(dt * 10.0f, 0.0f, 1.0f);
    }
    player->landing = player->landing > dt ? player->landing - dt : 0.0f;
    {
        const float crouch_target = player->landing > 0.0f ? 1.0f : 0.0f;
        player->pose.crouch += (crouch_target - player->pose.crouch) *
                               tomb_clamp(dt * 14.0f, 0.0f, 1.0f);
    }

    /* The orbit drifts behind the character while walking, unless the player
     * is steering it. Pure strafing (facing across the view) leaves the camera
     * alone so the player can circle an object without the view spinning. */
    if (controls->orbit == 0.0f && player->speed > 0.3f &&
        tomb_fabs(tomb_wrap_angle(player->yaw - player->camera_yaw)) < 1.1f) {
        player->camera_yaw = tomb_approach_angle(
            player->camera_yaw, player->yaw,
            TOMB_CAMERA_FOLLOW_RATE * dt *
                tomb_clamp(player->speed, 0.0f, 1.0f));
    }
    place_camera(player, level);
}

void tomb_player_camera(const tomb_player_t *player, float focal_length,
                        tomb_camera_t *camera_out) {
    camera_out->position = player->camera_position;
    camera_out->yaw = player->camera_yaw;
    camera_out->pitch = player->camera_pitch;
    camera_out->focal_length = focal_length;
    camera_out->near = 0.12f;
}
