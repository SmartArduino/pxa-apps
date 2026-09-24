#include "jump3d_game.h"

#include "jump3d_math.h"
#include "jump3d_palette.h"

/* Tuned against the original's numbers (weapp-jump game.js):
 *   gravity 720, vy = 135 + 15 t (cap 180), vz = 70 t (cap 150)
 * scaled by 1/10 into Jump Jump world units. A full charge therefore flies
 * roughly 2.7 units, which is the longest generated centre distance. */
#define J3_GRAVITY 72.0F
#define J3_VY_BASE 13.5F
#define J3_VY_STEP 1.5F
#define J3_VY_MAX 18.0F
#define J3_VZ_STEP 7.0F
#define J3_VZ_MAX 15.0F

#define J3_CHARGE_SQUASH 0.32F /* block/body shrink per second of charge */
#define J3_MIN_SQUASH 0.5F

#define J3_GAP_MIN 0.10F
#define J3_GAP_MAX_START 1.70F
#define J3_GAP_MAX_LIMIT 2.20F
#define J3_GAP_GROWTH 0.004F /* original: +0.03 per jump, softened */

#define J3_RADIUS_SCALE_MAX_START 1.00F
#define J3_RADIUS_SCALE_MIN_START 0.80F
#define J3_RADIUS_SCALE_MAX_FLOOR 0.62F
#define J3_RADIUS_SCALE_MIN_FLOOR 0.26F
#define J3_RADIUS_SCALE_DECAY 0.005F

#define J3_PERFECT_RADIUS 0.0707F /* original: |d|^2 < 0.5 in 10x units */
#define J3_PERFECT_RADIUS_SQ (J3_PERFECT_RADIUS * J3_PERFECT_RADIUS)
#define J3_QUICK_WINDOW 0.8F
#define J3_BONUS_STAY 2.0F
#define J3_START_DISTANCE 2.0F
#define J3_TIP_TIME 0.24F
#define J3_DROP_IN 2.6F /* block drop-in speed, 1/appear per second */

static float j3_random_unit(j3_game_t *game) {
    uint32_t state = game->rng;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    if (state == 0) state = 0x1234567u;
    game->rng = state;
    return (float)(state >> 8) * (1.0F / 16777216.0F);
}

static uint8_t j3_random_range(j3_game_t *game, uint8_t count) {
    const float unit = j3_random_unit(game) * (float)count;
    uint8_t value = (uint8_t)unit;
    if (value >= count) value = (uint8_t)(count - 1u);
    return value;
}

/* Bounce.easeOut, matching the original block drop-in. */
static float j3_ease_out_bounce(float t) {
    if (t < 1.0F / 2.75F) {
        return 7.5625F * t * t;
    } else if (t < 2.0F / 2.75F) {
        t -= 1.5F / 2.75F;
        return 7.5625F * t * t + 0.75F;
    } else if (t < 2.5F / 2.75F) {
        t -= 2.25F / 2.75F;
        return 7.5625F * t * t + 0.9375F;
    }
    t -= 2.625F / 2.75F;
    return 7.5625F * t * t + 0.984375F;
}

const j3_block_t *j3_game_current(const j3_game_t *game) {
    return &game->blocks[0];
}

const j3_block_t *j3_game_next(const j3_game_t *game) {
    return &game->blocks[1];
}

float j3_block_offset(const j3_block_t *block) {
    if (block->appear >= 1.0F) return 0.0F;
    return (1.0F - j3_ease_out_bounce(block->appear)) * 2.4F;
}

float j3_block_top(const j3_block_t *block) {
    return J3_BLOCK_HEIGHT * block->squash + j3_block_offset(block);
}

int j3_block_contains(const j3_block_t *block, float x, float z) {
    const float dx = x - block->x;
    const float dz = z - block->z;
    if (block->kind == J3_KIND_ROUND || block->kind == J3_KIND_WELL) {
        return dx * dx + dz * dz <= block->radius * block->radius;
    }
    return j3_fabs(dx) <= block->radius && j3_fabs(dz) <= block->radius;
}

uint16_t j3_block_bonus(const j3_block_t *block) {
    switch (block->kind) {
    case J3_KIND_MUSIC: return 30u;
    case J3_KIND_STORE: return 15u;
    case J3_KIND_WELL: return 5u;
    case J3_KIND_RUBIK: return 10u;
    default: return 0u;
    }
}

