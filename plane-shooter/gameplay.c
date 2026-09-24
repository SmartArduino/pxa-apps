#define PXA_ARCADE_MODULE_PREFIX pxa_plane_game_
#ifndef PXA_ARCADE_MODULE_PREFIX
#define PXA_ARCADE_MODULE_PREFIX pxa_arcade_plane_shooter_
#endif
#include "pxa_arcade_module.h"

#include "pxa_canvas.h"
#include "pxa_game_screen.h"
#include "pxa_game_sfx.h"

#define GAME_ROOT_NODE 1u
#define GAME_NODE 2u
#define LEFT_GESTURE_GUARD 40
#define PLAYER_PLANE_W 38
#define PLAYER_PLANE_H 28
#define ENEMY_PLANE_W 28
#define ENEMY_PLANE_H 20
#define PLAYER_BULLET_COUNT 18
#define ENEMY_BULLET_COUNT 14
#define ENEMY_COUNT 9
#define PICKUP_COUNT 3
#define PICKUP_SIZE 20
#define SPARK_COUNT 24
#define ENGINE_PARTICLE_COUNT 14
#define ENEMY_ENGINE_PARTICLE_COUNT 28
#define BOSS_WIDTH 64
#define BOSS_HEIGHT 46
#define GAME_TICK_MS 33u
#define GAME_MAX_CATCHUP_STEPS 2u
#define VOLUME_ACCELERATION_PER_TICK 1
#define VOLUME_MAX_SPEED_PER_TICK 5
#define WAVE_SCORE_STEP 15u
#define OVERDRIVE_TICKS 120u
#define SHIELD_TICKS 90u

/* Design-space metrics of the original 296x240 build. The battle reflows the
 * HUD band and play field inside the real safe area, so the same constants
 * become margins and reserves instead of absolute positions. */
#define HUD_PANEL_W 232
#define HUD_PANEL_H 27
#define HUD_BAND_H 39
#define LANE_RIGHT_MARGIN 72
#define FIELD_BOTTOM_RESERVE 24
#define PLAYER_BOTTOM_RESERVE 36
#define DIALOG_W 220
#define DIALOG_H 157
#define PAUSE_DIALOG_W 200
#define PAUSE_DIALOG_H 151
#define BOSS_STOP_LEFT 22

typedef struct {
    int16_t width;
    int16_t height;
    int16_t field_left;
    int16_t field_top;
    int16_t field_right;
    int16_t field_bottom;
    int16_t lane_x;
    int16_t player_min_y;
    int16_t player_max_y;
    int16_t hud_x;
    int16_t hud_y;
    int16_t hud_w;
    int16_t pause_x;
    int16_t pause_y;
    int16_t dialog_x;
    int16_t dialog_y;
    int16_t pause_dialog_x;
    int16_t pause_dialog_y;
} battle_layout_t;

static battle_layout_t layout;
static pxa_game_screen_t game_screen;

static void layout_update(void) {
    const int width = (int)game_screen.width;
    const int height = (int)game_screen.height;
    const int left = (int)game_screen.safe_left;
    const int top = (int)game_screen.safe_top;
    const int right = (int)game_screen.safe_right;
    const int bottom = (int)game_screen.safe_bottom;
    int hud_w = HUD_PANEL_W;
    const int max_hud_w = width - 2 * (left + 6);
    if (hud_w > max_hud_w) hud_w = max_hud_w > 0 ? max_hud_w : width;
    layout.width = (int16_t)width;
    layout.height = (int16_t)height;
    layout.hud_w = (int16_t)hud_w;
    layout.hud_x = (int16_t)((width - hud_w) / 2);
    layout.hud_y = (int16_t)(top + 5);
    layout.field_left = (int16_t)left;
    layout.field_top = (int16_t)(top + HUD_BAND_H);
    layout.field_right = (int16_t)(width - right);
    layout.field_bottom = (int16_t)(height - bottom - FIELD_BOTTOM_RESERVE);
    if (layout.field_bottom < layout.field_top + 60)
        layout.field_bottom = (int16_t)(height - bottom);
    layout.lane_x = (int16_t)(layout.field_right - LANE_RIGHT_MARGIN);
    layout.player_min_y = (int16_t)(layout.field_top + 3);
    layout.player_max_y = (int16_t)(height - bottom - PLAYER_BOTTOM_RESERVE);
    if (layout.player_max_y < layout.player_min_y)
        layout.player_max_y = layout.player_min_y;
    layout.pause_x = (int16_t)(layout.field_right - 16);
    layout.pause_y = (int16_t)(top + 16);
    layout.dialog_x = (int16_t)((width - DIALOG_W) / 2);
    layout.dialog_y = (int16_t)((height - DIALOG_H) / 2);
    layout.pause_dialog_x = (int16_t)((width - PAUSE_DIALOG_W) / 2);
    layout.pause_dialog_y = (int16_t)((height - PAUSE_DIALOG_H) / 2);
}

/* HUD columns keep their design offsets from the panel's left edge, scaled if
 * the panel had to shrink. */
static int16_t hud_px(int offset) {
    return (int16_t)(layout.hud_x +
                     offset * (int)layout.hud_w / HUD_PANEL_W);
}

static int16_t hud_pw(int width) {
    return (int16_t)(width * (int)layout.hud_w / HUD_PANEL_W);
}

enum {
    ENEMY_SCOUT = 0,
    ENEMY_DART,
    ENEMY_FIGHTER,
    ENEMY_CRUISER,
    ENEMY_GUNSHIP,
    ENEMY_BOSS,
};

enum {
    PICKUP_ENERGY = 0,
    PICKUP_SHIELD,
    PICKUP_OVERDRIVE,
};

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vx;
    int8_t vy;
    uint8_t active;
    uint8_t kind;
} shot_t;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vy;
    uint8_t active;
    uint8_t health;
    uint8_t kind;
    uint8_t fire_delay;
} enemy_t;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vx;
    int8_t vy;
    uint8_t active;
    uint8_t life;
    uint32_t color;
} spark_t;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vx;
    int8_t vy;
    uint8_t active;
    uint8_t life;
} engine_particle_t;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vx;
    int8_t vy;
    uint8_t active;
    uint8_t life;
    uint8_t kind;
} enemy_engine_particle_t;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vy;
    uint8_t active;
    uint8_t health;
    uint8_t max_health;
    uint8_t fire_delay;
    uint8_t kind;
} boss_t;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vy;
    uint8_t active;
    uint8_t kind;
    uint8_t pulse;
} pickup_t;

static uint8_t draw_data[16 * 1024];
static uint8_t ui_commands[3600];
static uint8_t packet[512];
static uint32_t random_state = 0xc4832f19u;
static uint32_t score;
static uint16_t threat_cleared;
static uint16_t star_scroll;
static uint16_t combo;
static uint8_t lives;
static uint8_t energy;
static uint8_t fire_counter;
static uint8_t spawn_counter;
static uint8_t combo_decay;
static uint8_t wave_notice_ticks;
static uint8_t overdrive_ticks;
static uint8_t invulnerable_ticks;
static uint8_t boss_wave;
static uint8_t initialized;
static uint8_t game_over;
static uint8_t mission_complete;
static uint8_t paused;
static uint8_t backgrounded;
static uint8_t exit_requested;
static uint8_t result_available;
static uint8_t result_stars;
static uint16_t result_reward;
static uint8_t starting_lives;
static uint8_t control_active;
static uint8_t configured_weapon_level;
static uint8_t configured_hull_level;
static uint8_t configured_ship_model;
static uint8_t configured_mission;
static uint8_t configured_module;
static int8_t volume_input_direction;
static int8_t player_velocity_y;
static int16_t player_x;
static int16_t player_y;
static int16_t control_y;
static uint64_t last_tick_us;
static shot_t player_bullets[PLAYER_BULLET_COUNT];
static shot_t enemy_bullets[ENEMY_BULLET_COUNT];
static enemy_t enemies[ENEMY_COUNT];
static spark_t sparks[SPARK_COUNT];
static engine_particle_t engine_particles[ENGINE_PARTICLE_COUNT];
static enemy_engine_particle_t enemy_engine_particles[ENEMY_ENGINE_PARTICLE_COUNT];
static pickup_t pickups[PICKUP_COUNT];
static boss_t boss;
pxa_game_sfx_t pxa_plane_sfx;
#define sfx pxa_plane_sfx

static const char player_asset[] = "assets/plane-shooter/player-plane-left.png";
static const char player_mk2_asset[] = "assets/plane-shooter/player-plane-mk2.png";
static const char enemy_scout_asset[] = "assets/plane-shooter/enemy-plane.png";
static const char enemy_dart_asset[] = "assets/plane-shooter/enemy-dart.png";
static const char enemy_fighter_asset[] = "assets/plane-shooter/enemy-fighter.png";
static const char enemy_cruiser_asset[] = "assets/plane-shooter/enemy-cruiser.png";
static const char enemy_gunship_asset[] = "assets/plane-shooter/enemy-gunship.png";
static const char boss_asset[] = "assets/plane-shooter/boss-dreadnought.png";
static const char boss_carrier_asset[] = "assets/plane-shooter/boss-carrier.png";
static const char boss_leviathan_asset[] = "assets/plane-shooter/boss-leviathan.png";
static const char pickup_energy_asset[] = "assets/plane-shooter/pickup-energy.png";
static const char pickup_shield_asset[] = "assets/plane-shooter/pickup-shield.png";
static const char pickup_overdrive_asset[] = "assets/plane-shooter/pickup-overdrive.png";
static const char hud_panel_asset[] = "assets/ui/hud-panel.png";
static const char dialog_panel_asset[] = "assets/ui/dialog-panel.png";

