#ifndef VOXEL_CRAFT_RENDER_H
#define VOXEL_CRAFT_RENDER_H

#include <stddef.h>
#include <stdint.h>

#include "game.h"

/* Upper bounds for the static buffers. The actual layout follows the UI
 * environment at runtime; screens larger than these bounds are centred with a
 * letterbox, smaller ones are filled natively. */
#define SCREEN_W_MAX 800
#define SCREEN_H_MAX 512
#define SCREEN_W_DEFAULT 296
#define SCREEN_H_DEFAULT 240
#define RENDER_SCENE_MAX_W 320
#define RENDER_SCENE_MAX_H 240
#define RENDER_SCENE_PIXELS_MAX \
    ((size_t)RENDER_SCENE_MAX_W * RENDER_SCENE_MAX_H)

#define HOTBAR_SLOT 24
#define QUALITY_MIN 1
#define QUALITY_BALANCED 2
#define QUALITY_PERFORMANCE 4
#define QUALITY_MAX QUALITY_PERFORMANCE

typedef struct {
    int screen_w;
    int screen_h;
    int view_x;
    int view_y;
    int view_w;
    int view_h;
    int ui_scale;
    int hotbar_slot;
    int hotbar_x;
    int hotbar_y;
    int jump_x;
    int jump_y;
    int jump_r;
    int action_x;
    int action_y;
    int action_r;
    int place_x;
    int place_y;
    int place_r;
    int down_x;
    int down_y;
    int down_r;
    int fly_x;
    int fly_y;
    int fly_r;
    int quality_x;
    int quality_y;
    int quality_w;
    int quality_h;
    int bag_x;
    int bag_y;
    int bag_r;
    int menu_x;
    int menu_y;
    int menu_r;
} render_layout_t;

typedef struct {
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    int craft_x;
    int craft_y;
    int result_x;
    int result_y;
    int main_x;
    int main_y;
    int inv_hotbar_x;
    int inv_hotbar_y;
    int close_x;
    int close_y;
    int close_r;
} inventory_layout_t;

typedef struct {
    uint32_t now_ms;
    uint32_t fps_x10;
    uint16_t guest_update_us_div_100;
    uint16_t guest_render_us_div_100;
    uint16_t guest_total_us_div_100;
    uint16_t buffer_wait_us_div_100;
    uint16_t dda_steps_x10;
    uint16_t dda_steps_max;
    uint8_t fog_terminated_percent;
    uint8_t solid_hit_percent;
    int32_t pos_x;
    int32_t pos_z;
    render_layout_t layout;
    uint8_t hotbar_selected;
    uint8_t hotbar_items[HOTBAR_SLOTS];
    uint8_t hotbar_counts[HOTBAR_SLOTS];
    uint8_t flying;
    uint8_t move_active;
    int16_t move_origin_x;
    int16_t move_origin_y;
    int16_t move_dx;
    int16_t move_dy;
    uint8_t jump_held;
    uint8_t down_held;
    uint8_t action_held;
    uint8_t action_mode;
    uint8_t quality;
    uint8_t quality_manual;
    uint8_t show_performance;
    uint8_t inventory_open;
    uint8_t craft_table;
    uint8_t target_table;
    uint8_t cursor_item;
    uint8_t cursor_count;
    int16_t pointer_x;
    int16_t pointer_y;
    float mine_progress;
    float swing;
    const char *toast;
} hud_state_t;

typedef struct {
    uint32_t rays;
    uint32_t total_steps;
    uint32_t fog_terminated_rays;
    uint32_t solid_hit_rays;
    uint16_t max_steps;
} render_perf_stats_t;

/* Inventory screen hit zones. */
#define INV_HIT_NONE (-1)
#define INV_HIT_CLOSE (-2)
#define INV_HIT_CRAFT 0
#define INV_HIT_RESULT (INV_HIT_CRAFT + TABLE_CRAFT_SLOTS)
#define INV_HIT_MAIN (INV_HIT_RESULT + 1)
#define INV_HIT_HOTBAR (INV_HIT_MAIN + INV_MAIN_SLOTS)

#define MENU_BUTTON_MAX 5
typedef struct {
    int x;
    int y;
    int w;
    int h;
    uint8_t enabled;
} menu_button_t;

typedef struct {
    uint8_t screen;  /* 0 = main menu, 1 = settings, 2 = pause */
    uint8_t overlay; /* dim the frozen game frame instead of a gradient */
    uint8_t has_save;
    uint8_t has_game;
    uint8_t quality_manual;
    uint8_t quality;
    uint8_t show_performance;
    uint32_t seed;
    const char *toast;
} menu_state_t;

extern render_layout_t g_layout;
extern inventory_layout_t g_inv_layout;
extern menu_button_t g_menu_buttons[MENU_BUTTON_MAX];
extern int g_menu_button_count;

/* Points the HUD row table at a view-sized pixel buffer. render_3d does this
 * itself; the menu path must call it before render_menu. */
void render_target(uint16_t *pixels, uint32_t stride_pixels);
void render_menu_layout(const menu_state_t *menu);
void render_menu(const menu_state_t *menu);

/* 0 selects the inventory's 2x2 grid, 1 the crafting table's 3x3 grid. */
void render_inventory_set_mode(int table);
/* Returns a hit zone or INV_HIT_NONE. */
int render_inventory_hit(int x, int y);
/* Maps a craft/main/hotbar hit to a craft slot (0..3) or inventory slot
 * (0..35); returns -1 for none, result or close. */
int render_inventory_slot(int hit);

/* Recomputes the layout, view area and scene grid for a logical screen size. */
void render_configure(int width, int height);

/* Sets the scene size limits for the active surface path. The Host raster path
 * passes the real view size so quality 1x renders at the display resolution;
 * the Guest CPU path passes RENDER_SCENE_MAX_* and a pixel budget. A
 * pixel_budget of 0 means unlimited. */
void render_set_scene_limits(int max_width, int max_height, int pixel_budget);

/* Applies the Host-published safe-area and system chrome insets in logical
 * pixels. Returns 1 when the stored insets changed; the caller must re-run
 * render_configure() afterwards. */
int render_set_safe_insets(int left, int top, int right, int bottom);

/* Dynamic resolution: scale N renders at 1/N of the view area. Higher scales
 * trade sharpness for frame rate. */
void render_set_quality(int scale);
/* Shrinks the view pixel budget after repeated present failures so a Host
 * with a smaller Canvas limit still gets frames. */
void render_shrink_view(void);
int render_quality(void);
int render_min_quality(void);
int render_scene_width(void);
int render_scene_height(void);
void render_get_perf_stats(render_perf_stats_t *stats);

int render_3d(uint16_t *pixels, uint32_t stride_pixels,
              const player_t *player, uint32_t now_ms,
              const ray_hit_t *target, float mine_progress);
void render_hud(const hud_state_t *hud);
uint16_t render_block_color(int block);

#endif