static void j3_start_wave(j3_game_t *game, uint8_t power) {
    uint8_t index;
    if (power == 0) power = 1;
    if (power > J3_WAVE_MAX) power = J3_WAVE_MAX;
    for (index = 0; index < J3_WAVE_MAX; ++index) {
        if (!game->waves[index].active) {
            game->waves[index].active = 1;
            game->waves[index].power = power;
            game->waves[index].t = 0.0F;
            return;
        }
    }
}

/* Points the little man at the block he is about to jump to. */
static void j3_face_next(j3_game_t *game) {
    const j3_block_t *next = &game->blocks[1];
    const float dx = next->x - game->px;
    const float dz = next->z - game->pz;
    if (j3_fabs(dx) >= j3_fabs(dz))
        game->root_yaw = dx >= 0.0F ? 0.0F : J3_PI;
    else
        game->root_yaw = dz <= 0.0F ? J3_HALF_PI : -J3_HALF_PI;
}

static void j3_spawn_block(j3_game_t *game, j3_block_t *block,
                           const j3_block_t *from, uint8_t allow_bonus) {
    float radius_scale =
        game->min_radius_scale +
        j3_random_unit(game) *
            (game->max_radius_scale - game->min_radius_scale);
    float gap;
    float distance;
    uint8_t color;

    if (radius_scale < J3_RADIUS_SCALE_MIN_FLOOR)
        radius_scale = J3_RADIUS_SCALE_MIN_FLOOR;
    block->radius = radius_scale * (J3_BLOCK_SIZE * 0.5F);
    block->squash = 1.0F;
    block->appear = 0.0F;
    block->bonus_timer = 0.0F;
    block->bonus_done = 0;
    block->center_dot = 0;

    gap = J3_GAP_MIN +
          j3_random_unit(game) * (game->max_distance - J3_GAP_MIN);
    distance = from->radius + gap + block->radius;

    /* The original lays the path on a staircase: the next block is either
     * straight ahead (+X, up and to the right on screen) or to the left
     * (-Z, up and to the left). */
    if (j3_random_unit(game) > 0.5F) {
        block->x = from->x + distance;
        block->z = from->z;
    } else {
        block->x = from->x;
        block->z = from->z - distance;
    }

    do {
        color = j3_random_range(game, J3_BLOCK_COLORS);
    } while (color == from->color && J3_BLOCK_COLORS > 1u);
    block->color = color;

    block->kind = j3_random_unit(game) < 0.42F ? J3_KIND_ROUND : J3_KIND_BOX;

    if (allow_bonus &&
        (uint16_t)(game->jump_count - game->last_bonus_jump) >=
            game->bonus_interval) {
        static const uint8_t bonuses[4] = {J3_KIND_MUSIC, J3_KIND_STORE,
                                           J3_KIND_WELL, J3_KIND_RUBIK};
        uint8_t pick = j3_random_range(game, 4);
        if (bonuses[pick] == game->last_bonus_kind)
            pick = (uint8_t)((pick + 1u) & 3u);
        block->kind = bonuses[pick];
        /* Bonus blocks always come on the small side, like the original. */
        if (block->radius > 0.42F) block->radius = 0.42F;
        game->last_bonus_kind = block->kind;
        game->last_bonus_jump = game->jump_count;
    }
}

void j3_game_reset(j3_game_t *game, uint32_t seed) {
    const uint32_t previous_best = game->best;
    uint32_t index;
    if (seed == 0) seed = 0x2545F491u;
    for (index = 0; index < (uint32_t)sizeof(j3_game_t); ++index)
        ((uint8_t *)game)[index] = 0;
    game->rng = seed | 1u;
    game->best = previous_best;
    game->min_radius_scale = J3_RADIUS_SCALE_MIN_START;
    game->max_radius_scale = J3_RADIUS_SCALE_MAX_START;
    game->max_distance = J3_GAP_MAX_START;
    game->bonus_interval = 5;
    game->last_bonus_jump = 0;
    game->last_bonus_kind = J3_KIND_BOX;

    game->blocks[0].kind = J3_KIND_BOX;
    game->blocks[0].color = 8u; /* warm white start block */
    game->blocks[0].x = 0.0F;
    game->blocks[0].z = 0.0F;
    game->blocks[0].radius = J3_BLOCK_SIZE * 0.5F;
    game->blocks[0].squash = 1.0F;
    game->blocks[0].appear = 1.0F;

    game->blocks[1].kind = J3_KIND_BOX;
    game->blocks[1].color = 0u;
    game->blocks[1].x = J3_START_DISTANCE;
    game->blocks[1].z = 0.0F;
    game->blocks[1].radius = J3_BLOCK_SIZE * 0.5F;
    game->blocks[1].squash = 1.0F;
    game->blocks[1].appear = 1.0F;
    j3_spawn_block(game, &game->blocks[2], &game->blocks[1], 0);

    game->block_count = 2;
    game->state = J3_STATE_READY;
    game->body_scale = 1.0F;
    game->land_scale = 1.0F;
    game->px = game->blocks[0].x;
    game->pz = game->blocks[0].z;
    game->py = j3_block_top(&game->blocks[0]);
    game->cam_x = (game->blocks[0].x + game->blocks[1].x) * 0.5F;
    game->cam_z = (game->blocks[0].z + game->blocks[1].z) * 0.5F;
    game->popup_kind = 0;
    game->quick_window = 0.0F;
    j3_face_next(game);
}

