#include "render.h"

#include <stddef.h>

#include "rc_math.h"
#include "voxel_sky.h"

#include "block_textures.h"

/* Large enough for a native (1X) render of the product's 296x240 view and of
 * a 320x240 view. Larger views fall back to 2X or coarser. */
#define SCENE_MAX_W RENDER_SCENE_MAX_W
#define SCENE_MAX_H RENDER_SCENE_MAX_H
#define TAN_HALF 0.70F
#define FOG_START 14.0F

#define RGB565(r, g, b) \
    ((uint16_t)((((uint16_t)(r)&0xF8u) << 8) | (((uint16_t)(g)&0xFCu) << 3) | \
                ((uint16_t)(b) >> 3)))

#define COL_WHITE RGB565(245, 245, 245)
#define COL_BLACK RGB565(10, 10, 14)
#define COL_SHADOW RGB565(12, 16, 24)
#define COL_PANEL RGB565(30, 36, 48)
#define COL_PANEL_LIGHT RGB565(52, 62, 80)
#define COL_SELECT RGB565(250, 214, 70)
#define COL_TEXT RGB565(232, 240, 248)
#define COL_MUTED RGB565(160, 176, 192)
#define COL_GREEN RGB565(96, 200, 110)
#define COL_DANGER RGB565(226, 86, 86)

#define FOG_R 168
#define FOG_G 205
#define FOG_B 244

static uint16_t *g_scene;
static uint16_t g_depth[SCENE_MAX_W * SCENE_MAX_H];
static float g_ndc_x[SCENE_MAX_W];
static float g_ndc_y[SCENE_MAX_H];
static uint16_t g_ray_len_q12[SCENE_MAX_W * SCENE_MAX_H];
static uint16_t *g_row_ptr[SCREEN_H_MAX];
static int g_scene_w = 148;
static int g_scene_h = 120;
static int g_scale = 2;
static int g_max_steps = 80;
static float g_fog_end = 46.0F;
static float g_fog_inv = 1.0F / 32.0F;
/* Keep the direct Surface within the Guest's fixed RGB565 frame buffer. */
static int g_view_pixel_budget = 115000;
/* Dynamic scene limits. The raster path can render at the real display
 * resolution, while the Guest CPU path stays inside its static tables. */
static int g_scene_limit_w = RENDER_SCENE_MAX_W;
static int g_scene_limit_h = RENDER_SCENE_MAX_H;
static render_perf_stats_t g_perf_stats;
/* Physical safe area merged with the Host's system chrome/gesture reserves.
 * The Host publishes them through the window snapshot; interactive controls
 * and HUD text stay inside this region. */
static int g_inset_left;
static int g_inset_top;
static int g_inset_right;
static int g_inset_bottom;

render_layout_t g_layout = {
    .screen_w = SCREEN_W_DEFAULT,
    .screen_h = SCREEN_H_DEFAULT,
    .view_w = SCREEN_W_DEFAULT,
    .view_h = SCREEN_H_DEFAULT,
    .ui_scale = 1,
    .hotbar_slot = HOTBAR_SLOT,
};

menu_button_t g_menu_buttons[MENU_BUTTON_MAX];
int g_menu_button_count;

inventory_layout_t g_inv_layout;
static int g_inv_mode = 2;

int render_min_quality(void) {
    int scale;
    static const uint8_t scales[] = {
        QUALITY_MIN, QUALITY_BALANCED, QUALITY_PERFORMANCE};
    size_t index;
    for (index = 0; index < sizeof(scales) / sizeof(scales[0]); ++index) {
        scale = scales[index];
        const int width = (g_layout.view_w + scale - 1) / scale;
        const int height = (g_layout.view_h + scale - 1) / scale;
        if (width <= g_scene_limit_w && height <= g_scene_limit_h &&
            (g_view_pixel_budget <= 0 ||
             width * height <= g_view_pixel_budget)) {
            return scale;
        }
    }
    return QUALITY_PERFORMANCE;
}

static void render_update_scene(void) {
    const int view_w = g_layout.view_w;
    const int view_h = g_layout.view_h;
    int width = (view_w + g_scale - 1) / g_scale;
    int height = (view_h + g_scale - 1) / g_scale;
    /* Clamp the scene proportionally so a very wide or tall view keeps its
     * aspect instead of stretching. The limits follow the active surface
     * path: the raster path may use the real display resolution, while the
     * Guest CPU renderer must stay inside its static tables. */
    if (width > g_scene_limit_w) {
        height = height * g_scene_limit_w / width;
        width = g_scene_limit_w;
    }
    if (height > g_scene_limit_h) {
        width = width * g_scene_limit_h / height;
        height = g_scene_limit_h;
    }
    if (width < 32) {
        width = 32;
    }
    if (height < 24) {
        height = 24;
    }
    g_scene_w = width;
    g_scene_h = height;
    /* Lower quality also shortens the view distance, which cuts the number of
     * DDA steps per ray on top of the lower ray count. */
    if (g_scale == QUALITY_PERFORMANCE) {
        g_max_steps = 48;
        g_fog_end = 30.0F;
    } else if (g_scale == QUALITY_BALANCED) {
        g_max_steps = 64;
        g_fog_end = 38.0F;
    } else {
        g_max_steps = 80;
        g_fog_end = 46.0F;
    }
    g_fog_inv = 1.0F / (g_fog_end - FOG_START);
    /* The NDC and ray-length tables belong to the Guest CPU renderer; the Host
     * raster path does not use them, so oversized scenes skip the fill. */
    if (g_scene_w <= SCENE_MAX_W && g_scene_h <= SCENE_MAX_H) {
        const float inv_w = 1.0F / (float)g_scene_w;
        const float inv_h = 1.0F / (float)g_scene_h;
        const float tan_x = TAN_HALF * ((float)g_scene_w / (float)g_scene_h);
        int y;
        for (int x = 0; x < g_scene_w; ++x) {
            g_ndc_x[x] =
                (2.0F * ((float)x + 0.5F) * inv_w - 1.0F) * tan_x;
        }
        for (y = 0; y < g_scene_h; ++y) {
            const float ndc_y =
                (1.0F - 2.0F * ((float)y + 0.5F) * inv_h) * TAN_HALF;
            uint16_t *lengths = &g_ray_len_q12[y * g_scene_w];
            g_ndc_y[y] = ndc_y;
            for (int x = 0; x < g_scene_w; ++x) {
                const float length = rc_sqrt(
                    1.0F + g_ndc_x[x] * g_ndc_x[x] + ndc_y * ndc_y);
                lengths[x] = (uint16_t)(length * 4096.0F + 0.5F);
            }
        }
    }
}

int render_set_safe_insets(int left, int top, int right, int bottom) {
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right < 0) right = 0;
    if (bottom < 0) bottom = 0;
    if (left == g_inset_left && top == g_inset_top &&
        right == g_inset_right && bottom == g_inset_bottom)
        return 0;
    g_inset_left = left;
    g_inset_top = top;
    g_inset_right = right;
    g_inset_bottom = bottom;
    return 1;
}

void render_configure(int width, int height) {
    int ui_scale;
    int slot;
    if (width < 120) {
        width = 120;
    }
    if (height < 90) {
        height = 90;
    }
    g_layout.screen_w = width;
    g_layout.screen_h = height;
    g_layout.view_w = width < SCREEN_W_MAX ? width : SCREEN_W_MAX;
    g_layout.view_h = height < SCREEN_H_MAX ? height : SCREEN_H_MAX;
    /* Keep layout coordinates in the full logical viewport. Render quality
     * controls the smaller Surface size in render_update_scene(); shrinking
     * the viewport itself leaves a low-resolution, unscaled image in the
     * middle of larger displays such as the 412x412 Watcher. */
    g_layout.view_x = (width - g_layout.view_w) / 2;
    g_layout.view_y = (height - g_layout.view_h) / 2;
    ui_scale = width >= 640 && height >= 400 ? 2 : 1;
    slot = width < 280 ? 20 : HOTBAR_SLOT * ui_scale;
    g_layout.ui_scale = ui_scale;
    g_layout.hotbar_slot = slot;
    /* Interactive controls stay clear of the union of the physical safe area
     * and the Host's system chrome/gesture reserves (status bar pull-down,
     * Home gesture, Back gesture). The 3D view itself stays edge to edge. */
    {
        int hud_x = g_layout.view_x + g_inset_left;
        int hud_y = g_layout.view_y + g_inset_top;
        int hud_w = g_layout.view_w - g_inset_left - g_inset_right;
        int hud_h = g_layout.view_h - g_inset_top - g_inset_bottom;
        if (hud_w < 96) {
            hud_x = g_layout.view_x;
            hud_w = g_layout.view_w;
        }
        if (hud_h < 72) {
            hud_y = g_layout.view_y;
            hud_h = g_layout.view_h;
        }
        g_layout.hotbar_y = hud_y + hud_h - 48 * ui_scale;
        g_layout.jump_x = hud_x + hud_w - 34 * ui_scale;
        g_layout.jump_y = hud_y + hud_h - 42 * ui_scale;
        g_layout.jump_r = 16 * ui_scale;
        if (ui_scale == 1) {
            const int hotbar_right =
                g_layout.jump_x - g_layout.jump_r - 8;
            g_layout.hotbar_x =
                hud_x + (hotbar_right - hud_x - HOTBAR_SLOTS * slot) / 2;
            if (g_layout.hotbar_x < hud_x + 4)
                g_layout.hotbar_x = hud_x + 4;
        } else {
            g_layout.hotbar_x =
                hud_x + (hud_w - HOTBAR_SLOTS * slot) / 2;
        }
        g_layout.action_x = g_layout.jump_x;
        g_layout.action_y = g_layout.jump_y - 38 * ui_scale;
        g_layout.action_r = 16 * ui_scale;
        g_layout.place_x = g_layout.jump_x;
        g_layout.place_y = g_layout.jump_y - 76 * ui_scale;
        g_layout.place_r = 16 * ui_scale;
        g_layout.down_x = g_layout.jump_x;
        g_layout.down_y = g_layout.jump_y - 112 * ui_scale;
        g_layout.down_r = 12 * ui_scale;
        g_layout.fly_x = hud_x + hud_w - 20 * ui_scale;
        g_layout.fly_y = hud_y + 28 * ui_scale;
        g_layout.fly_r = 11 * ui_scale;
        g_layout.quality_x = hud_x + 24 * ui_scale;
        g_layout.quality_y = hud_y + 24 * ui_scale;
        g_layout.quality_w = 150 * ui_scale;
        g_layout.quality_h = 18 * ui_scale;
        g_layout.bag_x = hud_x + hud_w - 48 * ui_scale;
        g_layout.bag_y = hud_y + 28 * ui_scale;
        g_layout.bag_r = 11 * ui_scale;
        g_layout.menu_x = hud_x + hud_w - 76 * ui_scale;
        g_layout.menu_y = hud_y + 28 * ui_scale;
        g_layout.menu_r = 11 * ui_scale;
        {
            const int quality_max_w =
                g_layout.menu_x - g_layout.menu_r - 4 * ui_scale -
                g_layout.quality_x;
            if (g_layout.quality_w > quality_max_w)
                g_layout.quality_w = quality_max_w;
            if (g_layout.quality_w < 60 * ui_scale)
                g_layout.quality_w = 60 * ui_scale;
        }
        g_inv_layout.panel_x = hud_x + 6 * ui_scale;
        g_inv_layout.panel_y = hud_y + 6 * ui_scale;
        g_inv_layout.panel_w = hud_w - 12 * ui_scale;
        g_inv_layout.panel_h = hud_h - 12 * ui_scale;
    }
    g_inv_layout.main_x =
        g_inv_layout.panel_x + (g_inv_layout.panel_w - 9 * slot) / 2;
    g_inv_layout.main_y =
        g_inv_layout.panel_y + (g_inv_mode == 3 ? 112 : 96) * ui_scale;
    g_inv_layout.inv_hotbar_x = g_inv_layout.main_x;
    g_inv_layout.inv_hotbar_y =
        g_inv_layout.main_y + 3 * slot + 8 * ui_scale;
    g_inv_layout.craft_x = g_inv_layout.main_x + 30 * ui_scale;
    g_inv_layout.craft_y = g_inv_layout.panel_y + 34 * ui_scale;
    g_inv_layout.result_x =
        g_inv_layout.craft_x + g_inv_mode * slot + 16 * ui_scale;
    g_inv_layout.result_y = g_inv_layout.craft_y +
                            (g_inv_mode * slot) / 2 - slot / 2;
    g_inv_layout.close_x =
        g_inv_layout.panel_x + g_inv_layout.panel_w - 18 * ui_scale;
    g_inv_layout.close_y = g_inv_layout.panel_y + 32 * ui_scale;
    g_inv_layout.close_r = 10 * ui_scale;
    if (g_scale < render_min_quality()) {
        g_scale = render_min_quality();
    }
    render_update_scene();
}

