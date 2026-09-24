#include "render.h"

#include "assets.h"
#include "font.h"
#include "palette.h"
#include "rc_math.h"
#include "sprites.h"

#define RAMP_GRAY 0
#define RAMP_BROWN 2
#define RAMP_RED 3
#define RAMP_ORANGE 4
#define RAMP_YELLOW 5
#define RAMP_GREEN 6
#define RAMP_GOLD 13
#define RAMP_CYAN 14
#define RAMP_WHITE 15

static ray_billboard_t g_billboards[MAX_THINGS];
static thing_t g_things[MAX_THINGS];

static uint16_t rgb565(int r, int g, int b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xF8) << 3) | (b >> 3));
}

static uint16_t ramp_color(int ramp, int level) {
    if (level < 1) {
        level = 1;
    } else if (level > LIGHT_LEVELS - 1) {
        level = LIGHT_LEVELS - 1;
    }
    return palette_color((uint8_t)(ramp * LIGHT_LEVELS + level));
}

static int design_px(const renderer_t *renderer, int value) {
    const int divisor = renderer->view.design_divisor;
    const int scaled = value * renderer->view.hud_scale;
    return (scaled + divisor / 2) / divisor;
}

static int design_size(const renderer_t *renderer, int value) {
    const int size = design_px(renderer, value);
    return size > 0 ? size : 1;
}

static int design_text_scale(const renderer_t *renderer, int value) {
    const int divisor = renderer->view.design_divisor;
    const int scale = (value * renderer->view.hud_scale + divisor - 1) /
                      divisor;
    return scale > 0 ? scale : 1;
}

void renderer_init(renderer_t *renderer, int width, int height, int hud_scale) {
    renderer->view.width = width;
    renderer->view.height = height;
    renderer->view.hud_scale = hud_scale < 1 ? 1 : hud_scale;
    renderer->view.design_divisor = width <= 148 ? 2 : 1;
    renderer->half_height = height / 2;
    renderer->hud_height = design_size(renderer, 26);
}

void renderer_draw_text(renderer_t *renderer, target_t *target, int x, int y,
                        const char *text, uint16_t color, int scale) {
    (void)renderer;
    for (; *text != '\0'; ++text) {
        int u0;
        int v0;
        if (!font_glyph_cell(*text, &u0, &v0)) {
            x += (GLYPH_WIDTH + 1) * scale;
            continue;
        }
        raster_solid_sprite(target, font_atlas(), x, y, GLYPH_WIDTH * scale,
                            GLYPH_HEIGHT * scale, u0, v0, GLYPH_WIDTH,
                            GLYPH_HEIGHT, color, 1);
        x += (GLYPH_WIDTH + 1) * scale;
    }
}

void renderer_draw_circle(renderer_t *renderer, target_t *target, int cx,
                          int cy, int radius, uint16_t color, int filled) {
    const int inner = radius - 2;
    int y;
    (void)renderer;
    for (y = -radius; y <= radius; ++y) {
        const int outer_half =
            (int)rc_sqrt((float)(radius * radius - y * y));
        if (filled || inner <= 0) {
            raster_rect(target, cx - outer_half, cy + y, 2 * outer_half + 1, 1,
                        color);
            continue;
        }
        {
            const int inner_sq = inner * inner - y * y;
            int inner_half;
            int arm;
            if (inner_sq < 0) {
                raster_rect(target, cx - outer_half, cy + y,
                            2 * outer_half + 1, 1, color);
                continue;
            }
            inner_half = (int)rc_sqrt((float)inner_sq);
            arm = outer_half - inner_half;
            if (arm <= 0) {
                continue;
            }
            raster_rect(target, cx - outer_half, cy + y, arm, 1, color);
            raster_rect(target, cx + inner_half + 1, cy + y, arm, 1, color);
        }
    }
}

static void draw_sprite(renderer_t *renderer, target_t *target, int sprite_id,
                        int x, int y, int scale) {
    const sprite_t *sprite = &kSprites[sprite_id];
    const int width = design_size(renderer, sprite->width * scale);
    const int height = design_size(renderer, sprite->height * scale);
    raster_sprite(target, sprite, palette_light(LIGHT_LEVELS - 1), x, y,
                  width, height, 0, 0, sprite->width, sprite->height, 1);
}