void j3_game_press(j3_game_t *game) {
    if (game->state != J3_STATE_READY) return;
    game->state = J3_STATE_CHARGING;
    game->charge = 0.0F;
    /* Jumping away cancels a pending stay bonus. */
    game->blocks[0].bonus_timer = 0.0F;
}

void j3_game_release(j3_game_t *game) {
    const j3_block_t *next = &game->blocks[1];
    float dx;
    float dz;
    float length;
    float speed;
    if (game->state != J3_STATE_CHARGING) return;

    dx = next->x - game->px;
    dz = next->z - game->pz;
    length = j3_sqrt(dx * dx + dz * dz);
    if (length < 0.0001F) {
        dx = 1.0F;
        dz = 0.0F;
        length = 1.0F;
    }
    game->dir_x = dx / length;
    game->dir_z = dz / length;

    speed = game->charge * J3_VZ_STEP;
    if (speed > J3_VZ_MAX) speed = J3_VZ_MAX;
    game->vy = J3_VY_BASE + game->charge * J3_VY_STEP;
    if (game->vy > J3_VY_MAX) game->vy = J3_VY_MAX;
    game->vx = game->dir_x * speed;
    game->vz = game->dir_z * speed;

    game->land_time = 2.0F * game->vy / J3_GRAVITY;
    game->land_x = game->px + game->vx * game->land_time;
    game->land_z = game->pz + game->vz * game->land_time;

    /* Classify like the original checkHit2: the centre decides, then the body
     * offsets decide between a clean miss and a tip-over. */
    if (j3_block_contains(next, game->land_x, game->land_z)) {
        const float ox = game->land_x - next->x;
        const float oz = game->land_z - next->z;
        game->land_result = (ox * ox + oz * oz) < J3_PERFECT_RADIUS_SQ
                                ? J3_LAND_PERFECT
                                : J3_LAND_OK;
    } else if (j3_block_contains(next, game->land_x - J3_BODY_RADIUS,
                                 game->land_z) ||
               j3_block_contains(next, game->land_x + J3_BODY_RADIUS,
                                 game->land_z) ||
               j3_block_contains(next, game->land_x,
                                 game->land_z - J3_BODY_RADIUS) ||
               j3_block_contains(next, game->land_x,
                                 game->land_z + J3_BODY_RADIUS)) {
        game->land_result = J3_LAND_EDGE;
    } else if (j3_block_contains(&game->blocks[0], game->land_x,
                                 game->land_z)) {
        game->land_result = J3_LAND_BACK;
    } else {
        game->land_result = J3_LAND_MISS;
    }

    game->spin = 0.0F;
    game->state = J3_STATE_FLYING;
    game->charge = 0.0F;
    game->body_scale = 1.0F;
    game->land_scale = 1.0F;
    game->blocks[0].squash = 1.0F;
}

