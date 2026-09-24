/* Orbital Strike Raster: GameRender battle prototype.
 *
 * The battle field is rendered entirely with a PXA GameRender draw list:
 * clear, flat quads for bullets/stars/particles and INDEX8 sprites for ships
 * and HUD text. The target resolution follows the real screen (half of it), so
 * the Host presents the frame at an exact 2x nearest upscale on 296x240 and
 * 412x412 panels alike. */
#include <stdint.h>

#include "pxa.h"
#include "pxa_canvas.h"
#include "pxa_game_render.h"
#include "pxa_game_screen.h"
#include "pxa_raster.h"

#define INPUT_NODE 2u
#define CREATE_REQUEST UINT32_C(1)
#define FRAME_PERIOD_MS 33u
#define TARGET_MIN_W 148
#define TARGET_MIN_H 120
#define TARGET_MAX_W 240
#define TARGET_MAX_H 240
#define PLAYER_SLOT 0u
#define ENEMY_SLOT 1u
#define FONT_SLOT 2u
#define FONT_GLYPHS 42u
#define FONT_WIDTH (FONT_GLYPHS * 4u)
#define MAX_ENEMIES 8u
#define MAX_BULLETS 20u
#define MAX_ENEMY_BULLETS 12u
#define MAX_PARTICLES 24u
#define MAX_STARS 22u
#define HUD_H 15
#define PLAYER_W 22
#define PLAYER_H 15
#define ENEMY_W 18
#define ENEMY_H 13
#define PLAYER_MARGIN 26
#define START_LIVES 3u
#define REQUIRED_CAPABILITIES \
    (PXA_RASTER_CAP_FLAT_QUAD | PXA_RASTER_CAP_TEXTURED_QUAD)

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vx;
    uint8_t active;
} bullet_t;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vy;
    uint8_t active;
    uint8_t hp;
} enemy_t;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t vx;
    int8_t vy;
    uint8_t life;
    uint8_t active;
} particle_t;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t speed;
} star_t;

static uint8_t g_packet[128];
static uint8_t g_upload[PXA_RASTER_UPLOAD_HEADER_BYTES + FONT_WIDTH * 5u];
static uint8_t g_draw[PXA_RASTER_MAX_DRAW_BYTES];
static uint8_t g_player_texture[16 * 16];
static uint8_t g_enemy_texture[16 * 16];
static uint8_t g_font[FONT_WIDTH * 5u];
static uint16_t g_palette[256];
static uint32_t g_context;
static uint32_t g_capabilities;
static uint64_t g_frame_id;
static pxa_game_screen_t g_screen;
static int g_target_w = 148;
static int g_target_h = 120;
static int g_player_y;
static int g_player_target_y;
static int g_input_ready;
static int g_started;
static int g_game_over;
static uint8_t g_lives;
static uint32_t g_score;
static uint8_t g_wave;
static uint8_t g_spawn_timer;
static uint8_t g_fire_timer;
static uint8_t g_hit_flash;
static uint16_t g_star_scroll;
static bullet_t g_bullets[MAX_BULLETS];
static bullet_t g_enemy_bullets[MAX_ENEMY_BULLETS];
static enemy_t g_enemies[MAX_ENEMIES];
static particle_t g_particles[MAX_PARTICLES];
static star_t g_stars[MAX_STARS];
static uint32_t g_random = 0x9e3779b9u;

static const char kFontCharacters[] =
    "0123456789.:-/+ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
static const uint8_t kFontRows[FONT_GLYPHS][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7},
    {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7},
    {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7},
    {7, 5, 7, 1, 7}, {0, 0, 0, 0, 2}, {0, 2, 0, 2, 0},
    {0, 0, 7, 0, 0}, {1, 1, 2, 4, 4}, {0, 2, 7, 2, 0},
    {7, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {7, 4, 4, 4, 7},
    {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4},
    {7, 4, 5, 5, 7}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7},
    {1, 1, 1, 5, 7}, {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7},
    {5, 7, 7, 5, 5}, {5, 7, 7, 7, 5}, {7, 5, 5, 5, 7},
    {7, 5, 7, 4, 4}, {7, 5, 5, 7, 1}, {6, 5, 6, 5, 5},
    {7, 4, 7, 1, 7}, {7, 2, 2, 2, 2}, {5, 5, 5, 5, 7},
    {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5},
    {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7}, {0, 0, 0, 0, 0},
};

