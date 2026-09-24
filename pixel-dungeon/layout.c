#include "layout.h"

#include <stddef.h>
#include <stdint.h>

#include "dungeon.h"

int pd_rect_contains(const pd_rect_t *rect, int x, int y) {
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w &&
           y < rect->y + rect->h;
}

static void set_rect(pd_rect_t *rect, int x, int y, int w, int h) {
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
}

void pd_layout_camera(const pd_layout_t *layout, int hero_x, int hero_y,
                      int *camera_x, int *camera_y) {
    int x = hero_x - layout->cols / 2;
    int y = hero_y - layout->rows / 2;
    if (x > PD_MAP_W - layout->cols) x = PD_MAP_W - layout->cols;
    if (y > PD_MAP_H - layout->rows) y = PD_MAP_H - layout->rows;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    *camera_x = x;
    *camera_y = y;
}

void pd_layout_set_zoom(pd_layout_t *layout, int zoom) {
    if (zoom < 1) zoom = 1;
    if (zoom > 3) zoom = 3;
    layout->zoom = zoom;
    layout->tile_pixels = PD_TILE_PIXELS * zoom;
    layout->cols = (layout->width + layout->tile_pixels - 1) /
                   layout->tile_pixels;
    layout->rows = (layout->height + layout->tile_pixels - 1) /
                   layout->tile_pixels;
    if (layout->cols < 1) layout->cols = 1;
    if (layout->rows < 1) layout->rows = 1;
    if (layout->cols > PD_MAP_W) layout->cols = PD_MAP_W;
    if (layout->rows > PD_MAP_H) layout->rows = PD_MAP_H;
    layout->map_w = layout->cols * layout->tile_pixels;
    layout->map_h = layout->rows * layout->tile_pixels;
    layout->map_x = (layout->width - layout->map_w) / 2;
    layout->map_y = (layout->height - layout->map_h) / 2;
}

static int rounded_inset(int radius, int edge_distance) {
    int inset = 0;
    if (edge_distance >= radius || radius <= 0) return 0;
    while (inset < radius &&
           (int64_t)(radius - inset) * (radius - inset) +
               (int64_t)(radius - edge_distance) * (radius - edge_distance) >
               (int64_t)radius * radius)
        ++inset;
    return inset;
}

static int shape_inset(const pd_layout_t *layout, uint32_t shape,
                       const int radii[4], int y, int right) {
    int inset = 0;
    if (shape == 2) {
        const int diameter = layout->width < layout->height
                                 ? layout->width : layout->height;
        const int64_t dy = 2 * (int64_t)y + 1 - layout->height;
        for (; inset < layout->width / 2; ++inset) {
            const int64_t dx = 2 * (int64_t)inset + 1 - layout->width;
            if (dx * dx + dy * dy <= (int64_t)diameter * diameter) break;
        }
    } else if (shape == 1 && radii != NULL) {
        const int top_radius = radii[right ? 1 : 0];
        const int bottom_radius = radii[right ? 2 : 3];
        inset = rounded_inset(top_radius, y);
        const int bottom_inset = rounded_inset(bottom_radius,
                                               layout->height - 1 - y);
        if (inset < bottom_inset) inset = bottom_inset;
    }
    return inset;
}

static int shape_rect_inset(const pd_layout_t *layout, uint32_t shape,
                            const int radii[4], int y, int height,
                            int right) {
    int inset = shape_inset(layout, shape, radii, y, right);
    const int bottom_inset = shape_inset(layout, shape, radii,
                                         y + height - 1, right);
    if (inset < bottom_inset) inset = bottom_inset;
    return inset + (shape == 1 || shape == 2 ? 2 : 0);
}