void render_set_quality(int scale) {
    const int minimum = render_min_quality();
    if (scale < minimum) {
        scale = minimum;
    } else if (scale > QUALITY_BALANCED && scale < QUALITY_PERFORMANCE) {
        scale = QUALITY_PERFORMANCE;
    } else if (scale > QUALITY_MAX) {
        scale = QUALITY_MAX;
    }
    g_scale = scale;
    render_update_scene();
}

void render_set_scene_limits(int max_width, int max_height,
                             int pixel_budget) {
    if (max_width < 32) max_width = 32;
    if (max_height < 24) max_height = 24;
    if (max_width > SCREEN_W_MAX) max_width = SCREEN_W_MAX;
    if (max_height > SCREEN_H_MAX) max_height = SCREEN_H_MAX;
    if (pixel_budget < 0) pixel_budget = 0;
    if (max_width == g_scene_limit_w && max_height == g_scene_limit_h &&
        pixel_budget == g_view_pixel_budget) {
        return;
    }
    g_scene_limit_w = max_width;
    g_scene_limit_h = max_height;
    g_view_pixel_budget = pixel_budget;
    render_configure(g_layout.screen_w, g_layout.screen_h);
}

void render_shrink_view(void) {
    int budget = g_view_pixel_budget;
    if (budget == 0) budget = g_scene_w * g_scene_h;
    if (budget > 40000) {
        budget = budget * 3 / 4;
    }
    if (budget != g_view_pixel_budget) {
        g_view_pixel_budget = budget;
    }
    render_configure(g_layout.screen_w, g_layout.screen_h);
}

int render_quality(void) { return g_scale; }
int render_scene_width(void) { return g_scene_w; }
int render_scene_height(void) { return g_scene_h; }
void render_get_perf_stats(render_perf_stats_t *stats) {
    if (stats != NULL) *stats = g_perf_stats;
}

static const uint8_t kBaseR[BLOCK_TYPE_COUNT] = {
    0,   95,  134, 126, 218, 102, 58,  48,  157, 150,
    170, 105, 140, 236, 128, 58,  44,  70,  232, 58};
static const uint8_t kBaseG[BLOCK_TYPE_COUNT] = {
    0,   159, 96,  126, 207, 81,  143, 96,  128, 70,
    210, 105, 110, 240, 120, 140, 110, 150, 230, 58};
static const uint8_t kBaseB[BLOCK_TYPE_COUNT] = {
    0,   53,  67,  126, 160, 50,  70,  200, 79,  55,
    225, 105, 70,  246, 112, 62,  50,  60,  220, 58};

/* Mob body colours by kind: slime, sheep, pig, cow. */
static const uint8_t kMobR[MOB_KIND_COUNT] = {104, 232, 232, 110};
static const uint8_t kMobG[MOB_KIND_COUNT] = {188, 232, 150, 80};
static const uint8_t kMobB[MOB_KIND_COUNT] = {104, 225, 160, 60};

/* Face order is face * 2 + (dir positive ? 0 : 1):
 * x+, x-, y-(bottom), y+(top), z+, z-. */
static const uint8_t kFaceShade[6] = {200, 175, 115, 255, 165, 145};

typedef struct {
    float fx;
    float fy;
    float fz;
    float rx;
    float ry;
    float rz;
    float ux;
    float uy;
    float uz;
    float cam_x;
    float cam_y;
    float cam_z;
    float tan_x;
    float tan_y;
} camera_t;

static void camera_build(camera_t *cam, const player_t *player) {
    const float cy = rc_cos(player->yaw);
    const float sy = rc_sin(player->yaw);
    const float cp = rc_cos(player->pitch);
    const float sp = rc_sin(player->pitch);
    cam->fx = sy * cp;
    cam->fy = sp;
    cam->fz = cy * cp;
    cam->rx = cy;
    cam->ry = 0.0F;
    cam->rz = -sy;
    cam->ux = -sy * sp;
    cam->uy = cp;
    cam->uz = -cy * sp;
    cam->cam_x = player->x;
    cam->cam_y = player->y + EYE_HEIGHT;
    cam->cam_z = player->z;
    cam->tan_y = TAN_HALF;
    cam->tan_x = TAN_HALF * ((float)g_scene_w / (float)g_scene_h);
}

static int camera_project(const camera_t *cam, float x, float y, float z,
                          float *out_x, float *out_y, float *out_depth) {
    const float dx = x - cam->cam_x;
    const float dy = y - cam->cam_y;
    const float dz = z - cam->cam_z;
    const float depth = dx * cam->fx + dy * cam->fy + dz * cam->fz;
    const float right = dx * cam->rx + dy * cam->ry + dz * cam->rz;
    const float up = dx * cam->ux + dy * cam->uy + dz * cam->uz;
    if (depth < 0.06F) {
        return 0;
    }
    *out_x = (float)g_scene_w * 0.5F +
             (right / depth) * ((float)g_scene_w * 0.5F) / cam->tan_x;
    *out_y = (float)g_scene_h * 0.5F -
             (up / depth) * ((float)g_scene_h * 0.5F) / cam->tan_y;
    *out_depth = depth;
    return 1;
}

static inline uint32_t hash3(int x, int y, int z) {
    uint32_t value = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u +
                     (uint32_t)z * 2246822519u;
    value = (value ^ (value >> 13)) * 1274126177u;
    return value ^ (value >> 16);
}

uint16_t render_block_color(int block) {
    if ((unsigned)block >= (unsigned)BLOCK_TYPE_COUNT) {
        block = BLOCK_DIRT;
    }
    return RGB565(kBaseR[block], kBaseG[block], kBaseB[block]);
}

static inline uint16_t *frame_row(int y) { return g_row_ptr[y]; }

static uint16_t sky_pixel(float ndx, float ndy, float ndz) {
    int r;
    int g;
    int b;
    float sun;
    if (ndy >= 0.0F) {
        r = 150 - (int)(ndy * 95.0F);
        g = 200 - (int)(ndy * 75.0F);
        b = 252 - (int)(ndy * 30.0F);
    } else {
        const float below = -ndy;
        r = 150 + (int)(below * 30.0F);
        g = 200 - (int)(below * 25.0F);
        b = 240 - (int)(below * 30.0F);
    }
    for (int index = 0; index < VOXEL_SKY_CLOUD_COUNT; ++index) {
        const float cx = kVoxelSkyCloudDirections[index][0];
        const float cz = kVoxelSkyCloudDirections[index][1];
        const float horizontal = ndx * cz - ndz * cx;
        const float elevation = index & 1 ? 0.30F : 0.39F;
        const float vertical = ndy - elevation;
        if (ndx * cx + ndz * cz > 0.65F &&
            rc_fabs(horizontal) < 0.15F &&
            ((rc_fabs(vertical) < 0.038F) ||
             (vertical >= 0.015F && vertical < 0.085F &&
              rc_fabs(horizontal) < 0.075F))) {
            r = vertical < -0.012F ? 220 : 247;
            g = vertical < -0.012F ? 233 : 249;
            b = vertical < -0.012F ? 239 : 250;
            break;
        }
    }
    sun = ndx * VOXEL_SKY_SUN_X + ndy * VOXEL_SKY_SUN_Y +
          ndz * VOXEL_SKY_SUN_Z;
    if (sun > 0.992F) {
        r = 255;
        g = 252;
        b = 235;
    } else if (sun > 0.975F) {
        const int glow = (int)((sun - 0.975F) * 1600.0F);
        r += glow;
        g += glow;
        b += glow >> 1;
    }
    if (r > 255) {
        r = 255;
    }
    if (g > 255) {
        g = 255;
    }
    if (b > 255) {
        b = 255;
    }
    return RGB565(r, g, b);
}

/* --- block textures ------------------------------------------------------
 * The 16 x 16 RGB565 tiles are generated and shared with the GameRender
 * raster path through block_textures.c; the ray caster and HUD item icons
 * sample the same tiles. */
static const block_texture_set_t *g_tex;

static void render_ensure_textures(void) {
    if (g_tex == NULL) {
        g_tex = block_textures();
    }
}

static uint16_t shade_block(uint8_t block, int face, int sign, float uu,
                            float vv, float distance, uint32_t now_ms) {
    const int kind = face == 1 ? (sign < 0 ? BLOCK_TEXTURE_TOP : BLOCK_TEXTURE_BOTTOM)
                               : BLOCK_TEXTURE_SIDE;
    const float frac_u = uu - rc_floor(uu);
    const float frac_v = vv - rc_floor(vv);
    int tx = (int)(frac_u * 16.0F) & 15;
    int ty = 15 - ((int)(frac_v * 16.0F) & 15);
    int r;
    int g;
    int b;
    int shade;
    if (block == BLOCK_WATER) {
        /* Blend two differently scrolled samples of the ripple tile. */
        const uint16_t *tile = g_tex[BLOCK_WATER][kind];
        const int shift_a = (int)(now_ms >> 7);
        const int shift_b = (int)(now_ms >> 8);
        const int ax = (tx + shift_a) & 15;
        const int by = (ty + shift_b) & 15;
        const uint16_t ca = tile[(ty << 4) | ax];
        const uint16_t cb = tile[(by << 4) | tx];
        r = (((ca >> 11) & 31) + ((cb >> 11) & 31)) >> 1;
        g = (((ca >> 5) & 63) + ((cb >> 5) & 63)) >> 1;
        b = ((ca & 31) + (cb & 31)) >> 1;
    } else {
        const uint16_t color = g_tex[block][kind][(ty << 4) | tx];
        r = (color >> 11) & 31;
        g = (color >> 5) & 63;
        b = color & 31;
    }
    shade = kFaceShade[face * 2 + (sign > 0 ? 0 : 1)];
    r = (r * shade) >> 8;
    g = (g * shade) >> 8;
    b = (b * shade) >> 8;
    {
        int fog = (int)((distance - FOG_START) * (256.0F * g_fog_inv));
        if (fog < 0) {
            fog = 0;
        } else if (fog > 256) {
            fog = 256;
        }
        if (fog != 0) {
            r = (r * (256 - fog) + (FOG_R >> 3) * fog) >> 8;
            g = (g * (256 - fog) + (FOG_G >> 2) * fog) >> 8;
            b = (b * (256 - fog) + (FOG_B >> 3) * fog) >> 8;
        }
    }
    return (uint16_t)((r << 11) | (g << 5) | b);
}

#define RAY_Q_SHIFT 12
#define RAY_Q_ONE (1 << RAY_Q_SHIFT)
#define RAY_DELTA_NUMERATOR (1 << (RAY_Q_SHIFT * 2))

