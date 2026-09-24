#include "layout.h"

#include <stddef.h>
#include <stdint.h>

#include "dungeon.h"

int pd_rect_contains(const pd_rect_t *rect, int x, int y) {
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w &&
           y < rect->y + rect->h;
}

pd_rect_t pd_layout_hero_portrait(const pd_layout_t *layout) {
    const pd_rect_t *pane = &layout->hero_info;
    pd_rect_t portrait = {pane->x, pane->y,
                          30 * pane->w / 82, 30 * pane->h / 38};
    return portrait;
}

static int hud_digits_width(const char *text, int scale, int spacing,
                             int condensed) {
    int width = 0;
    for (const char *character = text; *character != '\0'; ++character) {
        if ((*character < '0' || *character > '9') && *character != '/')
            continue;
        const int glyph_width = (*character == '/' || *character == '1' ||
                                 condensed) ? 2 : 3;
        width += glyph_width * scale + spacing;
    }
    return width > 0 ? width - spacing : 0;
}

pd_hud_digits_t pd_layout_hud_digits(int x, int width,
                                     const char *text, int scale) {
    pd_hud_digits_t digits = {x, 0, scale, scale, 0};
    if (hud_digits_width(text, digits.scale, digits.spacing, 0) > width)
        digits.spacing = 0;
    if (hud_digits_width(text, digits.scale, digits.spacing, 0) > width &&
        digits.scale > 1) {
        digits.scale = 1;
        digits.spacing = 1;
    }
    if (hud_digits_width(text, digits.scale, digits.spacing, 0) > width) {
        digits.condensed = 1;
        digits.spacing = 0;
    }
    digits.width = hud_digits_width(text, digits.scale, digits.spacing,
                                     digits.condensed);
    digits.x = x + (width - digits.width) / 2;
    return digits;
}

static void set_rect(pd_rect_t *rect, int x, int y, int w, int h) {
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
}

int pd_layout_save_page_size(const pd_layout_t *layout) {
    const int available = layout->height - layout->safe_top -
                          layout->safe_bottom;
    if (available < 155) return 2;
    if (available < 220) return 3;
    return PD_SAVE_SLOTS;
}

int pd_layout_save_page_start(const pd_layout_t *layout,
                              int selected_index) {
    const int page_size = pd_layout_save_page_size(layout);
    return selected_index / page_size * page_size;
}

pd_rect_t pd_layout_save_row(const pd_layout_t *layout, int visible_count,
                              int visible_index) {
    pd_rect_t rect = layout->save_row[visible_index];
    const int step = rect.h + 2;
    const int center_y = layout->safe_top +
                         (layout->height - layout->safe_top -
                          layout->safe_bottom) / 2;
    rect.y = center_y - (visible_count * step - 2) / 2 + visible_index * step;
    if (pd_layout_save_page_size(layout) < PD_SAVE_SLOTS) rect.y -= 8;
    return rect;
}

pd_rect_t pd_layout_save_page_button(const pd_layout_t *layout,
                                     int visible_count) {
    const pd_rect_t last = pd_layout_save_row(layout, visible_count,
                                              visible_count - 1);
    pd_rect_t button;
    set_rect(&button, layout->width / 2 - 32,
             last.y + last.h + 2, 64, 32);
    return button;
}

static int floor_div(int value, int divisor) {
    return value >= 0 ? value / divisor : -1 - (-1 - value) / divisor;
}