/* Original deep-space loop: an ascending lead over a compact pulse rhythm. */
static const pxa_game_music_note_t plane_shooter_music[] = {
    PXA_MUSIC_NOTE(PXA_MUSIC_A4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_A5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_G5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_A4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_D5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_F5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_E5, 2), PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_A4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_G5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_A5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_D5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_A4, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_G4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_A4, 2),
    PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_D5, 2), PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_A4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_REST, 1),
};

static const pxa_game_music_event_t plane_shooter_music_score[] = {
    PXA_MUSIC_EVENT(0, PXA_MUSIC_A3, 8, 42, PXA_MUSIC_TIMBRE_STRINGS),
    PXA_MUSIC_EVENT(0, PXA_MUSIC_E4, 8, 34, PXA_MUSIC_TIMBRE_STRINGS),
    PXA_MUSIC_EVENT(8, PXA_MUSIC_F3, 8, 42, PXA_MUSIC_TIMBRE_STRINGS),
    PXA_MUSIC_EVENT(8, PXA_MUSIC_C4, 8, 34, PXA_MUSIC_TIMBRE_STRINGS),
    PXA_MUSIC_EVENT(16, PXA_MUSIC_C4, 8, 42, PXA_MUSIC_TIMBRE_STRINGS),
    PXA_MUSIC_EVENT(16, PXA_MUSIC_G4, 8, 34, PXA_MUSIC_TIMBRE_STRINGS),
    PXA_MUSIC_EVENT(24, PXA_MUSIC_G3, 8, 42, PXA_MUSIC_TIMBRE_STRINGS),
    PXA_MUSIC_EVENT(24, PXA_MUSIC_D4, 8, 34, PXA_MUSIC_TIMBRE_STRINGS),
};

static const pxa_game_music_song_t plane_shooter_song = {
    plane_shooter_music, PXA_MUSIC_LENGTH(plane_shooter_music), 156,
    0x1111, 0x4040, 0xAAAA,
    {PXA_MUSIC_A2, PXA_MUSIC_E3, PXA_MUSIC_A2, PXA_MUSIC_E3,
     PXA_MUSIC_F2, PXA_MUSIC_C3, PXA_MUSIC_G2, PXA_MUSIC_D3},
    plane_shooter_music_score, PXA_MUSIC_LENGTH(plane_shooter_music_score),
    32,
};

/* The boss loop has a lower register and denser percussion so the phase change
 * remains audible on the small built-in speaker. */
static const pxa_game_music_note_t boss_music[] = {
    PXA_MUSIC_NOTE(PXA_MUSIC_A3, 1), PXA_MUSIC_NOTE(PXA_MUSIC_C4, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_D4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E4, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_F4, 2), PXA_MUSIC_NOTE(PXA_MUSIC_E4, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_D4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_C4, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_A3, 1), PXA_MUSIC_NOTE(PXA_MUSIC_C4, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_E4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_G4, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_A4, 2), PXA_MUSIC_NOTE(PXA_MUSIC_G4, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_E4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_REST, 1),
};

static const pxa_game_music_song_t boss_song = {
    boss_music, PXA_MUSIC_LENGTH(boss_music), 174, 0x5555, 0xAAAA, 0xFFFF,
    {PXA_MUSIC_A2, PXA_MUSIC_A2, PXA_MUSIC_F2, PXA_MUSIC_G2,
     PXA_MUSIC_A2, PXA_MUSIC_E2, PXA_MUSIC_F2, PXA_MUSIC_G2},
    NULL, 0, 16,
};

static int move_player_by(int16_t delta_y);
static int update_player_motion(void);

static uint32_t random_next(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void clear_shots(shot_t* shots, uint8_t count) {
    for (uint8_t index = 0; index < count; ++index)
        shots[index].active = 0;
}

static void clear_enemies(void) {
    for (uint8_t index = 0; index < ENEMY_COUNT; ++index)
        enemies[index].active = 0;
}

static void clear_sparks(void) {
    for (uint8_t index = 0; index < SPARK_COUNT; ++index)
        sparks[index].active = 0;
}

static void clear_engine_particles(void) {
    for (uint8_t index = 0; index < ENGINE_PARTICLE_COUNT; ++index)
        engine_particles[index].active = 0;
}

static void clear_enemy_engine_particles(void) {
    for (uint8_t index = 0; index < ENEMY_ENGINE_PARTICLE_COUNT; ++index)
        enemy_engine_particles[index].active = 0;
}

static void clear_pickups(void) {
    for (uint8_t index = 0; index < PICKUP_COUNT; ++index)
        pickups[index].active = 0;
}

static uint8_t current_wave(void) {
    return (uint8_t)(1u + threat_cleared / WAVE_SCORE_STEP);
}

static uint16_t mission_threat_target(void) {
    static const uint16_t targets[] = {30u, 45u, 60u};
    return targets[configured_mission];
}

static uint8_t combo_multiplier(void) {
    if (combo >= 20u) return 3u;
    if (combo >= 8u) return 2u;
    return 1u;
}

static uint16_t calculate_mission_reward(void) {
    static const uint16_t base_rewards[] = {80u, 120u, 180u};
    return (uint16_t)(base_rewards[configured_mission] + score * 2u +
                      result_stars * 30u + (combo >= 20u ? 40u : 0u));
}

static void reset_game(void) {
    score = 0;
    threat_cleared = 0;
    combo = 0;
    lives = (uint8_t)(3u + configured_hull_level + configured_ship_model +
                      (configured_module == 2u ? 1u : 0u));
    starting_lives = lives;
    energy = 0;
    star_scroll = 0;
    fire_counter = 0;
    spawn_counter = 0;
    combo_decay = 0;
    wave_notice_ticks = 50;
    overdrive_ticks = 0;
    invulnerable_ticks = configured_module == 2u ? 50u : 0u;
    control_active = 0;
    volume_input_direction = 0;
    player_velocity_y = 0;
    game_over = 0;
    mission_complete = 0;
    paused = 0;
    exit_requested = 0;
    result_available = 0;
    result_stars = 0;
    result_reward = 0;
    last_tick_us = 0;
    player_x = (int16_t)(layout.lane_x - configured_ship_model * 8u);
    player_y = (int16_t)((layout.player_min_y + layout.player_max_y) / 2);
    clear_shots(player_bullets, PLAYER_BULLET_COUNT);
    clear_shots(enemy_bullets, ENEMY_BULLET_COUNT);
    clear_enemies();
    clear_sparks();
    clear_engine_particles();
    clear_enemy_engine_particles();
    clear_pickups();
    boss.active = 0;
    boss_wave = 0;
    pxa_game_sfx_set_song(&sfx, &plane_shooter_song);
}

static void spawn_sparks(int16_t x, int16_t y, uint32_t color, uint8_t count) {
    for (uint8_t index = 0; index < SPARK_COUNT && count != 0; ++index) {
        spark_t* spark = &sparks[index];
        if (spark->active)
            continue;
        spark->active = 1;
        spark->x = x;
        spark->y = y;
        spark->vx = (int8_t)((int)(random_next() % 7u) - 3);
        spark->vy = (int8_t)((int)(random_next() % 7u) - 3);
        spark->life = (uint8_t)(8u + random_next() % 10u);
        spark->color = color;
        --count;
    }
}

static uint8_t enemy_health(uint8_t kind) {
    switch (kind) {
        case ENEMY_GUNSHIP: return 5u;
        case ENEMY_CRUISER: return 4u;
        case ENEMY_FIGHTER: return 2u;
        default: return 1u;
    }
}

static uint8_t enemy_points(uint8_t kind) {
    switch (kind) {
        case ENEMY_GUNSHIP: return 6u;
        case ENEMY_CRUISER: return 4u;
        case ENEMY_FIGHTER: return 2u;
        default: return 1u;
    }
}

static uint8_t enemy_speed(uint8_t kind, uint8_t wave) {
    const uint8_t base = kind == ENEMY_GUNSHIP ? 1u : kind == ENEMY_CRUISER ? 2u :
                         kind == ENEMY_FIGHTER ? 3u : kind == ENEMY_DART ? 5u : 4u;
    return (uint8_t)(base + (wave >= 4 ? 1u : 0u));
}

static uint32_t enemy_color(uint8_t kind) {
    switch (kind) {
        case ENEMY_GUNSHIP: return 0xFF9F43;
        case ENEMY_CRUISER: return 0xFF6B8B;
        case ENEMY_FIGHTER: return 0xB580FF;
        case ENEMY_DART: return 0x58D7FF;
        case ENEMY_BOSS: return 0xFF4D6D;
        default: return 0xFFDA6B;
    }
}

static void spawn_player_bullet(int16_t y_offset) {
    for (uint8_t index = 0; index < PLAYER_BULLET_COUNT; ++index) {
        shot_t* bullet = &player_bullets[index];
        if (bullet->active)
            continue;
        bullet->active = 1;
        bullet->x = (int16_t)(player_x - 5);
        bullet->y = (int16_t)(player_y + y_offset);
        bullet->vx = overdrive_ticks != 0 ? -9 : -7;
        bullet->vy = 0;
        bullet->kind = 0;
        return;
    }
}

static void spawn_enemy_bullet_at(int16_t x, int16_t y, int8_t vx, int8_t vy,
                                  uint8_t kind) {
    for (uint8_t index = 0; index < ENEMY_BULLET_COUNT; ++index) {
        shot_t* bullet = &enemy_bullets[index];
        if (bullet->active)
            continue;
        bullet->active = 1;
        bullet->x = x;
        bullet->y = y;
        bullet->vx = vx;
        bullet->vy = vy;
        bullet->kind = kind;
        return;
    }
}

static void spawn_enemy_bullet(const enemy_t* enemy) {
    const int8_t vy = (int8_t)((player_y + 12 < enemy->y) ? -1 :
                                (player_y + 12 > enemy->y + 16) ? 1 : 0);
    spawn_enemy_bullet_at((int16_t)(enemy->x + 25), (int16_t)(enemy->y + 10),
                          (int8_t)(enemy->kind == ENEMY_GUNSHIP ? 4 : 5), vy,
                          enemy->kind);
    if (enemy->kind == ENEMY_GUNSHIP)
        spawn_enemy_bullet_at((int16_t)(enemy->x + 22), (int16_t)(enemy->y + 16),
                              4, (int8_t)(vy == 0 ? 1 : vy), enemy->kind);
}

static void spawn_enemy(void) {
    const uint8_t wave = current_wave();
    for (uint8_t index = 0; index < ENEMY_COUNT; ++index) {
        enemy_t* enemy = &enemies[index];
        uint8_t kind = ENEMY_SCOUT;
        if (enemy->active)
            continue;
        if (configured_mission == 2u && random_next() % 4u == 0)
            kind = random_next() & 1u ? ENEMY_GUNSHIP : ENEMY_CRUISER;
        else if (configured_mission == 1u && random_next() % 3u == 0)
            kind = random_next() & 1u ? ENEMY_DART : ENEMY_FIGHTER;
        else if (wave >= 5 && random_next() % 9u == 0)
            kind = ENEMY_GUNSHIP;
        else if (wave >= 4 && random_next() % 7u == 0)
            kind = ENEMY_CRUISER;
        else if (wave >= 2 && random_next() % 3u == 0)
            kind = ENEMY_FIGHTER;
        else if (random_next() % 3u == 0)
            kind = ENEMY_DART;
        enemy->active = 1;
        enemy->kind = kind;
        enemy->x = (int16_t)(-(ENEMY_PLANE_W + 10 + (int16_t)(random_next() % 44u)));
        {
            const int span = (int)layout.field_bottom - (int)layout.field_top -
                             ENEMY_PLANE_H - 8;
            enemy->y = (int16_t)(layout.field_top + 8 +
                                 (int)(random_next() % (span > 1 ? span : 1)));
        }
        enemy->vy = (int8_t)((random_next() & 1u) == 0 ? 1 : -1);
        enemy->health = enemy_health(kind);
        enemy->fire_delay = (uint8_t)(18u + random_next() % 30u);
        return;
    }
}

static void start_boss(uint8_t wave) {
    clear_enemies();
    boss.active = 1;
    boss.x = -BOSS_WIDTH;
    boss.y = (int16_t)(layout.field_top +
                       ((int)layout.field_bottom - (int)layout.field_top -
                        BOSS_HEIGHT) /
                           2);
    boss.vy = 1;
    boss.max_health = (uint8_t)(32u + wave * 7u + configured_mission * 14u);
    boss.health = boss.max_health;
    boss.fire_delay = 45;
    boss.kind = configured_mission;
    boss_wave = wave;
    wave_notice_ticks = 45;
    pxa_game_sfx_set_song(&sfx, &boss_song);
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ALERT);
}