static void j3_award_landing(j3_game_t *game) {
    j3_block_t *block = &game->blocks[1];
    uint32_t points = 1;
    const uint8_t perfect = game->land_result == J3_LAND_PERFECT;
    const uint8_t quick = game->quick_window > 0.0F;

    if (perfect) {
        game->combo = (uint16_t)(game->combo + 1u);
        points = game->combo <= 1u ? 2u : (uint32_t)game->combo * 2u;
        if (quick && points <= 2u) points *= 2u;
        if (points > 32u) points = 32u;
        game->popup_kind = quick ? 3u : 2u;
        j3_start_wave(game, (uint8_t)game->combo);
    } else {
        game->combo = 0;
        game->popup_kind = 1u;
        j3_start_wave(game, 1);
    }
    game->popup_points = (uint16_t)points;
    game->popup_timer = 0.9F;
    game->score += points;
    if (game->score > game->best) game->best = game->score;
    game->quick_window = J3_QUICK_WINDOW;

    /* Stay bonus timer for the special blocks. */
    if (j3_block_bonus(block) != 0u && !block->bonus_done)
        block->bonus_timer = J3_BONUS_STAY;
}

static void j3_shift_blocks(j3_game_t *game) {
    uint8_t index;
    for (index = 0; index + 1u < J3_BLOCK_MAX; ++index)
        game->blocks[index] = game->blocks[index + 1u];
    game->blocks[1].appear = 0.0F;
    game->blocks[1].squash = 1.0F;
    game->blocks[1].bonus_timer = 0.0F;
    game->blocks[1].bonus_done = 0;
}

static void j3_land(j3_game_t *game) {
    j3_block_t *block = &game->blocks[1];
    game->px = game->land_x;
    game->pz = game->land_z;
    game->py = j3_block_top(block);
    game->vx = 0.0F;
    game->vz = 0.0F;
    game->vy = 0.0F;
    game->spin = 0.0F;
    game->tilt = 0.0F;
    game->land_scale = 0.82F;
    game->jump_count = (uint16_t)(game->jump_count + 1u);

    j3_award_landing(game);

    /* Difficulty ramps up like the original. */
    game->min_radius_scale -= J3_RADIUS_SCALE_DECAY;
    if (game->min_radius_scale < J3_RADIUS_SCALE_MIN_FLOOR)
        game->min_radius_scale = J3_RADIUS_SCALE_MIN_FLOOR;
    game->max_radius_scale -= J3_RADIUS_SCALE_DECAY;
    if (game->max_radius_scale < J3_RADIUS_SCALE_MAX_FLOOR)
        game->max_radius_scale = J3_RADIUS_SCALE_MAX_FLOOR;
    game->max_distance += J3_GAP_GROWTH;
    if (game->max_distance > J3_GAP_MAX_LIMIT)
        game->max_distance = J3_GAP_MAX_LIMIT;
    if (game->score >= 1000u) game->bonus_interval = 6;
    if (game->score >= 3000u) game->bonus_interval = 7;

    j3_shift_blocks(game);
    j3_spawn_block(game, &game->blocks[2], &game->blocks[1], 1);
    game->block_count = 3;
    if (game->land_result == J3_LAND_PERFECT) game->blocks[1].center_dot = 1;
    game->state = J3_STATE_READY;
    j3_face_next(game);
}

static void j3_fail(j3_game_t *game) {
    game->combo = 0;
    game->quick_window = 0.0F;
    game->blocks[0].squash = 1.0F;
    game->blocks[0].bonus_timer = 0.0F;
    game->blocks[1].squash = 1.0F;
    game->blocks[1].bonus_timer = 0.0F;
    if (game->land_result == J3_LAND_EDGE ||
        game->land_result == J3_LAND_BACK) {
        const j3_block_t *reference = game->land_result == J3_LAND_BACK
                                          ? &game->blocks[0]
                                          : &game->blocks[1];
        const float forward =
            (game->land_x - reference->x) * game->dir_x +
            (game->land_z - reference->z) * game->dir_z;
        game->state = J3_STATE_TIP;
        game->tilt = forward >= 0.0F ? -0.001F : 0.001F;
        game->px = j3_clamp(game->px, reference->x - reference->radius,
                            reference->x + reference->radius);
        game->pz = j3_clamp(game->pz, reference->z - reference->radius,
                            reference->z + reference->radius);
        game->py = j3_block_top(reference);
        return;
    }
    /* A clean miss just drops past the blocks. */
    game->state = J3_STATE_FALL;
    game->tilt = 0.0F;
    game->vy = -0.4F;
    game->vx = game->dir_x * 0.25F;
    game->vz = game->dir_z * 0.25F;
}