void pd_layout_camera_pixels(const pd_layout_t *layout, int hero_x, int hero_y,
                             int *camera_x, int *camera_y) {
    const int cell = layout->tile_pixels;
    int x = (hero_x - layout->cols / 2) * cell + layout->camera_dx;
    int y = (hero_y - layout->rows / 2) * cell + layout->camera_dy;
    int left = layout->hero_info.x + layout->hero_info.w;
    int top = layout->hero_info.y + layout->hero_info.h;
    if (top < layout->menu_pane.y + layout->menu_pane.h)
        top = layout->menu_pane.y + layout->menu_pane.h;
    if (top < layout->depth.y + layout->depth.h)
        top = layout->depth.y + layout->depth.h;
    if (left < layout->depth.x + layout->depth.w)
        left = layout->depth.x + layout->depth.w;
    int upper_margin = top + 6 * layout->zoom - layout->map_y;
    int left_margin = left + 6 * layout->zoom - layout->map_x;
    int right_margin = layout->map_x + layout->map_w -
                       layout->menu_pane.x + 6 * layout->zoom;
    int bottom_margin = layout->map_y + layout->map_h -
                        layout->button[0].y + 6 * layout->zoom;
    int max_x = (PD_MAP_W - layout->cols) * cell;
    int max_y = (PD_MAP_H - layout->rows) * cell;
    const int horizontal_limit = layout->width - 3 * cell;
    const int vertical_limit = layout->height - 3 * cell;
    if (upper_margin < 0) upper_margin = 0;
    if (left_margin < 0) left_margin = 0;
    if (right_margin < 0) right_margin = 0;
    if (bottom_margin < 0) bottom_margin = 0;
    if (upper_margin > vertical_limit) upper_margin = vertical_limit;
    if (bottom_margin > vertical_limit) bottom_margin = vertical_limit;
    if (left_margin > horizontal_limit) left_margin = horizontal_limit;
    if (right_margin > horizontal_limit) right_margin = horizontal_limit;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (layout->camera_manual) {
        max_x += right_margin;
        max_y += bottom_margin;
    }
    if (x > max_x) x = max_x;
    if (y > max_y) y = max_y;
    if (x < (layout->camera_manual ? -left_margin : 0))
        x = layout->camera_manual ? -left_margin : 0;
    if (y < (layout->camera_manual ? -upper_margin : 0))
        y = layout->camera_manual ? -upper_margin : 0;
    *camera_x = x;
    *camera_y = y;
}

void pd_layout_camera(const pd_layout_t *layout, int hero_x, int hero_y,
                      int *camera_x, int *camera_y) {
    pd_layout_camera_pixels(layout, hero_x, hero_y, camera_x, camera_y);
    *camera_x = floor_div(*camera_x, layout->tile_pixels);
    *camera_y = floor_div(*camera_y, layout->tile_pixels);
}

void pd_layout_camera_visual_pixels(const pd_layout_t *layout,
                                    int hero_x, int hero_y,
                                    int from_x, int from_y, int moving,
                                    int *camera_x, int *camera_y) {
    pd_layout_camera_pixels(layout, hero_x, hero_y, camera_x, camera_y);
    if (moving <= 0) return;
    int previous_x;
    int previous_y;
    pd_layout_camera_pixels(layout, from_x, from_y,
                            &previous_x, &previous_y);
    if (moving > 12) moving = 12;
    *camera_x += (previous_x - *camera_x) * moving / 12;
    *camera_y += (previous_y - *camera_y) * moving / 12;
}

void pd_layout_pan_pixels(pd_layout_t *layout, int hero_x, int hero_y,
                          int delta_x, int delta_y) {
    int camera_x;
    int camera_y;
    const int base_x = (hero_x - layout->cols / 2) * layout->tile_pixels;
    const int base_y = (hero_y - layout->rows / 2) * layout->tile_pixels;
    pd_layout_camera_pixels(layout, hero_x, hero_y, &camera_x, &camera_y);
    layout->camera_manual = 1;
    layout->camera_dx = camera_x + delta_x - base_x;
    layout->camera_dy = camera_y + delta_y - base_y;
    pd_layout_camera_pixels(layout, hero_x, hero_y, &camera_x, &camera_y);
    layout->camera_dx = camera_x - base_x;
    layout->camera_dy = camera_y - base_y;
}

void pd_layout_pan(pd_layout_t *layout, int hero_x, int hero_y,
                   int step_x, int step_y) {
    pd_layout_pan_pixels(layout, hero_x, hero_y,
                         step_x * layout->tile_pixels,
                         step_y * layout->tile_pixels);
}

