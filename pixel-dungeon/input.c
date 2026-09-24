#include "input.h"

#include "dungeon.h"
#include "rng.h"
#include "strings.h"

typedef struct {
    uint8_t active;
    uint8_t id;
    int x, y, start_x, start_y, drag_x, drag_y;
    uint8_t dragged;
} pd_touch_t;

static pd_touch_t g_touches[2];
static int g_pinch_distance;
static uint8_t g_pinch_consumed;

static void select_visible_slot(pd_game_t *game) {
    uint8_t slots[PD_SAVE_SLOTS];
    const int count = pd_game_visible_save_slots(game, slots);
    for (int index = 0; index < count; ++index)
        if (slots[index] == game->selected_slot) return;
    game->selected_slot = slots[0];
}

static int touch_distance(void) {
    const int dx = g_touches[0].x - g_touches[1].x;
    const int dy = g_touches[0].y - g_touches[1].y;
    const int abs_x = dx < 0 ? -dx : dx;
    const int abs_y = dy < 0 ? -dy : dy;
    return abs_x > abs_y ? abs_x + abs_y / 2 : abs_y + abs_x / 2;
}

static void handle_play_tap(pd_game_t *game, const pd_layout_t *layout, int x,
                            int y) {
    int camera_x;
    int camera_y;
    int tile_x;
    int tile_y;

    for (int index = 0; index < PD_BUTTON_COUNT; ++index) {
        if (!pd_rect_contains(&layout->button[index], x, y)) continue;
        switch (index) {
            case PD_BUTTON_WAIT:
                pd_game_hero_wait(game);
                break;
            case PD_BUTTON_SEARCH:
                pd_game_hero_search(game);
                break;
            case PD_BUTTON_POTION:
                pd_game_hero_potion(game);
                break;
            case PD_BUTTON_BAG:
                game->phase = PD_PHASE_BAG;
                break;
            default:
                pd_game_hero_stairs(game);
                break;
        }
        return;
    }
    const pd_rect_t portrait = pd_layout_hero_portrait(layout);
    if (pd_rect_contains(&portrait, x, y)) {
        game->phase = PD_PHASE_INFO;
        return;
    }
    if (pd_rect_contains(&layout->settings, x, y)) {
        game->phase = PD_PHASE_PAUSE;
        return;
    }
    if (pd_rect_contains(&layout->journal, x, y) ||
        pd_rect_contains(&layout->depth, x, y)) {
        game->phase = PD_PHASE_JOURNAL;
        return;
    }
    if (x < layout->map_x || y < layout->map_y ||
        x >= layout->map_x + layout->map_w ||
        y >= layout->map_y + layout->map_h)
        return;
    pd_layout_camera_visual_pixels(layout, game->hero.x, game->hero.y,
                                   game->hero_from_x, game->hero_from_y,
                                   game->hero_moving, &camera_x, &camera_y);
    tile_x = x - layout->map_x + camera_x;
    tile_y = y - layout->map_y + camera_y;
    if (tile_x < 0 || tile_y < 0) return;
    tile_x /= layout->tile_pixels;
    tile_y /= layout->tile_pixels;
    pd_game_tap(game, tile_x, tile_y);
}

