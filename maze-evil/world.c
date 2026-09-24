#include "world.h"

#include "assets.h"
#include "rc_math.h"
#include "sprites.h"

/* Tiles are stored as the wall texture slot; the floor and doors get
 * out-of-band values. */
#define TILE_FLOOR 0xFFu
#define TILE_DOOR 0xFEu

#define PLAYER_RADIUS 0.28F
#define IMP_RADIUS 0.3F
#define MOVE_SPEED 3.2F
#define STRAFE_SPEED 2.6F
#define IMP_SPEED 1.5F
#define IMP_HEALTH 100
#define SHOT_DAMAGE 34
#define SHOT_COOLDOWN 0.55F
#define FIREBALL_SPEED 5.5F
#define FIREBALL_DAMAGE 14
#define MELEE_DAMAGE 9
#define DOOR_SPEED (1.0F / 0.6F)
#define DOOR_PASSABLE 0.7F
#define DOOR_HOLD 2.5F

/* clang-format off */
static const char *const kLevelRows[MAP_HEIGHT] = {
    "################################",
    "#......#.......%%%%%%%%%%%%%%%%#",
    "#.S....#.......%.......T......%#",
    "#......D.......%..............%#",
    "#......#.......%...B......B...%#",
    "#..T...#.......%..............%#",
    "########.......%.....E....E...%#",
    "#......#.......%..............%#",
    "#..A...#..E....%..H...........%#",
    "#......D.......%%%%D%%%%%%%%%%%#",
    "#......#...............#.......#",
    "########...............#.......#",
    "====================D==#..E.A..#",
    "=....=....=....=......=#.......#",
    "=.E..=..T.=..E.=......=#.......#",
    "=....D....D....D......=#...T...#",
    "=....=....=....=......=####.####",
    "==D=====D=====D=......=&&&&D&&&&",
    "=.....................=&.......&",
    "=..T......A.......T...=&..E....&",
    "=.....................D........&",
    "=..............H......=&.......&",
    "=...E......B....B.....=&&&&&&D&&",
    "==========D===========D........&",
    "WWWWWWWWWW.WWWWWWWWWWWW&.......&",
    "W.......W.............W&..E.E..&",
    "W..A.H..D......T......W&.......&",
    "W.......W.............W&..H....&",
    "W..T....W.....E..E....W&&&&D&&&&",
    "W.......W.............D.......X#",
    "W.......W......E......W.......X#",
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",
};
/* clang-format on */

static float length2(float x, float y) { return rc_sqrt(x * x + y * y); }

static uint8_t tile_from_symbol(char symbol) {
    switch (symbol) {
        case '#':
            return TEX_BRICK;
        case '%':
            return TEX_STONE;
        case '=':
            return TEX_TECH;
        case '&':
            return TEX_FLESH;
        case 'W':
            return TEX_WOOD;
        case 'D':
            return TILE_DOOR;
        case 'X':
            return TEX_EXIT;
        default:
            return TILE_FLOOR;
    }
}

static int is_wall_tile(uint8_t tile) {
    return tile != TILE_FLOOR && tile != TILE_DOOR;
}

static uint32_t rng_next(world_t *world) {
    world->rng ^= world->rng << 13;
    world->rng ^= world->rng >> 17;
    world->rng ^= world->rng << 5;
    return world->rng;
}

static float rng_unit(world_t *world) {
    return (float)(rng_next(world) >> 8) * (1.0F / 16777216.0F);
}

static uint8_t tile_at(const world_t *world, int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_WIDTH || y >= MAP_HEIGHT) {
        return TEX_BRICK;
    }
    return world->tiles[y][x];
}

static door_t *door_at(world_t *world, int x, int y) {
    int i;
    for (i = 0; i < world->door_count; ++i) {
        if (world->doors[i].x == x && world->doors[i].y == y) {
            return &world->doors[i];
        }
    }
    return 0;
}

static const door_t *door_at_const(const world_t *world, int x, int y) {
    int i;
    for (i = 0; i < world->door_count; ++i) {
        if (world->doors[i].x == x && world->doors[i].y == y) {
            return &world->doors[i];
        }
    }
    return 0;
}

static float door_open(const world_t *world, int x, int y) {
    const door_t *door = door_at_const(world, x, y);
    return door == 0 ? 0.0F : door->open;
}