static int32_t ray_delta_q12(float direction) {
    const int32_t fixed = (int32_t)(direction * (float)RAY_Q_ONE);
    const uint32_t magnitude = fixed < 0
                                   ? (uint32_t)(-(fixed + 1)) + 1u
                                   : (uint32_t)fixed;
    if (magnitude == 0) return RAY_DELTA_NUMERATOR;
    {
        const uint32_t delta = RAY_DELTA_NUMERATOR / magnitude;
        return (int32_t)(delta == 0 ? 1u : delta);
    }
}

static int32_t ray_side_q12(float distance, int32_t delta_q12) {
    int32_t distance_q12 = (int32_t)(distance * (float)RAY_Q_ONE);
    if (distance_q12 < 0) distance_q12 = 0;
    if (distance_q12 > RAY_Q_ONE) distance_q12 = RAY_Q_ONE;
    return (int32_t)(((int64_t)distance_q12 * delta_q12) >> RAY_Q_SHIFT);
}

static void render_scene(const camera_t *cam, uint32_t now_ms) {
    const float fx = cam->fx;
    const float fy = cam->fy;
    const float fz = cam->fz;
    const float rx = cam->rx;
    const float rz = cam->rz;
    const float ux = cam->ux;
    const float uy = cam->uy;
    const float uz = cam->uz;
    const float cam_x = cam->cam_x;
    const float cam_y = cam->cam_y;
    const float cam_z = cam->cam_z;
    const int initial_map_x = rc_floor_int(cam_x);
    const int initial_map_y = rc_floor_int(cam_y);
    const int initial_map_z = rc_floor_int(cam_z);
    const float negative_x = cam_x - (float)initial_map_x;
    const float negative_y = cam_y - (float)initial_map_y;
    const float negative_z = cam_z - (float)initial_map_z;
    const float positive_x = 1.0F - negative_x;
    const float positive_y = 1.0F - negative_y;
    const float positive_z = 1.0F - negative_z;
    const int32_t fog_end_q12 = (int32_t)(g_fog_end * (float)RAY_Q_ONE);
    /* WAMR Guest execution cannot sustain one full DDA ray per internal pixel
     * at the balanced touch profile. Reuse each result over a 2x2 block so
     * AUTO 2X stays within the application's 30 FPS frame budget. */
    const int ray_pixel_step =
        g_scale >= QUALITY_BALANCED ? 2 : 1;
    int py;
    g_perf_stats = (render_perf_stats_t){0};
    for (py = 0; py < g_scene_h; py += ray_pixel_step) {
        const float ndc_y = g_ndc_y[py];
        const float dir_y = fy + uy * ndc_y;
        const uint16_t *ray_lengths = &g_ray_len_q12[py * g_scene_w];
        uint16_t *out = &g_scene[py * g_scene_w];
        uint16_t *depth_out = &g_depth[py * g_scene_w];
        int px;
        for (px = 0; px < g_scene_w; px += ray_pixel_step) {
            const float ndc_x = g_ndc_x[px];
            const float dir_x = fx + rx * ndc_x + ux * ndc_y;
            const float dir_z = fz + rz * ndc_x + uz * ndc_y;
            const float ray_len = (float)ray_lengths[px] * (1.0F / 4096.0F);
            int map_x = initial_map_x;
            int map_y = initial_map_y;
            int map_z = initial_map_z;
            const int step_x = dir_x > 0.0F ? 1 : -1;
            const int step_y = dir_y > 0.0F ? 1 : -1;
            const int step_z = dir_z > 0.0F ? 1 : -1;
            const int32_t delta_x = ray_delta_q12(dir_x);
            const int32_t delta_y = ray_delta_q12(dir_y);
            const int32_t delta_z = ray_delta_q12(dir_z);
            int32_t side_x = ray_side_q12(
                dir_x > 0.0F ? positive_x : negative_x, delta_x);
            int32_t side_y = ray_side_q12(
                dir_y > 0.0F ? positive_y : negative_y, delta_y);
            int32_t side_z = ray_side_q12(
                dir_z > 0.0F ? positive_z : negative_z, delta_z);
            uint8_t block = BLOCK_AIR;
            int face = 0;
            int sign = 1;
            int fog_terminated = 0;
            int32_t travel_q12 = 0;
            int step;
            /* Rays stay inside one chunk for many steps; cache the chunk
             * pointer so the hot loop only pays one data load per step. */
            int cache_i = 0x7FFFFFFF;
            int cache_j = 0;
            chunk_t *chunk = g_chunk_grid[0][0];
            for (step = 0; step < g_max_steps; ++step) {
                if (side_x <= side_y && side_x <= side_z) {
                    map_x += step_x;
                    travel_q12 = side_x;
                    side_x += delta_x;
                    face = 0;
                    sign = step_x;
                } else if (side_y <= side_z) {
                    map_y += step_y;
                    travel_q12 = side_y;
                    side_y += delta_y;
                    face = 1;
                    sign = step_y;
                } else {
                    map_z += step_z;
                    travel_q12 = side_z;
                    side_z += delta_z;
                    face = 2;
                    sign = step_z;
                }
                if (travel_q12 > fog_end_q12) {
                    block = BLOCK_AIR;
                    fog_terminated = 1;
                    break;
                }
                {
                    const int ci = (map_x >> CHUNK_BITS) - g_chunk_origin_cx;
                    const int cj = (map_z >> CHUNK_BITS) - g_chunk_origin_cz;
                    if ((unsigned)map_y >= (unsigned)CHUNK_HEIGHT ||
                        (unsigned)ci >= (unsigned)GRID_W ||
                        (unsigned)cj >= (unsigned)GRID_W) {
                        break;
                    }
                    if (ci != cache_i || cj != cache_j) {
                        cache_i = ci;
                        cache_j = cj;
                        chunk = g_chunk_grid[cj][ci];
                    }
                    block = chunk != NULL && chunk->loaded
                                ? chunk->blocks[((map_y << 8) |
                                                 ((map_z & CHUNK_MASK)
                                                  << CHUNK_BITS) |
                                                 (map_x & CHUNK_MASK))]
                                : BLOCK_AIR;
                }
                if (block != BLOCK_AIR) {
                    break;
                }
            }
            {
                const uint16_t steps = (uint16_t)(
                    step < g_max_steps ? step + 1 : g_max_steps);
                ++g_perf_stats.rays;
                g_perf_stats.total_steps += steps;
                if (steps > g_perf_stats.max_steps)
                    g_perf_stats.max_steps = steps;
                if (fog_terminated) ++g_perf_stats.fog_terminated_rays;
                if (block != BLOCK_AIR) ++g_perf_stats.solid_hit_rays;
            }
            if (block == BLOCK_AIR) {
                const float inv_len = 1.0F / ray_len;
                out[px] = sky_pixel(dir_x * inv_len, dir_y * inv_len,
                                    dir_z * inv_len);
                depth_out[px] = 0xFFFFu;
            } else {
                const float travel =
                    (float)travel_q12 * (1.0F / (float)RAY_Q_ONE);
                const float hit_x = cam_x + dir_x * travel;
                const float hit_y = cam_y + dir_y * travel;
                const float hit_z = cam_z + dir_z * travel;
                const float distance = travel * ray_len;
                float uu;
                float vv;
                int fixed;
                if (face == 0) {
                    uu = hit_z;
                    vv = hit_y;
                } else if (face == 1) {
                    uu = hit_x;
                    vv = hit_z;
                } else {
                    uu = hit_x;
                    vv = hit_y;
                }
                out[px] = shade_block(block, face, sign, uu, vv, distance,
                                      now_ms);
                fixed = (int)(distance * 16.0F);
                depth_out[px] =
                    fixed > 0xFFFE ? 0xFFFE : (uint16_t)fixed;
            }
            if (ray_pixel_step != 1) {
                const uint16_t color = out[px];
                const uint16_t depth = depth_out[px];
                const int last_x = px + ray_pixel_step < g_scene_w
                                       ? px + ray_pixel_step
                                       : g_scene_w;
                const int last_y = py + ray_pixel_step < g_scene_h
                                       ? py + ray_pixel_step
                                       : g_scene_h;
                int fill_y;
                for (fill_y = py; fill_y < last_y; ++fill_y) {
                    uint16_t *fill_color =
                        &g_scene[fill_y * g_scene_w + px];
                    uint16_t *fill_depth =
                        &g_depth[fill_y * g_scene_w + px];
                    int fill_x;
                    for (fill_x = px; fill_x < last_x; ++fill_x) {
                        fill_color[fill_x - px] = color;
                        fill_depth[fill_x - px] = depth;
                    }
                }
            }
        }
    }
}