/* '#' body, '+' highlight, '=' engine, '.' transparent. */
static const char kPlayerArt[16][17] = {
    "................",
    "................",
    "................",
    "................",
    ".......+........",
    "......++........",
    ".....+++........",
    "###..++++.......",
    "####+++++==.....",
    "###..++++.......",
    ".....+++........",
    "......++........",
    ".......+........",
    "................",
    "................",
    "................",
};
static const char kEnemyArt[16][17] = {
    "................",
    "................",
    "................",
    "....#......#....",
    "...###....###...",
    "...####..####...",
    "..#############.",
    "..#############.",
    "..#############.",
    "..#############.",
    "...####..####...",
    "...###....###...",
    "....#......#....",
    "................",
    "................",
    "................",
};

static uint32_t random_next(void) {
    g_random ^= g_random << 13;
    g_random ^= g_random >> 17;
    g_random ^= g_random << 5;
    return g_random;
}

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return (uint16_t)(((uint16_t)(red >> 3) << 11) |
                      ((uint16_t)(green >> 2) << 5) | (blue >> 3));
}

static void build_texture(uint8_t *texture, const char art[16][17],
                          uint8_t body_index) {
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const char value = art[y][x];
            uint8_t index = 0;
            if (value == '#') index = body_index;
            else if (value == '+') index = 2;
            else if (value == '=') index = 3;
            texture[y * 16 + x] = index;
        }
    }
}

static void build_resources(void) {
    for (uint32_t index = 0; index < 256; ++index) g_palette[index] = 0;
    g_palette[0] = 0;
    g_palette[1] = rgb565(80, 200, 250);
    g_palette[2] = rgb565(228, 248, 255);
    g_palette[3] = rgb565(255, 205, 80);
    g_palette[4] = rgb565(255, 110, 70);
    g_palette[5] = rgb565(130, 60, 50);
    g_palette[6] = rgb565(180, 230, 255);
    g_palette[7] = rgb565(120, 215, 235);
    g_palette[8] = rgb565(16, 43, 67);
    g_palette[9] = rgb565(255, 225, 105);
    g_palette[10] = rgb565(255, 225, 105);
    g_palette[11] = rgb565(255, 140, 130);
    g_palette[12] = rgb565(255, 170, 65);
    g_palette[13] = rgb565(26, 58, 90);
    g_palette[14] = rgb565(10, 35, 56);
    g_palette[15] = rgb565(26, 82, 109);
    g_palette[255] = 0xffff;
    build_texture(g_player_texture, kPlayerArt, 1);
    build_texture(g_enemy_texture, kEnemyArt, 4);
    for (uint32_t index = 0; index < FONT_GLYPHS; ++index) {
        for (uint8_t row = 0; row < 5; ++row) {
            for (uint8_t column = 0; column < 3; ++column) {
                if ((kFontRows[index][row] & (4u >> column)) != 0)
                    g_font[(size_t)row * FONT_WIDTH + index * 4u + column] =
                        255;
            }
        }
    }
}

static int upload_resources(void) {
    return pxa_raster_upload_palette_rgb565(
               g_context, g_palette, g_upload,
               PXA_RASTER_UPLOAD_HEADER_BYTES + 512u) ==
               (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + 512u) &&
           pxa_raster_upload_texture_index8(
               g_context, PLAYER_SLOT, 16, 16, g_player_texture, g_upload,
               PXA_RASTER_UPLOAD_HEADER_BYTES + sizeof(g_player_texture)) ==
               (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                         sizeof(g_player_texture)) &&
           pxa_raster_upload_texture_index8(
               g_context, ENEMY_SLOT, 16, 16, g_enemy_texture, g_upload,
               PXA_RASTER_UPLOAD_HEADER_BYTES + sizeof(g_enemy_texture)) ==
               (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                         sizeof(g_enemy_texture)) &&
           pxa_raster_upload_texture_index8(
               g_context, FONT_SLOT, FONT_WIDTH, 5, g_font, g_upload,
               sizeof(g_upload)) == (int32_t)sizeof(g_upload);
}