static void maybe_start_boss(void) {
    const uint8_t wave = current_wave();
    if (!boss.active && threat_cleared >= mission_threat_target() &&
        boss_wave == 0u)
        start_boss(wave);
}

static void take_hit(uint8_t collided) {
    if (invulnerable_ticks != 0 || game_over)
        return;
    if (lives != 0)
        --lives;
    invulnerable_ticks = 36;
    combo = 0;
    combo_decay = 0;
    spawn_sparks((int16_t)(player_x + 19), (int16_t)(player_y + 13), 0x78D9FF, 10);
    if (collided)
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_EXPLODE);
    else
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ALERT);
    if (lives == 0) {
        game_over = 1;
        control_active = 0;
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_DEATH);
    }
}

static void activate_overdrive(void) {
    if (energy != 100u || overdrive_ticks != 0)
        return;
    energy = 0;
    overdrive_ticks = OVERDRIVE_TICKS;
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_WIN);
}

static void add_energy(uint8_t amount) {
    const uint16_t total = (uint16_t)energy + amount;
    energy = (uint8_t)(total > 100u ? 100u : total);
    activate_overdrive();
}

static void spawn_pickup(uint8_t kind, int16_t x, int16_t y) {
    for (uint8_t index = 0; index < PICKUP_COUNT; ++index) {
        pickup_t* pickup = &pickups[index];
        if (pickup->active)
            continue;
        pickup->active = 1;
        pickup->kind = kind;
        pickup->x = x;
        pickup->y = y;
        pickup->vy = (int8_t)((random_next() & 1u) == 0 ? 1 : -1);
        pickup->pulse = (uint8_t)random_next();
        return;
    }
}

static uint32_t pickup_color(uint8_t kind) {
    switch (kind) {
        case PICKUP_SHIELD: return 0x58D7FF;
        case PICKUP_OVERDRIVE: return 0xFF71D2;
        default: return 0xF6D365;
    }
}

static const char* pickup_asset(uint8_t kind, size_t* length) {
    switch (kind) {
        case PICKUP_SHIELD:
            *length = sizeof(pickup_shield_asset) - 1;
            return pickup_shield_asset;
        case PICKUP_OVERDRIVE:
            *length = sizeof(pickup_overdrive_asset) - 1;
            return pickup_overdrive_asset;
        default:
            *length = sizeof(pickup_energy_asset) - 1;
            return pickup_energy_asset;
    }
}

static void collect_pickup(pickup_t* pickup) {
    const uint32_t color = pickup_color(pickup->kind);
    pickup->active = 0;
    if (pickup->kind == PICKUP_ENERGY) {
        add_energy(32u);
    } else if (pickup->kind == PICKUP_SHIELD) {
        if (invulnerable_ticks < SHIELD_TICKS)
            invulnerable_ticks = SHIELD_TICKS;
    } else {
        const uint16_t total = (uint16_t)overdrive_ticks + 70u;
        overdrive_ticks = (uint8_t)(total > 180u ? 180u : total);
    }
    spawn_sparks((int16_t)(player_x + 16), (int16_t)(player_y + 13), color, 8);
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_WIN);
}

static void destroy_enemy(enemy_t* enemy) {
    const uint8_t points = enemy_points(enemy->kind);
    const uint8_t previous_wave = current_wave();
    enemy->active = 0;
    threat_cleared = (uint16_t)(threat_cleared + points);
    combo = (uint16_t)(combo + 1u);
    combo_decay = 50;
    score += (uint32_t)points * combo_multiplier();
    if (combo == 8u || combo == 20u)
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
    add_energy((uint8_t)(points * 9u));
    spawn_sparks((int16_t)(enemy->x + 13), (int16_t)(enemy->y + 9),
                 enemy_color(enemy->kind), (uint8_t)(5u + points));
    if ((points >= 4u && random_next() % 2u == 0) || random_next() % 7u == 0) {
        const uint8_t kind = points >= 4u && random_next() % 3u == 0
                                 ? PICKUP_SHIELD
                                 : (random_next() & 1u ? PICKUP_ENERGY
                                                        : PICKUP_OVERDRIVE);
        spawn_pickup(kind, (int16_t)(enemy->x + 6), (int16_t)(enemy->y + 1));
    }
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_EXPLODE);
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_SCORE);
    if (current_wave() != previous_wave) {
        wave_notice_ticks = 45;
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
    }
    maybe_start_boss();
}