static void j3_tick_flight(j3_game_t *game, float dt) {
    const float top = j3_block_top(&game->blocks[1]);
    game->vy -= J3_GRAVITY * dt;
    game->px += game->vx * dt;
    game->py += game->vy * dt;
    game->pz += game->vz * dt;
    if (game->land_time > 0.0F) {
        game->spin -= (J3_TWO_PI / (game->land_time * 0.88F)) * dt;
        if (game->spin < -J3_TWO_PI) game->spin += J3_TWO_PI;
    }

    if (game->vy < 0.0F && game->py <= top) {
        if (game->land_result <= J3_LAND_PERFECT) {
            j3_land(game);
            return;
        }
        game->py = top;
        j3_fail(game);
        return;
    }
    if (game->py < -2.5F) j3_fail(game);
}

static void j3_tick_bonus(j3_game_t *game, float dt) {
    j3_block_t *block = &game->blocks[0];
    const uint16_t points = j3_block_bonus(block);
    if (points == 0u || block->bonus_done || block->bonus_timer <= 0.0F)
        return;
    block->bonus_timer -= dt;
    if (block->bonus_timer > 0.0F) return;
    block->bonus_timer = 0.0F;
    block->bonus_done = 1;
    game->score += points;
    if (game->score > game->best) game->best = game->score;
    game->bonus_points = points;
    game->bonus_kind = block->kind;
    game->bonus_popup_timer = 1.1F;
}

void j3_game_tick(j3_game_t *game, float dt) {
    uint8_t index;
    const float smooth = j3_clamp(dt * 5.5F, 0.0F, 1.0F);
    const float target_x = (game->blocks[0].x + game->blocks[1].x) * 0.5F;
    const float target_z = (game->blocks[0].z + game->blocks[1].z) * 0.5F;

    game->time += dt;
    if (game->quick_window > 0.0F) {
        game->quick_window -= dt;
        if (game->quick_window < 0.0F) game->quick_window = 0.0F;
    }
    if (game->popup_timer > 0.0F) game->popup_timer -= dt;
    if (game->bonus_popup_timer > 0.0F) game->bonus_popup_timer -= dt;
    if (game->land_scale < 1.0F) {
        game->land_scale += dt * 1.6F;
        if (game->land_scale > 1.0F) game->land_scale = 1.0F;
    }
    for (index = 0; index < J3_WAVE_MAX; ++index) {
        if (!game->waves[index].active) continue;
        game->waves[index].t += dt;
        if (game->waves[index].t >= 0.62F) game->waves[index].active = 0;
    }
    for (index = 0; index < game->block_count; ++index) {
        if (game->blocks[index].appear < 1.0F) {
            game->blocks[index].appear += dt * J3_DROP_IN;
            if (game->blocks[index].appear > 1.0F)
                game->blocks[index].appear = 1.0F;
        }
    }

    switch (game->state) {
    case J3_STATE_CHARGING:
        game->charge += dt;
        game->body_scale = 1.0F - game->charge * J3_CHARGE_SQUASH;
        if (game->body_scale < J3_MIN_SQUASH)
            game->body_scale = J3_MIN_SQUASH;
        game->blocks[0].squash = game->body_scale;
        break;
    case J3_STATE_FLYING:
        j3_tick_flight(game, dt);
        break;
    case J3_STATE_TIP: {
        const float rate = J3_HALF_PI / J3_TIP_TIME;
        if (game->tilt >= 0.0F)
            game->tilt = j3_clamp(game->tilt + rate * dt, 0.0F, J3_HALF_PI);
        else
            game->tilt = j3_clamp(game->tilt - rate * dt, -J3_HALF_PI, 0.0F);
        if (j3_fabs(game->tilt) >= J3_HALF_PI) {
            game->state = J3_STATE_FALL;
            game->vy = -0.4F;
            game->vx = game->dir_x * 0.3F;
            game->vz = game->dir_z * 0.3F;
        }
        break;
    }
    case J3_STATE_FALL:
        game->vy -= J3_GRAVITY * 0.55F * dt;
        game->px += game->vx * dt;
        game->pz += game->vz * dt;
        game->py += game->vy * dt;
        game->spin -= dt * 2.4F;
        if (game->py < -1.8F) {
            game->py = -1.8F;
            game->state = J3_STATE_OVER;
        }
        break;
    case J3_STATE_OVER:
        break;
    default:
        j3_tick_bonus(game, dt);
        break;
    }

    if (game->state == J3_STATE_READY || game->state == J3_STATE_CHARGING)
        game->py = j3_block_top(&game->blocks[0]);

    game->cam_x += (target_x - game->cam_x) * smooth;
    game->cam_z += (target_z - game->cam_z) * smooth;
}
