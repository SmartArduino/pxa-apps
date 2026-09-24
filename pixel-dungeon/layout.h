/* Shared screen layout. The renderer draws from it and the input handler hit
 * tests against it, so both always agree on where the controls are. */
#ifndef PD_LAYOUT_H
#define PD_LAYOUT_H

#include <stdint.h>

#define PD_TILE_PIXELS 16
#define PD_BUTTON_COUNT 5
#define PD_CLASS_COUNT 3
#define PD_BAG_ROWS 12
#define PD_SAVE_SLOTS 5

enum {
    PD_BUTTON_WAIT = 0,
    PD_BUTTON_SEARCH,
    PD_BUTTON_POTION,
    PD_BUTTON_BAG,
    PD_BUTTON_STAIRS,
};

typedef struct {
    int x, y, w, h;
} pd_rect_t;

typedef struct {
    int x, width, scale, spacing, condensed;
} pd_hud_digits_t;

typedef struct {
    int width, height;
    int safe_top, safe_right, safe_bottom, safe_left;
    uint32_t display_shape;
    int hud_h;
    int bar_h;
    int map_x, map_y, map_w, map_h;
    int tile_pixels, zoom;
    int cols, rows;
    int camera_dx, camera_dy;
    uint8_t camera_manual;
    pd_rect_t button[PD_BUTTON_COUNT];
    pd_rect_t menu_primary, menu_secondary, menu_back, hero_info;
    pd_rect_t title_settings, title_rankings, title_journal;
    pd_rect_t save_row[PD_SAVE_SLOTS];
    pd_rect_t menu_pane, settings, journal, depth, zoom_out, zoom_in, settings_back;
    pd_rect_t pause_settings, pause_menu;
    pd_rect_t shop_buy, shop_close;
    pd_rect_t class_button[PD_CLASS_COUNT];
    pd_rect_t bag_row[PD_BAG_ROWS];
    pd_rect_t bag_use[PD_BAG_ROWS];
    pd_rect_t bag_drop[PD_BAG_ROWS];
    pd_rect_t bag_close, info_close, journal_close;
    int font_scale;
} pd_layout_t;

void pd_layout_build(pd_layout_t *layout, int width, int height,
                     int safe_top, int safe_right, int safe_bottom,
                     int safe_left);
void pd_layout_set_zoom(pd_layout_t *layout, int zoom);
void pd_layout_fit_display_shape(pd_layout_t *layout, uint32_t shape,
                                 const int corner_radii[4]);
int pd_rect_contains(const pd_rect_t *rect, int x, int y);
pd_hud_digits_t pd_layout_hud_digits(int x, int width,
                                     const char *text, int scale);
pd_rect_t pd_layout_save_row(const pd_layout_t *layout, int visible_count,
                              int visible_index);
int pd_layout_save_page_size(const pd_layout_t *layout);
int pd_layout_save_page_start(const pd_layout_t *layout, int selected_index);
pd_rect_t pd_layout_save_page_button(const pd_layout_t *layout,
                                     int visible_count);

/* Top-left tile of the map viewport for a hero at (hero_x, hero_y). */
void pd_layout_camera(const pd_layout_t *layout, int hero_x, int hero_y,
                      int *camera_x, int *camera_y);
void pd_layout_pan(pd_layout_t *layout, int hero_x, int hero_y,
                   int step_x, int step_y);

#endif
