/* Shared screen layout. The renderer draws from it and the input handler hit
 * tests against it, so both always agree on where the controls are. */
#ifndef PD_LAYOUT_H
#define PD_LAYOUT_H

#include <stdint.h>

#define PD_TILE_PIXELS 16
#define PD_BUTTON_COUNT 5
#define PD_CLASS_COUNT 3
#define PD_BAG_ROWS 12

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
    int width, height;
    int safe_top, safe_right, safe_bottom, safe_left;
    uint32_t display_shape;
    int hud_h;
    int bar_h;
    int map_x, map_y, map_w, map_h;
    int tile_pixels, zoom;
    int cols, rows;
    pd_rect_t button[PD_BUTTON_COUNT];
    pd_rect_t menu_primary, menu_secondary, menu_back, hero_info;
    pd_rect_t menu_pane, settings, journal, depth, zoom_out, zoom_in, settings_back;
    pd_rect_t pause_settings, pause_menu;
    pd_rect_t class_button[PD_CLASS_COUNT];
    pd_rect_t bag_row[PD_BAG_ROWS];
    pd_rect_t bag_use[PD_BAG_ROWS];
    pd_rect_t bag_drop[PD_BAG_ROWS];
    pd_rect_t bag_close;
    int font_scale;
} pd_layout_t;

void pd_layout_build(pd_layout_t *layout, int width, int height,
                     int safe_top, int safe_right, int safe_bottom,
                     int safe_left);
void pd_layout_set_zoom(pd_layout_t *layout, int zoom);
void pd_layout_fit_display_shape(pd_layout_t *layout, uint32_t shape,
                                 const int corner_radii[4]);
int pd_rect_contains(const pd_rect_t *rect, int x, int y);

/* Top-left tile of the map viewport for a hero at (hero_x, hero_y). */
void pd_layout_camera(const pd_layout_t *layout, int hero_x, int hero_y,
                      int *camera_x, int *camera_y);

#endif
