#include "input.h"

#include "dungeon.h"
#include "rng.h"
#include "strings.h"

typedef struct {
    uint8_t active;
    uint8_t id;
    int x, y;
} pd_touch_t;

static pd_touch_t g_touches[2];
static int g_pinch_distance;
static uint8_t g_pinch_consumed;

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
    if (pd_rect_contains(&layout->hero_info, x, y)) {
        game->phase = PD_PHASE_INFO;
        return;
    }
    if (pd_rect_contains(&layout->settings, x, y)) {
        game->phase = PD_PHASE_PAUSE;
        return;
    }
    if (pd_rect_contains(&layout->journal, x, y) ||
        pd_rect_contains(&layout->depth, x, y)) {
        game->phase = PD_PHASE_INFO;
        return;
    }
    if (x < layout->map_x || y < layout->map_y ||
        x >= layout->map_x + layout->map_w ||
        y >= layout->map_y + layout->map_h)
        return;
    pd_layout_camera(layout, game->hero.x, game->hero.y, &camera_x, &camera_y);
    tile_x = camera_x + (x - layout->map_x) / layout->tile_pixels;
    tile_y = camera_y + (y - layout->map_y) / layout->tile_pixels;
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
        }
        return;
    }
    g_touches[slot].active = 0;
    if (phase != PXA_POINTER_UP || g_pinch_consumed) {
        if (!g_touches[0].active && !g_touches[1].active)
            g_pinch_consumed = 0;
        return;
    }
    switch (game->phase) {
        case PD_PHASE_TITLE:
            if (pd_rect_contains(&layout->menu_primary, x, y))
                game->phase = PD_PHASE_SAVES;
            return;
        case PD_PHASE_SAVES:
            if (pd_rect_contains(&layout->menu_back, x, y)) {
                game->phase = PD_PHASE_TITLE;
            } else if (pd_rect_contains(&layout->menu_secondary, x, y)) {
                game->phase = PD_PHASE_CLASS;
            } else if (pd_rect_contains(&layout->menu_primary, x, y)) {
                game->phase = game->hero.hp > 0 ? PD_PHASE_PLAY : PD_PHASE_CLASS;
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
        case PD_PHASE_SETTINGS:
            if (pd_rect_contains(&layout->zoom_out, x, y))
                pd_layout_set_zoom(layout, layout->zoom - 1);
            else if (pd_rect_contains(&layout->zoom_in, x, y))
                pd_layout_set_zoom(layout, layout->zoom + 1);
            else if (pd_rect_contains(&layout->settings_back, x, y))
                game->phase = PD_PHASE_PAUSE;
            return;
        case PD_PHASE_PAUSE:
            if (pd_rect_contains(&layout->pause_settings, x, y))
                game->phase = PD_PHASE_SETTINGS;
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
        if ((pressed & PXA_CONTROLLER_A) != 0) game->phase = PD_PHASE_SAVES;
        return;
    }
    if (game->phase == PD_PHASE_SAVES) {
        if ((pressed & PXA_CONTROLLER_A) != 0)
            game->phase = game->hero.hp > 0 ? PD_PHASE_PLAY : PD_PHASE_CLASS;
        else if ((pressed & PXA_CONTROLLER_START) != 0)
            game->phase = PD_PHASE_CLASS;
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
        game->phase == PD_PHASE_SETTINGS || game->phase == PD_PHASE_PAUSE) {
        if ((pressed & (PXA_CONTROLLER_B | PXA_CONTROLLER_START)) != 0)
            game->phase = game->phase == PD_PHASE_SETTINGS ? PD_PHASE_PAUSE :
                          PD_PHASE_PLAY;
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
            game->phase = PD_PHASE_PAUSE;
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