static int blocked_at(const world_t *world, int tile_x, int tile_y) {
    const uint8_t tile = tile_at(world, tile_x, tile_y);
    if (tile == TILE_DOOR) {
        return door_open(world, tile_x, tile_y) < DOOR_PASSABLE;
    }
    return is_wall_tile(tile);
}

static int actor_blocked(const world_t *world, float x, float y, float radius) {
    const int min_x = rc_floor_int(x - radius);
    const int max_x = rc_floor_int(x + radius);
    const int min_y = rc_floor_int(y - radius);
    const int max_y = rc_floor_int(y + radius);
    int ty;
    int i;
    for (ty = min_y; ty <= max_y; ++ty) {
        int tx;
        for (tx = min_x; tx <= max_x; ++tx) {
            if (blocked_at(world, tx, ty)) {
                return 1;
            }
        }
    }
    for (i = 0; i < world->decoration_count; ++i) {
        const decoration_t *d = &world->decorations[i];
        if (d->solid && length2(d->x - x, d->y - y) < radius + 0.3F) {
            return 1;
        }
    }
    return 0;
}

static int move_actor(const world_t *world, float *x, float *y, float dx,
                      float dy, float radius) {
    int moved = 0;
    if (!actor_blocked(world, *x + dx, *y, radius)) {
        *x += dx;
        moved = 1;
    }
    if (!actor_blocked(world, *x, *y + dy, radius)) {
        *y += dy;
        moved = 1;
    }
    return moved;
}

static int line_of_sight(const world_t *world, float x0, float y0, float x1,
                         float y1) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    int map_x = rc_floor_int(x0);
    int map_y = rc_floor_int(y0);
    const int end_x = rc_floor_int(x1);
    const int end_y = rc_floor_int(y1);
    const float delta_x = dx == 0.0F ? 1e30F : rc_fabs(1.0F / dx);
    const float delta_y = dy == 0.0F ? 1e30F : rc_fabs(1.0F / dy);
    const int step_x = dx < 0.0F ? -1 : 1;
    const int step_y = dy < 0.0F ? -1 : 1;
    float side_x =
        dx < 0.0F ? (x0 - (float)map_x) * delta_x
                  : ((float)map_x + 1.0F - x0) * delta_x;
    float side_y =
        dy < 0.0F ? (y0 - (float)map_y) * delta_y
                  : ((float)map_y + 1.0F - y0) * delta_y;
    int guard;
    for (guard = 0; guard < 128; ++guard) {
        uint8_t tile;
        if (map_x == end_x && map_y == end_y) {
            return 1;
        }
        if (side_x < side_y) {
            side_x += delta_x;
            map_x += step_x;
        } else {
            side_y += delta_y;
            map_y += step_y;
        }
        tile = tile_at(world, map_x, map_y);
        if (is_wall_tile(tile) ||
            (tile == TILE_DOOR && door_open(world, map_x, map_y) < 0.5F)) {
            return 0;
        }
    }
    return 0;
}

static void show_message(world_t *world, const char *text, float seconds) {
    world->message = text;
    world->message_timer = seconds;
}

static void emit(world_t *world, uint8_t id) {
    if (world->pending_sound_count < MAX_PENDING_SOUNDS) {
        world->pending_sounds[world->pending_sound_count].id = id;
        world->pending_sounds[world->pending_sound_count].gain = 255;
        ++world->pending_sound_count;
    }
}

static void emit_at(world_t *world, uint8_t id, float x, float y) {
    const float distance =
        length2(x - world->player.x, y - world->player.y);
    /* Inverse-distance rolloff; inaudible beyond ~14 tiles. */
    const float gain =
        distance > 14.0F ? 0.0F : 1.0F / (1.0F + distance * 0.35F);
    if (gain > 0.03F && world->pending_sound_count < MAX_PENDING_SOUNDS) {
        world->pending_sounds[world->pending_sound_count].id = id;
        world->pending_sounds[world->pending_sound_count].gain =
            (uint8_t)(gain * 255.0F);
        ++world->pending_sound_count;
    }
}

int world_take_sounds(world_t *world, sound_event_t *out, int capacity) {
    const int count = world->pending_sound_count < capacity
                          ? world->pending_sound_count
                          : capacity;
    int i;
    for (i = 0; i < count; ++i) {
        out[i] = world->pending_sounds[i];
    }
    world->pending_sound_count = 0;
    return count;
}