static int overlaps(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                    int16_t bx, int16_t by, int16_t bw, int16_t bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void draw_starfield(pxa_canvas_frame_t* frame) {
    static const uint32_t planet_outer[] = {0x0B2741, 0x42152E, 0x173C3E};
    static const uint32_t planet_inner[] = {0x123856, 0x6B2446, 0x246864};
    const int field_h = (int)layout.field_bottom - (int)layout.field_top;
    for (uint8_t index = 0; index < 3; ++index) {
        const uint16_t range = (uint16_t)(layout.width + 44);
        const uint16_t phase =
            (uint16_t)((star_scroll / (index + 3u) + index * 103u) % range);
        const int16_t x = (int16_t)(phase - 22);
        const int16_t y =
            (int16_t)(layout.field_top + field_h * (index + 1) / 4);
        const uint32_t color = index == 1 ? planet_outer[configured_mission]
                                           : 0x102540;
        pxa_canvas_circle(frame, x, y, (uint16_t)(30 - index * 5u), color);
        pxa_canvas_circle(frame, (int16_t)(x + 9), (int16_t)(y - 5),
                          (uint16_t)(16 - index * 3u),
                          planet_inner[configured_mission]);
    }
    for (uint8_t index = 0; index < 34; ++index) {
        const uint8_t layer = (uint8_t)(index % 3u);
        const uint16_t range = (uint16_t)(layout.width + 24);
        const uint16_t phase = (uint16_t)((index * 67u + star_scroll * (layer + 1u)) % range);
        const int16_t x = (int16_t)(phase - 12);
        const int16_t y = (int16_t)(layout.field_top + 4 +
                                    (int)((index * 41u) % (uint16_t)(field_h > 8 ? field_h - 8 : 1)));
        const uint32_t color = layer == 0 ? 0x5E88AD : layer == 1 ? 0x96CBF2 : 0xE5F4FF;
        pxa_canvas_circle(frame, x, y, (uint16_t)(layer == 2 ? 2 : 1), color);
    }
    for (uint8_t index = 0; index < 4; ++index) {
        const int width = layout.width > 32 ? layout.width - 32 : 1;
        const int16_t x = (int16_t)(16 + (int)((index * 79 + star_scroll / 3u) % (uint16_t)width));
        const int16_t y = (int16_t)(layout.field_top + 18 + index * field_h / 6);
        pxa_canvas_circle(frame, x, y, (uint16_t)(10 - index),
                          index & 1u ? 0x0B2944 : 0x102E4A);
        pxa_canvas_circle(frame, (int16_t)(x - 3), (int16_t)(y - 2),
                          (uint16_t)(4 - index / 2u), 0x174365);
    }
}

static void draw_player(pxa_canvas_frame_t* frame) {
    const int16_t center_y = (int16_t)(player_y + 13);
    if (invulnerable_ticks != 0)
        pxa_canvas_circle(frame, (int16_t)(player_x + 19), center_y, 22, 0x1C567B);
    if (invulnerable_ticks == 0 || (invulnerable_ticks / 3u) % 2u == 0)
        pxa_canvas_image(frame, player_x, (int16_t)(player_y - 2), PLAYER_PLANE_W,
                         PLAYER_PLANE_H, 255, 0,
                         configured_ship_model ? player_mk2_asset : player_asset,
                         configured_ship_model ? sizeof(player_mk2_asset) - 1u
                                               : sizeof(player_asset) - 1u);
}

static void draw_engine_particles(pxa_canvas_frame_t* frame) {
    for (uint8_t index = 0; index < ENGINE_PARTICLE_COUNT; ++index) {
        const engine_particle_t* particle = &engine_particles[index];
        int16_t trail_x;
        uint32_t color;
        if (!particle->active)
            continue;
        trail_x = (int16_t)(particle->x - particle->vx);
        color = particle->life >= 5 ? 0xFFF1A6 :
                particle->life >= 3 ? 0x6EEAF2 : 0x217BA8;
        pxa_canvas_line(frame, trail_x, particle->y, (int16_t)(particle->x + 2),
                        particle->y, color, particle->life >= 5 ? 2 : 1);
        pxa_canvas_circle(frame, particle->x, particle->y, 1,
                          particle->life >= 5 ? 0xFFF9CF : color);
    }
}

static void draw_enemy_engine_particles(pxa_canvas_frame_t* frame) {
    for (uint8_t index = 0; index < ENEMY_ENGINE_PARTICLE_COUNT; ++index) {
        const enemy_engine_particle_t* particle = &enemy_engine_particles[index];
        int16_t trail_x;
        uint32_t color;
        if (!particle->active)
            continue;
        trail_x = (int16_t)(particle->x - particle->vx);
        color = particle->life >= 4 ? 0xFFE0A6 : enemy_color(particle->kind);
        pxa_canvas_line(frame, trail_x, particle->y, (int16_t)(particle->x - 1),
                        particle->y, color, particle->life >= 4 ? 2 : 1);
        pxa_canvas_circle(frame, particle->x, particle->y, 1, color);
    }
}

static void draw_enemy(pxa_canvas_frame_t* frame, const enemy_t* enemy) {
    const uint32_t color = enemy_color(enemy->kind);
    const char* asset = enemy_scout_asset;
    size_t asset_length = sizeof(enemy_scout_asset) - 1;
    switch (enemy->kind) {
        case ENEMY_DART:
            asset = enemy_dart_asset;
            asset_length = sizeof(enemy_dart_asset) - 1;
            break;
        case ENEMY_FIGHTER:
            asset = enemy_fighter_asset;
            asset_length = sizeof(enemy_fighter_asset) - 1;
            break;
        case ENEMY_CRUISER:
            asset = enemy_cruiser_asset;
            asset_length = sizeof(enemy_cruiser_asset) - 1;
            break;
        case ENEMY_GUNSHIP:
            asset = enemy_gunship_asset;
            asset_length = sizeof(enemy_gunship_asset) - 1;
            break;
        default:
            break;
    }
    pxa_canvas_image(frame, enemy->x, enemy->y, ENEMY_PLANE_W, ENEMY_PLANE_H,
                     255, 0, asset, asset_length);
    if (enemy->health > 1) {
        const uint8_t maximum = enemy_health(enemy->kind);
        pxa_canvas_rect(frame, enemy->x, (int16_t)(enemy->y - 5), 28, 3, 0x331B32, 1);
        pxa_canvas_rect(frame, enemy->x, (int16_t)(enemy->y - 5),
                        (uint16_t)(28u * enemy->health / maximum), 3, color, 1);
    }
}

static void draw_boss(pxa_canvas_frame_t* frame) {
    const uint8_t enraged = (uint16_t)boss.health * 2u <= boss.max_health;
    const uint32_t glow = enraged ? 0x5A1834 : 0x1D2B56;
    const uint32_t core = enraged ? 0xFF5A86 : 0xFFB870;
    pxa_canvas_circle(frame, (int16_t)(boss.x + BOSS_WIDTH / 2),
                      (int16_t)(boss.y + BOSS_HEIGHT / 2),
                      (uint16_t)(28 + (star_scroll & 1u)), glow);
    pxa_canvas_line(frame, (int16_t)(boss.x - 12), (int16_t)(boss.y + 15),
                    (int16_t)(boss.x + 5), (int16_t)(boss.y + 15), core, 2);
    pxa_canvas_line(frame, (int16_t)(boss.x - 16), (int16_t)(boss.y + 31),
                    (int16_t)(boss.x + 5), (int16_t)(boss.y + 31), core, 2);
    const char *asset = boss.kind == 1u ? boss_carrier_asset
                                       : boss.kind == 2u ? boss_leviathan_asset
                                                         : boss_asset;
    const size_t asset_length = boss.kind == 1u
                                    ? sizeof(boss_carrier_asset) - 1u
                                    : boss.kind == 2u
                                          ? sizeof(boss_leviathan_asset) - 1u
                                          : sizeof(boss_asset) - 1u;
    pxa_canvas_image(frame, boss.x, boss.y, BOSS_WIDTH, BOSS_HEIGHT, 255,
                     PXA_UI_IMAGE_FIT_STRETCH, asset, asset_length);
    if (enraged)
        pxa_canvas_circle(frame, (int16_t)(boss.x + 32), (int16_t)(boss.y + 23), 4,
                          0xFFF0AD);
}

static void draw_pickups(pxa_canvas_frame_t* frame) {
    for (uint8_t index = 0; index < PICKUP_COUNT; ++index) {
        const pickup_t* pickup = &pickups[index];
        size_t asset_length;
        const char* asset;
        if (!pickup->active)
            continue;
        asset = pickup_asset(pickup->kind, &asset_length);
        pxa_canvas_circle(frame, (int16_t)(pickup->x + PICKUP_SIZE / 2),
                          (int16_t)(pickup->y + PICKUP_SIZE / 2),
                          (uint16_t)(10 + (pickup->pulse / 4u & 1u)),
                          pickup_color(pickup->kind));
        pxa_canvas_circle(frame, (int16_t)(pickup->x + PICKUP_SIZE / 2),
                          (int16_t)(pickup->y + PICKUP_SIZE / 2), 7, 0x102940);
        pxa_canvas_image(frame, pickup->x, pickup->y, PICKUP_SIZE, PICKUP_SIZE, 255, 0,
                         asset, asset_length);
    }
}

static int render(void) {
    pxa_canvas_frame_t frame;
    char score_text[10];
    char wave_text[4];
    char combo_text[10];
    char threat_text[12];
    const uint16_t threat_target = mission_threat_target();
    const uint16_t displayed_threat = threat_cleared > threat_target
                                          ? threat_target : threat_cleared;
    const size_t score_length = pxa_canvas_u32_text(score_text, score);
    const size_t wave_length = pxa_canvas_u32_text(wave_text, current_wave());
    const size_t combo_length = pxa_canvas_u32_text(combo_text, combo);
    size_t threat_length = pxa_canvas_u32_text(threat_text, threat_cleared);
    threat_text[threat_length++] = '/';
    threat_length += pxa_canvas_u32_text(threat_text + threat_length,
                                         threat_target);

    pxa_canvas_begin(&frame, draw_data, sizeof(draw_data));
    static const uint32_t scene_colors[] = {0x061321, 0x190C1B, 0x071A1C};
    pxa_canvas_rect(&frame, 0, 0, layout.width, layout.height,
                    scene_colors[configured_mission], 0);
    draw_starfield(&frame);
    {
        const int field_h = (int)layout.field_bottom - (int)layout.field_top;
        const int field_w = (int)layout.field_right - (int)layout.field_left;
        for (uint8_t index = 0; index < 5; ++index) {
            const int16_t y =
                (int16_t)(layout.field_top + field_h * (index + 1) / 6);
            pxa_canvas_line(&frame, layout.field_left, y, layout.field_right, y,
                            0x0A2338, 1);
        }
        for (uint8_t index = 0; index < 5; ++index) {
            const int16_t y =
                (int16_t)(layout.field_top + field_h * (index + 1) / 6);
            const int16_t x = (int16_t)(layout.field_left +
                (int)((star_scroll * 3u + index * 57u) %
                      (uint16_t)(field_w > 1 ? field_w : 1)));
            pxa_canvas_line(&frame, x, y, (int16_t)(x + 28), y, 0x1A526D, 1);
        }
    }

    for (uint8_t index = 0; index < PLAYER_BULLET_COUNT; ++index)
        if (player_bullets[index].active) {
            const shot_t* bullet = &player_bullets[index];
            pxa_canvas_line(&frame, (int16_t)(bullet->x + 5), bullet->y,
                            (int16_t)(bullet->x - 7), bullet->y,
                            overdrive_ticks != 0 ? 0x61F5E5 : 0xFFE16A, 2);
        }
    for (uint8_t index = 0; index < ENEMY_BULLET_COUNT; ++index)
        if (enemy_bullets[index].active) {
            const shot_t* bullet = &enemy_bullets[index];
            pxa_canvas_line(&frame, (int16_t)(bullet->x - 5), bullet->y,
                            (int16_t)(bullet->x + 4), (int16_t)(bullet->y - bullet->vy),
                            enemy_color(bullet->kind), 2);
            pxa_canvas_circle(&frame, bullet->x, bullet->y, 2, 0xFFD4DF);
        }
    draw_enemy_engine_particles(&frame);
    for (uint8_t index = 0; index < ENEMY_COUNT; ++index)
        if (enemies[index].active)
            draw_enemy(&frame, &enemies[index]);
    if (boss.active)
        draw_boss(&frame);
    draw_pickups(&frame);
    for (uint8_t index = 0; index < SPARK_COUNT; ++index)
        if (sparks[index].active)
            pxa_canvas_circle(&frame, sparks[index].x, sparks[index].y,
                              (uint16_t)(sparks[index].life > 8 ? 2 : 1), sparks[index].color);
    draw_engine_particles(&frame);
    draw_player(&frame);

    pxa_canvas_image(&frame, layout.hud_x, layout.hud_y, layout.hud_w,
                     HUD_PANEL_H, 255, PXA_UI_IMAGE_FIT_STRETCH, hud_panel_asset,
                     sizeof(hud_panel_asset) - 1);
    const char *score_label = PXA_PLANE_MSG(PXA_MSG_BATTLE_SCORE);
    const char *wave_label = PXA_PLANE_MSG(PXA_MSG_BATTLE_WAVE);
    const char *chain_label = PXA_PLANE_MSG(PXA_MSG_BATTLE_CHAIN);
    const int16_t hud_text_y = (int16_t)(layout.hud_y + 1);
    pxa_canvas_text_box(&frame, hud_px(11), hud_text_y, hud_pw(42), 24, 0x95C5E8,
                        PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        score_label, pxa_plane_text_size(score_label));
    pxa_canvas_text_box(&frame, hud_px(50), hud_text_y, hud_pw(45), 24, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        score_text, score_length);
    pxa_canvas_text_box(&frame, hud_px(97), hud_text_y, hud_pw(22), 24, 0x95C5E8,
                        PXA_CANVAS_ALIGN_RIGHT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        wave_label, pxa_plane_text_size(wave_label));
    pxa_canvas_text_box(&frame, hud_px(119), hud_text_y, hud_pw(24), 24, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_LEFT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        wave_text, wave_length);
    pxa_canvas_text_box(&frame, hud_px(145), hud_text_y, hud_pw(22), 24, 0x95C5E8,
                        PXA_CANVAS_ALIGN_RIGHT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        chain_label, pxa_plane_text_size(chain_label));
    pxa_canvas_text_box(&frame, hud_px(167), hud_text_y, hud_pw(30), 24,
                        combo > 1 ? 0xFFE36A : 0xFFFFFF, PXA_CANVAS_ALIGN_LEFT,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, combo_text, combo_length);
    for (uint8_t index = 0; index < lives; ++index) {
        pxa_canvas_circle(&frame, (int16_t)(hud_px(218) - index * 12),
                          (int16_t)(layout.hud_y + 13), 4, 0x68E5AD);
        pxa_canvas_circle(&frame, (int16_t)(hud_px(217) - index * 12),
                          (int16_t)(layout.hud_y + 12), 1, 0xD4FFEB);
    }
    pxa_canvas_rect(&frame, hud_px(50), (int16_t)(layout.hud_y + 29), hud_pw(132),
                    3, 0x17344B, 1);
    pxa_canvas_rect(&frame, hud_px(50), (int16_t)(layout.hud_y + 29),
                    (uint16_t)(hud_pw(132) * energy / 100u), 3,
                    overdrive_ticks != 0 ? 0x58F4D6 : 0xF6C85F, 1);
    {
        const int16_t threat_y =
            (int16_t)(layout.height - (int)game_screen.safe_bottom - 28);
        const int16_t threat_x = (int16_t)(layout.field_left + 2);
        pxa_canvas_rect(&frame, threat_x, threat_y, 92, 3, 0x17344B, 1);
        pxa_canvas_rect(&frame, threat_x, threat_y,
                        (uint16_t)(92u * displayed_threat / threat_target), 3,
                        0x55D7C1, 1);
        const char *threat_label = PXA_PLANE_MSG(PXA_MSG_BATTLE_THREAT);
        pxa_canvas_text_box(&frame, (int16_t)(threat_x + 96),
                            (int16_t)(threat_y - 8), 34, 16, 0x9CCDE3,
                            PXA_CANVAS_ALIGN_LEFT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                            threat_label, pxa_plane_text_size(threat_label));
        pxa_canvas_text_box(&frame, (int16_t)(threat_x + 132),
                            (int16_t)(threat_y - 8), 64, 16, 0x9CCDE3,
                            PXA_CANVAS_ALIGN_LEFT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                            threat_text, threat_length);
    }
    if (boss.active) {
        const char *boss_labels[] = {
            PXA_PLANE_MSG(PXA_MSG_MISSION_ONE_BOSS),
            PXA_PLANE_MSG(PXA_MSG_MISSION_TWO_BOSS),
            PXA_PLANE_MSG(PXA_MSG_MISSION_THREE_BOSS)};
        const int16_t boss_bar_x = (int16_t)((layout.width - 132) / 2);
        const int16_t boss_bar_y = (int16_t)(layout.hud_y + 40);
        pxa_canvas_text_box_role(
            &frame, (int16_t)(boss_bar_x - 62), (int16_t)(boss_bar_y - 5), 60,
            16, pxa_canvas_rgba(0xFF9FB0), PXA_CANVAS_FONT_CAPTION,
            PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
            boss_labels[boss.kind], pxa_plane_text_size(boss_labels[boss.kind]));
        pxa_canvas_rect(&frame, boss_bar_x, boss_bar_y, 132, 5, 0x3A1A30, 2);
        pxa_canvas_rect(&frame, boss_bar_x, boss_bar_y,
                        (uint16_t)(132u * boss.health / boss.max_health), 5,
                        0xFF4D6D, 2);
    }
    if (overdrive_ticks != 0) {
        static const char boost[] = "OVERDRIVE";
        pxa_canvas_text_box(&frame, (int16_t)((layout.width - 90) / 2),
                            (int16_t)(layout.hud_y + 37), 90, 18, 0x83FFF0,
                            PXA_CANVAS_ALIGN_CENTER,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, boost,
                            sizeof(boost) - 1);
    } else if (wave_notice_ticks != 0) {
        const char *wave_notice = PXA_PLANE_MSG(PXA_MSG_BATTLE_INCOMING);
        const int16_t notice_x = (int16_t)((layout.width - 82) / 2);
        pxa_canvas_text_box(&frame, notice_x, (int16_t)(layout.hud_y + 39), 26,
                            20, 0xFFE36A, PXA_CANVAS_ALIGN_CENTER,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, wave_text,
                            wave_length);
        pxa_canvas_text_box(&frame, (int16_t)(notice_x + 26),
                            (int16_t)(layout.hud_y + 39), 56, 20, 0xD9F2FF,
                            PXA_CANVAS_ALIGN_LEFT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                            wave_notice, pxa_plane_text_size(wave_notice));
    }
    if (score == 0 && !game_over && wave_notice_ticks == 0) {
        const char *hint = PXA_PLANE_MSG(PXA_MSG_BATTLE_HINT);
        pxa_canvas_text_box(
            &frame, (int16_t)((layout.width - 130) / 2),
            (int16_t)(layout.height - (int)game_screen.safe_bottom - 20), 130,
            18, 0x8EB6D4, PXA_CANVAS_ALIGN_CENTER,
            PXA_CANVAS_TEXT_ALIGN_MIDDLE, hint, pxa_plane_text_size(hint));
    }
    if (game_over) {
        const char *over = PXA_PLANE_MSG(PXA_MSG_BATTLE_LOST);
        const int16_t panel_x = (int16_t)((layout.width - 220) / 2);
        const int16_t panel_y = (int16_t)((layout.height - 52) / 2);
        pxa_canvas_image(&frame, panel_x, panel_y, 220, 52, 255,
                         PXA_UI_IMAGE_FIT_STRETCH, dialog_panel_asset,
                         sizeof(dialog_panel_asset) - 1);
        pxa_canvas_text(&frame, (int16_t)(panel_x + 12),
                        (int16_t)(panel_y + 17), 196, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_CENTER, over,
                        pxa_plane_text_size(over));
    }
    if (mission_complete) {
        const char *complete = PXA_PLANE_MSG(PXA_MSG_BATTLE_COMPLETE);
        const char *score_label = PXA_PLANE_MSG(PXA_MSG_RESULT_SCORE);
        const char *reward_label = PXA_PLANE_MSG(PXA_MSG_CURRENCY_CREDITS);
        const char *report = PXA_PLANE_MSG(PXA_MSG_BATTLE_REPORT);
        char result_score_text[10];
        char reward_text[10];
        const size_t result_score_length =
            pxa_canvas_u32_text(result_score_text, score);
        const size_t reward_length =
            pxa_canvas_u32_text(reward_text, result_reward);
        pxa_canvas_rect_rgba(&frame, 0, 0, layout.width, layout.height,
                             UINT32_C(0x020812e6), 0, 0, 0);
        pxa_canvas_rect(&frame, layout.dialog_x, layout.dialog_y, DIALOG_W,
                        DIALOG_H, 0x10283E, 8);
        pxa_canvas_line(&frame, (int16_t)(layout.dialog_x + 20),
                        (int16_t)(layout.dialog_y + 32),
                        (int16_t)(layout.dialog_x + 200),
                        (int16_t)(layout.dialog_y + 32), 0x55D7C1, 1);
        pxa_canvas_text_box_role(
            &frame, (int16_t)(layout.dialog_x + 30),
            (int16_t)(layout.dialog_y + 5), 160, 26, pxa_canvas_rgba(0xE7F8FF),
            PXA_CANVAS_FONT_TITLE, PXA_CANVAS_ALIGN_CENTER,
            PXA_CANVAS_TEXT_ALIGN_MIDDLE, complete,
            pxa_plane_text_size(complete));
        for (uint8_t index = 0; index < 3u; ++index) {
            const int16_t x = (int16_t)(layout.dialog_x + 86 + index * 24);
            const int16_t star_y = (int16_t)(layout.dialog_y + 49);
            const uint32_t color = index < result_stars ? 0xFFE36A : 0x29445A;
            pxa_canvas_circle(&frame, x, star_y, 8, color);
            if (index < result_stars)
                pxa_canvas_circle(&frame, (int16_t)(x - 2),
                                  (int16_t)(star_y - 2), 2, 0xFFF8C7);
        }
        pxa_canvas_text_box(&frame, (int16_t)(layout.dialog_x + 29),
                            (int16_t)(layout.dialog_y + 63), 54, 22, 0x8EB6D4,
                            PXA_CANVAS_ALIGN_RIGHT,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_label,
                            pxa_plane_text_size(score_label));
        pxa_canvas_text_box(&frame, (int16_t)(layout.dialog_x + 89),
                            (int16_t)(layout.dialog_y + 63), 70, 22, 0xFFFFFF,
                            PXA_CANVAS_ALIGN_LEFT,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, result_score_text,
                            result_score_length);
        pxa_canvas_text_box(&frame, (int16_t)(layout.dialog_x + 29),
                            (int16_t)(layout.dialog_y + 84), 54, 22, 0x8EB6D4,
                            PXA_CANVAS_ALIGN_RIGHT,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, reward_label,
                            pxa_plane_text_size(reward_label));
        pxa_canvas_text_box(&frame, (int16_t)(layout.dialog_x + 89),
                            (int16_t)(layout.dialog_y + 84), 70, 22, 0xFFE36A,
                            PXA_CANVAS_ALIGN_LEFT,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, reward_text,
                            reward_length);
        pxa_canvas_rect(&frame, (int16_t)(layout.dialog_x + 31),
                        (int16_t)(layout.dialog_y + 116), 158, 29, 0x137EAA, 6);
        pxa_canvas_text_box(&frame, (int16_t)(layout.dialog_x + 39),
                            (int16_t)(layout.dialog_y + 117), 142, 27, 0xFFFFFF,
                            PXA_CANVAS_ALIGN_CENTER,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, report,
                            pxa_plane_text_size(report));
    }
    if (!mission_complete) {
        pxa_canvas_circle(&frame, layout.pause_x, layout.pause_y, 12,
                          paused ? 0x6750A5 : 0x123F5B);
        pxa_canvas_rect(&frame, (int16_t)(layout.pause_x - 5),
                        (int16_t)(layout.pause_y - 6), 3, 12, 0xEAF9FF, 1);
        pxa_canvas_rect(&frame, (int16_t)(layout.pause_x + 2),
                        (int16_t)(layout.pause_y - 6), 3, 12, 0xEAF9FF, 1);
    }
    if (paused) {
        const char *pause_title = PXA_PLANE_MSG(PXA_MSG_PAUSE_TITLE);
        const char *resume = PXA_PLANE_MSG(PXA_MSG_PAUSE_RESUME);
        const char *restart = PXA_PLANE_MSG(PXA_MSG_PAUSE_RESTART);
        const char *exit = PXA_PLANE_MSG(PXA_MSG_PAUSE_EXIT);
        pxa_canvas_rect_rgba(&frame, 0, 0, layout.width, layout.height,
                             UINT32_C(0x020812e6), 0, 0, 0);
        pxa_canvas_rect(&frame, layout.pause_dialog_x, layout.pause_dialog_y,
                        PAUSE_DIALOG_W, PAUSE_DIALOG_H, 0x10283E, 8);
        pxa_canvas_line(&frame, (int16_t)(layout.pause_dialog_x + 17),
                        (int16_t)(layout.pause_dialog_y + 33),
                        (int16_t)(layout.pause_dialog_x + 183),
                        (int16_t)(layout.pause_dialog_y + 33), 0x3DAED1, 1);
        pxa_canvas_text_box_role(
            &frame, (int16_t)(layout.pause_dialog_x + 22),
            (int16_t)(layout.pause_dialog_y + 6), 156, 25,
            pxa_canvas_rgba(0xE7F8FF), PXA_CANVAS_FONT_TITLE,
            PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE, pause_title,
            pxa_plane_text_size(pause_title));
        pxa_canvas_rect(&frame, (int16_t)(layout.pause_dialog_x + 21),
                        (int16_t)(layout.pause_dialog_y + 41), 158, 29,
                        0x137EAA, 6);
        pxa_canvas_text_box(&frame, (int16_t)(layout.pause_dialog_x + 29),
                            (int16_t)(layout.pause_dialog_y + 42), 142, 27,
                            0xFFFFFF, PXA_CANVAS_ALIGN_CENTER,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, resume,
                            pxa_plane_text_size(resume));
        pxa_canvas_rect(&frame, (int16_t)(layout.pause_dialog_x + 21),
                        (int16_t)(layout.pause_dialog_y + 79), 158, 29,
                        0x175A7C, 6);
        pxa_canvas_text_box(&frame, (int16_t)(layout.pause_dialog_x + 29),
                            (int16_t)(layout.pause_dialog_y + 80), 142, 27,
                            0xFFFFFF, PXA_CANVAS_ALIGN_CENTER,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, restart,
                            pxa_plane_text_size(restart));
        pxa_canvas_rect(&frame, (int16_t)(layout.pause_dialog_x + 21),
                        (int16_t)(layout.pause_dialog_y + 117), 158, 29,
                        0x5B334B, 6);
        pxa_canvas_text_box(&frame, (int16_t)(layout.pause_dialog_x + 29),
                            (int16_t)(layout.pause_dialog_y + 118), 142, 27,
                            0xFFDCE5, PXA_CANVAS_ALIGN_CENTER,
                            PXA_CANVAS_TEXT_ALIGN_MIDDLE, exit,
                            pxa_plane_text_size(exit));
    }
    return pxa_canvas_present_with_root_event_mask(
        GAME_NODE, &frame, &pxa_arcade_ui_generation, &initialized,
        PXA_UI_EVENT_MASK_KEY, ui_commands, sizeof(ui_commands), packet,
        sizeof(packet));
}

static void update_sparks(void) {
    for (uint8_t index = 0; index < SPARK_COUNT; ++index) {
        spark_t* spark = &sparks[index];
        if (!spark->active)
            continue;
        spark->x = (int16_t)(spark->x + spark->vx);
        spark->y = (int16_t)(spark->y + spark->vy);
        if (--spark->life == 0)
            spark->active = 0;
    }
}

static void update_engine_particles(void) {
    for (uint8_t index = 0; index < ENGINE_PARTICLE_COUNT; ++index) {
        engine_particle_t* particle = &engine_particles[index];
        if (!particle->active)
            continue;
        particle->x = (int16_t)(particle->x + particle->vx);
        particle->y = (int16_t)(particle->y + particle->vy);
        if (--particle->life == 0)
            particle->active = 0;
    }
}

static void spawn_engine_particles(void) {
    uint8_t count = overdrive_ticks != 0 ? 2 : 1;
    for (uint8_t index = 0; index < ENGINE_PARTICLE_COUNT && count != 0; ++index) {
        engine_particle_t* particle = &engine_particles[index];
        if (particle->active)
            continue;
        particle->active = 1;
        particle->x = (int16_t)(player_x + 33);
        particle->y = (int16_t)(player_y + 12 + (int)(random_next() % 3u) - 1);
        particle->vx = (int8_t)(1 + random_next() % (overdrive_ticks != 0 ? 4u : 3u));
        particle->vy = (int8_t)((int)(random_next() % 3u) - 1);
        particle->life = (uint8_t)(4 + random_next() % 3u);
        --count;
    }
}

static void update_enemy_engine_particles(void) {
    for (uint8_t index = 0; index < ENEMY_ENGINE_PARTICLE_COUNT; ++index) {
        enemy_engine_particle_t* particle = &enemy_engine_particles[index];
        if (!particle->active)
            continue;
        particle->x = (int16_t)(particle->x + particle->vx);
        particle->y = (int16_t)(particle->y + particle->vy);
        if (--particle->life == 0)
            particle->active = 0;
    }
}

static void spawn_enemy_engine_particle(int16_t x, int16_t y, uint8_t kind) {
    for (uint8_t index = 0; index < ENEMY_ENGINE_PARTICLE_COUNT; ++index) {
        enemy_engine_particle_t* particle = &enemy_engine_particles[index];
        if (particle->active)
            continue;
        particle->active = 1;
        particle->x = x;
        particle->y = (int16_t)(y + (int)(random_next() % 3u) - 1);
        particle->vx = (int8_t)(-1 - (int8_t)(random_next() % 3u));
        particle->vy = (int8_t)((int)(random_next() % 3u) - 1);
        particle->life = (uint8_t)(3 + random_next() % 3u);
        particle->kind = kind;
        return;
    }
}

static void spawn_enemy_engine_particles(void) {
    for (uint8_t index = 0; index < ENEMY_COUNT; ++index) {
        const enemy_t* enemy = &enemies[index];
        if (!enemy->active || (random_next() & 1u) == 0)
            continue;
        spawn_enemy_engine_particle((int16_t)(enemy->x + 2),
                                    (int16_t)(enemy->y + ENEMY_PLANE_H / 2),
                                    enemy->kind);
    }
    if (boss.active)
        spawn_enemy_engine_particle((int16_t)(boss.x + 3),
                                    (int16_t)(boss.y + BOSS_HEIGHT / 2),
                                    ENEMY_BOSS);
}

static void update_player_bullets(void) {
    for (uint8_t index = 0; index < PLAYER_BULLET_COUNT; ++index) {
        shot_t* bullet = &player_bullets[index];
        if (!bullet->active)
            continue;
        bullet->x = (int16_t)(bullet->x + bullet->vx);
        if (bullet->x < -8)
            bullet->active = 0;
    }
}

static void update_enemy_bullets(void) {
    for (uint8_t index = 0; index < ENEMY_BULLET_COUNT; ++index) {
        shot_t* bullet = &enemy_bullets[index];
        if (!bullet->active)
            continue;
        bullet->x = (int16_t)(bullet->x + bullet->vx);
        bullet->y = (int16_t)(bullet->y + bullet->vy);
        if (bullet->x > layout.field_right + 8 || bullet->y < layout.field_top ||
            bullet->y > layout.field_bottom)
            bullet->active = 0;
        else if (overlaps(bullet->x - 2, bullet->y - 2, 4, 4,
                          player_x + 5, player_y + 4, 28, 20)) {
            bullet->active = 0;
            take_hit(0);
        }
    }
}

static void update_pickups(void) {
    for (uint8_t index = 0; index < PICKUP_COUNT; ++index) {
        pickup_t* pickup = &pickups[index];
        if (!pickup->active)
            continue;
        pickup->x = (int16_t)(pickup->x + 2);
        pickup->y = (int16_t)(pickup->y + pickup->vy);
        ++pickup->pulse;
        if (pickup->y < layout.field_top + 5 ||
            pickup->y > layout.field_bottom - PICKUP_SIZE - 4)
            pickup->vy = (int8_t)-pickup->vy;
        if (pickup->x > layout.field_right + PICKUP_SIZE) {
            pickup->active = 0;
            continue;
        }
        if (overlaps((int16_t)(pickup->x + 3), (int16_t)(pickup->y + 3), 14, 14,
                     player_x + 5, player_y + 4, 28, 20)) {
            collect_pickup(pickup);
        }
    }
}

static void destroy_boss(void) {
    boss.active = 0;
    score += 12;
    combo = (uint16_t)(combo + 6u);
    combo_decay = 75;
    add_energy(40u);
    spawn_pickup(PICKUP_SHIELD, (int16_t)(boss.x + 20), (int16_t)(boss.y + 8));
    spawn_pickup(PICKUP_OVERDRIVE, (int16_t)(boss.x + 38), (int16_t)(boss.y + 22));
    wave_notice_ticks = 45;
    spawn_sparks((int16_t)(boss.x + BOSS_WIDTH / 2),
                 (int16_t)(boss.y + BOSS_HEIGHT / 2), 0xFF7691, 18);
    mission_complete = 1;
    control_active = 0;
    volume_input_direction = 0;
    player_velocity_y = 0;
    result_stars = 1u;
    if (lives == starting_lives) ++result_stars;
    if (combo >= 12u) ++result_stars;
    result_reward = calculate_mission_reward();
    result_available = 1;
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_EXPLODE);
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_WIN);
}