void pd_layout_set_zoom(pd_layout_t *layout, int zoom) {
    if (zoom < 1) zoom = 1;
    if (zoom > 3) zoom = 3;
    if (layout->zoom > 0 && layout->zoom != zoom) {
        layout->camera_dx = layout->camera_dx * zoom / layout->zoom;
        layout->camera_dy = layout->camera_dy * zoom / layout->zoom;
    }
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
    int min_side = layout->width < layout->height ?
                   layout->width : layout->height;
    int touch_size = min_side / 9;
    if (touch_size < 32) touch_size = 32;
    if (touch_size > 44) touch_size = 44;
    const int pane_w = touch_size * 2 + (touch_size > 40 ? 8 : 0);
    const int pane_h = touch_size;
    const int depth_w = touch_size > 32 ? 28 : 12;
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
        const int available = layout->height - layout->safe_top -
                              layout->safe_bottom;
        int class_h = (available - 66) / PD_CLASS_COUNT;
        if (class_h < 34) class_h = 34;
        if (class_h > 66) class_h = 66;
        int class_top = layout->safe_top +
            (available - PD_CLASS_COUNT * class_h -
             4 * (PD_CLASS_COUNT - 1)) / 2;
        if (available < 155) {
            class_h = 32;
            class_top = layout->safe_top + 17;
            set_rect(&layout->menu_back, (layout->width - 32) / 2,
                     layout->safe_top - 17, 32, 32);
        }
        if (available > 220 && class_top < layout->menu_back.y +
                                             layout->menu_back.h + 4)
            class_top = layout->menu_back.y + layout->menu_back.h + 4;
        for (int index = 0; index < PD_CLASS_COUNT; ++index)
            set_rect(&layout->class_button[index], layout->safe_left + 4,
                     class_top + index * (class_h + (available < 155 ? 2 : 4)),
                     layout->width - layout->safe_left -
                         layout->safe_right - 8, class_h);
    }
    if (shape == 2) {
        const int initial_y = layout->hero_info.y;
        while (layout->hero_info.y + layout->hero_info.h <
               layout->height - layout->safe_bottom - layout->bar_h) {
            int left_bound = shape_rect_inset(layout, shape, radii,
                                              layout->hero_info.y,
                                              layout->hero_info.h, 0);
            int right_bound = shape_rect_inset(layout, shape, radii,
                                               layout->hero_info.y,
                                               layout->hero_info.h, 1);
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
                                layout->hero_info.h, 0);
    if (left < layout->hero_info.x) left = layout->hero_info.x;
    layout->hero_info.x = left;
    left = shape_rect_inset(layout, shape, radii, layout->menu_back.y,
                            layout->menu_back.h, 1);
    if (left > layout->safe_right)
        layout->menu_back.x = layout->width - left - layout->menu_back.w;
    left = shape_rect_inset(layout, shape, radii, y, pane_h, 1);
    if (right < left) right = left;
    if (shape == 2 && layout->width - right - pane_w - depth_w <
                          layout->hero_info.x + layout->hero_info.w + 2) {
        y = layout->hero_info.y + layout->hero_info.h + 3;
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
    const int min_side = width < height ? width : height;
    int touch_size = min_side / 9;
    if (touch_size < 32) touch_size = 32;
    if (touch_size > 44) touch_size = 44;

    layout->width = width;
    layout->height = height;
    /* Panel insets keep the controls inside the touchable area. */
    layout->safe_top = safe_top < 0 ? 0 : safe_top;
    layout->safe_right = safe_right < 0 ? 0 : safe_right;
    layout->safe_bottom = safe_bottom < 0 ? 0 : safe_bottom;
    layout->safe_left = safe_left < 0 ? 0 : safe_left;
    layout->display_shape = 0;
    layout->zoom = 0;
    layout->camera_dx = 0;
    layout->camera_dy = 0;
    layout->camera_manual = 0;
    layout->hud_h = 40;
    layout->bar_h = touch_size + 6;
    bar_y = height - layout->safe_bottom - layout->bar_h;
    content_x = layout->safe_left;
    content_w = width - layout->safe_left - layout->safe_right;

    pd_layout_set_zoom(layout, 1);

    /* The original has no movement pad: taps walk the hero. Five toolbar slots
     * carry the wait, search, potion, pack and stairs actions. */
    button_w = content_w < touch_size * PD_BUTTON_COUNT + 12 ?
               (content_w - 12) / PD_BUTTON_COUNT : touch_size;
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
        int menu_w = content_w - 24;
        if (menu_w > 224) menu_w = 224;
        const int small_w = content_w < 224 ?
                            (content_w - 12) / 3 : touch_size + 40;
        const int small_start = center - (3 * small_w + 6) / 2;
        set_rect(&layout->menu_primary, center - menu_w / 2,
                 menu_y - 15, menu_w, touch_size + 2);
        set_rect(&layout->menu_secondary, center - menu_w / 2,
                 menu_y + touch_size - 10, menu_w, touch_size);
        set_rect(&layout->menu_back, width - layout->safe_right - touch_size,
                 layout->safe_top + 2, touch_size, touch_size);
        set_rect(&layout->title_rankings, small_start,
                 menu_y + touch_size + 17, small_w, touch_size);
        set_rect(&layout->title_journal, small_start + small_w + 3,
                 menu_y + touch_size + 17, small_w, touch_size);
        set_rect(&layout->title_settings, small_start + 2 * (small_w + 3),
                 menu_y + touch_size + 17, small_w, touch_size);
        for (int index = 0; index < PD_SAVE_SLOTS; ++index)
            set_rect(&layout->save_row[index], center - menu_w / 2,
                     menu_y - 84 + index * (touch_size + 2),
                     menu_w, touch_size);
        if (touch_size > 32) layout->hud_h = 58;
        set_rect(&layout->hero_info, layout->safe_left + 1,
                 layout->safe_top, touch_size > 32 ? 124 : 82,
                 touch_size > 32 ? 57 : 38);
        const int pause_w = content_w < 156 ? content_w - 18 : 138;
        const int pause_left = center - pause_w / 2;
        const int pause_top = menu_y - 35;
        set_rect(&layout->pause_settings, pause_left, pause_top,
                 pause_w, touch_size);
        set_rect(&layout->pause_menu, pause_left, pause_top + touch_size + 4,
                 pause_w, touch_size);
        set_rect(&layout->shop_buy, pause_left, menu_y + 6,
                 pause_w, touch_size);
        const int shop_w = content_w < 170 ? content_w - 10 : 170;
        set_rect(&layout->shop_close, center + shop_w / 2 - touch_size - 4,
                 menu_y - 70, touch_size, touch_size);
        set_rect(&layout->zoom_out, center - 61, menu_y - 14, 42,
                 touch_size);
        set_rect(&layout->zoom_in, center + 19, menu_y - 14, 42,
                 touch_size);
        const int settings_w = content_w < 218 ? content_w - 12 : 206;
        set_rect(&layout->settings_back,
                 center + settings_w / 2 - touch_size - 4,
                 (height - 160) / 2, touch_size, touch_size);
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
        const int max_panel_h = touch_size > 32 ? 244 : 184;
        const int panel_h = available_h < max_panel_h ?
                            available_h : max_panel_h;
        const int panel_y = layout->safe_top +
                            (height - layout->safe_top - layout->safe_bottom - panel_h) / 2;
        const int header_h = panel_h < 170 ? 30 : 36;
        const int top = panel_y + header_h;
        const int row_step = (panel_h - header_h - touch_size - 30) / 3;
        const int slot_w = (panel_w - 24) / 5;
        for (int index = 0; index < PD_BAG_ROWS; ++index) {
            set_rect(&layout->bag_row[index], panel_x + 12 +
                     (index % 5) * slot_w,
                     top + (index / 5) * row_step,
                     slot_w - 2, row_step - 2);
            set_rect(&layout->bag_use[index], panel_x + 12,
                     panel_y + panel_h - touch_size - 5,
                     panel_w / 2 - 16, touch_size);
            set_rect(&layout->bag_drop[index], panel_x + panel_w / 2 + 4,
                     panel_y + panel_h - touch_size - 5,
                     panel_w / 2 - 16, touch_size);
        }
        set_rect(&layout->bag_close,
                 panel_x + panel_w - touch_size - 4,
                 panel_y, touch_size, touch_size);
        const int info_w = content_w - 16 < 210 ? content_w - 16 : 210;
        const int info_h = available_h - 4 < 156 ? available_h - 4 : 156;
        set_rect(&layout->info_close,
                 width / 2 + info_w / 2 - touch_size - 4,
                 (height - info_h) / 2, touch_size, touch_size);
        const int journal_w = content_w - 12;
        set_rect(&layout->journal_close,
                 width / 2 + journal_w / 2 - touch_size - 4,
                 layout->safe_top + 12, touch_size, touch_size);
    }
}