static void damage_player(world_t *world, int amount) {
    world->player.health -= amount;
    world->player.damage_flash = 0.35F;
    if (world->player.health <= 0) {
        world->player.health = 0;
        world->phase = PHASE_DEAD;
        show_message(world, "YOU DIED. TAP TO RETRY", 60.0F);
        emit(world, SND_DIE);
    } else {
        emit(world, SND_PLAYER_PAIN);
    }
}

static void spawn_fireball(world_t *world, float x, float y, float target_x,
                           float target_y) {
    int i;
    for (i = 0; i < MAX_FIREBALLS; ++i) {
        fireball_t *fb = &world->fireballs[i];
        float vx;
        float vy;
        float len;
        float wobble;
        float c;
        float s;
        float nx;
        float ny;
        if (fb->alive) {
            continue;
        }
        vx = target_x - x;
        vy = target_y - y;
        len = length2(vx, vy);
        if (len < 0.01F) {
            return;
        }
        /* Slight inaccuracy so the player can dodge at range. */
        wobble = (rng_unit(world) - 0.5F) * 0.12F;
        c = rc_cos(wobble);
        s = rc_sin(wobble);
        nx = (vx * c - vy * s) / len;
        ny = (vx * s + vy * c) / len;
        fb->alive = 1;
        fb->x = x + nx * 0.4F;
        fb->y = y + ny * 0.4F;
        fb->vx = nx * FIREBALL_SPEED;
        fb->vy = ny * FIREBALL_SPEED;
        fb->age = 0.0F;
        emit_at(world, SND_IMP_FIREBALL, x, y);
        return;
    }
}

static void shoot(world_t *world) {
    player_t *p = &world->player;
    imp_t *target = 0;
    float best = 1e9F;
    int i;
    p->fire_cooldown = SHOT_COOLDOWN;
    if (p->ammo <= 0) {
        show_message(world, "OUT OF SHELLS", 1.0F);
        emit(world, SND_EMPTY_CLICK);
        return;
    }
    --p->ammo;
    p->recoil = 1.0F;
    emit(world, SND_SHOTGUN);

    /* Hitscan: the closest imp inside a narrow cone with clear line of sight. */
    for (i = 0; i < world->imp_count; ++i) {
        imp_t *imp = &world->imps[i];
        float vx;
        float vy;
        float dist;
        float along;
        float tolerance;
        if (imp->state == IMP_DEAD) {
            continue;
        }
        vx = imp->x - p->x;
        vy = imp->y - p->y;
        dist = length2(vx, vy);
        if (dist < 0.01F || dist > 14.0F) {
            continue;
        }
        along = (vx * p->dir_x + vy * p->dir_y) / dist;
        tolerance = rc_atan(IMP_RADIUS / dist) + 0.03F; /* shotgun spread */
        if (along < rc_cos(tolerance)) {
            continue;
        }
        if (dist < best && line_of_sight(world, p->x, p->y, imp->x, imp->y)) {
            best = dist;
            target = imp;
        }
    }
    if (target == 0) {
        return;
    }
    /* Damage falls off with range like a shotgun. */
    {
        const int damage = best < 3.0F ? SHOT_DAMAGE + 16
                                       : (best < 7.0F ? SHOT_DAMAGE
                                                      : SHOT_DAMAGE - 10);
        target->health -= damage;
    }
    if (target->health <= 0) {
        target->state = IMP_DEAD;
        target->timer = 0.0F;
        ++world->kills;
        emit_at(world, SND_IMP_DEATH, target->x, target->y);
        if (world->kills == world->imp_count) {
            show_message(world, "ALL IMPS DEAD. GET TO THE EXIT!", 3.0F);
        }
    } else {
        target->state = IMP_PAIN;
        target->timer = 0.35F;
        emit_at(world, SND_IMP_PAIN, target->x, target->y);
    }
}