void pd_layout_fit_display_shape(pd_layout_t *layout, uint32_t shape,
                                  const int radii[4]) {
    const int pane_w = layout->width < 190 ? 36 : 42;
    const int pane_h = layout->height < 190 ? 20 : 22;
    const int depth_w = 12;
    int y;
    int right;
    if (shape == 2) {
        const int diameter = layout->width < layout->height
                                 ? layout->width : layout->height;
        const int inscribed = diameter * 707 / 1000;
        const int horizontal = (layout->width - inscribed) / 2 + 2;
        const int vertical = (layout->height - inscribed) / 2 + 2;
        const int left = layout->safe_left > horizontal
                             ? layout->safe_left : horizontal;
        const int right_inset = layout->safe_right > horizontal
                                    ? layout->safe_right : horizontal;
        const int top = layout->safe_top > vertical
                            ? layout->safe_top : vertical;
        const int bottom = layout->safe_bottom > vertical
                               ? layout->safe_bottom : vertical;
        if (left != layout->safe_left || right_inset != layout->safe_right ||
            top != layout->safe_top || bottom != layout->safe_bottom)
            pd_layout_build(layout, layout->width, layout->height,
                            top, right_inset, bottom, left);
    }
    y = layout->safe_top + 10;
    right = layout->safe_right;
    layout->display_shape = shape;
    if (shape == 2) {
        const int compact = layout->height < 220;
        const int class_h = compact ? 30 : 40;
        const int class_top = layout->safe_top + (compact ? 26 : 27);
        for (int index = 0; index < PD_CLASS_COUNT; ++index)
            set_rect(&layout->class_button[index], layout->safe_left + 4,
                     class_top + index * (class_h + 2),
                     layout->width - layout->safe_left -
                         layout->safe_right - 8, class_h);
    }
    if (shape == 2) {
        const int initial_y = layout->hero_info.y;
        while (layout->hero_info.y + 38 <
               layout->height - layout->safe_bottom - layout->bar_h) {
            int left_bound = shape_rect_inset(layout, shape, radii,
                                              layout->hero_info.y, 38, 0);
            int right_bound = shape_rect_inset(layout, shape, radii,
                                               layout->hero_info.y, 38, 1);
            if (left_bound < layout->safe_left + 1)
                left_bound = layout->safe_left + 1;
            if (right_bound < layout->safe_right)
                right_bound = layout->safe_right;
            if (left_bound + layout->hero_info.w <=
                layout->width - right_bound) break;
            ++layout->hero_info.y;
        }
        layout->hud_h += layout->hero_info.y - initial_y;
    }
    int left = shape_rect_inset(layout, shape, radii, layout->hero_info.y,
                                38, 0);
    if (left < layout->hero_info.x) left = layout->hero_info.x;
    layout->hero_info.x = left;
    left = shape_rect_inset(layout, shape, radii, layout->menu_back.y,
                            layout->menu_back.h, 0);
    if (left > layout->menu_back.x) layout->menu_back.x = left;
    left = shape_rect_inset(layout, shape, radii, y, pane_h, 1);
    if (right < left) right = left;
    if (shape == 2 && layout->width - right - pane_w - depth_w <
                          layout->hero_info.x + layout->hero_info.w + 2) {
        y = layout->hero_info.y + 41;
        right = layout->safe_right;
        left = shape_rect_inset(layout, shape, radii, y, pane_h, 1);
        if (right < left) right = left;
    }
    if (right > layout->width - layout->safe_left - pane_w)
        right = layout->width - layout->safe_left - pane_w;
    if (right < 0) right = 0;
    set_rect(&layout->menu_pane, layout->width - right - pane_w, y,
             pane_w, pane_h);
    set_rect(&layout->depth, layout->menu_pane.x - depth_w, y,
             depth_w, pane_h);
    set_rect(&layout->journal, layout->menu_pane.x, y,
             pane_w / 2, pane_h);
    set_rect(&layout->settings, layout->menu_pane.x + pane_w / 2, y,
             pane_w - pane_w / 2, pane_h);
    if (shape == 2) {
        int shift = 0;
        const int min_y = layout->safe_top + layout->hud_h;
        while (layout->button[0].y - shift > min_y) {
            const int button_y = layout->button[0].y - shift;
            const int button_h = layout->button[0].h;
            const int button_left = layout->button[0].x;
            const pd_rect_t *last = &layout->button[PD_BUTTON_COUNT - 1];
            const int button_right = last->x + last->w;
            const int left_bound = shape_rect_inset(layout, shape, radii,
                                                   button_y, button_h, 0);
            const int right_bound = shape_rect_inset(layout, shape, radii,
                                                    button_y, button_h, 1);
            if (button_left >= left_bound &&
                button_right <= layout->width - right_bound)
                break;
            ++shift;
        }
        for (int index = 0; index < PD_BUTTON_COUNT; ++index)
            layout->button[index].y -= shift;
        layout->bar_h += shift;
        pd_layout_set_zoom(layout, layout->zoom);
    }
}