static void update_boss(void) {
    const uint8_t enraged = (uint16_t)boss.health * 2u <= boss.max_health;
    if (!boss.active)
        return;
    if (boss.x < layout.field_left + BOSS_STOP_LEFT)
        boss.x = (int16_t)(boss.x + 2);
    else {
        boss.y = (int16_t)(boss.y + boss.vy * (enraged ? 2 : 1));
        if (boss.y < layout.field_top + 16 ||
            boss.y > layout.field_bottom - BOSS_HEIGHT - 8)
            boss.vy = (int8_t)-boss.vy;
        if (boss.fire_delay != 0)
            --boss.fire_delay;
        else {
            const int16_t bullet_x = (int16_t)(boss.x + BOSS_WIDTH - 4);
            const int16_t bullet_y = (int16_t)(boss.y + BOSS_HEIGHT / 2);
            const int8_t spread = boss.kind == 2u ? 4 : enraged ? 3 : 2;
            for (int8_t offset = (int8_t)-spread; offset <= spread; offset += 2)
                spawn_enemy_bullet_at(bullet_x, bullet_y, 5, offset, ENEMY_BOSS);
            if (boss.kind == 1u) {
                spawn_enemy_bullet_at(bullet_x, (int16_t)(bullet_y - 13), 4, -1,
                                      ENEMY_BOSS);
                spawn_enemy_bullet_at(bullet_x, (int16_t)(bullet_y + 13), 4, 1,
                                      ENEMY_BOSS);
            }
            boss.fire_delay = boss.kind == 2u ? 16u : enraged ? 20u : 30u;
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_FIRE);
        }
    }
    if (overlaps(boss.x + 3, boss.y + 5, BOSS_WIDTH - 6, BOSS_HEIGHT - 10,
                 player_x + 4, player_y + 4, 30, 20)) {
        take_hit(1);
        return;
    }
    for (uint8_t index = 0; index < PLAYER_BULLET_COUNT && boss.active; ++index) {
        shot_t* bullet = &player_bullets[index];
        if (!bullet->active || !overlaps(bullet->x, bullet->y - 2, 10, 4,
                                         boss.x + 2, boss.y + 4,
                                         BOSS_WIDTH - 4, BOSS_HEIGHT - 8))
            continue;
        bullet->active = 0;
        if (--boss.health == 0)
            destroy_boss();
        else {
            spawn_sparks(bullet->x, bullet->y, 0xFFE36A, 3);
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_HIT);
        }
    }
}