static void append_rect(pxa_raster_draw_list_t *list, int x, int y, int width,
                        int height, uint16_t color) {
    int16_t xy[8];
    xy[0] = xy[6] = (int16_t)(x << 4);
    xy[1] = xy[3] = (int16_t)(y << 4);
    xy[2] = xy[4] = (int16_t)((x + width) << 4);
    xy[5] = xy[7] = (int16_t)((y + height) << 4);
    (void)pxa_raster_flat_quad(list, xy, color);
}

static int font_index(char character) {
    for (uint8_t index = 0; index < FONT_GLYPHS; ++index)
        if (kFontCharacters[index] == character) return index;
    return FONT_GLYPHS - 1u;
}

static void append_text(pxa_raster_draw_list_t *list, int x, int y,
                        const char *text, uint16_t color) {
    while (*text != '\0') {
        const int glyph = font_index(*text++);
        (void)pxa_raster_sprite(
            list, FONT_SLOT,
            PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 |
                PXA_RASTER_SPRITE_SOLID_COLOR,
            g_capabilities, (int16_t)x, (int16_t)y, 3, 5,
            (uint16_t)(glyph * 4), 0, 3, 5, color);
        x += 4;
    }
}

static void write_u32(char *output, uint32_t value) {
    char reverse[10];
    uint8_t count = 0;
    do {
        reverse[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(reverse));
    for (uint8_t index = 0; index < count; ++index)
        output[index] = reverse[count - index - 1u];
    output[count] = '\0';
}

static void reset_game(void) {
    g_player_y = g_target_h / 2;
    g_player_target_y = g_player_y;
    g_lives = START_LIVES;
    g_score = 0;
    g_wave = 1;
    g_spawn_timer = 30;
    g_fire_timer = 0;
    g_hit_flash = 0;
    g_star_scroll = 0;
    g_game_over = 0;
    for (uint8_t index = 0; index < MAX_BULLETS; ++index)
        g_bullets[index].active = 0;
    for (uint8_t index = 0; index < MAX_ENEMY_BULLETS; ++index)
        g_enemy_bullets[index].active = 0;
    for (uint8_t index = 0; index < MAX_ENEMIES; ++index)
        g_enemies[index].active = 0;
    for (uint8_t index = 0; index < MAX_PARTICLES; ++index)
        g_particles[index].active = 0;
    for (uint8_t index = 0; index < MAX_STARS; ++index) {
        g_stars[index].x = (int16_t)(random_next() % (uint32_t)g_target_w);
        g_stars[index].y =
            (int16_t)(HUD_H + random_next() % (uint32_t)(g_target_h - HUD_H));
        g_stars[index].speed = (uint8_t)(1u + random_next() % 3u);
    }
}

static void spawn_particles(int x, int y, uint8_t count) {
    for (uint8_t index = 0; index < MAX_PARTICLES && count != 0; ++index) {
        particle_t *particle = &g_particles[index];
        if (particle->active) continue;
        particle->active = 1;
        particle->x = (int16_t)x;
        particle->y = (int16_t)y;
        particle->vx = (int8_t)((int)(random_next() % 5u) - 2);
        particle->vy = (int8_t)((int)(random_next() % 5u) - 2);
        particle->life = (uint8_t)(6u + random_next() % 8u);
        --count;
    }
}

static void spawn_enemy(void) {
    for (uint8_t index = 0; index < MAX_ENEMIES; ++index) {
        enemy_t *enemy = &g_enemies[index];
        if (enemy->active) continue;
        enemy->active = 1;
        enemy->x = (int16_t)(-(int)ENEMY_W - 4);
        enemy->y = (int16_t)(HUD_H + 4 +
                             (int)(random_next() %
                                   (uint32_t)(g_target_h - HUD_H - ENEMY_H -
                                              8)));
        enemy->vy = (int8_t)((random_next() & 1u) ? 1 : -1);
        enemy->hp = (uint8_t)(1u + g_wave / 3u);
        return;
    }
}

static void spawn_player_bullet(void) {
    for (uint8_t index = 0; index < MAX_BULLETS; ++index) {
        bullet_t *bullet = &g_bullets[index];
        if (bullet->active) continue;
        bullet->active = 1;
        bullet->x = (int16_t)(g_target_w - PLAYER_MARGIN - 4);
        bullet->y = (int16_t)(g_player_y + PLAYER_H / 2);
        bullet->vx = -8;
        return;
    }
}

static void spawn_enemy_bullet(const enemy_t *enemy) {
    for (uint8_t index = 0; index < MAX_ENEMY_BULLETS; ++index) {
        bullet_t *bullet = &g_enemy_bullets[index];
        if (bullet->active) continue;
        bullet->active = 1;
        bullet->x = (int16_t)(enemy->x + ENEMY_W);
        bullet->y = (int16_t)(enemy->y + ENEMY_H / 2);
        bullet->vx = 5;
        return;
    }
}

static void add_score(uint32_t points) {
    g_score += points;
    const uint8_t wave = (uint8_t)(1u + g_score / 40u);
    if (wave != g_wave) {
        g_wave = wave;
        spawn_particles(g_target_w / 2, g_target_h / 2, 4);
    }
}

static void tick_game(void) {
    if (g_game_over) return;
    ++g_star_scroll;
    for (uint8_t index = 0; index < MAX_STARS; ++index) {
        g_stars[index].x = (int16_t)(g_stars[index].x - g_stars[index].speed);
        if (g_stars[index].x < 0)
            g_stars[index].x = (int16_t)(g_target_w + 2);
    }
    {
        const int delta = g_player_target_y - g_player_y;
        if (delta > 3) g_player_y += 3;
        else if (delta < -3) g_player_y -= 3;
        else g_player_y = g_player_target_y;
        if (g_player_y < HUD_H + 2) g_player_y = HUD_H + 2;
        if (g_player_y > g_target_h - PLAYER_H - 2)
            g_player_y = g_target_h - PLAYER_H - 2;
    }
    if (++g_fire_timer >= 6) {
        g_fire_timer = 0;
        spawn_player_bullet();
    }
    if (--g_spawn_timer == 0) {
        g_spawn_timer = (uint8_t)(g_wave >= 5u ? 22u : 34u - g_wave * 2u);
        spawn_enemy();
    }
    for (uint8_t index = 0; index < MAX_BULLETS; ++index) {
        bullet_t *bullet = &g_bullets[index];
        if (!bullet->active) continue;
        bullet->x = (int16_t)(bullet->x + bullet->vx);
        if (bullet->x < -4) bullet->active = 0;
    }
    for (uint8_t index = 0; index < MAX_ENEMY_BULLETS; ++index) {
        bullet_t *bullet = &g_enemy_bullets[index];
        if (!bullet->active) continue;
        bullet->x = (int16_t)(bullet->x + bullet->vx);
        if (bullet->x > g_target_w + 4) bullet->active = 0;
        else if (bullet->x >= g_target_w - PLAYER_MARGIN - 2 &&
                 bullet->x <= g_target_w - PLAYER_MARGIN + PLAYER_W &&
                 bullet->y >= g_player_y && bullet->y <= g_player_y + PLAYER_H) {
            bullet->active = 0;
            g_hit_flash = 6;
            spawn_particles(bullet->x, bullet->y, 3);
            if (g_lives != 0 && --g_lives == 0) g_game_over = 1;
        }
    }
    for (uint8_t index = 0; index < MAX_ENEMIES; ++index) {
        enemy_t *enemy = &g_enemies[index];
        if (!enemy->active) continue;
        enemy->x = (int16_t)(enemy->x + 1);
        enemy->y = (int16_t)(enemy->y + enemy->vy);
        if (enemy->y < HUD_H + 2 || enemy->y > g_target_h - ENEMY_H - 2)
            enemy->vy = (int8_t)-enemy->vy;
        if (enemy->x > g_target_w) {
            enemy->active = 0;
            g_hit_flash = 6;
            if (g_lives != 0 && --g_lives == 0) g_game_over = 1;
            continue;
        }
        if (enemy->x > 20 && (random_next() & 63u) == 0)
            spawn_enemy_bullet(enemy);
        if (enemy->x + ENEMY_W >= g_target_w - PLAYER_MARGIN &&
            enemy->x <= g_target_w - PLAYER_MARGIN + PLAYER_W &&
            enemy->y + ENEMY_H >= g_player_y &&
            enemy->y <= g_player_y + PLAYER_H) {
            enemy->active = 0;
            spawn_particles(enemy->x, enemy->y, 6);
            g_hit_flash = 6;
            if (g_lives != 0 && --g_lives == 0) g_game_over = 1;
            continue;
        }
        for (uint8_t bullet_index = 0;
             bullet_index < MAX_BULLETS && enemy->active; ++bullet_index) {
            bullet_t *bullet = &g_bullets[bullet_index];
            if (!bullet->active) continue;
            if (bullet->x + 6 < enemy->x || bullet->x > enemy->x + ENEMY_W ||
                bullet->y < enemy->y || bullet->y > enemy->y + ENEMY_H)
                continue;
            bullet->active = 0;
            if (--enemy->hp == 0) {
                enemy->active = 0;
                spawn_particles(enemy->x, enemy->y, 5);
                add_score(10);
            }
        }
    }
    for (uint8_t index = 0; index < MAX_PARTICLES; ++index) {
        particle_t *particle = &g_particles[index];
        if (!particle->active) continue;
        particle->x = (int16_t)(particle->x + particle->vx);
        particle->y = (int16_t)(particle->y + particle->vy);
        if (--particle->life == 0) particle->active = 0;
    }
    if (g_hit_flash != 0) --g_hit_flash;
}

static int render_frame(void) {
    pxa_raster_draw_list_t list;
    char score_text[12] = "SCORE ";
    char wave_text[10] = "WAVE ";
    char score_digits[10];
    char wave_digits[10];
    write_u32(score_digits, g_score);
    write_u32(wave_digits, g_wave);
    {
        char *target = score_text + 6;
        for (const char *source = score_digits; *source != '\0';)
            *target++ = *source++;
        *target = '\0';
    }
    {
        char *target = wave_text + 5;
        for (const char *source = wave_digits; *source != '\0';)
            *target++ = *source++;
        *target = '\0';
    }
    pxa_raster_draw_list_begin(&list, g_draw, sizeof(g_draw), ++g_frame_id);
    (void)pxa_raster_clear(&list, g_palette[14]);
    append_rect(&list, 0, 0, g_target_w, HUD_H, g_palette[8]);
    append_rect(&list, 0, HUD_H, g_target_w, 1, g_palette[15]);
    for (uint8_t index = 0; index < 4; ++index)
        append_rect(&list, 0, HUD_H + 14 + index * (g_target_h - HUD_H) / 5,
                    g_target_w, 1, g_palette[15]);
    for (uint8_t index = 0; index < MAX_STARS; ++index) {
        const star_t *star = &g_stars[index];
        append_rect(&list, star->x, star->y, star->speed == 1 ? 1 : 2, 1,
                    g_palette[star->speed == 1 ? 6 : 7]);
    }
    for (uint8_t index = 0; index < MAX_BULLETS; ++index) {
        const bullet_t *bullet = &g_bullets[index];
        if (bullet->active)
            append_rect(&list, bullet->x, bullet->y - 1, 6, 2, g_palette[10]);
    }
    for (uint8_t index = 0; index < MAX_ENEMY_BULLETS; ++index) {
        const bullet_t *bullet = &g_enemy_bullets[index];
        if (bullet->active)
            append_rect(&list, bullet->x, bullet->y - 1, 5, 2, g_palette[11]);
    }
    for (uint8_t index = 0; index < MAX_ENEMIES; ++index) {
        const enemy_t *enemy = &g_enemies[index];
        if (!enemy->active) continue;
        (void)pxa_raster_sprite(&list, ENEMY_SLOT,
                                PXA_RASTER_SPRITE_TRANSPARENT_INDEX0,
                                g_capabilities, enemy->x, enemy->y, ENEMY_W,
                                ENEMY_H, 0, 0, 16, 16, 0);
    }
    for (uint8_t index = 0; index < MAX_PARTICLES; ++index) {
        const particle_t *particle = &g_particles[index];
        if (particle->active)
            append_rect(&list, particle->x, particle->y, 2, 2,
                        g_palette[particle->life > 6 ? 9 : 12]);
    }
    (void)pxa_raster_sprite(&list, PLAYER_SLOT,
                            PXA_RASTER_SPRITE_TRANSPARENT_INDEX0,
                            g_capabilities, g_target_w - PLAYER_MARGIN,
                            g_player_y, PLAYER_W, PLAYER_H, 0, 0, 16, 16, 0);
    append_text(&list, 4, 4, score_text, 0xffff);
    append_text(&list, g_target_w / 2 - 12, 4, wave_text, g_palette[7]);
    for (uint8_t index = 0; index < g_lives; ++index)
        append_rect(&list, g_target_w - 8 - index * 8, 5, 5, 5, g_palette[9]);
    if (g_hit_flash != 0 && (g_hit_flash & 1) == 0)
        append_rect(&list, 0, HUD_H, g_target_w, g_target_h - HUD_H, 0x7800);
    if (g_game_over) {
        append_rect(&list, 0, HUD_H, g_target_w, g_target_h - HUD_H, 0x0000);
        append_text(&list, g_target_w / 2 - 26, g_target_h / 2 - 6,
                    "GAME OVER", 0xffff);
        append_text(&list, g_target_w / 2 - 34, g_target_h / 2 + 6,
                    "TAP TO RESTART", g_palette[9]);
    }
    return pxa_raster_submit(g_context, &list) > 0;
}

static int initialize_input_surface(void) {
    pxa_ui_transaction_t transaction = {0};
    if (g_input_ready) return 1;
    if (!pxa_ui_transaction_begin(&transaction, 1,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE, g_packet,
                                  sizeof(g_packet)) ||
        !pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) ||
        !pxa_ui_create(&transaction, INPUT_NODE, 1, 0, PXA_UI_NODE_CANVAS) ||
        !pxa_ui_set_length(&transaction, INPUT_NODE, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_length(&transaction, INPUT_NODE, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_event_mask(&transaction, INPUT_NODE,
                               PXA_UI_EVENT_MASK_POINTER) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    g_input_ready = 1;
    return 1;
}

static void apply_screen(void) {
    int width = (int)g_screen.width / 2;
    int height = (int)g_screen.height / 2;
    if (width < TARGET_MIN_W) width = TARGET_MIN_W;
    if (width > TARGET_MAX_W) width = TARGET_MAX_W;
    if (height < TARGET_MIN_H) height = TARGET_MIN_H;
    if (height > TARGET_MAX_H) height = TARGET_MAX_H;
    g_target_w = width;
    g_target_h = height;
}

static int target_from_screen_y(int y) {
    const int height = (int)g_screen.height;
    if (height <= 0) return y;
    return y * g_target_h / height;
}

int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    pxa_game_screen_from_start(&g_screen, config, config_length);
    apply_screen();
    build_resources();
    reset_game();
    g_frame_id = 0;
    g_started = 0;
    if (!initialize_input_surface() || !pxa_window_fullscreen())
        return PXA_STATUS_INTERNAL;
    if (!pxa_game_render_create(CREATE_REQUEST, (uint16_t)g_target_w,
                                (uint16_t)g_target_h, 3, 1, g_packet,
                                sizeof(g_packet)))
        return PXA_STATUS_INTERNAL;
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (parsed.request_id == CREATE_REQUEST) {
        pxa_game_render_create_result_t created;
        if (!pxa_game_render_parse_create(&parsed, &created))
            return PXA_EVENT_UNHANDLED;
        if (created.status != PXA_STATUS_OK ||
            (created.capabilities & REQUIRED_CAPABILITIES) !=
                REQUIRED_CAPABILITIES)
            return PXA_EVENT_HANDLED;
        g_context = created.context_handle;
        g_capabilities = created.capabilities;
        if (!upload_resources()) return PXA_EVENT_HANDLED;
        g_started = 1;
        (void)render_frame();
        (void)pxa_clock_set_period(FRAME_PERIOD_MS);
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_SERVICE_CLOCK && parsed.opcode == PXA_CLOCK_TICK &&
        g_context != 0 && g_started) {
        tick_game();
        (void)render_frame();
        return PXA_EVENT_HANDLED;
    }
    {
        pxa_ui_pointer_data_t pointer;
        if (pxa_ui_parse_pointer(&parsed, &pointer) &&
            pointer.node == INPUT_NODE) {
            const int target_y = target_from_screen_y(pointer.y);
            if (pointer.phase == PXA_POINTER_DOWN ||
                pointer.phase == PXA_POINTER_MOVE) {
                if (g_game_over && pointer.phase == PXA_POINTER_DOWN)
                    reset_game();
                else if (!g_game_over)
                    g_player_target_y = target_y;
            }
            return PXA_EVENT_HANDLED;
        }
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
    if (g_context != 0) (void)pxa_close_handle(g_context);
    g_context = 0;
}