static void render_entities(const camera_t *cam) {
    int index;
    for (index = 0; index < MAX_MOBS; ++index) {
        const mob_t *mob = &g_mobs[index];
        float min_x;
        float min_y;
        float min_z;
        float max_x;
        float max_y;
        float max_z;
        float corners[8][3];
        float bbox[4] = {1.0e9F, 1.0e9F, -1.0e9F, -1.0e9F};
        int visible = 0;
        int corner;
        int px0;
        int px1;
        int py0;
        int py1;
        int px;
        int py;
        float mob_near = 0.0F;
        if (!mob->alive) {
            continue;
        }
        min_x = mob->x - MOB_HALF;
        min_y = mob->y;
        min_z = mob->z - MOB_HALF;
        max_x = mob->x + MOB_HALF;
        max_y = mob->y + MOB_HEIGHT;
        max_z = mob->z + MOB_HALF;
        corners[0][0] = min_x;
        corners[0][1] = min_y;
        corners[0][2] = min_z;
        corners[1][0] = max_x;
        corners[1][1] = min_y;
        corners[1][2] = min_z;
        corners[2][0] = min_x;
        corners[2][1] = max_y;
        corners[2][2] = min_z;
        corners[3][0] = max_x;
        corners[3][1] = max_y;
        corners[3][2] = min_z;
        corners[4][0] = min_x;
        corners[4][1] = min_y;
        corners[4][2] = max_z;
        corners[5][0] = max_x;
        corners[5][1] = min_y;
        corners[5][2] = max_z;
        corners[6][0] = min_x;
        corners[6][1] = max_y;
        corners[6][2] = max_z;
        corners[7][0] = max_x;
        corners[7][1] = max_y;
        corners[7][2] = max_z;
        for (corner = 0; corner < 8; ++corner) {
            float sx;
            float sy;
            float depth;
            if (!camera_project(cam, corners[corner][0], corners[corner][1],
                                corners[corner][2], &sx, &sy, &depth)) {
                continue;
            }
            visible = 1;
            if (sx < bbox[0]) {
                bbox[0] = sx;
            }
            if (sy < bbox[1]) {
                bbox[1] = sy;
            }
            if (sx > bbox[2]) {
                bbox[2] = sx;
            }
            if (sy > bbox[3]) {
                bbox[3] = sy;
            }
        }
        if (!visible) {
            continue;
        }
        {
            const float dx = mob->x - cam->cam_x;
            const float dy = mob->y + MOB_HEIGHT * 0.5F - cam->cam_y;
            const float dz = mob->z - cam->cam_z;
            mob_near = rc_sqrt(dx * dx + dy * dy + dz * dz) - 0.9F;
            if (mob_near < 0.0F) {
                mob_near = 0.0F;
            }
        }
        px0 = (int)bbox[0];
        py0 = (int)bbox[1];
        px1 = (int)bbox[2] + 1;
        py1 = (int)bbox[3] + 1;
        if (px0 < 0) {
            px0 = 0;
        }
        if (py0 < 0) {
            py0 = 0;
        }
        if (px1 > g_scene_w - 1) {
            px1 = g_scene_w - 1;
        }
        if (py1 > g_scene_h - 1) {
            py1 = g_scene_h - 1;
        }
        for (py = py0; py <= py1; ++py) {
            for (px = px0; px <= px1; ++px) {
                if ((int)(mob_near * 16.0F) >=
                    (int)g_depth[py * g_scene_w + px]) {
                    continue;
                }
                const float ndc_x = g_ndc_x[px];
                const float ndc_y = g_ndc_y[py];
                const float dir_x = cam->fx + cam->rx * ndc_x +
                                    cam->ux * ndc_y;
                const float dir_y = cam->fy + cam->uy * ndc_y;
                const float dir_z = cam->fz + cam->rz * ndc_x +
                                    cam->uz * ndc_y;
                const float ray_len =
                    (float)g_ray_len_q12[py * g_scene_w + px] *
                    (1.0F / 4096.0F);
                float t_min = -1.0e30F;
                float t_max = 1.0e30F;
                int face = 0;
                int sign = 1;
                float distance;
                int ok = 1;
                if (dir_x != 0.0F) {
                    const float inv = 1.0F / dir_x;
                    float t1 = (min_x - cam->cam_x) * inv;
                    float t2 = (max_x - cam->cam_x) * inv;
                    if (t1 > t2) {
                        const float swap = t1;
                        t1 = t2;
                        t2 = swap;
                    }
                    if (t1 > t_min) {
                        t_min = t1;
                        face = 0;
                        sign = dir_x > 0.0F ? -1 : 1;
                    }
                    if (t2 < t_max) {
                        t_max = t2;
                    }
                } else if (cam->cam_x < min_x || cam->cam_x > max_x) {
                    ok = 0;
                }
                if (ok && dir_y != 0.0F) {
                    const float inv = 1.0F / dir_y;
                    float t1 = (min_y - cam->cam_y) * inv;
                    float t2 = (max_y - cam->cam_y) * inv;
                    if (t1 > t2) {
                        const float swap = t1;
                        t1 = t2;
                        t2 = swap;
                    }
                    if (t1 > t_min) {
                        t_min = t1;
                        face = 1;
                        sign = dir_y > 0.0F ? -1 : 1;
                    }
                    if (t2 < t_max) {
                        t_max = t2;
                    }
                } else if (ok &&
                           (cam->cam_y < min_y || cam->cam_y > max_y)) {
                    ok = 0;
                }
                if (ok && dir_z != 0.0F) {
                    const float inv = 1.0F / dir_z;
                    float t1 = (min_z - cam->cam_z) * inv;
                    float t2 = (max_z - cam->cam_z) * inv;
                    if (t1 > t2) {
                        const float swap = t1;
                        t1 = t2;
                        t2 = swap;
                    }
                    if (t1 > t_min) {
                        t_min = t1;
                        face = 2;
                        sign = dir_z > 0.0F ? -1 : 1;
                    }
                    if (t2 < t_max) {
                        t_max = t2;
                    }
                } else if (ok &&
                           (cam->cam_z < min_z || cam->cam_z > max_z)) {
                    ok = 0;
                }
                if (!ok || t_max < t_min || t_max < 0.0F) {
                    continue;
                }
                if (t_min < 0.0F) {
                    t_min = 0.0F;
                }
                distance = t_min * ray_len;
                if ((int)(distance * 16.0F) >=
                    (int)g_depth[py * g_scene_w + px]) {
                    continue;
                }
                {
                    const int shade =
                        kFaceShade[face * 2 + (sign > 0 ? 0 : 1)];
                    const int kind = mob->kind < MOB_KIND_COUNT
                                         ? mob->kind
                                         : MOB_SLIME;
                    int r = (kMobR[kind] * shade) >> 8;
                    int g = (kMobG[kind] * shade) >> 8;
                    int b = (kMobB[kind] * shade) >> 8;
                    if (mob->hurt > 0.0F) {
                        r = 255;
                        g = (g * 2) / 5;
                        b = (b * 2) / 5;
                    }
                    if (face != 1) {
                        const float hx = cam->cam_x + dir_x * t_min;
                        const float hy = cam->cam_y + dir_y * t_min;
                        const float hz = cam->cam_z + dir_z * t_min;
                        const float hu = face == 0 ? hz : hx;
                        const float hv = hy;
                        const float fu = hu - rc_floor(hu);
                        const float fv = hv - rc_floor(hv);
                        if (fv > 0.5F && fv < 0.85F &&
                            ((fu > 0.18F && fu < 0.38F) ||
                             (fu > 0.62F && fu < 0.82F))) {
                            r = 28;
                            g = 40;
                            b = 32;
                        }
                    }
                    g_scene[py * g_scene_w + px] = RGB565(r, g, b);
                }
            }
        }
    }

    for (index = 0; index < MAX_PARTICLES; ++index) {
        const particle_t *particle = &g_particles[index];
        float sx;
        float sy;
        float depth;
        int px;
        int py;
        int size;
        uint16_t color;
        if (particle->life <= 0.0F) {
            continue;
        }
        if (!camera_project(cam, particle->x, particle->y, particle->z, &sx,
                            &sy, &depth)) {
            continue;
        }
        px = (int)sx;
        py = (int)sy;
        if ((unsigned)px >= (unsigned)g_scene_w ||
            (unsigned)py >= (unsigned)g_scene_h) {
            continue;
        }
        if ((int)(depth * 16.0F) >= (int)g_depth[py * g_scene_w + px]) {
            continue;
        }
        {
            const int block = particle->block;
            const int bright =
                particle->life > 0.6F ? 255 : (particle->life > 0.3F ? 215 : 170);
            color = RGB565((kBaseR[block] * bright) >> 8,
                           (kBaseG[block] * bright) >> 8,
                           (kBaseB[block] * bright) >> 8);
        }
        size = particle->life > 0.3F ? 1 : 0;
        g_scene[py * g_scene_w + px] = color;
        if (size == 1) {
            if (px + 1 < g_scene_w) {
                g_scene[py * g_scene_w + px + 1] = color;
            }
            if (py + 1 < g_scene_h) {
                g_scene[(py + 1) * g_scene_w + px] = color;
                if (px + 1 < g_scene_w) {
                    g_scene[(py + 1) * g_scene_w + px + 1] = color;
                }
            }
        }
    }
}

static void darken_pixel(int x, int y) {
    if ((unsigned)x >= (unsigned)g_scene_w ||
        (unsigned)y >= (unsigned)g_scene_h) {
        return;
    }
    g_scene[y * g_scene_w + x] =
        (uint16_t)((g_scene[y * g_scene_w + x] >> 1) & 0x7BEFu);
}

static void darken_line(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps;
    int index;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    steps = dx > dy ? dx : dy;
    if (steps == 0) {
        darken_pixel(x0, y0);
        return;
    }
    for (index = 0; index <= steps; ++index) {
        darken_pixel(x0 + (x1 - x0) * index / steps,
                     y0 + (y1 - y0) * index / steps);
    }
}

static void render_highlight(const camera_t *cam, const ray_hit_t *target,
                             float progress) {
    float corners[8][3];
    float screen[8][2];
    int visible[8];
    int index;
    static const uint8_t edges[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
        {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };
    if (target == NULL || !target->hit) {
        return;
    }
    corners[0][0] = (float)target->x;
    corners[0][1] = (float)target->y;
    corners[0][2] = (float)target->z;
    corners[1][0] = (float)target->x + 1.0F;
    corners[1][1] = (float)target->y;
    corners[1][2] = (float)target->z;
    corners[2][0] = (float)target->x;
    corners[2][1] = (float)target->y + 1.0F;
    corners[2][2] = (float)target->z;
    corners[3][0] = (float)target->x + 1.0F;
    corners[3][1] = (float)target->y + 1.0F;
    corners[3][2] = (float)target->z;
    corners[4][0] = (float)target->x;
    corners[4][1] = (float)target->y;
    corners[4][2] = (float)target->z + 1.0F;
    corners[5][0] = (float)target->x + 1.0F;
    corners[5][1] = (float)target->y;
    corners[5][2] = (float)target->z + 1.0F;
    corners[6][0] = (float)target->x;
    corners[6][1] = (float)target->y + 1.0F;
    corners[6][2] = (float)target->z + 1.0F;
    corners[7][0] = (float)target->x + 1.0F;
    corners[7][1] = (float)target->y + 1.0F;
    corners[7][2] = (float)target->z + 1.0F;
    for (index = 0; index < 8; ++index) {
        float sx;
        float sy;
        float depth;
        visible[index] = camera_project(cam, corners[index][0],
                                        corners[index][1],
                                        corners[index][2], &sx, &sy, &depth);
        screen[index][0] = sx;
        screen[index][1] = sy;
    }
    for (index = 0; index < 12; ++index) {
        const int a = edges[index][0];
        const int b = edges[index][1];
        if (visible[a] && visible[b]) {
            darken_line((int)screen[a][0], (int)screen[a][1],
                        (int)screen[b][0], (int)screen[b][1]);
        }
    }
    if (progress > 0.01F) {
        const int stage = 1 + (int)(progress * 9.0F);
        int crack;
        for (crack = 0; crack < stage; ++crack) {
            const uint32_t seed =
                hash3(target->x, target->y, target->z + crack);
            const float f = (float)((seed >> 4) & 15u) / 16.0F;
            const float g = (float)((seed >> 12) & 15u) / 16.0F;
            float a[3];
            float b[3];
            float sa[2];
            float sb[2];
            float depth;
            int i;
            for (i = 0; i < 3; ++i) {
                a[i] = (float)target->x;
                b[i] = (float)target->x + 1.0F;
            }
            if (target->face == 0) {
                const float plane = (target->x - target->place_x) > 0
                                        ? (float)target->x
                                        : (float)target->x + 1.0F;
                a[0] = plane;
                b[0] = plane;
                a[1] = (float)target->y + f;
                b[1] = (float)target->y + g;
                a[2] = (float)target->z + g;
                b[2] = (float)target->z + f;
            } else if (target->face == 1) {
                const float plane = (target->y - target->place_y) > 0
                                        ? (float)target->y
                                        : (float)target->y + 1.0F;
                a[1] = plane;
                b[1] = plane;
                a[0] = (float)target->x + f;
                b[0] = (float)target->x + g;
                a[2] = (float)target->z + g;
                b[2] = (float)target->z + f;
            } else {
                const float plane = (target->z - target->place_z) > 0
                                        ? (float)target->z
                                        : (float)target->z + 1.0F;
                a[2] = plane;
                b[2] = plane;
                a[0] = (float)target->x + f;
                b[0] = (float)target->x + g;
                a[1] = (float)target->y + g;
                b[1] = (float)target->y + f;
            }
            if (camera_project(cam, a[0], a[1], a[2], &sa[0], &sa[1],
                               &depth) &&
                camera_project(cam, b[0], b[1], b[2], &sb[0], &sb[1],
                               &depth)) {
                darken_line((int)sa[0], (int)sa[1], (int)sb[0], (int)sb[1]);
            }
        }
    }
}

void render_target(uint16_t *pixels, uint32_t stride_pixels) {
    int row;
    g_scene = pixels;
    for (row = 0; row < g_scene_h; ++row) {
        g_row_ptr[row] = pixels + (size_t)row * stride_pixels;
    }
}

int render_3d(uint16_t *pixels, uint32_t stride_pixels,
              const player_t *player, uint32_t now_ms,
              const ray_hit_t *target, float mine_progress) {
    camera_t cam;
    if (pixels == NULL || stride_pixels < (uint32_t)g_scene_w ||
        g_scene_w > SCENE_MAX_W || g_scene_h > SCENE_MAX_H) {
        return 0;
    }
    render_ensure_textures();
    render_target(pixels, stride_pixels);
    camera_build(&cam, player);
    render_scene(&cam, now_ms);
    render_entities(&cam);
    render_highlight(&cam, target, mine_progress);
    return 1;
}

/* --- HUD primitives ----------------------------------------------------- */

static void hud_pixel(int x, int y, uint16_t color) {
    int target_x;
    int target_y;
    if (x < g_layout.view_x || x >= g_layout.view_x + g_layout.view_w ||
        y < g_layout.view_y || y >= g_layout.view_y + g_layout.view_h) {
        return;
    }
    target_x = (x - g_layout.view_x) * g_scene_w / g_layout.view_w;
    target_y = (y - g_layout.view_y) * g_scene_h / g_layout.view_h;
    frame_row(target_y)[target_x] = color;
}

static void hud_rect(int x, int y, int width, int height, uint16_t color) {
    int row;
    for (row = y; row < y + height; ++row) {
        int column;
        if (row < g_layout.view_y ||
            row >= g_layout.view_y + g_layout.view_h) {
            continue;
        }
        for (column = x; column < x + width; ++column) {
            hud_pixel(column, row, color);
        }
    }
}

static void hud_rect_outline(int x, int y, int width, int height, int thickness,
                             uint16_t color) {
    hud_rect(x, y, width, thickness, color);
    hud_rect(x, y + height - thickness, width, thickness, color);
    hud_rect(x, y, thickness, height, color);
    hud_rect(x + width - thickness, y, thickness, height, color);
}

static void hud_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps;
    int index;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    steps = dx > dy ? dx : dy;
    if (steps == 0) {
        hud_pixel(x0, y0, color);
        return;
    }
    for (index = 0; index <= steps; ++index) {
        const int x = x0 + (x1 - x0) * index / steps;
        const int y = y0 + (y1 - y0) * index / steps;
        hud_pixel(x, y, color);
    }
}