void pd_input_pointer(pd_game_t *game, pd_layout_t *layout, int x, int y,
                      uint8_t pointer_id, uint8_t phase, uint64_t timestamp_us) {
    int slot = -1;
    for (int index = 0; index < 2; ++index)
        if (g_touches[index].active && g_touches[index].id == pointer_id)
            slot = index;
    if (phase == PXA_POINTER_DOWN) {
        if (slot < 0)
            for (int index = 0; index < 2; ++index)
                if (!g_touches[index].active) {
                    slot = index;
                    break;
                }
        if (slot < 0) return;
        g_touches[slot].active = 1;
        g_touches[slot].id = pointer_id;
        g_touches[slot].x = x;
        g_touches[slot].y = y;
        g_touches[slot].start_x = x;
        g_touches[slot].start_y = y;
        g_touches[slot].drag_x = x;
        g_touches[slot].drag_y = y;
        g_touches[slot].dragged = 0;
        if (g_touches[0].active && g_touches[1].active) {
            g_pinch_distance = touch_distance();
            g_pinch_consumed = 1;
        } else g_pinch_consumed = 0;
        return;
    }
    if (slot < 0) return;
    g_touches[slot].x = x;
    g_touches[slot].y = y;
    if (phase == PXA_POINTER_MOVE) {
        if (g_touches[0].active && g_touches[1].active &&
            game->phase == PD_PHASE_PLAY) {
            const int distance = touch_distance();
            const int delta = distance - g_pinch_distance;
            if (delta >= 20 || delta <= -20) {
                pd_layout_set_zoom(layout, layout->zoom + (delta > 0 ? 1 : -1));
                g_pinch_distance = distance;
            }
        } else if (game->phase == PD_PHASE_PLAY && !g_pinch_consumed &&
                   x >= layout->map_x && y >= layout->map_y &&
                   g_touches[slot].start_x >= layout->map_x &&
                   g_touches[slot].start_y >= layout->map_y &&
                   g_touches[slot].start_x < layout->map_x + layout->map_w &&
                   g_touches[slot].start_y < layout->map_y + layout->map_h) {
            const int dx = g_touches[slot].start_x - x;
            const int dy = g_touches[slot].start_y - y;
            if (dx * dx + dy * dy > 64) g_touches[slot].dragged = 1;
            if (g_touches[slot].dragged) {
                pd_layout_pan_pixels(layout, game->hero.x, game->hero.y,
                                     g_touches[slot].drag_x - x,
                                     g_touches[slot].drag_y - y);
                g_touches[slot].drag_x = x;
                g_touches[slot].drag_y = y;
                game->walk_active = 0;
            }
        }
        return;
    }
    const uint8_t dragged = g_touches[slot].dragged;
    g_touches[slot].active = 0;
    if (phase != PXA_POINTER_UP || g_pinch_consumed || dragged) {
        if (!g_touches[0].active && !g_touches[1].active)
            g_pinch_consumed = 0;
        return;
    }
    switch (game->phase) {
        case PD_PHASE_TITLE:
            if (pd_rect_contains(&layout->menu_primary, x, y)) {
                game->settings_from_title = 0;
                game->phase = PD_PHASE_SAVES;
                select_visible_slot(game);
            } else if (pd_rect_contains(&layout->title_settings, x, y)) {
                game->settings_from_title = 1;
                game->phase = PD_PHASE_SETTINGS;
            } else if (pd_rect_contains(&layout->title_rankings, x, y))
                game->phase = PD_PHASE_RANKINGS;
            else if (pd_rect_contains(&layout->title_journal, x, y)) {
                game->settings_from_title = 1;
                game->phase = PD_PHASE_JOURNAL;
            }
            return;
        case PD_PHASE_SAVES:
            if (pd_rect_contains(&layout->menu_back, x, y)) {
                game->phase = PD_PHASE_TITLE;
            } else {
                uint8_t slots[PD_SAVE_SLOTS];
                const int count = pd_game_visible_save_slots(game, slots);
                const int page_size = pd_layout_save_page_size(layout);
                int selected_index = 0;
                for (int index = 0; index < count; ++index)
                    if (slots[index] == game->selected_slot)
                        selected_index = index;
                const int start = pd_layout_save_page_start(layout,
                                                             selected_index);
                const int visible = count - start < page_size ?
                                    count - start : page_size;
                if (count > page_size) {
                    const pd_rect_t button = pd_layout_save_page_button(
                        layout, visible);
                    if (pd_rect_contains(&button, x, y)) {
                        const int next = start + page_size;
                        game->selected_slot = slots[next < count ? next : 0];
                        return;
                    }
                }
                for (int index = 0; index < visible; ++index) {
                    const pd_rect_t row = pd_layout_save_row(layout, visible,
                                                               index);
                    if (!pd_rect_contains(&row, x, y)) continue;
                    game->selected_slot = slots[start + index];
                    game->phase = game->slots[slots[start + index]].occupied ?
                                  PD_PHASE_PLAY : PD_PHASE_CLASS;
                    break;
                }
            }
            return;
        case PD_PHASE_CLASS:
            if (pd_rect_contains(&layout->menu_back, x, y)) {
                game->phase = PD_PHASE_SAVES;
                return;
            }
            for (int index = 0; index < PD_CLASS_COUNT; ++index) {
                if (pd_rect_contains(&layout->class_button[index], x, y)) {
                    pd_game_reset(game, pd_rng_mix(
                        (uint32_t)timestamp_us ^ game->run_seed,
                        (uint32_t)(timestamp_us >> 32) ^ game->turn));
                    pd_game_start_run(game, (uint8_t)index);
                    return;
                }
            }
            return;
        case PD_PHASE_DEAD:
            if (pd_rect_contains(&layout->menu_primary, x, y)) {
                game->phase = PD_PHASE_CLASS;
                game->message_total = 0;
            } else if (pd_rect_contains(&layout->menu_secondary, x, y)) {
                game->phase = PD_PHASE_TITLE;
                game->message_total = 0;
            }
            return;
        case PD_PHASE_WON:
            game->phase = PD_PHASE_TITLE;
            game->message_total = 0;
            return;
        case PD_PHASE_BAG:
            if (pd_rect_contains(&layout->bag_close, x, y)) {
                game->phase = PD_PHASE_PLAY;
                return;
            }
            for (int index = 0; index < game->bag_count; ++index)
                if (pd_rect_contains(&layout->bag_row[index], x, y)) {
                    game->bag_selected = (int8_t)index;
                    return;
                }
            if (game->bag_count > 0) {
                int selected = game->bag_selected;
                if (selected < 0 || selected >= game->bag_count) selected = 0;
                if (pd_rect_contains(&layout->bag_use[0], x, y)) {
                    pd_game_bag_use(game, selected);
                    game->phase = PD_PHASE_PLAY;
                } else if (pd_rect_contains(&layout->bag_drop[0], x, y)) {
                    pd_game_bag_drop(game, selected);
                    game->phase = PD_PHASE_PLAY;
                }
            }
            return;
        case PD_PHASE_INFO:
            game->phase = PD_PHASE_PLAY;
            return;
        case PD_PHASE_JOURNAL:
            game->phase = game->settings_from_title ? PD_PHASE_TITLE : PD_PHASE_PLAY;
            return;
        case PD_PHASE_RANKINGS:
            game->phase = PD_PHASE_TITLE;
            return;
        case PD_PHASE_SETTINGS:
            if (pd_rect_contains(&layout->zoom_out, x, y))
                pd_layout_set_zoom(layout, layout->zoom - 1);
            else if (pd_rect_contains(&layout->zoom_in, x, y))
                pd_layout_set_zoom(layout, layout->zoom + 1);
            else if (pd_rect_contains(&layout->settings_back, x, y))
                game->phase = game->settings_from_title ?
                              PD_PHASE_TITLE : PD_PHASE_PAUSE;
            return;
        case PD_PHASE_SHOP:
            if (pd_rect_contains(&layout->shop_buy, x, y))
                pd_game_shop_buy(game);
            else if (pd_rect_contains(&layout->shop_close, x, y))
                game->phase = PD_PHASE_PLAY;
            return;
        case PD_PHASE_PAUSE:
            if (pd_rect_contains(&layout->pause_settings, x, y)) {
                game->settings_from_title = 0;
                game->phase = PD_PHASE_SETTINGS;
            }
            else if (pd_rect_contains(&layout->pause_menu, x, y))
                game->phase = PD_PHASE_TITLE;
            else
                game->phase = PD_PHASE_PLAY;
            return;
        default:
            handle_play_tap(game, layout, x, y);
            return;
    }
}