static void update_player(world_t *world, float dt,
                          const controls_t *controls) {
    player_t *p = &world->player;
    float forward;
    float strafe;
    float dx;
    float dy;
    p->angle += controls->turn;
    if (p->angle > RC_PI) {
        p->angle -= 2.0F * RC_PI;
    } else if (p->angle < -RC_PI) {
        p->angle += 2.0F * RC_PI;
    }
    p->dir_x = rc_cos(p->angle);
    p->dir_y = rc_sin(p->angle);
    p->plane_x = -p->dir_y * 0.66F;
    p->plane_y = p->dir_x * 0.66F;

    forward = controls->forward * MOVE_SPEED * dt;
    strafe = controls->strafe * STRAFE_SPEED * dt;
    dx = p->dir_x * forward - p->dir_y * strafe;
    dy = p->dir_y * forward + p->dir_x * strafe;
    move_actor(world, &p->x, &p->y, dx, dy, PLAYER_RADIUS);
    p->speed = length2(dx, dy) / (dt > 0.0F ? dt : 1.0F);
    if (p->speed > 0.2F) {
        p->bob_phase += dt * 9.0F;
    }

    if (p->fire_cooldown > 0.0F) {
        p->fire_cooldown -= dt;
    }
    if (p->recoil > 0.0F) {
        p->recoil -= dt * 4.0F;
        if (p->recoil < 0.0F) {
            p->recoil = 0.0F;
        }
    }
    if (p->damage_flash > 0.0F) {
        p->damage_flash -= dt;
    }
    if (controls->fire && p->fire_cooldown <= 0.0F) {
        shoot(world);
    }
}

static void update_imps(world_t *world, float dt) {
    const player_t *p = &world->player;
    int i;
    for (i = 0; i < world->imp_count; ++i) {
        imp_t *imp = &world->imps[i];
        float vx;
        float vy;
        float dist;
        if (imp->state == IMP_DEAD) {
            continue;
        }
        vx = p->x - imp->x;
        vy = p->y - imp->y;
        dist = length2(vx, vy);
        imp->attack_cooldown -= dt;

        switch (imp->state) {
            case IMP_IDLE:
                if (dist < 11.0F &&
                    line_of_sight(world, imp->x, imp->y, p->x, p->y)) {
                    imp->state = IMP_CHASE;
                    emit_at(world, SND_IMP_ALERT, imp->x, imp->y);
                }
                break;
            case IMP_PAIN:
                imp->timer -= dt;
                if (imp->timer <= 0.0F) {
                    imp->state = IMP_CHASE;
                }
                break;
            case IMP_CHASE: {
                const int sees =
                    line_of_sight(world, imp->x, imp->y, p->x, p->y);
                imp->anim += dt;
                if (imp->attack_cooldown <= 0.0F && sees && dist < 8.0F) {
                    imp->state = IMP_ATTACK;
                    imp->timer = 0.0F;
                    imp->fired = 0;
                    break;
                }
                if (dist > 1.1F) {
                    const float step = IMP_SPEED * dt;
                    /* Home in; try the axis-aligned fallback when blocked. */
                    if (!move_actor(world, &imp->x, &imp->y, vx / dist * step,
                                    vy / dist * step, IMP_RADIUS)) {
                        move_actor(world, &imp->x, &imp->y,
                                   (vx > 0 ? step : -step), 0.0F, IMP_RADIUS);
                    }
                    /* Keep imps from stacking into each other. */
                    {
                        int j;
                        for (j = 0; j < world->imp_count; ++j) {
                            float sx;
                            float sy;
                            float sd;
                            if (j == i || world->imps[j].state == IMP_DEAD) {
                                continue;
                            }
                            sx = imp->x - world->imps[j].x;
                            sy = imp->y - world->imps[j].y;
                            sd = length2(sx, sy);
                            if (sd > 0.001F && sd < 0.6F) {
                                move_actor(world, &imp->x, &imp->y,
                                           sx / sd * step * 0.5F,
                                           sy / sd * step * 0.5F, IMP_RADIUS);
                            }
                        }
                    }
                }
                break;
            }
            case IMP_ATTACK:
                imp->timer += dt;
                if (!imp->fired && imp->timer >= 0.3F) {
                    imp->fired = 1;
                    if (dist < 1.4F) {
                        emit(world, SND_IMP_MELEE);
                        damage_player(world, MELEE_DAMAGE);
                    } else {
                        spawn_fireball(world, imp->x, imp->y, p->x, p->y);
                    }
                }
                if (imp->timer >= 0.65F) {
                    imp->state = IMP_CHASE;
                    imp->attack_cooldown = 1.4F + rng_unit(world) * 1.0F;
                }
                break;
            default:
                break;
        }
    }
}