static void hud_circle(int cx, int cy, int radius, uint16_t color) {
    int dy;
    for (dy = -radius; dy <= radius; ++dy) {
        int dx;
        for (dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                hud_pixel(cx + dx, cy + dy, color);
            }
        }
    }
}

static void hud_circle_outline(int cx, int cy, int radius, uint16_t color) {
    int dy;
    for (dy = -radius; dy <= radius; ++dy) {
        int dx;
        for (dx = -radius; dx <= radius; ++dx) {
            const int d = dx * dx + dy * dy;
            if (d <= radius * radius && d >= (radius - 2) * (radius - 2)) {
                hud_pixel(cx + dx, cy + dy, color);
            }
        }
    }
}

typedef struct {
    char ch;
    uint8_t rows[7];
} glyph_t;

typedef struct {
    char ch;
    uint8_t rows[5];
} compact_glyph_t;

static const glyph_t kGlyphs[] = {
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'.', {0, 0, 0, 0, 0, 12, 12}},
    {':', {0, 12, 12, 0, 12, 12, 0}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},
    {'/', {1, 1, 2, 4, 8, 16, 16}},
    {'+', {0, 4, 4, 31, 4, 4, 0}},
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}},
    {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},
    {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},
    {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {' ', {0, 0, 0, 0, 0, 0, 0}},
};

/* At 4X, a logical 2-pixel font stroke aliases into half a source pixel.
 * Draw this compact font on the source-pixel grid so labels stay readable
 * after nearest-neighbor scanout from the 74x60 Performance surface. */
static const compact_glyph_t kCompactGlyphs[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}},
    {'2', {7, 1, 7, 4, 7}}, {'3', {7, 1, 7, 1, 7}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}},
    {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 1, 1}},
    {'8', {7, 5, 7, 5, 7}}, {'9', {7, 5, 7, 1, 7}},
    {'.', {0, 0, 0, 0, 2}}, {':', {0, 2, 0, 2, 0}},
    {'-', {0, 0, 7, 0, 0}}, {'/', {1, 1, 2, 4, 4}},
    {'+', {0, 2, 7, 2, 0}}, {'A', {7, 5, 7, 5, 5}},
    {'B', {6, 5, 6, 5, 6}}, {'C', {7, 4, 4, 4, 7}},
    {'D', {6, 5, 5, 5, 6}}, {'E', {7, 4, 6, 4, 7}},
    {'F', {7, 4, 6, 4, 4}}, {'G', {7, 4, 5, 5, 7}},
    {'H', {5, 5, 7, 5, 5}}, {'I', {7, 2, 2, 2, 7}},
    {'J', {1, 1, 1, 5, 7}}, {'K', {5, 5, 6, 5, 5}},
    {'L', {4, 4, 4, 4, 7}}, {'M', {5, 7, 7, 5, 5}},
    {'N', {5, 7, 7, 7, 5}}, {'O', {7, 5, 5, 5, 7}},
    {'P', {7, 5, 7, 4, 4}}, {'Q', {7, 5, 5, 7, 1}},
    {'R', {6, 5, 6, 5, 5}}, {'S', {7, 4, 7, 1, 7}},
    {'T', {7, 2, 2, 2, 2}}, {'U', {5, 5, 5, 5, 7}},
    {'V', {5, 5, 5, 5, 2}}, {'W', {5, 5, 7, 7, 5}},
    {'X', {5, 5, 2, 5, 5}}, {'Y', {5, 5, 2, 2, 2}},
    {'Z', {7, 1, 2, 4, 7}}, {' ', {0, 0, 0, 0, 0}},
};

static const uint8_t *glyph_for(char ch) {
    unsigned index;
    for (index = 0; index < sizeof(kGlyphs) / sizeof(kGlyphs[0]); ++index) {
        if (kGlyphs[index].ch == ch) {
            return kGlyphs[index].rows;
        }
    }
    return kGlyphs[sizeof(kGlyphs) / sizeof(kGlyphs[0]) - 1].rows;
}

static const uint8_t *compact_glyph_for(char ch) {
    unsigned index;
    for (index = 0;
         index < sizeof(kCompactGlyphs) / sizeof(kCompactGlyphs[0]);
         ++index) {
        if (kCompactGlyphs[index].ch == ch) {
            return kCompactGlyphs[index].rows;
        }
    }
    return kCompactGlyphs[
        sizeof(kCompactGlyphs) / sizeof(kCompactGlyphs[0]) - 1].rows;
}

static int text_width(const char *text, int scale) {
    int length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    if (g_scale == QUALITY_PERFORMANCE) {
        return length * 4 * QUALITY_PERFORMANCE;
    }
    return length * 6 * scale;
}

static void hud_text(int x, int y, const char *text, uint16_t color,
                     int scale) {
    int cursor = x;
    if (g_scale == QUALITY_PERFORMANCE) {
        const int pixel = QUALITY_PERFORMANCE;
        const int aligned_y = g_layout.view_y +
            ((y - g_layout.view_y + pixel / 2) / pixel) * pixel;
        cursor = g_layout.view_x +
            ((x - g_layout.view_x + pixel / 2) / pixel) * pixel;
        while (*text != '\0') {
            const uint8_t *rows = compact_glyph_for(*text++);
            int row;
            for (row = 0; row < 5; ++row) {
                int column;
                for (column = 0; column < 3; ++column) {
                    if ((rows[row] & (4u >> column)) != 0u) {
                        hud_rect(cursor + column * pixel,
                                 aligned_y + row * pixel,
                                 pixel, pixel, color);
                    }
                }
            }
            cursor += 4 * pixel;
        }
        return;
    }
    while (*text != '\0') {
        const uint8_t *rows = glyph_for(*text++);
        int row;
        for (row = 0; row < 7; ++row) {
            int column;
            for (column = 0; column < 5; ++column) {
                if ((rows[row] & (16u >> column)) == 0u) {
                    continue;
                }
                if (scale == 1) {
                    hud_pixel(cursor + column, y + row, color);
                } else {
                    hud_rect(cursor + column * scale, y + row * scale, scale,
                             scale, color);
                }
            }
        }
        cursor += 6 * scale;
    }
}

static void hud_text_centered(int y, const char *text, uint16_t color,
                              int scale) {
    hud_text(g_layout.view_x +
                 (g_layout.view_w - text_width(text, scale)) / 2,
             y, text, color, scale);
}

static void draw_crosshair(float swing) {
    const int cx = g_layout.view_x + g_layout.view_w / 2;
    const int cy = g_layout.view_y + g_layout.view_h / 2;
    hud_line(cx - 8, cy, cx - 2, cy, COL_SHADOW);
    hud_line(cx + 2, cy, cx + 8, cy, COL_SHADOW);
    hud_line(cx, cy - 8, cx, cy - 2, COL_SHADOW);
    hud_line(cx, cy + 2, cx, cy + 8, COL_SHADOW);
    hud_line(cx - 7, cy, cx - 3, cy, COL_WHITE);
    hud_line(cx + 3, cy, cx + 7, cy, COL_WHITE);
    hud_line(cx, cy - 7, cx, cy - 3, COL_WHITE);
    hud_line(cx, cy + 3, cx, cy + 7, COL_WHITE);
    hud_pixel(cx, cy, COL_WHITE);
    if (swing > 0.0F) {
        hud_line(cx - 10, cy - 10, cx - 5, cy - 5, COL_DANGER);
        hud_line(cx + 5, cy - 5, cx + 10, cy - 10, COL_DANGER);
        hud_line(cx - 10, cy + 10, cx - 5, cy + 5, COL_DANGER);
        hud_line(cx + 5, cy + 5, cx + 10, cy + 10, COL_DANGER);
    }
}

static void draw_item_icon(int x, int y, int size, uint8_t item);
static void draw_count(int x, int y, uint8_t count);

static void draw_hotbar(const hud_state_t *hud) {
    const int slot_size = g_layout.hotbar_slot;
    const int padding = 2 * g_layout.ui_scale;
    const int icon = slot_size - 2 * padding;
    int slot;
    for (slot = 0; slot < HOTBAR_SLOTS; ++slot) {
        const int x = g_layout.hotbar_x + slot * slot_size;
        const int y = g_layout.hotbar_y;
        const item_stack_t *value = &g_inventory[slot];
        hud_rect(x, y, slot_size, slot_size, COL_PANEL);
        if (value->item != BLOCK_AIR) {
            draw_item_icon(x + padding, y + padding, icon, value->item);
            hud_rect(x + padding, y + 7 * g_layout.ui_scale, icon,
                     g_layout.ui_scale, COL_SHADOW);
            draw_count(x + slot_size - padding, y + slot_size - 1,
                       value->count);
        }
        if (slot == hud->hotbar_selected) {
            hud_rect_outline(x, y - 2 * g_layout.ui_scale, slot_size,
                             slot_size + 2 * g_layout.ui_scale,
                             2 * g_layout.ui_scale, COL_SELECT);
        } else {
            hud_rect_outline(x, y, slot_size, slot_size, g_layout.ui_scale,
                             COL_SHADOW);
        }
    }
}

static void draw_button(int cx, int cy, int radius, uint16_t fill,
                        uint8_t active) {
    hud_circle(cx, cy, radius, COL_SHADOW);
    hud_circle(cx, cy, radius - 1, active ? COL_GREEN : fill);
}

static void draw_arrow(int cx, int cy, int direction) {
    const int scale = g_layout.ui_scale;
    int index;
    for (index = 0; index < 6 * scale; ++index) {
        const int half = direction > 0 ? index : 6 * scale - 1 - index;
        hud_line(cx - half, cy - 2 * scale + index, cx + half,
                 cy - 2 * scale + index, COL_WHITE);
    }
}

static void draw_pickaxe(int cx, int cy) {
    const int s = g_layout.ui_scale;
    hud_line(cx - 5 * s, cy + 6 * s, cx + 2 * s, cy - 2 * s, COL_WHITE);
    hud_line(cx - 8 * s, cy - 3 * s, cx + 6 * s, cy - 3 * s, COL_WHITE);
    hud_line(cx - 8 * s, cy - 3 * s, cx - 6 * s, cy - 6 * s, COL_WHITE);
    hud_line(cx + 6 * s, cy - 3 * s, cx + 4 * s, cy - 6 * s, COL_WHITE);
}

static void draw_sword(int cx, int cy) {
    const int s = g_layout.ui_scale;
    hud_line(cx - 4 * s, cy + 6 * s, cx + 4 * s, cy - 4 * s, COL_WHITE);
    hud_line(cx - 6 * s, cy + 2 * s, cx - s, cy + 7 * s, COL_WHITE);
    hud_rect(cx + 5 * s, cy - 6 * s, s, 2 * s, COL_WHITE);
}