void pd_layout_build(pd_layout_t *layout, int width, int height,
                     int safe_top, int safe_right, int safe_bottom,
                     int safe_left) {
    int button_gap = 3;
    int button_w;
    int bar_y;
    int content_x;
    int content_w;

    layout->width = width;
    layout->height = height;
    /* Panel insets keep the controls inside the touchable area. */
    layout->safe_top = safe_top < 0 ? 0 : safe_top;
    layout->safe_right = safe_right < 0 ? 0 : safe_right;
    layout->safe_bottom = safe_bottom < 0 ? 0 : safe_bottom;
    layout->safe_left = safe_left < 0 ? 0 : safe_left;
    layout->display_shape = 0;
    layout->hud_h = 40;
    layout->bar_h = 38;
    bar_y = height - layout->safe_bottom - layout->bar_h;
    content_x = layout->safe_left;
    content_w = width - layout->safe_left - layout->safe_right;

    pd_layout_set_zoom(layout, 1);

    /* The original has no movement pad: taps walk the hero. Five toolbar slots
     * carry the wait, search, potion, pack and stairs actions. */
    button_w = content_w < 174 ? (content_w - 12) / PD_BUTTON_COUNT : 32;
    {
        const int total = button_w * PD_BUTTON_COUNT +
                          button_gap * (PD_BUTTON_COUNT - 1);
        const int start = layout->safe_left + (content_w - total) / 2;
        for (int index = 0; index < PD_BUTTON_COUNT; ++index) {
            set_rect(&layout->button[index],
                     start + index * (button_w + button_gap), bar_y + 3,
                     button_w, layout->bar_h - 6);
        }
    }
    layout->font_scale = 1;

    {
        const int center = layout->safe_left + content_w / 2;
        const int menu_y = layout->safe_top + (height - layout->safe_top -
                            layout->safe_bottom) / 2;
        const int menu_w = content_w < 190 ? content_w - 24 : 172;
        set_rect(&layout->menu_primary, center - menu_w / 2,
                 menu_y - 10, menu_w, 26);
        set_rect(&layout->menu_secondary, center - menu_w / 2,
                 menu_y + 21, menu_w, 26);
        set_rect(&layout->menu_back, layout->safe_left + 4,
                 layout->safe_top + 4, 30, 24);
        set_rect(&layout->hero_info, layout->safe_left + 1,
                 layout->safe_top, 82, layout->hud_h);
        const int pause_w = content_w < 156 ? content_w - 18 : 138;
        const int pause_left = center - pause_w / 2;
        const int pause_top = menu_y - 26;
        set_rect(&layout->pause_settings, pause_left, pause_top,
                 pause_w, 24);
        set_rect(&layout->pause_menu, pause_left, pause_top + 27,
                 pause_w, 24);
        set_rect(&layout->zoom_out, center - 61, menu_y - 14, 42, 32);
        set_rect(&layout->zoom_in, center + 19, menu_y - 14, 42, 32);
        set_rect(&layout->settings_back, center - 44, menu_y + 43, 88, 26);
    }
    pd_layout_fit_display_shape(layout, 0, NULL);

    /* Title screen class picker. */
    {
        if (content_w < 270 && height > 300) {
            const int class_w = content_w - 24;
            const int class_h = 74;
            const int top = layout->safe_top +
                (height - layout->safe_top - layout->safe_bottom -
                 (PD_CLASS_COUNT * class_h + 16)) / 2;
            for (int index = 0; index < PD_CLASS_COUNT; ++index)
                set_rect(&layout->class_button[index],
                         content_x + 12, top + index * (class_h + 8),
                         class_w, class_h);
        } else {
            int class_w = (content_w - 40) / PD_CLASS_COUNT;
            if (class_w > 96) class_w = 96;
            if (class_w < 1) class_w = 1;
            const int total = class_w * PD_CLASS_COUNT + 8 * (PD_CLASS_COUNT - 1);
            const int start = content_x + (content_w - total) / 2;
            const int top = layout->safe_top +
                            (height - layout->safe_top - layout->safe_bottom) / 2 - 42;
            for (int index = 0; index < PD_CLASS_COUNT; ++index)
                set_rect(&layout->class_button[index],
                         start + index * (class_w + 8), top, class_w, 103);
        }
    }

    /* The pack uses the original five-column inventory grid. */
    {
        const int panel_w = content_w < 250 ? content_w - 12 : 240;
        const int panel_x = content_x + (content_w - panel_w) / 2;
        const int available_h = height - layout->safe_top - layout->safe_bottom - 8;
        const int panel_h = available_h < 184 ? available_h : 184;
        const int panel_y = layout->safe_top +
                            (height - layout->safe_top - layout->safe_bottom - panel_h) / 2;
        const int header_h = panel_h < 170 ? 30 : 36;
        const int top = panel_y + header_h;
        const int row_step = (panel_h - header_h - 55) / 3;
        const int slot_w = (panel_w - 24) / 5;
        for (int index = 0; index < PD_BAG_ROWS; ++index) {
            set_rect(&layout->bag_row[index], panel_x + 12 +
                     (index % 5) * slot_w,
                     top + (index / 5) * row_step,
                     slot_w - 2, row_step - 2);
            set_rect(&layout->bag_use[index], panel_x + 12,
                     panel_y + panel_h - 28, panel_w / 2 - 16, 23);
            set_rect(&layout->bag_drop[index], panel_x + panel_w / 2 + 4,
                     panel_y + panel_h - 28, panel_w / 2 - 16, 23);
        }
        set_rect(&layout->bag_close, panel_x + panel_w - 30,
                 panel_y + 5, 22, 20);
    }
}