static void update_fireballs(world_t *world, float dt) {
    int i;
    for (i = 0; i < MAX_FIREBALLS; ++i) {
        fireball_t *fb = &world->fireballs[i];
        int tx;
        int ty;
        if (!fb->alive) {
            continue;
        }
        fb->age += dt;
        fb->x += fb->vx * dt;
        fb->y += fb->vy * dt;
        tx = rc_floor_int(fb->x);
        ty = rc_floor_int(fb->y);
        if (fb->age > 4.0F || blocked_at(world, tx, ty)) {
            fb->alive = 0;
            if (fb->age <= 4.0F) {
                emit_at(world, SND_FIREBALL_EXPLODE, fb->x, fb->y);
            }
            continue;
        }
        if (length2(fb->x - world->player.x, fb->y - world->player.y) <
            0.45F) {
            fb->alive = 0;
            emit(world, SND_FIREBALL_EXPLODE);
            damage_player(world, FIREBALL_DAMAGE);
        }
    }
}

static void update_doors(world_t *world, float dt) {
    int i;
    for (i = 0; i < world->door_count; ++i) {
        door_t *door = &world->doors[i];
        const float cx = door->x + 0.5F;
        const float cy = door->y + 0.5F;
        int wants_open = length2(world->player.x - cx, world->player.y - cy) < 1.4F;
        int occupied = length2(world->player.x - cx, world->player.y - cy) < 0.8F;
        int j;
        for (j = 0; j < world->imp_count && !wants_open; ++j) {
            const imp_t *imp = &world->imps[j];
            if (imp->state == IMP_CHASE || imp->state == IMP_ATTACK) {
                const float d = length2(imp->x - cx, imp->y - cy);
                wants_open = d < 1.2F;
                occupied = occupied || d < 0.8F;
            }
        }
        switch (door->state) {
            case DOOR_CLOSED:
                if (wants_open) {
                    door->state = DOOR_OPENING;
                    emit_at(world, SND_DOOR_OPEN, cx, cy);
                }
                break;
            case DOOR_OPENING:
                door->open += DOOR_SPEED * dt;
                if (door->open >= 1.0F) {
                    door->open = 1.0F;
                    door->state = DOOR_OPEN;
                    door->timer = DOOR_HOLD;
                }
                break;
            case DOOR_OPEN:
                if (wants_open) {
                    door->timer = DOOR_HOLD;
                } else {
                    door->timer -= dt;
                    if (door->timer <= 0.0F && !occupied) {
                        door->state = DOOR_CLOSING;
                        emit_at(world, SND_DOOR_CLOSE, cx, cy);
                    }
                }
                break;
            case DOOR_CLOSING:
                if (wants_open || occupied) {
                    door->state = DOOR_OPENING;
                    break;
                }
                door->open -= DOOR_SPEED * dt;
                if (door->open <= 0.0F) {
                    door->open = 0.0F;
                    door->state = DOOR_CLOSED;
                }
                break;
            default:
                break;
        }
        world->cells[door->y][door->x].kind = RAY_CELL_SLAB;
        world->cells[door->y][door->x].texture_slot = TEX_DOOR;
        world->cells[door->y][door->x].open =
            (uint16_t)(door->open * 32768.0F + 0.5F);
    }
}

static void update_items(world_t *world) {
    int i;
    for (i = 0; i < world->item_count; ++i) {
        item_t *item = &world->items[i];
        if (!item->alive ||
            length2(item->x - world->player.x, item->y - world->player.y) >
                0.6F) {
            continue;
        }
        if (item->kind == ITEM_MEDKIT) {
            if (world->player.health >= 100) {
                continue;
            }
            world->player.health = world->player.health + 25 > 100
                                       ? 100
                                       : world->player.health + 25;
            show_message(world, "PICKED UP A MEDKIT", 1.2F);
            emit(world, SND_PICKUP_HEALTH);
        } else {
            if (world->player.ammo >= 50) {
                continue;
            }
            world->player.ammo =
                world->player.ammo + 10 > 50 ? 50 : world->player.ammo + 10;
            show_message(world, "PICKED UP SHELLS", 1.2F);
            emit(world, SND_PICKUP_AMMO);
        }
        item->alive = 0;
    }
}