/* --- inventory screen --------------------------------------------------- */

void render_inventory_set_mode(int table) {
    const int slot = g_layout.hotbar_slot;
    g_inv_mode = table ? 3 : 2;
    g_inv_layout.main_y = g_inv_layout.panel_y +
                          (g_inv_mode == 3 ? 112 : 96) * g_layout.ui_scale;
    g_inv_layout.inv_hotbar_y =
        g_inv_layout.main_y + 3 * slot + 8 * g_layout.ui_scale;
    g_inv_layout.result_x =
        g_inv_layout.craft_x + g_inv_mode * slot + 16 * g_layout.ui_scale;
    g_inv_layout.result_y = g_inv_layout.craft_y +
                            (g_inv_mode * slot) / 2 - slot / 2;
}

int render_inventory_slot(int hit) {
    if (hit >= INV_HIT_CRAFT && hit < INV_HIT_RESULT) {
        return hit - INV_HIT_CRAFT;
    }
    if (hit >= INV_HIT_MAIN && hit < INV_HIT_MAIN + INV_MAIN_SLOTS) {
        return HOTBAR_SLOTS + (hit - INV_HIT_MAIN);
    }
    if (hit >= INV_HIT_HOTBAR && hit < INV_HIT_HOTBAR + HOTBAR_SLOTS) {
        return hit - INV_HIT_HOTBAR;
    }
    return -1;
}

int render_inventory_hit(int x, int y) {
    const int slot = g_layout.hotbar_slot;
    int column;
    int row;
    if (x >= g_inv_layout.close_x - g_inv_layout.close_r &&
        x < g_inv_layout.close_x + g_inv_layout.close_r &&
        y >= g_inv_layout.close_y - g_inv_layout.close_r &&
        y < g_inv_layout.close_y + g_inv_layout.close_r) {
        return INV_HIT_CLOSE;
    }
    for (row = 0; row < g_inv_mode; ++row) {
        for (column = 0; column < g_inv_mode; ++column) {
            const int sx = g_inv_layout.craft_x + column * slot;
            const int sy = g_inv_layout.craft_y + row * slot;
            if (x >= sx && x < sx + slot && y >= sy && y < sy + slot) {
                return INV_HIT_CRAFT + row * g_inv_mode + column;
            }
        }
    }
    if (x >= g_inv_layout.result_x &&
        x < g_inv_layout.result_x + slot &&
        y >= g_inv_layout.result_y &&
        y < g_inv_layout.result_y + slot) {
        return INV_HIT_RESULT;
    }
    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 9; ++column) {
            const int sx = g_inv_layout.main_x + column * slot;
            const int sy = g_inv_layout.main_y + row * slot;
            if (x >= sx && x < sx + slot && y >= sy && y < sy + slot) {
                return INV_HIT_MAIN + row * 9 + column;
            }
        }
    }
    for (column = 0; column < 9; ++column) {
        const int sx = g_inv_layout.inv_hotbar_x + column * slot;
        const int sy = g_inv_layout.inv_hotbar_y;
        if (x >= sx && x < sx + slot && y >= sy && y < sy + slot) {
            return INV_HIT_HOTBAR + column;
        }
    }
    return INV_HIT_NONE;
}

static void dim_region(int x, int y, int width, int height) {
    int row;
    for (row = y; row < y + height; ++row) {
        int column;
        for (column = x; column < x + width; ++column) {
            if (x < 0 || row < g_layout.view_y ||
                row >= g_layout.view_y + g_layout.view_h ||
                column < g_layout.view_x ||
                column >= g_layout.view_x + g_layout.view_w) {
                continue;
            }
            if (((column ^ row) & 1) == 0 &&
                ((column - g_layout.view_x) * g_scene_w) %
                        g_layout.view_w ==
                    0 &&
                ((row - g_layout.view_y) * g_scene_h) % g_layout.view_h ==
                    0) {
                const int target_x =
                    (column - g_layout.view_x) * g_scene_w / g_layout.view_w;
                const int target_y =
                    (row - g_layout.view_y) * g_scene_h / g_layout.view_h;
                uint16_t *pixel = &frame_row(target_y)[target_x];
                *pixel = (uint16_t)((*pixel >> 1) & 0x7BEFu);
            }
        }
    }
}

static void draw_tool_icon(int x, int y, int size, uint8_t item) {
    const int wooden = item <= ITEM_WOOD_SWORD;
    const int kind = (int)(item - ITEM_FIRST_TOOL) % 4;
    const uint16_t handle = RGB565(118, 84, 48);
    const uint16_t handle_light = RGB565(146, 106, 62);
    const uint16_t head =
        wooden ? RGB565(174, 134, 84) : RGB565(158, 158, 158);
    const uint16_t head_light =
        wooden ? RGB565(198, 158, 104) : RGB565(188, 188, 188);
    const int u = size;
#define TOOL_X(a) (x + (u * (a)) / 20)
#define TOOL_Y(a) (y + (u * (a)) / 20)
    /* Handle: lower-left to the middle. */
    hud_line(TOOL_X(6), TOOL_Y(17), TOOL_X(12), TOOL_Y(8), handle);
    hud_line(TOOL_X(7), TOOL_Y(17), TOOL_X(13), TOOL_Y(8), handle_light);
    if (kind == 0) {
        /* Pickaxe: a wide head with two down-turned tips. */
        hud_line(TOOL_X(3), TOOL_Y(6), TOOL_X(16), TOOL_Y(6), head);
        hud_line(TOOL_X(3), TOOL_Y(7), TOOL_X(16), TOOL_Y(7), head_light);
        hud_line(TOOL_X(3), TOOL_Y(6), TOOL_X(2), TOOL_Y(10), head);
        hud_line(TOOL_X(16), TOOL_Y(6), TOOL_X(17), TOOL_Y(10), head);
    } else if (kind == 1) {
        /* Axe: a blade on the upper right. */
        hud_rect(TOOL_X(10), TOOL_Y(3), (u * 7) / 20, (u * 7) / 20, head);
        hud_rect(TOOL_X(10), TOOL_Y(3), (u * 7) / 20, (u * 2) / 20,
                 head_light);
    } else if (kind == 2) {
        /* Shovel: a spade at the top. */
        hud_rect(TOOL_X(9), TOOL_Y(3), (u * 6) / 20, (u * 6) / 20, head);
        hud_rect(TOOL_X(9), TOOL_Y(3), (u * 6) / 20, (u * 2) / 20,
                 head_light);
    } else {
        /* Sword: a long blade, guard and pommel. */
        hud_line(TOOL_X(6), TOOL_Y(15), TOOL_X(16), TOOL_Y(3), head);
        hud_line(TOOL_X(7), TOOL_Y(15), TOOL_X(17), TOOL_Y(3), head_light);
        hud_line(TOOL_X(4), TOOL_Y(11), TOOL_X(10), TOOL_Y(16), handle);
        hud_pixel(TOOL_X(4), TOOL_Y(17), handle_light);
    }
#undef TOOL_X
#undef TOOL_Y
}

static void draw_item_icon(int x, int y, int size, uint8_t item) {
    int ix;
    int iy;
    if (game_item_is_tool(item)) {
        draw_tool_icon(x, y, size, item);
        return;
    }
    if (item == BLOCK_AIR || item >= BLOCK_TYPE_COUNT) {
        return;
    }
    for (iy = 0; iy < size; ++iy) {
        const int kind = iy < size / 3 ? BLOCK_TEXTURE_TOP : BLOCK_TEXTURE_SIDE;
        const int sy = iy < size / 3
                           ? iy * 16 / (size / 3)
                           : (iy - size / 3) * 16 / (size - size / 3);
        const int row = sy << 4;
        for (ix = 0; ix < size; ++ix) {
            const int sx = ix * 16 / size;
            hud_pixel(x + ix, y + iy, g_tex[item][kind][row | sx]);
        }
    }
}

static void draw_count(int x, int y, uint8_t count) {
    char text[4];
    if (count <= 1) {
        return;
    }
    text[0] = (char)('0' + (count / 10) % 10);
    text[1] = (char)('0' + count % 10);
    text[2] = '\0';
    if (count >= 10) {
        hud_text(x - 13, y - 6, text, COL_SHADOW, 1);
        hud_text(x - 14, y - 7, text, COL_WHITE, 1);
    } else {
        hud_text(x - 9, y - 6, text + 1, COL_SHADOW, 1);
        hud_text(x - 10, y - 7, text + 1, COL_WHITE, 1);
    }
}

static void draw_slot(int x, int y, uint8_t item, uint8_t count,
                      uint8_t selected) {
    const int slot = g_layout.hotbar_slot;
    const int padding = 2 * g_layout.ui_scale;
    hud_rect(x, y, slot, slot, COL_PANEL);
    if (item != BLOCK_AIR) {
        draw_item_icon(x + padding, y + padding, slot - 2 * padding, item);
        draw_count(x + slot - padding, y + slot - 1, count);
    }
    hud_rect_outline(x, y, slot, slot,
                     (selected ? 2 : 1) * g_layout.ui_scale,
                     selected ? COL_SELECT : COL_SHADOW);
}

static void draw_inventory(const hud_state_t *hud) {
    const int ui_scale = g_layout.ui_scale;
    const int slot_size = g_layout.hotbar_slot;
    const int padding = 2 * ui_scale;
    int row;
    int column;
    char *out;
    char title[24];
    dim_region(g_layout.view_x, g_layout.view_y, g_layout.view_w,
               g_layout.view_h);
    hud_rect(g_inv_layout.panel_x, g_inv_layout.panel_y,
             g_inv_layout.panel_w, g_inv_layout.panel_h, COL_PANEL);
    hud_rect_outline(g_inv_layout.panel_x, g_inv_layout.panel_y,
                     g_inv_layout.panel_w, g_inv_layout.panel_h, 2 * ui_scale,
                     COL_PANEL_LIGHT);
    {
        const char *label = hud->craft_table ? "CRAFTING" : "INVENTORY";
        out = title;
        while (*label != '\0' && out < title + sizeof(title) - 1) {
            *out++ = *label++;
        }
        *out = '\0';
    }
    hud_text_centered(g_inv_layout.panel_y + 8 * ui_scale, title, COL_TEXT, 2);
    hud_text_centered(g_inv_layout.panel_y + 22 * ui_scale,
                      "TAP MOVE  HOLD SPLIT",
                      COL_MUTED, 1);

    /* Crafting grid, arrow and result. */
    for (row = 0; row < g_inv_mode; ++row) {
        for (column = 0; column < g_inv_mode; ++column) {
            const int x = g_inv_layout.craft_x + column * slot_size;
            const int y = g_inv_layout.craft_y + row * slot_size;
            const int index = row * g_inv_mode + column;
            const item_stack_t *slot =
                hud->craft_table ? &g_table_craft[index] : &g_craft[index];
            draw_slot(x, y, slot->item, slot->count, 0);
        }
    }
    {
        const int arrow_x = g_inv_layout.craft_x +
                            g_inv_mode * slot_size + 4 * ui_scale;
        const int arrow_y = g_inv_layout.craft_y +
                            (g_inv_mode * slot_size) / 2;
        const item_stack_t *result =
            hud->craft_table ? &g_table_result : &g_craft_result;
        hud_line(arrow_x, arrow_y, arrow_x + 10 * ui_scale, arrow_y,
                 COL_MUTED);
        hud_line(arrow_x + 6 * ui_scale, arrow_y - 4 * ui_scale,
                 arrow_x + 10 * ui_scale, arrow_y,
                 COL_MUTED);
        hud_line(arrow_x + 6 * ui_scale, arrow_y + 4 * ui_scale,
                 arrow_x + 10 * ui_scale, arrow_y,
                 COL_MUTED);
        draw_slot(g_inv_layout.result_x, g_inv_layout.result_y,
                  result->item, result->count, result->item != BLOCK_AIR);
    }

    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 9; ++column) {
            const int x = g_inv_layout.main_x + column * slot_size;
            const int y = g_inv_layout.main_y + row * slot_size;
            const item_stack_t *slot =
                &g_inventory[HOTBAR_SLOTS + row * 9 + column];
            draw_slot(x, y, slot->item, slot->count, 0);
        }
    }
    for (column = 0; column < 9; ++column) {
        const int x = g_inv_layout.inv_hotbar_x + column * slot_size;
        const item_stack_t *slot = &g_inventory[column];
        draw_slot(x, g_inv_layout.inv_hotbar_y, slot->item, slot->count,
                  (uint8_t)(column == hud->hotbar_selected));
    }
    hud_line(g_inv_layout.close_x - 7 * ui_scale,
             g_inv_layout.close_y - 7 * ui_scale,
             g_inv_layout.close_x + 7 * ui_scale,
             g_inv_layout.close_y + 7 * ui_scale, COL_SHADOW);
    hud_line(g_inv_layout.close_x - 7 * ui_scale,
             g_inv_layout.close_y + 7 * ui_scale,
             g_inv_layout.close_x + 7 * ui_scale,
             g_inv_layout.close_y - 7 * ui_scale, COL_SHADOW);
    hud_line(g_inv_layout.close_x - 6 * ui_scale,
             g_inv_layout.close_y - 6 * ui_scale,
             g_inv_layout.close_x + 6 * ui_scale,
             g_inv_layout.close_y + 6 * ui_scale, COL_TEXT);
    hud_line(g_inv_layout.close_x - 6 * ui_scale,
             g_inv_layout.close_y + 6 * ui_scale,
             g_inv_layout.close_x + 6 * ui_scale,
             g_inv_layout.close_y - 6 * ui_scale, COL_TEXT);

    if (hud->cursor_item != BLOCK_AIR && hud->cursor_count != 0) {
        const int x = hud->pointer_x - slot_size / 2;
        const int y = hud->pointer_y - slot_size - 6 * ui_scale;
        hud_rect(x, y, slot_size, slot_size, COL_PANEL);
        draw_item_icon(x + padding, y + padding, slot_size - 2 * padding,
                       hud->cursor_item);
        draw_count(x + slot_size - padding, y + slot_size - 1,
                   hud->cursor_count);
    }
}