static void update_enemies(void) {
    const uint8_t wave = current_wave();
    for (uint8_t index = 0; index < ENEMY_COUNT; ++index) {
        enemy_t* enemy = &enemies[index];
        if (!enemy->active)
            continue;
        enemy->x = (int16_t)(enemy->x + enemy_speed(enemy->kind, wave));
        enemy->y = (int16_t)(enemy->y + enemy->vy);
        if (enemy->y < layout.field_top + 4 ||
            enemy->y > layout.field_bottom - ENEMY_PLANE_H - 4)
            enemy->vy = (int8_t)-enemy->vy;
        if (enemy->x > layout.field_right) {
            enemy->active = 0;
            take_hit(0);
            continue;
        }
        if (enemy->x > layout.field_left + 48 && enemy->fire_delay != 0)
            --enemy->fire_delay;
        else if (enemy->x > layout.field_left + 48) {
            spawn_enemy_bullet(enemy);
            enemy->fire_delay = (uint8_t)(enemy->kind == ENEMY_GUNSHIP ? 20u :
                                           enemy->kind == ENEMY_CRUISER ? 24u : 38u);
        }
        if (enemy->active && overlaps(enemy->x + 2, enemy->y + 2, 24, 16,
                                      player_x + 4, player_y + 4, 30, 20)) {
            enemy->active = 0;
            spawn_sparks((int16_t)(enemy->x + 13), (int16_t)(enemy->y + 10),
                         enemy_color(enemy->kind), 7);
            take_hit(1);
            continue;
        }
        for (uint8_t bullet_index = 0; bullet_index < PLAYER_BULLET_COUNT && enemy->active;
             ++bullet_index) {
            shot_t* bullet = &player_bullets[bullet_index];
            if (!bullet->active || !overlaps(bullet->x, bullet->y - 2, 10, 4,
                                             enemy->x + 2, enemy->y + 2, 24, 16))
                continue;
            bullet->active = 0;
            if (--enemy->health == 0)
                destroy_enemy(enemy);
            else {
                spawn_sparks(bullet->x, bullet->y, 0xFFE36A, 3);
                pxa_game_sfx_play(&sfx, PXA_GAME_SFX_HIT);
            }
        }
    }
}