static void check_exit(world_t *world) {
    const int px = rc_floor_int(world->player.x);
    const int py = rc_floor_int(world->player.y);
    int touching = 0;
    int dy;
    for (dy = -1; dy <= 1 && !touching; ++dy) {
        int dx;
        for (dx = -1; dx <= 1; ++dx) {
            if (tile_at(world, px + dx, py + dy) == TEX_EXIT) {
                /* Require the player to actually press against the exit face. */
                const float ex = (float)(px + dx) + 0.5F;
                const float ey = (float)(py + dy) + 0.5F;
                if (length2(ex - world->player.x, ey - world->player.y) <
                    0.85F) {
                    touching = 1;
                    break;
                }
            }
        }
    }
    if (!touching) {
        return;
    }
    if (world->kills >= world->imp_count) {
        world->phase = PHASE_WON;
        show_message(world, "AREA CLEARED! TAP TO PLAY AGAIN", 60.0F);
        emit(world, SND_WIN);
    } else if (world->message_timer <= 0.0F) {
        show_message(world, "THE EXIT IS SEALED. KILL EVERY IMP", 2.0F);
        emit(world, SND_EXIT_SEALED);
    }
}

void world_reset(world_t *world) {
    int y;
    int i;
    world->imp_count = 0;
    world->item_count = 0;
    world->decoration_count = 0;
    world->door_count = 0;
    world->kills = 0;
    world->phase = PHASE_PLAYING;
    world->time = 0.0F;
    world->message = 0;
    world->message_timer = 0.0F;
    world->fire_was_down = 1; /* The tap that restarted must not fire. */
    world->pending_sound_count = 0;
    world->rng = 0x9E3779B9u;
    for (i = 0; i < MAX_FIREBALLS; ++i) {
        world->fireballs[i].alive = 0;
        world->fireballs[i].age = 0.0F;
    }
    world->player.health = 100;
    world->player.ammo = 24;
    world->player.angle = 0.0F;
    world->player.x = 0.0F;
    world->player.y = 0.0F;
    world->player.recoil = 0.0F;
    world->player.fire_cooldown = 0.0F;
    world->player.damage_flash = 0.0F;
    world->player.bob_phase = 0.0F;
    world->player.speed = 0.0F;

    for (y = 0; y < MAP_HEIGHT; ++y) {
        int x;
        for (x = 0; x < MAP_WIDTH; ++x) {
            const char symbol = kLevelRows[y][x];
            const uint8_t tile = tile_from_symbol(symbol);
            const float cx = (float)x + 0.5F;
            const float cy = (float)y + 0.5F;
            world->tiles[y][x] = tile;
            switch (symbol) {
                case 'S':
                    world->player.x = cx;
                    world->player.y = cy;
                    break;
                case 'E':
                    if (world->imp_count < MAX_IMPS) {
                        imp_t *imp = &world->imps[world->imp_count++];
                        imp->x = cx;
                        imp->y = cy;
                        imp->health = IMP_HEALTH;
                        imp->state = IMP_IDLE;
                        imp->timer = 0.0F;
                        imp->anim = 0.0F;
                        imp->attack_cooldown = 0.0F;
                        imp->fired = 0;
                    }
                    break;
                case 'H':
                    if (world->item_count < MAX_ITEMS) {
                        item_t *item = &world->items[world->item_count++];
                        item->alive = 1;
                        item->kind = ITEM_MEDKIT;
                        item->x = cx;
                        item->y = cy;
                    }
                    break;
                case 'A':
                    if (world->item_count < MAX_ITEMS) {
                        item_t *item = &world->items[world->item_count++];
                        item->alive = 1;
                        item->kind = ITEM_AMMO;
                        item->x = cx;
                        item->y = cy;
                    }
                    break;
                case 'T':
                    if (world->decoration_count < MAX_DECORATIONS) {
                        decoration_t *d =
                            &world->decorations[world->decoration_count++];
                        d->sprite = SPR_TORCH_A;
                        d->solid = 0;
                        d->animated = 1;
                        d->x = cx;
                        d->y = cy;
                    }
                    break;
                case 'B':
                    if (world->decoration_count < MAX_DECORATIONS) {
                        decoration_t *d =
                            &world->decorations[world->decoration_count++];
                        d->sprite = SPR_BARREL;
                        d->solid = 1;
                        d->animated = 0;
                        d->x = cx;
                        d->y = cy;
                    }
                    break;
                case 'D':
                    if (world->door_count < MAX_DOORS) {
                        door_t *door = &world->doors[world->door_count++];
                        door->x = x;
                        door->y = y;
                        door->open = 0.0F;
                        door->state = DOOR_CLOSED;
                        door->timer = 0.0F;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    world->player.angle = 0.0F;
    world->player.dir_x = rc_cos(world->player.angle);
    world->player.dir_y = rc_sin(world->player.angle);
    world->player.plane_x = -world->player.dir_y * 0.66F;
    world->player.plane_y = world->player.dir_x * 0.66F;
    for (y = 0; y < MAP_HEIGHT; ++y) {
        int x;
        for (x = 0; x < MAP_WIDTH; ++x) {
            const uint8_t tile = world->tiles[y][x];
            ray_cell_t *cell = &world->cells[y][x];
            if (tile == TILE_DOOR) {
                cell->kind = RAY_CELL_SLAB;
                cell->texture_slot = TEX_DOOR;
                cell->open = 0;
            } else if (is_wall_tile(tile)) {
                cell->kind = RAY_CELL_WALL;
                cell->texture_slot = tile;
                cell->open = 0;
            } else {
                cell->kind = RAY_CELL_EMPTY;
                cell->texture_slot = 0;
                cell->open = 0;
            }
        }
    }
    show_message(world, "FIND THE EXIT. KILL EVERY IMP.", 4.0F);
}

void world_update(world_t *world, float dt, const controls_t *controls) {
    int i;
    world->time += dt;
    if (world->message_timer > 0.0F) {
        world->message_timer -= dt;
    }
    if (world->phase != PHASE_PLAYING) {
        /* Any tap restarts once the previous press is released. */
        if (controls->fire && !world->fire_was_down) {
            world_reset(world);
        }
        world->fire_was_down = controls->fire;
        return;
    }
    update_player(world, dt, controls);
    update_imps(world, dt);
    update_fireballs(world, dt);
    update_doors(world, dt);
    update_items(world);
    check_exit(world);
    for (i = 0; i < world->decoration_count; ++i) {
        if (world->decorations[i].animated) {
            world->decorations[i].sprite =
                torch_frame((int)(world->time * 8.0F));
        }
    }
    world->fire_was_down = controls->fire;
}

int world_collect_things(const world_t *world, thing_t *out, int capacity) {
    int count = 0;
    int i;
    for (i = 0; i < world->decoration_count && count < capacity; ++i) {
        const decoration_t *d = &world->decorations[i];
        out[count].x = d->x;
        out[count].y = d->y;
        out[count].sprite = d->sprite;
        if (d->sprite == SPR_BARREL) {
            out[count].height = 0.55F;
            out[count].lift = 0.0F;
        } else {
            out[count].height = 0.9F;
            out[count].lift = 0.05F;
        }
        ++count;
    }
    for (i = 0; i < world->item_count && count < capacity; ++i) {
        if (world->items[i].alive) {
            out[count].x = world->items[i].x;
            out[count].y = world->items[i].y;
            out[count].sprite = world->items[i].kind == ITEM_MEDKIT
                                    ? SPR_MEDKIT
                                    : SPR_AMMO;
            out[count].height = 0.3F;
            out[count].lift = 0.0F;
            ++count;
        }
    }
    for (i = 0; i < world->imp_count && count < capacity; ++i) {
        const imp_t *imp = &world->imps[i];
        int sprite = SPR_IMP_WALK_A;
        switch (imp->state) {
            case IMP_IDLE:
                sprite = SPR_IMP_WALK_A;
                break;
            case IMP_CHASE:
                sprite = ((int)(imp->anim * 5.0F) & 1) ? SPR_IMP_WALK_B
                                                       : SPR_IMP_WALK_A;
                break;
            case IMP_ATTACK:
                sprite = SPR_IMP_ATTACK;
                break;
            case IMP_PAIN:
                sprite = SPR_IMP_PAIN;
                break;
            case IMP_DEAD:
                sprite = SPR_IMP_DEAD;
                break;
            default:
                break;
        }
        out[count].x = imp->x;
        out[count].y = imp->y;
        out[count].sprite = sprite;
        out[count].height = 0.85F;
        out[count].lift = 0.0F;
        ++count;
    }
    for (i = 0; i < MAX_FIREBALLS && count < capacity; ++i) {
        const fireball_t *fb = &world->fireballs[i];
        if (fb->alive) {
            out[count].x = fb->x;
            out[count].y = fb->y;
            out[count].sprite = ((int)(fb->age * 12.0F) & 1) ? SPR_FIREBALL_B
                                                             : SPR_FIREBALL_A;
            out[count].height = 0.3F;
            out[count].lift = 0.35F;
            ++count;
        }
    }
    return count;
}