static void draw_use_icon(int cx, int cy) {
    const int s = g_layout.ui_scale;
    /* A small crafting table: top board and two legs. */
    hud_rect(cx - 7 * s, cy - 5 * s, 15 * s, 3 * s, COL_WHITE);
    hud_rect(cx - 6 * s, cy - 2 * s, 3 * s, 7 * s, COL_WHITE);
    hud_rect(cx + 4 * s, cy - 2 * s, 3 * s, 7 * s, COL_WHITE);
}

static void draw_controls(const hud_state_t *hud) {
    const int s = g_layout.ui_scale;
    draw_button(g_layout.jump_x, g_layout.jump_y, g_layout.jump_r,
                COL_PANEL_LIGHT, hud->jump_held);
    draw_arrow(g_layout.jump_x, g_layout.jump_y - 3 * s, 1);

    /* Context action: mine, attack or use. */
    draw_button(g_layout.action_x, g_layout.action_y, g_layout.action_r,
                COL_PANEL_LIGHT,
                (uint8_t)(hud->action_held ||
                          hud->action_mode != 0));
    if (hud->action_mode == 1) {
        draw_sword(g_layout.action_x, g_layout.action_y);
    } else if (hud->action_mode == 2) {
        draw_use_icon(g_layout.action_x, g_layout.action_y);
    } else {
        draw_pickaxe(g_layout.action_x, g_layout.action_y);
    }

    draw_button(g_layout.place_x, g_layout.place_y, g_layout.place_r,
                COL_PANEL_LIGHT, 0);
    hud_rect(g_layout.place_x - 7 * s, g_layout.place_y - s, 15 * s, 3 * s,
             COL_WHITE);
    hud_rect(g_layout.place_x - s, g_layout.place_y - 7 * s, 3 * s, 15 * s,
             COL_WHITE);

    if (hud->flying) {
        draw_button(g_layout.down_x, g_layout.down_y, g_layout.down_r,
                    COL_PANEL_LIGHT, hud->down_held);
        draw_arrow(g_layout.down_x, g_layout.down_y - 3 * s, -1);
    }

    draw_button(g_layout.fly_x, g_layout.fly_y, g_layout.fly_r,
                COL_PANEL_LIGHT, hud->flying);
    hud_text(g_layout.fly_x - 5 * s, g_layout.fly_y - 2 * s, "F", COL_WHITE,
             s);

    /* Menu: three bars. */
    draw_button(g_layout.menu_x, g_layout.menu_y, g_layout.menu_r,
                COL_PANEL_LIGHT, 0);
    hud_rect(g_layout.menu_x - 6 * s, g_layout.menu_y - 5 * s, 13 * s,
             2 * s, COL_TEXT);
    hud_rect(g_layout.menu_x - 6 * s, g_layout.menu_y - s, 13 * s, 2 * s,
             COL_TEXT);
    hud_rect(g_layout.menu_x - 6 * s, g_layout.menu_y + 3 * s, 13 * s,
             2 * s, COL_TEXT);

    /* Backpack: a small bag outline. */
    draw_button(g_layout.bag_x, g_layout.bag_y, g_layout.bag_r,
                COL_PANEL_LIGHT, hud->inventory_open);
    hud_rect(g_layout.bag_x - 6 * s, g_layout.bag_y - 4 * s, 13 * s,
             9 * s, COL_TEXT);
    hud_rect(g_layout.bag_x - 4 * s, g_layout.bag_y - 2 * s, 9 * s,
             5 * s, COL_PANEL);
    hud_rect(g_layout.bag_x - 3 * s, g_layout.bag_y - 6 * s, 7 * s,
             3 * s, COL_TEXT);
}

static void draw_joystick(const hud_state_t *hud) {
    const int s = g_layout.ui_scale;
    if (!hud->move_active) {
        return;
    }
    hud_circle_outline(hud->move_origin_x, hud->move_origin_y, 36 * s,
                       COL_SHADOW);
    hud_circle_outline(hud->move_origin_x, hud->move_origin_y, 34 * s,
                       COL_MUTED);
    hud_circle((int)(hud->move_origin_x + hud->move_dx),
               (int)(hud->move_origin_y + hud->move_dy), 11 * s, COL_SHADOW);
    hud_circle((int)(hud->move_origin_x + hud->move_dx),
               (int)(hud->move_origin_y + hud->move_dy), 9 * s,
               COL_PANEL_LIGHT);
}

static void draw_use_hint(const hud_state_t *hud) {
    if (!hud->target_table || hud->inventory_open) {
        return;
    }
    hud_text_centered(g_layout.view_y + g_layout.view_h / 2 + 20,
                      "ACTION: USE", COL_SHADOW, 1);
    hud_text_centered(g_layout.view_y + g_layout.view_h / 2 + 19,
                      "ACTION: USE", COL_SELECT, 1);
}

static void draw_mine_progress(float progress) {
    const int x = g_layout.view_x + g_layout.view_w / 2 - 22;
    const int y = g_layout.view_y + g_layout.view_h / 2 + 14;
    if (progress <= 0.01F) {
        return;
    }
    if (progress > 1.0F) {
        progress = 1.0F;
    }
    hud_rect(x - 1, y - 1, 46, 7, COL_SHADOW);
    hud_rect(x, y, 44, 5, COL_PANEL);
    hud_rect(x, y, (int)(44.0F * progress), 5, COL_SELECT);
}