static int tick_step(void) {
    const uint8_t wave = current_wave();
    if (game_over || mission_complete)
        return 1;
    ++star_scroll;
    (void)update_player_motion();
    if (invulnerable_ticks != 0)
        --invulnerable_ticks;
    if (wave_notice_ticks != 0)
        --wave_notice_ticks;
    if (overdrive_ticks != 0)
        --overdrive_ticks;
    if (combo_decay != 0) {
        --combo_decay;
        if (combo_decay == 0)
            combo = 0;
    }
    if (++fire_counter >= (overdrive_ticks != 0 ? 3u :
                           (uint8_t)(6u - configured_weapon_level -
                                     configured_ship_model))) {
        fire_counter = 0;
        if (overdrive_ticks != 0) {
            spawn_player_bullet(8);
            spawn_player_bullet(16);
        } else {
            spawn_player_bullet(12);
            if (configured_weapon_level != 0) {
                spawn_player_bullet(configured_weapon_level == 2u ? 5 : 8);
                if (configured_weapon_level == 2u)
                    spawn_player_bullet(19);
            }
            if (configured_module == 1u) {
                spawn_player_bullet(3);
                spawn_player_bullet(21);
            }
        }
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_FIRE);
    }
    if (!boss.active && ++spawn_counter >= (uint8_t)(wave >= 7 ? 10u : 25u - wave * 2u)) {
        spawn_counter = 0;
        spawn_enemy();
    }
    update_sparks();
    update_engine_particles();
    update_enemy_engine_particles();
    spawn_engine_particles();
    update_player_bullets();
    update_enemy_bullets();
    update_boss();
    update_enemies();
    update_pickups();
    spawn_enemy_engine_particles();
    return 1;
}