void pd_input_controller(pd_game_t *game, uint32_t buttons, uint32_t previous) {
    const uint32_t pressed = buttons & ~previous;
    if (game->phase == PD_PHASE_TITLE) {
        if ((pressed & PXA_CONTROLLER_A) != 0) {
            game->settings_from_title = 0;
            game->phase = PD_PHASE_SAVES;
            select_visible_slot(game);
        }
        return;
    }
    if (game->phase == PD_PHASE_SAVES) {
        uint8_t slots[PD_SAVE_SLOTS];
        const int count = pd_game_visible_save_slots(game, slots);
        int current = 0;
        for (int index = 0; index < count; ++index)
            if (slots[index] == game->selected_slot) current = index;
        if ((pressed & PXA_CONTROLLER_DOWN) != 0)
            game->selected_slot = slots[(current + 1) % count];
        else if ((pressed & PXA_CONTROLLER_UP) != 0)
            game->selected_slot = slots[(current + count - 1) % count];
        else if ((pressed & PXA_CONTROLLER_A) != 0)
            game->phase = game->slots[game->selected_slot].occupied ?
                          PD_PHASE_PLAY : PD_PHASE_CLASS;
        else if ((pressed & PXA_CONTROLLER_B) != 0)
            game->phase = PD_PHASE_TITLE;
        return;
    }
    if (game->phase == PD_PHASE_CLASS) {
        if ((pressed & PXA_CONTROLLER_A) != 0) {
            pd_game_reset(game, pd_rng_mix(game->run_seed, game->turn + 1u));
            pd_game_start_run(game, 0);
        } else if ((pressed & PXA_CONTROLLER_B) != 0)
            game->phase = PD_PHASE_SAVES;
        return;
    }
    if (game->phase == PD_PHASE_DEAD || game->phase == PD_PHASE_WON) {
        if ((pressed & PXA_CONTROLLER_A) != 0) game->phase = PD_PHASE_TITLE;
        return;
    }
    if (game->phase == PD_PHASE_BAG || game->phase == PD_PHASE_INFO ||
        game->phase == PD_PHASE_SETTINGS || game->phase == PD_PHASE_PAUSE ||
        game->phase == PD_PHASE_JOURNAL || game->phase == PD_PHASE_RANKINGS) {
        if ((pressed & (PXA_CONTROLLER_B | PXA_CONTROLLER_START)) != 0)
            game->phase = game->phase == PD_PHASE_SETTINGS ?
                          (game->settings_from_title ? PD_PHASE_TITLE : PD_PHASE_PAUSE) :
                          (game->phase == PD_PHASE_RANKINGS ||
                           (game->phase == PD_PHASE_JOURNAL && game->settings_from_title) ?
                           PD_PHASE_TITLE : PD_PHASE_PLAY);
        return;
    }
    if (game->phase == PD_PHASE_SHOP) {
        if ((pressed & PXA_CONTROLLER_A) != 0) pd_game_shop_buy(game);
        else if ((pressed & PXA_CONTROLLER_B) != 0) game->phase = PD_PHASE_PLAY;
        return;
    }
    if ((pressed & PXA_CONTROLLER_UP) != 0)
        pd_game_hero_step(game, 0, -1);
    else if ((pressed & PXA_CONTROLLER_DOWN) != 0)
        pd_game_hero_step(game, 0, 1);
    else if ((pressed & PXA_CONTROLLER_LEFT) != 0)
        pd_game_hero_step(game, -1, 0);
    else if ((pressed & PXA_CONTROLLER_RIGHT) != 0)
        pd_game_hero_step(game, 1, 0);
    else if ((pressed & PXA_CONTROLLER_A) != 0)
        pd_game_hero_search(game);
    else if ((pressed & PXA_CONTROLLER_B) != 0)
        game->phase = PD_PHASE_BAG;
    else if ((pressed & PXA_CONTROLLER_START) != 0)
        pd_game_hero_stairs(game);
}

int pd_input_back(pd_game_t *game) {
    switch (game->phase) {
        case PD_PHASE_TITLE:
            return 0;
        case PD_PHASE_PLAY:
            game->phase = PD_PHASE_PAUSE;
            break;
        case PD_PHASE_SETTINGS:
            game->phase = game->settings_from_title ? PD_PHASE_TITLE : PD_PHASE_PAUSE;
            break;
        case PD_PHASE_RANKINGS:
            game->phase = PD_PHASE_TITLE;
            break;
        case PD_PHASE_JOURNAL:
            game->phase = game->settings_from_title ? PD_PHASE_TITLE : PD_PHASE_PLAY;
            break;
        case PD_PHASE_SAVES:
            game->phase = PD_PHASE_TITLE;
            break;
        case PD_PHASE_CLASS:
            game->phase = PD_PHASE_SAVES;
            break;
        case PD_PHASE_DEAD:
        case PD_PHASE_WON:
            game->phase = PD_PHASE_TITLE;
            game->message_total = 0;
            break;
        default:
            game->phase = PD_PHASE_PLAY;
            break;
    }
    return 1;
}