static void draw_weapon(renderer_t *renderer, target_t *target,
                        const world_t *world) {
    const int width = renderer->view.width;
    const int height = renderer->view.height;
    const player_t *p = &world->player;
    const sprite_t *gun = &kSprites[SPR_SHOTGUN];
    const int bob_amount = p->speed > 0.2F ? 1 : 0;
    const int bob_x =
        design_px(renderer,
                  (int)(rc_sin(p->bob_phase) * 6.0F * (float)bob_amount));
    const int bob_y =
        design_px(renderer, (int)(rc_fabs(rc_cos(p->bob_phase)) * 4.0F *
                                  (float)bob_amount));
    const int recoil = design_px(renderer, (int)(p->recoil * 16.0F));
    const int resting_y =
        height - renderer->hud_height - design_size(renderer, gun->height) +
        design_px(renderer, 8);
    const int muzzle_y = resting_y + design_px(renderer, 2);
    const int gun_x = width / 2 - design_px(renderer, 48) +
                      (muzzle_y - height / 2) / 3 + bob_x;
    const int gun_y = resting_y + bob_y + recoil;
    if (world->phase == PHASE_DEAD) {
        return;
    }
    if (world_muzzle_flash(world)) {
        const sprite_t *flash = &kSprites[SPR_MUZZLE_FLASH];
        draw_sprite(renderer, target, SPR_MUZZLE_FLASH,
                    gun_x + design_px(renderer, 48) -
                        design_size(renderer, flash->width) / 2,
                    gun_y - design_size(renderer, flash->height) +
                        design_px(renderer, 6),
                    1);
    }
    draw_sprite(renderer, target, SPR_SHOTGUN, gun_x, gun_y, 1);
}

static void draw_damage_tint(renderer_t *renderer, target_t *target,
                             const world_t *world) {
    float strength;
    int alpha;
    if (world->player.damage_flash <= 0.0F && world->phase != PHASE_DEAD) {
        return;
    }
    strength = world->phase == PHASE_DEAD
                   ? 0.55F
                   : rc_clampf(world->player.damage_flash, 0.0F, 1.0F);
    alpha = rc_clampi((int)(strength * 140.0F), 24, 140);
    raster_rect_alpha(target, 0, 0, renderer->view.width,
                      renderer->view.height, rgb565(220, 16, 16), alpha);
}

typedef struct {
    char text[64];
    int size;
} text_builder_t;

static void tb_append(text_builder_t *builder, const char *text) {
    while (*text != '\0' && builder->size + 1 < (int)sizeof(builder->text)) {
        builder->text[builder->size++] = *text++;
    }
    builder->text[builder->size] = '\0';
}