static char *put_i32(char *out, int32_t value) {
    char reversed[12];
    int digits = 0;
    uint32_t magnitude;
    if (value < 0) {
        *out++ = '-';
        magnitude = (uint32_t)(-(value + 1)) + 1u;
    } else {
        magnitude = (uint32_t)value;
    }
    do {
        reversed[digits++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0);
    while (digits != 0) {
        *out++ = reversed[--digits];
    }
    return out;
}

static void draw_compact_performance(const hud_state_t *hud, int fps) {
    char line[20];
    char *out = line;
    const int x = g_layout.view_x + g_inset_left + 8;

    if (hud->flying) {
        *out++ = 'F';
        *out++ = ' ';
    }
    *out++ = 'F';
    out = put_i32(out, fps);
    *out++ = ' ';
    *out++ = 'Q';
    *out++ = (char)('0' + hud->quality);
    *out = '\0';
    hud_text(x, g_layout.view_y + g_inset_top + 4, line, COL_TEXT, 1);

    out = line;
    *out++ = 'D';
    out = put_i32(out, (int32_t)(hud->dda_steps_x10 / 10u));
    *out++ = '/';
    out = put_i32(out, hud->dda_steps_max);
    *out = '\0';
    hud_text(x, g_layout.view_y + g_inset_top + 28, line, COL_TEXT, 1);

    out = line;
    *out++ = 'R';
    out = put_i32(out, hud->guest_render_us_div_100 / 10u);
    *out++ = ' ';
    *out++ = 'B';
    out = put_i32(out, hud->buffer_wait_us_div_100 / 10u);
    *out = '\0';
    hud_text(x, g_layout.view_y + g_inset_top + 52, line, COL_TEXT, 1);
}

static void draw_status(const hud_state_t *hud) {
    char line[32];
    char *out = line;
    const int fps = (int)((hud->fps_x10 + 5u) / 10u);
    const char *selected =
        g_inventory[hud->hotbar_selected].item == BLOCK_AIR
            ? "EMPTY"
            : game_item_name(g_inventory[hud->hotbar_selected].item);
    if (hud->flying) {
        *out++ = 'F';
        *out++ = 'L';
        *out++ = 'Y';
        *out++ = ' ';
    }
    *out++ = 'G';
    *out++ = 'F';
    *out++ = 'P';
    *out++ = 'S';
    *out++ = ' ';
    if (fps >= 100) {
        *out++ = (char)('0' + (fps / 100) % 10);
    }
    if (fps >= 10) {
        *out++ = (char)('0' + (fps / 10) % 10);
    }
    *out++ = (char)('0' + fps % 10);
    *out++ = ' ';
    if (hud->quality_manual != 0) {
        *out++ = 'M';
        *out++ = 'A';
        *out++ = 'N';
    } else {
        *out++ = 'A';
        *out++ = 'U';
        *out++ = 'T';
        *out++ = 'O';
    }
    *out++ = ' ';
    *out++ = (char)('0' + hud->quality);
    *out++ = 'X';
    *out = '\0';
    if (hud->show_performance && g_scale == QUALITY_PERFORMANCE) {
        draw_compact_performance(hud, fps);
    } else if (hud->show_performance) {
        hud_text_centered(g_layout.view_y + g_inset_top + 5, line, COL_SHADOW, 2);
        hud_text_centered(g_layout.view_y + g_inset_top + 4, line, COL_TEXT, 2);
    }

    if (hud->show_performance && g_scale != QUALITY_PERFORMANCE) {
        char position[32];
        char *out = position;
        *out++ = 'X';
        *out++ = ' ';
        out = put_i32(out, hud->pos_x);
        *out++ = ' ';
        *out++ = 'Z';
        *out++ = ' ';
        out = put_i32(out, hud->pos_z);
        *out = '\0';
        hud_text_centered(g_layout.view_y + g_inset_top + 19, position, COL_SHADOW, 1);
        hud_text_centered(g_layout.view_y + g_inset_top + 18, position, COL_TEXT, 1);
        out = position;
        *out++ = 'D';
        *out++ = 'D';
        *out++ = 'A';
        *out++ = ' ';
        out = put_i32(out, (int32_t)(hud->dda_steps_x10 / 10u));
        *out++ = '.';
        *out++ = (char)('0' + hud->dda_steps_x10 % 10u);
        *out++ = '/';
        out = put_i32(out, hud->dda_steps_max);
        *out++ = ' ';
        *out++ = 'F';
        out = put_i32(out, hud->fog_terminated_percent);
        *out++ = ' ';
        *out++ = 'H';
        out = put_i32(out, hud->solid_hit_percent);
        *out = '\0';
        hud_text_centered(g_layout.view_y + g_inset_top + 31, position, COL_SHADOW, 1);
        hud_text_centered(g_layout.view_y + g_inset_top + 30, position, COL_TEXT, 1);
        out = position;
        *out++ = 'U';
        *out++ = ' ';
        out = put_i32(out, hud->guest_update_us_div_100 / 10u);
        *out++ = '.';
        *out++ = (char)('0' + hud->guest_update_us_div_100 % 10u);
        *out++ = ' ';
        *out++ = 'R';
        *out++ = ' ';
        out = put_i32(out, hud->guest_render_us_div_100 / 10u);
        *out++ = '.';
        *out++ = (char)('0' + hud->guest_render_us_div_100 % 10u);
        *out++ = 'M';
        *out++ = 'S';
        *out = '\0';
        hud_text_centered(g_layout.view_y + g_inset_top + 43, position, COL_SHADOW, 1);
        hud_text_centered(g_layout.view_y + g_inset_top + 42, position, COL_TEXT, 1);
        out = position;
        *out++ = 'T';
        *out++ = ' ';
        out = put_i32(out, hud->guest_total_us_div_100 / 10u);
        *out++ = '.';
        *out++ = (char)('0' + hud->guest_total_us_div_100 % 10u);
        *out++ = ' ';
        *out++ = 'B';
        *out++ = ' ';
        out = put_i32(out, hud->buffer_wait_us_div_100 / 10u);
        *out++ = '.';
        *out++ = (char)('0' + hud->buffer_wait_us_div_100 % 10u);
        *out++ = 'M';
        *out++ = 'S';
        *out = '\0';
        hud_text_centered(g_layout.view_y + g_inset_top + 55, position, COL_SHADOW, 1);
        hud_text_centered(g_layout.view_y + g_inset_top + 54, position, COL_TEXT, 1);
    }

    if (hud->now_ms < 10000u && !hud->show_performance &&
        g_scale != QUALITY_PERFORMANCE) {
        hud_text_centered(34, "LEFT MOVE   RIGHT LOOK", COL_SHADOW, 1);
        hud_text_centered(33, "LEFT MOVE   RIGHT LOOK", COL_TEXT, 1);
        hud_text_centered(46, "HOLD ACTION: MINE/ATTACK/USE", COL_SHADOW, 1);
        hud_text_centered(45, "HOLD ACTION: MINE/ATTACK/USE", COL_TEXT, 1);
        hud_text_centered(58, "TAP PLACE TO BUILD  2X JUMP: FLY",
                          COL_SHADOW, 1);
        hud_text_centered(57, "TAP PLACE TO BUILD  2X JUMP: FLY",
                          COL_TEXT, 1);
        hud_text_centered(70, "PAD: Z JUMP  X ACTION  ENT PLACE  TAB ACTION",
                          COL_SHADOW, 1);
        hud_text_centered(69, "PAD: Z JUMP  X ACTION  ENT PLACE  TAB ACTION",
                          COL_TEXT, 1);
    }
    if (hud->toast != NULL && hud->toast[0] != '\0') {
        const int width = text_width(hud->toast, 1);
        const int x = g_layout.view_x + (g_layout.view_w - width) / 2;
        const int y = g_layout.hotbar_y - 14;
        hud_rect(x - 4, y - 3, width + 8, 11, COL_SHADOW);
        hud_text(x, y, hud->toast, COL_TEXT, 1);
    }
    {
        const int width = text_width(selected, 1);
        hud_text(g_layout.view_x + (g_layout.view_w - width) / 2,
                 g_layout.hotbar_y - 26, selected, COL_MUTED, 1);
    }
}

static void hud_text_centered_width(int x, int w, int y, const char *text,
                                     uint16_t color, int scale) {
    hud_text(x + (w - text_width(text, scale)) / 2, y, text, color, scale);
}

static void menu_draw_button(int index, int y, int height, const char *label,
                             uint8_t enabled) {
    const int ui_scale = g_layout.ui_scale;
    const int w = 200 * ui_scale;
    const int x = g_layout.view_x + (g_layout.view_w - w) / 2;
    g_menu_buttons[index].x = x;
    g_menu_buttons[index].y = y;
    g_menu_buttons[index].w = w;
    g_menu_buttons[index].h = height;
    g_menu_buttons[index].enabled = enabled;
    hud_rect(x, y, w, height, enabled ? COL_PANEL_LIGHT : COL_PANEL);
    hud_rect_outline(x, y, w, height, 2 * ui_scale,
                     enabled ? COL_SELECT : COL_SHADOW);
    hud_text_centered_width(x, w, y + (height - 14) / 2, label,
                            enabled ? COL_TEXT : COL_MUTED, 2);
}

void render_menu_layout(const menu_state_t *menu) {
    const int ui_scale = g_layout.ui_scale;
    const int w = 200 * ui_scale;
    const int x = g_layout.view_x + (g_layout.view_w - w) / 2;
    int heights[MENU_BUTTON_MAX];
    int enabled[MENU_BUTTON_MAX];
    int y;
    int spacing;
    int index;
    if (menu->screen == 2) {
        g_menu_button_count = 4;
        y = g_layout.view_y + 72 * ui_scale;
        spacing = 34 * ui_scale;
        for (index = 0; index < g_menu_button_count; ++index) {
            heights[index] = 30 * ui_scale;
            enabled[index] = index != 1 || menu->has_game;
        }
    } else if (menu->screen == 0) {
        g_menu_button_count = 3;
        y = g_layout.view_y + 88 * ui_scale;
        spacing = 42 * ui_scale;
        for (index = 0; index < g_menu_button_count; ++index) {
            heights[index] = 34 * ui_scale;
            enabled[index] = index != 1 || menu->has_save;
        }
    } else {
        g_menu_button_count = 5;
        y = g_layout.view_y + 68 * ui_scale;
        spacing = 30 * ui_scale;
        for (index = 0; index < g_menu_button_count; ++index) {
            heights[index] = 28 * ui_scale;
            enabled[index] = index != 2 || menu->has_game;
            if (index == 3) enabled[index] = menu->has_save;
        }
    }
    for (index = 0; index < g_menu_button_count; ++index) {
        g_menu_buttons[index].x = x;
        g_menu_buttons[index].y = y + index * spacing;
        g_menu_buttons[index].w = w;
        g_menu_buttons[index].h = heights[index];
        g_menu_buttons[index].enabled = (uint8_t)enabled[index];
    }
}

void render_menu(const menu_state_t *menu) {
    char seed_text[28];
    char quality_text[28];
    char *out;
    int row;
    int y;
    const int ui_scale = g_layout.ui_scale;
    render_menu_layout(menu);
    if (menu->overlay) {
        dim_region(g_layout.view_x, g_layout.view_y, g_layout.view_w,
                   g_layout.view_h);
    } else {
        for (row = 0; row < g_layout.view_h; ++row) {
            const int t =
                row * 255 / (g_layout.view_h > 0 ? g_layout.view_h : 1);
            const int r = 12 + t * 26 / 255;
            const int g = 22 + t * 52 / 255;
            const int b = 48 + t * 96 / 255;
            hud_rect(g_layout.view_x, g_layout.view_y + row, g_layout.view_w,
                     1, RGB565(r, g, b));
        }
    }
    if (menu->screen == 2) {
        hud_text_centered(g_layout.view_y + 30 * ui_scale, "PAUSED",
                          COL_SELECT, 3);
    } else {
        hud_text_centered(g_layout.view_y + 34 * ui_scale, "VOXEL CRAFT",
                          COL_SELECT, 3);
    }

    out = seed_text;
    *out++ = 'S';
    *out++ = 'E';
    *out++ = 'E';
    *out++ = 'D';
    *out++ = ' ';
    {
        char digits[12];
        int count = 0;
        uint32_t value = menu->seed;
        char *p = seed_text + 5;
        do {
            digits[count++] = (char)('0' + value % 10u);
            value /= 10u;
        } while (value != 0 && count < (int)sizeof(digits));
        while (count != 0) {
            *p++ = digits[--count];
        }
        *p = '\0';
    }
    if (menu->screen == 0) {
        hud_text_centered(g_layout.view_y + 60 * ui_scale, seed_text,
                          COL_MUTED, 2);
    }

    if (menu->screen == 2) {
        g_menu_button_count = 4;
        y = g_layout.view_y + 72 * ui_scale;
        menu_draw_button(0, y, 30 * ui_scale, "RESUME", 1);
        menu_draw_button(1, y + 34 * ui_scale, 30 * ui_scale, "SAVE GAME",
                         menu->has_game);
        menu_draw_button(2, y + 68 * ui_scale, 30 * ui_scale, "SETTINGS", 1);
        menu_draw_button(3, y + 102 * ui_scale, 30 * ui_scale, "MAIN MENU", 1);
    } else if (menu->screen == 0) {
        g_menu_button_count = 3;
        y = g_layout.view_y + 88 * ui_scale;
        menu_draw_button(0, y, 34 * ui_scale, "NEW GAME", 1);
        menu_draw_button(1, y + 42 * ui_scale, 34 * ui_scale, "LOAD SAVE",
                         menu->has_save);
        menu_draw_button(2, y + 84 * ui_scale, 34 * ui_scale, "SETTINGS", 1);
    } else {
        out = quality_text;
        *out++ = 'Q';
        *out++ = 'U';
        *out++ = 'A';
        *out++ = 'L';
        *out++ = 'I';
        *out++ = 'T';
        *out++ = 'Y';
        *out++ = ':';
        *out++ = ' ';
        if (menu->quality_manual == 0) {
            *out++ = 'A';
            *out++ = 'U';
            *out++ = 'T';
            *out++ = 'O';
        } else {
            *out++ = (char)('0' + menu->quality);
            *out++ = 'X';
        }
        *out = '\0';
        g_menu_button_count = 5;
        y = g_layout.view_y + 68 * ui_scale;
        menu_draw_button(0, y, 28 * ui_scale, quality_text, 1);
        menu_draw_button(1, y + 30 * ui_scale, 28 * ui_scale,
                         g_scale == QUALITY_PERFORMANCE
                             ? (menu->show_performance ? "PERF: ON"
                                                       : "PERF: OFF")
                             : (menu->show_performance ? "PERFORMANCE: ON"
                                                       : "PERFORMANCE: OFF"),
                         1);
        menu_draw_button(2, y + 60 * ui_scale, 28 * ui_scale, "SAVE GAME",
                         menu->has_game);
        menu_draw_button(3, y + 90 * ui_scale, 28 * ui_scale, "DELETE SAVE",
                         menu->has_save);
        menu_draw_button(4, y + 120 * ui_scale, 28 * ui_scale, "BACK", 1);
    }
    if (menu->toast != NULL && menu->toast[0] != '\0') {
        hud_text_centered(g_layout.view_y + g_layout.view_h - 16,
                          menu->toast, COL_TEXT, 1);
    }
    if (menu->screen == 0) {
        hud_text_centered(g_layout.view_y + g_layout.view_h - 18,
                          "TAP TO SELECT", COL_MUTED, 2);
    }
}

void render_hud(const hud_state_t *hud) {
    render_ensure_textures();
    if (hud->inventory_open) {
        draw_inventory(hud);
        return;
    }
    draw_joystick(hud);
    draw_crosshair(hud->swing);
    draw_use_hint(hud);
    draw_mine_progress(hud->mine_progress);
    draw_hotbar(hud);
    draw_controls(hud);
    draw_status(hud);
}