static int tick(uint8_t steps) {
    if (steps == 0 || game_over || mission_complete)
        return 1;
    while (steps-- != 0) {
        if (!tick_step())
            return 0;
        if (game_over)
            break;
    }
    return render();
}

static int move_player_by(int16_t delta_y) {
    int16_t next_y = (int16_t)(player_y + delta_y);
    if (next_y < layout.player_min_y)
        next_y = layout.player_min_y;
    if (next_y > layout.player_max_y)
        next_y = layout.player_max_y;
    if (next_y == player_y)
        return 0;
    player_y = next_y;
    return 1;
}

static int update_player_motion(void) {
    if (volume_input_direction != 0) {
        const int8_t next_velocity = (int8_t)(player_velocity_y +
                                               volume_input_direction *
                                               VOLUME_ACCELERATION_PER_TICK);
        if (next_velocity >= -VOLUME_MAX_SPEED_PER_TICK &&
            next_velocity <= VOLUME_MAX_SPEED_PER_TICK) {
            player_velocity_y = next_velocity;
        }
    } else {
        player_velocity_y = 0;
    }
    return player_velocity_y != 0 && move_player_by(player_velocity_y);
}

void pxa_plane_game_set_screen(const pxa_game_screen_t *screen) {
    if (screen == NULL) return;
    game_screen = *screen;
    layout_update();
}

void pxa_plane_game_configure(uint8_t weapon_level, uint8_t hull_level,
                              uint8_t ship_model, uint8_t mission,
                              uint8_t module) {
    configured_weapon_level = weapon_level > 2u ? 2u : weapon_level;
    configured_hull_level = hull_level > 2u ? 2u : hull_level;
    configured_ship_model = ship_model > 1u ? 1u : ship_model;
    configured_mission = mission > 2u ? 0u : mission;
    configured_module = module > 2u ? 0u : module;
}

int pxa_plane_game_take_exit_request(void) {
    const uint8_t requested = exit_requested;
    exit_requested = 0;
    return requested != 0;
}

int pxa_plane_game_take_result(uint32_t *final_score, uint8_t *stars,
                               uint16_t *reward) {
    if (!result_available)
        return 0;
    if (final_score != NULL) *final_score = score;
    if (stars != NULL) *stars = result_stars;
    if (reward != NULL) *reward = result_reward;
    result_available = 0;
    return 1;
}


int32_t pxa_app_start(const uint8_t* config, uint32_t config_length) {
    /* The shell forwards the real screen metrics through
     * pxa_plane_game_set_screen(); only fall back to the start environment
     * when the module is launched standalone. */
    if (config != NULL && config_length != 0)
        pxa_game_screen_from_start(&game_screen, config, config_length);
    layout_update();
    initialized = 0;
    backgrounded = 0;
    reset_game();
    if (!render() || !pxa_clock_set_period(GAME_TICK_MS))
        return PXA_STATUS_INTERNAL;
    pxa_game_sfx_set_theme(&sfx, PXA_GAME_SFX_THEME_PLANE);
    pxa_game_sfx_set_song(&sfx, &plane_shooter_song);
    pxa_game_sfx_start(&sfx, packet, sizeof(packet));
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t* event, uint32_t length) {
    pxa_canvas_event_t parsed;
    pxa_ui_event_data_t ui_event;
    pxa_ui_pointer_data_t pointer;
    if (!pxa_canvas_parse_event(event, length, &parsed))
        return PXA_EVENT_UNHANDLED;
    if (parsed.service == PXA_SERVICE_SYSTEM &&
        parsed.opcode == PXA_SYSTEM_LIFECYCLE_EVENT &&
        parsed.payload_length == 1) {
        backgrounded = parsed.payload[0] == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
        control_active = 0;
        player_velocity_y = 0;
        volume_input_direction = 0;
        last_tick_us = 0;
        return PXA_EVENT_HANDLED;
    }
    if (pxa_game_screen_handle_event(&game_screen, &parsed)) {
        layout_update();
        if (player_y > layout.player_max_y) player_y = layout.player_max_y;
        if (player_y < layout.player_min_y) player_y = layout.player_min_y;
        player_x = (int16_t)(layout.lane_x - configured_ship_model * 8u);
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SYSTEM &&
        parsed.opcode == PXA_SYSTEM_CONFIGURATION_EVENT)
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    if (pxa_game_sfx_handle_event(&sfx, &parsed, packet, sizeof(packet)))
        return PXA_EVENT_HANDLED;
    if (parsed.service == PXA_SERVICE_CLOCK && parsed.opcode == PXA_CLOCK_TICK &&
        parsed.payload_length == 8) {
        const uint8_t steps = pxa_clock_tick_steps(
            &last_tick_us, &parsed, GAME_TICK_MS, GAME_MAX_CATCHUP_STEPS);
        if (!paused && !backgrounded) {
            pxa_game_sfx_tick(&sfx, &parsed);
        } else {
            sfx.music_tick_us = pxa_read_u64(parsed.payload);
            sfx.music_remainder_us = 0;
        }
        random_state ^= (uint32_t)pxa_read_u64(parsed.payload);
        if (!paused && !backgrounded && !tick(steps))
            return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    }
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == GAME_ROOT_NODE &&
        ui_event.kind == PXA_UI_EVENT_KEY_KIND && ui_event.data_size == 4) {
        int8_t direction = 0;
        if (paused || backgrounded || mission_complete) {
            volume_input_direction = 0;
            player_velocity_y = 0;
            return PXA_EVENT_HANDLED;
        }
        if (!game_over && ui_event.value == PXA_UI_KEY_VOLUME_UP)
            direction = -1;
        else if (!game_over && ui_event.value == PXA_UI_KEY_VOLUME_DOWN)
            direction = 1;
        else if (ui_event.value == PXA_UI_KEY_VOLUME_UP_RELEASED &&
                 volume_input_direction < 0) {
            volume_input_direction = 0;
            player_velocity_y = 0;
            return PXA_EVENT_HANDLED;
        } else if (ui_event.value == PXA_UI_KEY_VOLUME_DOWN_RELEASED &&
                   volume_input_direction > 0) {
            volume_input_direction = 0;
            player_velocity_y = 0;
            return PXA_EVENT_HANDLED;
        }
        if (direction == 0)
            return PXA_EVENT_HANDLED;

        const uint8_t direction_changed = volume_input_direction != direction;
        if (direction_changed && player_velocity_y * direction < 0)
            player_velocity_y = 0;
        volume_input_direction = direction;
        if (direction_changed && update_player_motion() &&
            !render())
            return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    }
    if (!pxa_canvas_parse_pointer(&parsed, GAME_NODE, &pointer))
        return PXA_EVENT_UNHANDLED;

    const int16_t x = (int16_t)pointer.x;
    const int16_t y = (int16_t)pointer.y;
    if (mission_complete) {
        if (pointer.phase == PXA_POINTER_DOWN &&
            x >= layout.dialog_x + 31 && x < layout.dialog_x + 31 + 158 &&
            y >= layout.dialog_y + 116 && y < layout.dialog_y + 116 + 29) {
            exit_requested = 1;
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
        }
        return PXA_EVENT_HANDLED;
    }
    if (pointer.phase == PXA_POINTER_DOWN && x >= layout.pause_x - 16 &&
        x < layout.pause_x + 16 && y >= layout.pause_y - 15 &&
        y < layout.pause_y + 15) {
        paused = 1;
        control_active = 0;
        volume_input_direction = 0;
        player_velocity_y = 0;
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_TAP);
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (paused) {
        if (x < LEFT_GESTURE_GUARD) return PXA_EVENT_UNHANDLED;
        if (pointer.phase != PXA_POINTER_DOWN) return PXA_EVENT_HANDLED;
        if (x >= layout.pause_dialog_x + 21 &&
            x < layout.pause_dialog_x + 21 + 158 &&
            y >= layout.pause_dialog_y + 41 &&
            y < layout.pause_dialog_y + 41 + 30) {
            paused = 0;
            last_tick_us = 0;
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (x >= layout.pause_dialog_x + 21 &&
            x < layout.pause_dialog_x + 21 + 158 &&
            y >= layout.pause_dialog_y + 79 &&
            y < layout.pause_dialog_y + 79 + 30) {
            reset_game();
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (x >= layout.pause_dialog_x + 21 &&
            x < layout.pause_dialog_x + 21 + 158 &&
            y >= layout.pause_dialog_y + 117 &&
            y < layout.pause_dialog_y + 117 + 30) {
            exit_requested = 1;
            return PXA_EVENT_HANDLED;
        }
        return PXA_EVENT_HANDLED;
    }
    if (game_over) {
        if (pointer.phase == PXA_POINTER_DOWN && x >= LEFT_GESTURE_GUARD &&
            y >= layout.field_top) {
            reset_game();
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_UNHANDLED;
    }
    if (pointer.phase == PXA_POINTER_DOWN) {
        if (x < LEFT_GESTURE_GUARD || y < layout.field_top)
            return PXA_EVENT_UNHANDLED;
        volume_input_direction = 0;
        player_velocity_y = 0;
        control_active = 1;
        control_y = y;
        return PXA_EVENT_HANDLED;
    }
    if (pointer.phase == PXA_POINTER_MOVE && control_active) {
        const int16_t delta_y = (int16_t)(y - control_y);
        control_y = y;
        /* Rendering each touch sample blocks ESP input; the game clock presents it. */
        (void)move_player_by(delta_y);
        return PXA_EVENT_HANDLED;
    }
    if (pointer.phase == PXA_POINTER_UP || pointer.phase == PXA_POINTER_CANCEL) {
        const uint8_t was_active = control_active;
        control_active = 0;
        return was_active ? PXA_EVENT_HANDLED : PXA_EVENT_UNHANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }

uint32_t pxa_arcade_ui_generation;