static void tb_append_int(text_builder_t *builder, int value) {
    char reversed[12];
    int count = 0;
    if (value < 0) {
        tb_append(builder, "-");
        value = -value;
    }
    do {
        reversed[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0 && count < 11);
    while (count != 0 && builder->size + 1 < (int)sizeof(builder->text)) {
        builder->text[builder->size++] = reversed[--count];
    }
    builder->text[builder->size] = '\0';
}

static void tb_append_tenths(text_builder_t *builder, uint32_t tenths) {
    tb_append_int(builder, (int)(tenths / 10U));
    tb_append(builder, ".");
    tb_append_int(builder, (int)(tenths % 10U));
}

static void draw_hud(renderer_t *renderer, target_t *target,
                     const world_t *world, const hud_stats_t *hud) {
    const int width = renderer->view.width;
    const int height = renderer->view.height;
    const int half = renderer->half_height;
    const int label_scale = design_text_scale(renderer, 1);
    const int value_scale = design_text_scale(renderer, 2);
    const player_t *p = &world->player;
    const int bar_top = height - renderer->hud_height;
    const uint16_t label = ramp_color(RAMP_GRAY, 9);
    const uint16_t health_color =
        ramp_color(p->health > 50 ? RAMP_GREEN
                                  : (p->health > 25 ? RAMP_YELLOW : RAMP_RED),
                   p->health > 50 ? 12 : (p->health > 25 ? 13 : 12));
    const uint16_t ammo_color = ramp_color(RAMP_GOLD, 12);
    const uint16_t kills_color = ramp_color(RAMP_CYAN, 12);
    text_builder_t text;

    raster_rect(target, 0, bar_top, width, 1, ramp_color(RAMP_GRAY, 5));
    raster_rect(target, 0, bar_top + 1, width, renderer->hud_height - 1,
                ramp_color(RAMP_GRAY, 1));

    renderer_draw_text(renderer, target, design_px(renderer, 8),
                       bar_top + design_px(renderer, 3), "HEALTH", label,
                       label_scale);
    text.size = 0;
    text.text[0] = '\0';
    tb_append_int(&text, p->health);
    tb_append(&text, "%");
    renderer_draw_text(renderer, target, design_px(renderer, 8),
                       bar_top + design_px(renderer, 11), text.text,
                       health_color, value_scale);

    renderer_draw_text(renderer, target, design_px(renderer, 92),
                       bar_top + design_px(renderer, 3), "SHELLS", label,
                       label_scale);
    text.size = 0;
    text.text[0] = '\0';
    tb_append_int(&text, p->ammo);
    renderer_draw_text(renderer, target, design_px(renderer, 92),
                       bar_top + design_px(renderer, 11), text.text,
                       ammo_color, value_scale);

    renderer_draw_text(renderer, target, design_px(renderer, 164),
                       bar_top + design_px(renderer, 3), "IMPS", label,
                       label_scale);
    text.size = 0;
    text.text[0] = '\0';
    tb_append_int(&text, world->kills);
    tb_append(&text, "/");
    tb_append_int(&text, world->imp_count);
    renderer_draw_text(renderer, target, design_px(renderer, 164),
                       bar_top + design_px(renderer, 11), text.text,
                       kills_color, value_scale);

    /* Crosshair: four arms leaving the centre open. */
    if (world->phase == PHASE_PLAYING) {
        const uint16_t white = ramp_color(RAMP_WHITE, 15);
        const int cx = width / 2;
        const int arm = design_size(renderer, 3);
        const int stroke = design_size(renderer, 1);
        raster_rect(target, cx - design_px(renderer, 4), half, arm, stroke,
                    white);
        raster_rect(target, cx + design_px(renderer, 2), half, arm, stroke,
                    white);
        raster_rect(target, cx, half - design_px(renderer, 4), stroke, arm,
                    white);
        raster_rect(target, cx, half + design_px(renderer, 2), stroke, arm,
                    white);
    }

    /* Centre message with a drop shadow. */
    if (world_message(world) != 0) {
        const char *message = world_message(world);
        const int text_width = font_text_width(message, label_scale);
        const int x = (width - text_width) / 2;
        const int y = world->phase == PHASE_PLAYING
                          ? design_px(renderer, 88)
                          : half - design_px(renderer, 16);
        renderer_draw_text(renderer, target, x + design_size(renderer, 1),
                           y + design_size(renderer, 1), message,
                           ramp_color(RAMP_GRAY, 1), label_scale);
        renderer_draw_text(renderer, target, x, y, message,
                           ramp_color(RAMP_YELLOW, 14), label_scale);
    }

    if (hud->show_perf) {
        text.size = 0;
        text.text[0] = '\0';
        if (renderer->view.design_divisor > 1) {
            tb_append(&text, "F");
            tb_append_int(&text, (int)hud->fps);
            tb_append(&text, " R");
            tb_append_tenths(&text, hud->render_ms_x10);
            tb_append(&text, " T");
            tb_append_int(&text, (int)hud->tick_avg_ms);
            tb_append(&text, "/");
            tb_append_int(&text, (int)hud->tick_max_ms);
        } else {
            tb_append(&text, "FPS ");
            tb_append_int(&text, (int)hud->fps);
            tb_append(&text, " RENDER ");
            tb_append_tenths(&text, hud->render_ms_x10);
            tb_append(&text, "MS TICK ");
            tb_append_int(&text, (int)hud->tick_avg_ms);
            tb_append(&text, "/");
            tb_append_int(&text, (int)hud->tick_max_ms);
            tb_append(&text, "MS");
        }
        renderer_draw_text(renderer, target, design_px(renderer, 15),
                           design_px(renderer, 3), text.text,
                           ramp_color(RAMP_GRAY, 1), label_scale);
        renderer_draw_text(renderer, target, design_px(renderer, 14),
                           design_px(renderer, 2), text.text,
                           ramp_color(RAMP_CYAN, 13), label_scale);
    }
}

void renderer_render(renderer_t *renderer, const world_t *world,
                     const hud_stats_t *hud, target_t *target) {
    const int count = world_collect_things(world, g_things, MAX_THINGS);
    const ray_camera_t camera = {world->player.x,      world->player.y,
                                 world->player.dir_x,  world->player.dir_y,
                                 world->player.plane_x, world->player.plane_y};
    int i;
    for (i = 0; i < count; ++i) {
        const sprite_t *sprite = &kSprites[g_things[i].sprite];
        g_billboards[i].x = g_things[i].x;
        g_billboards[i].y = g_things[i].y;
        g_billboards[i].height = g_things[i].height;
        g_billboards[i].lift = g_things[i].lift;
        g_billboards[i].texture_slot = (uint8_t)g_things[i].sprite;
        g_billboards[i].texture_width = sprite->width;
        g_billboards[i].texture_height = sprite->height;
        g_billboards[i].self_lit =
            g_things[i].sprite == SPR_FIREBALL_A ||
            g_things[i].sprite == SPR_FIREBALL_B ||
            sprite_is_torch(g_things[i].sprite);
    }
    raycast_frame(&world->cells[0][0], MAP_WIDTH, MAP_HEIGHT, &camera,
                  kTextures, kSprites, g_billboards, count, target);
    draw_weapon(renderer, target, world);
    draw_damage_tint(renderer, target, world);
    if (hud->visible) {
        draw_hud(renderer, target, world, hud);
    }
}

typedef struct {
    int origin_x;
    int origin_y;
    int unit;
    int text_scale;
} diagram_t;

static int dia_x(const diagram_t *dia, int value) {
    return dia->origin_x + value * dia->unit / 240;
}

static int dia_y(const diagram_t *dia, int value) {
    return dia->origin_y + value * dia->unit / 240;
}

static void dia_rect(const diagram_t *dia, target_t *target, int x, int y,
                     int w, int h, uint16_t color) {
    const int rw = w * dia->unit / 240 > 0 ? w * dia->unit / 240 : 1;
    const int rh = h * dia->unit / 240 > 0 ? h * dia->unit / 240 : 1;
    raster_rect(target, dia_x(dia, x), dia_y(dia, y), rw, rh, color);
}

static void dia_circle(renderer_t *renderer, const diagram_t *dia,
                       target_t *target, int x, int y, int radius,
                       uint16_t color, int filled) {
    renderer_draw_circle(renderer, target, dia_x(dia, x), dia_y(dia, y),
                         radius * dia->unit / 240, color, filled);
}

static void dia_text(renderer_t *renderer, const diagram_t *dia,
                     target_t *target, int x, int y, const char *label,
                     uint16_t color) {
    const int width = font_text_width(label, dia->text_scale);
    renderer_draw_text(renderer, target, dia_x(dia, x) - width / 2,
                       dia_y(dia, y), label, color, dia->text_scale);
}

static void dia_arrow(const diagram_t *dia, target_t *target, int x, int y,
                      int dx, int dy, uint16_t color) {
    int i;
    for (i = 0; i < 13; ++i) {
        dia_rect(dia, target, x + dx * i, y + dy * i, 2, 2, color);
    }
    for (i = 0; i < 5; ++i) {
        dia_rect(dia, target, x + dx * (12 - i) + dy * i,
                 y + dy * (12 - i) + dx * i, 2, 2, color);
        dia_rect(dia, target, x + dx * (12 - i) - dy * i,
                 y + dy * (12 - i) - dx * i, 2, 2, color);
    }
}

void renderer_draw_instructions(renderer_t *renderer, target_t *target) {
    const int width = renderer->view.width;
    const int height = renderer->view.height;
    const int unit = width < height ? width : height;
    diagram_t dia;
    const uint16_t white = ramp_color(RAMP_WHITE, 14);
    const uint16_t cyan = ramp_color(RAMP_CYAN, 13);
    const uint16_t orange = ramp_color(RAMP_ORANGE, 13);
    int i;

    dia.origin_x = (width - unit) / 2;
    dia.origin_y = (height - unit * 220 / 240) / 2;
    dia.unit = unit;
    dia.text_scale = unit >= 440 ? 2 : 1;

    raster_rect_alpha(target, 0, 0, width, height, rgb565(12, 18, 28), 90);

    /* Side view: a person looks at an upright screen, held in front of the
     * face. Pixel-art arrows keep the diagram in the game's visual language. */
    dia_circle(renderer, &dia, target, 72, 32, 9, white, 0);
    dia_rect(&dia, target, 79, 30, 5, 3, white);  /* nose points at the screen */
    dia_rect(&dia, target, 69, 42, 4, 24, white);
    dia_rect(&dia, target, 73, 52, 24, 3, white); /* arm and hand */
    dia_rect(&dia, target, 95, 48, 4, 7, white);
    dia_rect(&dia, target, 103, 23, 6, 35, cyan);
    dia_rect(&dia, target, 104, 26, 2, 28, white);
    for (i = 87; i < 102; i += 5) {
        dia_rect(&dia, target, i, 33, 2, 1, cyan); /* eye line */
    }
    dia_arrow(&dia, target, 116, 43, 0, -1, cyan);
    dia_text(renderer, &dia, target, 171, 29, "GET READY", white);
    dia_text(renderer, &dia, target, 171, 44, "TOUCH MODE", cyan);
    dia_text(renderer, &dia, target, 120, 73, "DRAG RIGHT SIDE TO LOOK", white);

    /* The coloured panels cover the actual left and right touch halves. */
    raster_rect_alpha(target, 0, dia_y(&dia, 91), width / 2,
                      dia_y(&dia, 183) - dia_y(&dia, 91), rgb565(15, 48, 61),
                      125);
    raster_rect_alpha(target, width / 2, dia_y(&dia, 91), width - width / 2,
                      dia_y(&dia, 183) - dia_y(&dia, 91), rgb565(63, 34, 24),
                      125);
    dia_rect(&dia, target, 119, 91, 2, 92, white);
    dia_text(renderer, &dia, target, 60, 97, "MOVE", cyan);
    dia_text(renderer, &dia, target, 180, 97, "FIRE", orange);
    dia_circle(renderer, &dia, target, 60, 138, 17, cyan, 0);
    dia_circle(renderer, &dia, target, 60, 138, 6, cyan, 1);
    dia_arrow(&dia, target, 60, 117, 0, -1, cyan);
    dia_arrow(&dia, target, 60, 159, 0, 1, cyan);
    dia_arrow(&dia, target, 39, 138, -1, 0, cyan);
    dia_arrow(&dia, target, 81, 138, 1, 0, cyan);
    /* A crosshair inside a large fire pad, distinct from the move stick. */
    dia_circle(renderer, &dia, target, 180, 137, 23, orange, 0);
    dia_circle(renderer, &dia, target, 180, 137, 10, orange, 0);
    dia_rect(&dia, target, 179, 120, 2, 10, orange);
    dia_rect(&dia, target, 179, 145, 2, 10, orange);
    dia_rect(&dia, target, 163, 136, 10, 2, orange);
    dia_rect(&dia, target, 188, 136, 10, 2, orange);
    dia_circle(renderer, &dia, target, 180, 137, 2, white, 1);
    dia_text(renderer, &dia, target, 60, 174, "DRAG LEFT", white);
    dia_text(renderer, &dia, target, 180, 174, "TAP / HOLD", white);
    dia_text(renderer, &dia, target, 120, 191, "FUNCTION: FIRE", white);
    dia_rect(&dia, target, 12, 207, 216, 13, cyan);
    dia_text(renderer, &dia, target, 120, 210, "TAP ANYWHERE TO START",
             ramp_color(RAMP_GRAY, 1));
}

void renderer_draw_stick(renderer_t *renderer, target_t *target,
                         int stick_active, int origin_x, int origin_y,
                         int stick_x, int stick_y) {
    const uint16_t ring = ramp_color(RAMP_WHITE, 9);
    const uint16_t knob = ramp_color(RAMP_CYAN, 12);
    const int ring_radius = design_size(renderer, 30);
    const int knob_radius = design_size(renderer, 8);
    int dx;
    int dy;
    float len;
    if (!stick_active) {
        return;
    }
    renderer_draw_circle(renderer, target, origin_x, origin_y, ring_radius, ring,
                         0);
    dx = stick_x - origin_x;
    dy = stick_y - origin_y;
    len = rc_sqrt((float)(dx * dx + dy * dy));
    if (len > (float)ring_radius) {
        dx = (int)((float)dx * (float)ring_radius / len);
        dy = (int)((float)dy * (float)ring_radius / len);
    }
    renderer_draw_circle(renderer, target, origin_x + dx, origin_y + dy,
                         knob_radius, knob, 1);
}
