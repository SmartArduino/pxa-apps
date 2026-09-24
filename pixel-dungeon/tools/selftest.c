/* Native self-test for the Pixel Dungeon game model.
 *
 * Links dungeon.c/game.c/assets.c without any Host dependency and checks the
 * invariants that are hard to see on a device: floor connectivity, entity
 * placement, save/restore round-trips and a long scripted play session.
 *
 *   cc -O1 -fsanitize=address,undefined -I. \
 *      -I../../../deps/pxa-system/sdk/guest-c/include tools/selftest.c \
 *      game.c dungeon.c layout.c input.c font.c assets.c strings.c font_data.c \
 *      -o /tmp/pd-selftest && /tmp/pd-selftest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeon.h"
#include "font.h"
#include "game.h"
#include "input.h"
#include "layout.h"
#include "strings.h"
#include "wall_tiles.h"

static int g_failures;

static void check(int condition, const char *what) {
    if (condition) return;
    printf("FAIL: %s\n", what);
    ++g_failures;
}

static int walkable_at(const pd_level_t *level, int x, int y) {
    return pd_in_bounds(x, y) && pd_tile_walkable(pd_tile_at(level, x, y));
}

static void tap_rect(pd_game_t *game, pd_layout_t *layout,
                     const pd_rect_t *rect) {
    const int x = rect->x + rect->w / 2;
    const int y = rect->y + rect->h / 2;
    pd_input_pointer(game, layout, x, y, 0, PXA_POINTER_DOWN, 100);
    pd_input_pointer(game, layout, x, y, 0, PXA_POINTER_UP, 101);
}

static void test_menus_and_slots(void) {
    pd_game_t game = {0};
    pd_layout_t layout;
    pd_layout_build(&layout, 296, 240, 8, 10, 8, 10);
    pd_game_reset(&game, 713);
    check(layout.menu_back.x > layout.width / 2,
          "scene exit icon is on the original's right side");
    tap_rect(&game, &layout, &layout.title_settings);
    check(game.phase == PD_PHASE_SETTINGS, "title opens settings");
    tap_rect(&game, &layout, &layout.settings_back);
    check(game.phase == PD_PHASE_TITLE,
          "settings close icon returns to its originating title");
    tap_rect(&game, &layout, &layout.title_rankings);
    check(game.phase == PD_PHASE_RANKINGS, "title opens rankings");
    tap_rect(&game, &layout, &layout.menu_back);
    check(game.phase == PD_PHASE_TITLE,
          "rankings exit icon returns to the title");
    tap_rect(&game, &layout, &layout.menu_primary);
    check(game.phase == PD_PHASE_SAVES, "title opens save slots");
    uint8_t slots[PD_SAVE_SLOTS];
    check(pd_game_visible_save_slots(&game, slots) == 1 && slots[0] == 0,
          "empty save screen shows only the next new-game slot");
    for (int index = 0; index < PD_SAVE_SLOTS - 1; ++index)
        game.slots[index].occupied = 1;
    check(pd_game_visible_save_slots(&game, slots) == PD_SAVE_SLOTS,
          "occupied slots plus one empty slot match the original");
    const pd_rect_t last = pd_layout_save_row(&layout, PD_SAVE_SLOTS,
                                               PD_SAVE_SLOTS - 1);
    tap_rect(&game, &layout, &last);
    check(game.phase == PD_PHASE_CLASS &&
          game.selected_slot == PD_SAVE_SLOTS - 1,
          "fifth empty slot starts hero selection");
    tap_rect(&game, &layout, &layout.menu_back);
    check(game.phase == PD_PHASE_SAVES,
          "hero selection exit icon returns to save slots");
    pd_layout_build(&layout, 176, 176, 8, 10, 8, 10);
    pd_layout_fit_display_shape(&layout, 2, NULL);
    check(pd_layout_save_page_size(&layout) == 2,
          "very small circles show two save slots per page");
    game.selected_slot = slots[0];
    const pd_rect_t page_button = pd_layout_save_page_button(&layout, 2);
    check(page_button.y + page_button.h <=
              layout.height - layout.safe_bottom,
          "save page control fits the tiny screen safe area");
    tap_rect(&game, &layout, &page_button);
    check(game.phase == PD_PHASE_SAVES && game.selected_slot == slots[2],
          "save page control advances to the next pair");
    tap_rect(&game, &layout, &page_button);
    check(game.selected_slot == slots[4],
          "save page control reaches the fifth slot");
    const pd_rect_t compact_last = pd_layout_save_row(&layout, 1, 0);
    tap_rect(&game, &layout, &compact_last);
    check(game.phase == PD_PHASE_CLASS && game.selected_slot == slots[4],
          "fifth slot remains tappable on the tiny round display");
}

static void test_back_navigation(void) {
    pd_game_t game;
    memset(&game, 0, sizeof(game));
    game.phase = PD_PHASE_TITLE;
    check(!pd_input_back(&game), "Back exits from the title page");
    game.phase = PD_PHASE_PLAY;
    check(pd_input_back(&game) && game.phase == PD_PHASE_PAUSE,
          "Back pauses play instead of exiting");
    check(pd_input_back(&game) && game.phase == PD_PHASE_PLAY,
          "Back resumes from pause");
    game.phase = PD_PHASE_SETTINGS;
    check(pd_input_back(&game) && game.phase == PD_PHASE_PAUSE,
          "Back returns from settings to pause");
    game.phase = PD_PHASE_BAG;
    check(pd_input_back(&game) && game.phase == PD_PHASE_PLAY,
          "Back closes the inventory");
    game.phase = PD_PHASE_INFO;
    check(pd_input_back(&game) && game.phase == PD_PHASE_PLAY,
          "Back closes hero information");
    game.phase = PD_PHASE_CLASS;
    check(pd_input_back(&game) && game.phase == PD_PHASE_SAVES,
          "Back returns from hero selection to saves");
    check(pd_input_back(&game) && game.phase == PD_PHASE_TITLE,
          "Back returns from saves to the title page");
}

static void test_generation(void) {
    static uint8_t path_x[8];
    static uint8_t path_y[8];
    pd_level_t level;
    for (uint32_t seed = 1; seed <= 25; ++seed) {
        for (int depth = 1; depth <= 25; ++depth) {
            pd_level_generate(&level, seed * 7919u, (uint8_t)depth);
            check(level.room_count >= 1, "a floor has at least one room");
            {
                int detached_walls = 0;
                for (int cell = 0; cell < PD_MAP_TILES; ++cell) {
                    const int x = cell % PD_MAP_W;
                    const int y = cell / PD_MAP_W;
                    int adjacent_to_floor = 0;
                    if (!pd_tile_wall(level.tiles[cell])) continue;
                    for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                        for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                            const uint8_t neighbor = pd_tile_at(
                                &level, x + offset_x, y + offset_y);
                            if (neighbor != PD_TILE_VOID &&
                                !pd_tile_wall(neighbor))
                                adjacent_to_floor = 1;
                        }
                    }
                    if (!adjacent_to_floor) ++detached_walls;
                }
                check(detached_walls == 0,
                      "walls do not spread away from carved terrain");
            }
            {
                int doors = 0;
                for (int cell = 0; cell < PD_MAP_TILES; ++cell) {
                    if (!pd_tile_door(level.tiles[cell])) continue;
                    ++doors;
                    const int x = cell % PD_MAP_W;
                    const int y = cell / PD_MAP_W;
                    for (int offset_y = -1; offset_y <= 1; ++offset_y)
                        for (int offset_x = -1; offset_x <= 1; ++offset_x)
                            if (offset_x != 0 || offset_y != 0)
                                if (y + offset_y > y ||
                                    (y + offset_y == y && x + offset_x > x))
                                    check(!pd_tile_door(pd_tile_at(
                                              &level, x + offset_x,
                                              y + offset_y)),
                                          "doorways do not touch");
                }
                check(doors > 0, "a floor has real doorways");
            }
            check(pd_tile_walkable(pd_tile_at(&level, level.entrance_x,
                                              level.entrance_y)),
                  "entrance is walkable");
            if (depth < 25)
                check(pd_tile_walkable(pd_tile_at(&level, level.exit_x,
                                                  level.exit_y)),
                      "exit is walkable");
            if (depth < 25)
                check(level.entrance_x != level.exit_x ||
                          level.entrance_y != level.exit_y,
                      "exit differs from the entrance");
            for (int index = 0; index < level.room_count; ++index) {
                const pd_room_t *room = &level.rooms[index];
                int open_x = -1;
                int open_y = -1;
                int steps;
                /* Statues and pots may block the exact centre, so reach the
                 * first open tile of the room instead. */
                for (int row = 0; row < room->h && open_x < 0; ++row) {
                    for (int column = 0; column < room->w; ++column) {
                        if (walkable_at(&level, room->x + column,
                                        room->y + row)) {
                            open_x = room->x + column;
                            open_y = room->y + row;
                            break;
                        }
                    }
                }
                check(open_x >= 0, "every room has an open tile");
                if (open_x < 0) continue;
                if (open_x == level.entrance_x && open_y == level.entrance_y)
                    continue; /* the entrance tile is the origin */
                steps = pd_level_path(&level, level.entrance_x,
                                      level.entrance_y, open_x, open_y, path_x,
                                      path_y, 8);
                check(steps > 0, "every room is reachable from the entrance");
            }
            /* A floor must connect the entrance to the exit. */
            if (depth < 25)
                check(pd_level_path(&level, level.entrance_x,
                                    level.entrance_y, level.exit_x,
                                    level.exit_y, path_x, path_y, 8) > 0,
                      "stairs connect");
            /* Regeneration is deterministic. */
            {
                pd_level_t again;
                pd_level_generate(&again, seed * 7919u, (uint8_t)depth);
                check(memcmp(level.tiles, again.tiles, PD_MAP_TILES) == 0,
                      "generation is deterministic");
            }
        }
    }
    {
        pd_level_t old_level;
        pd_level_t repeated;
        pd_level_generate_legacy(&old_level, 777, 5);
        pd_level_generate_legacy(&repeated, 777, 5);
        check(memcmp(old_level.tiles, repeated.tiles, PD_MAP_TILES) == 0,
              "legacy floors remain deterministic");
        pd_level_generate(&repeated, 777, 5);
        check(memcmp(old_level.tiles, repeated.tiles, PD_MAP_TILES) != 0,
              "new runs use the updated floor layout");
    }
}

static void test_mob_death(void) {
    pd_game_t game;
    pd_game_reset(&game, 239);
    pd_game_start_run(&game, 0);
    for (int index = 0; index < PD_MOBS_MAX; ++index)
        game.mobs[index].type = 0xFF;
    game.hero.hp = game.hero.max_hp = 3000;
    game.hero.str = 100;
    game.mobs[0].type = 0;
    game.mobs[0].hp = 1;
    game.mobs[0].x = game.hero.x + 1;
    game.mobs[0].y = game.hero.y;
    game.level.tiles[game.mobs[0].y * PD_MAP_W + game.mobs[0].x] =
        game.level.floor_tile;
    for (int attempt = 0; attempt < 50 && game.kills == 0; ++attempt)
        pd_game_hero_step(&game, 1, 0);
    check(game.kills == 1 && game.mobs[0].hp == 0 &&
          game.mobs[0].dying > 0, "lethal strike sets HP to zero once");
    for (int frame = 0; frame < 22; ++frame) pd_game_tick(&game);
    check(game.mobs[0].type == 0xFF && game.mobs[0].hp == 0,
          "mob never revives after death animation");
}

static void test_mob_movement(void) {
    pd_game_t game;
    pd_game_reset(&game, 194);
    pd_game_start_run(&game, 0);
    for (int index = 0; index < PD_MOBS_MAX; ++index)
        game.mobs[index].type = 0xFF;
    game.mobs[0].type = 0;
    game.mobs[0].hp = 10;
    game.mobs[0].awake = 1;
    game.mobs[0].x = game.hero.x + 3;
    game.mobs[0].y = game.hero.y;
    for (int dx = 1; dx <= 3; ++dx)
        game.level.tiles[game.hero.y * PD_MAP_W + game.hero.x + dx] =
            game.level.floor_tile;
    pd_game_hero_wait(&game);
    check(game.mobs[0].x == game.hero.x + 2 &&
          game.mobs[0].from_x == game.hero.x + 3 &&
          game.mobs[0].moving > 0, "mob walk retains previous position");
    for (int tick = 0; tick < 8; ++tick) pd_game_tick(&game);
    check(game.mobs[0].moving == 0, "mob walk interpolation completes");
}

static int depth_boss_check(pd_game_t *game) {
    int found = 0;
    for (int index = 0; index < PD_MOBS_MAX; ++index)
        if (game->mobs[index].type == 9) found = 1;
    if (!found) printf("FAIL: depth 25 has no boss\n");
    return found;
}

static void test_spawn_and_fov(void) {
    pd_game_t game;
    pd_game_reset(&game, 4242);
    pd_game_start_run(&game, 0);
    for (int depth = 1; depth <= 25; ++depth) {
        pd_game_enter_depth(&game, (uint8_t)depth);
        check(game.ground_count <= PD_GROUND_MAX, "ground item budget");
        for (int index = 0; index < PD_MOBS_MAX; ++index) {
            const pd_mob_t *mob = &game.mobs[index];
            if (mob->type == 0xFF) continue;
            check(walkable_at(&game.level, mob->x, mob->y),
                  "mobs stand on walkable tiles");
            check(mob->x != game.hero.x || mob->y != game.hero.y,
                  "no mob spawns on the hero");
        }
        for (int index = 0; index < game.ground_count; ++index) {
            const pd_ground_t *entry = &game.ground[index];
            if (!entry->used) continue;
            check(walkable_at(&game.level, entry->x, entry->y),
                  "items sit on walkable tiles");
        }
        check(game.level.visible[game.hero.y * PD_MAP_W + game.hero.x] == 1,
              "hero tile is visible");
    }
    if (depth_boss_check(&game) == 0) ++g_failures;
}

/* Walk the hero around, fight, use items and take the stairs when found. */
static void test_play_session(void) {
    pd_game_t game;
    uint32_t rng = 99;
    int descents = 0;
    pd_game_reset(&game, 31337);
    pd_game_start_run(&game, 1);
    for (int turn = 0; turn < 4000 && game.phase == PD_PHASE_PLAY; ++turn) {
        if (game.hero.hp * 3 < game.hero.max_hp) pd_game_hero_potion(&game);
        if (game.hero.hunger < 40) {
            for (int slot = 0; slot < game.bag_count; ++slot)
                if (game.bag[slot].kind == PD_ITEM_FOOD)
                    pd_game_bag_use(&game, slot);
        }
        if (pd_tile_stairs_down(
                pd_tile_at(&game.level, game.hero.x, game.hero.y))) {
            pd_game_hero_stairs(&game);
            ++descents;
            continue;
        }
        /* Amble toward the stairs, with detours so mobs get to act. */
        {
            static uint8_t path_x[2];
            static uint8_t path_y[2];
            int target_x = game.level.exit_x;
            int target_y = game.level.exit_y;
            if ((pd_rng_below(&rng, 4) == 0) && game.ground_count > 0) {
                const int pick = (int)pd_rng_below(&rng, (uint32_t)game.ground_count);
                target_x = game.ground[pick].x;
                target_y = game.ground[pick].y;
            }
            if (pd_level_path(&game.level, game.hero.x, game.hero.y, target_x,
                              target_y, path_x, path_y, 1) > 0) {
                pd_game_hero_step(&game, (int)path_x[0] - game.hero.x,
                                  (int)path_y[0] - game.hero.y);
            } else {
                const int step = (int)pd_rng_below(&rng, 4);
                static const int8_t kSteps[4][2] = {
                    {0, -1}, {0, 1}, {-1, 0}, {1, 0}};
                pd_game_hero_step(&game, kSteps[step][0], kSteps[step][1]);
            }
        }
        check(game.hero.hp <= game.hero.max_hp, "hp stays bounded");
        check(pd_tile_walkable(pd_tile_at(&game.level, game.hero.x,
                                          game.hero.y)),
              "hero stays on walkable tiles");
        check(game.hero.hunger <= 320, "hunger stays bounded");
    }
    printf("session: phase=%u depth=%u level=%u kills=%u gold=%u descents=%d "
           "turns=%u\n",
           game.phase, game.depth, game.hero.level, game.kills, game.hero.gold,
           descents, game.turn);
    check(game.phase == PD_PHASE_PLAY || game.phase == PD_PHASE_DEAD ||
              game.phase == PD_PHASE_WON,
          "session ends in a valid phase");
}

static void test_save_restore(void) {
    pd_game_t game;
    pd_game_t restored;
    pd_save_slot_t summary = {0};
    static uint8_t blob[2048];
    int length;
    pd_game_reset(&game, 777);
    pd_game_start_run(&game, 2);
    pd_game_enter_depth(&game, 5);
    pd_game_hero_step(&game, 0, 0);
    length = pd_game_serialize(&game, blob, sizeof(blob));
    check(length > 0, "serialize produces a blob");
    check(pd_game_save_summary(blob, length, &summary) &&
          summary.depth == game.depth && summary.cls == game.hero.cls,
          "save summaries drive independent slot previews");
    check(!pd_game_save_summary(blob, 24, &summary),
          "truncated slot previews are rejected");
    game.hero.keys = 2;
    length = pd_game_serialize(&game, blob, sizeof(blob));
    check(blob[38] == 1, "new saves record their generator version");
    check(length <= 2048, "save fits the Storage value limit");
    memset(&restored, 0, sizeof(restored));
    check(pd_game_restore(&restored, blob, length), "restore accepts the blob");
    check(restored.depth == game.depth, "depth survives a save");
    check(restored.generation == 1, "new saves retain their generator version");
    check(restored.hero.hp == game.hero.hp, "hp survives a save");
    check(restored.hero.max_hp == game.hero.max_hp, "max hp survives a save");
    check(restored.hero.x == game.hero.x && restored.hero.y == game.hero.y,
          "hero position survives a save");
    check(restored.hero.gold == game.hero.gold, "gold survives a save");
    check(restored.hero.keys == 2, "iron keys survive a save");
    check(restored.bag_count == game.bag_count, "pack size survives a save");
    check(restored.ground_count == game.ground_count,
          "ground items survive a save");
    check(memcmp(restored.level.tiles, game.level.tiles, PD_MAP_TILES) == 0,
          "the floor is rebuilt identically");
    check(memcmp(restored.level.explored, game.level.explored,
                 PD_MAP_TILES) == 0,
          "explored tiles survive a save");
    game.generation = 0;
    pd_level_generate_legacy(&game.level, game.run_seed, game.depth);
    game.hero.x = game.level.entrance_x;
    game.hero.y = game.level.entrance_y;
    game.change_count = 0;
    length = pd_game_serialize(&game, blob, sizeof(blob));
    check(blob[38] == 0, "older saves retain their generator version");
    check(pd_game_restore(&restored, blob, length), "older saves restore");
    check(restored.generation == 0, "older runs keep their floor layout");
    check(memcmp(restored.level.tiles, game.level.tiles, PD_MAP_TILES) == 0,
          "older floor tiles rebuild identically");
    pd_game_enter_depth(&game, 6);
    length = pd_game_serialize(&game, blob, sizeof(blob));
    check(pd_game_restore(&restored, blob, length),
          "descending older runs still restore");
    check(memcmp(restored.level.tiles, game.level.tiles, PD_MAP_TILES) == 0,
          "a new floor does not replay previous floor changes");
    pd_level_generate_legacy(&game.level, game.run_seed, game.depth);
    game.change_count = 1;
    game.changes[0].x = game.level.entrance_x;
    game.changes[0].y = game.level.entrance_y;
    game.changes[0].tile =
        (uint8_t)(24 * game.level.theme + PD_TILEK_DOOR_OPEN);
    game.level.tiles[game.level.entrance_y * PD_MAP_W +
                     game.level.entrance_x] =
        PD_TILE(game.level.theme, PD_TILEK_DOOR_OPEN);
    length = pd_game_serialize(&game, blob, sizeof(blob));
    check(pd_game_restore(&restored, blob, length),
          "older palette saves restore");
    check(memcmp(restored.level.tiles, game.level.tiles, PD_MAP_TILES) == 0,
          "older tile change indices migrate to the current atlas");
    /* A corrupted blob must be rejected. */
    blob[0] ^= 0xFF;
    check(!pd_game_restore(&restored, blob, length),
          "a corrupt save is rejected");
}

static void test_keys_shop_and_camera(void) {
    pd_game_t game;
    pd_layout_t layout;
    for (uint32_t seed = 1; seed <= 20; ++seed) {
        pd_game_reset(&game, seed * 7919u);
        pd_game_start_run(&game, 0);
        pd_game_enter_depth(&game, 2);
        check(game.lock_x < PD_MAP_W &&
              (game.hero.keys > 0 || game.ground_count > 0),
              "generated runs have a locked door and a reachable key");
        pd_game_enter_depth(&game, 6);
        check(game.shop_x < PD_MAP_W && game.shop_y < PD_MAP_H,
              "region transition floors have a merchant");
    }
    pd_game_reset(&game, 17982);
    pd_game_start_run(&game, 0);
    pd_game_enter_depth(&game, 2);
    check(game.lock_x < PD_MAP_W && game.lock_y < PD_MAP_H,
          "a generated floor has an iron-key door");
    if (game.lock_x < PD_MAP_W) {
        int key_present = game.hero.keys > 0;
        for (int index = 0; index < game.ground_count; ++index)
            if (game.ground[index].item.kind == PD_ITEM_IRON_KEY)
                key_present = 1;
        check(key_present, "locked floors provide a key");
        for (int index = 0; index < PD_MOBS_MAX; ++index)
            game.mobs[index].type = 0xFF;
        const int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (int index = 0; index < 4; ++index) {
            const int x = game.lock_x - directions[index][0];
            const int y = game.lock_y - directions[index][1];
            if (!walkable_at(&game.level, x, y) ||
                PD_TILE_KIND(pd_tile_at(&game.level, x, y)) == PD_TILEK_DOOR)
                continue;
            game.hero.x = (uint8_t)x;
            game.hero.y = (uint8_t)y;
            game.hero.keys = 0;
            pd_game_hero_step(&game, directions[index][0], directions[index][1]);
            check(game.hero.x == x && game.hero.y == y,
                  "a locked door rejects a hero without a key");
            game.hero.keys = 1;
            pd_game_hero_step(&game, directions[index][0], directions[index][1]);
            check(game.hero.x == game.lock_x && game.hero.y == game.lock_y &&
                  game.hero.keys == 0, "key unlocks the door and is consumed");
            break;
        }
    }
    pd_game_enter_depth(&game, 6);
    check(game.shop_x < PD_MAP_W && game.shop_y < PD_MAP_H,
          "a region entrance has a shop");
    game.phase = PD_PHASE_SHOP;
    game.hero.gold = 100;
    const int old_bag = game.bag_count;
    pd_game_shop_buy(&game);
    check(game.bag_count == old_bag + 1 &&
          game.hero.gold == 100 - 40 - 6 * 4,
          "gold purchases a healing potion");
    pd_layout_build(&layout, 296, 240, 8, 10, 8, 10);
    int camera_x, camera_y;
    pd_layout_camera(&layout, 20, 18, &camera_x, &camera_y);
    const int start_x = camera_x;
    game.phase = PD_PHASE_PLAY;
    game.hero.x = 20;
    game.hero.y = 18;
    const int screen_x = layout.map_x + layout.map_w / 2;
    const int screen_y = layout.map_y + layout.map_h / 2;
    pd_input_pointer(&game, &layout, screen_x, screen_y, 0, PXA_POINTER_DOWN, 10);
    pd_input_pointer(&game, &layout, screen_x - 48, screen_y, 0, PXA_POINTER_MOVE, 11);
    pd_input_pointer(&game, &layout, screen_x - 48, screen_y, 0, PXA_POINTER_UP, 12);
    pd_layout_camera(&layout, 20, 18, &camera_x, &camera_y);
    check(camera_x > start_x && game.hero.x == 20,
          "dragging pans the map without moving the hero");
}

static void test_auto_walk(void) {
    pd_game_t game;
    pd_game_reset(&game, 5150);
    pd_game_start_run(&game, 0);
    for (int depth = 1; depth <= 6; ++depth) {
        pd_game_enter_depth(&game, (uint8_t)depth);
        pd_game_tap(&game, game.level.exit_x, game.level.exit_y);
        for (int tick = 0; tick < 1600 && game.walk_active; ++tick)
            pd_game_tick(&game);
        check(!game.walk_active, "auto-walk terminates");
        check(pd_tile_walkable(pd_tile_at(&game.level, game.hero.x,
                                          game.hero.y)),
              "auto-walk keeps the hero on walkable tiles");
    }
}

static void test_responsive_layout(void) {
    static const int screens[][2] = {
        {296, 240}, {400, 240}, {240, 400}, {800, 480},
        {480, 800}, {176, 176},
    };
    for (int screen = 0; screen < 6; ++screen) {
        const int width = screens[screen][0];
        const int height = screens[screen][1];
        pd_layout_t layout;
        pd_layout_build(&layout, width, height, 8, 10, 8, 10);
        check(layout.menu_pane.w > 31 && layout.menu_pane.h >= 20,
              "top controls are larger than the original tiny targets");
        check(layout.journal.x + layout.journal.w == layout.settings.x &&
              layout.settings.x + layout.settings.w ==
                  layout.menu_pane.x + layout.menu_pane.w,
              "top buttons and artwork occupy the same pane");
        check(layout.depth.x + layout.depth.w == layout.journal.x &&
              layout.depth.y == layout.menu_pane.y &&
              layout.depth.h == layout.menu_pane.h,
              "floor indicator stays beside the top buttons");
        const int initial_bar_h = layout.bar_h;
        const int initial_hud_h = layout.hud_h;
        const int radii[4] = {48, 48, 48, 48};
        pd_layout_fit_display_shape(&layout, width <= 296 ? 1u : 0u, radii);
        check(layout.journal.w >= 32 && layout.journal.h >= 32 &&
              layout.settings.w >= 32 && layout.settings.h >= 32 &&
              layout.menu_back.w >= 32 && layout.menu_back.h >= 32,
              "narrow display menus and exit retain touch-friendly targets");
        if (width <= 296) {
            const int radius = 48;
            const int y = layout.menu_pane.y;
            const int inset = width - layout.menu_pane.x - layout.menu_pane.w;
            check((radius - inset) * (radius - inset) +
                      (radius - y) * (radius - y) <= radius * radius,
                  "rounded corner does not clip the top-right pane");
        }
        check(layout.menu_pane.x >= layout.safe_left &&
              layout.menu_pane.x + layout.menu_pane.w <= width - layout.safe_right,
              "top pane stays within the safe area");
        if (width <= 296) {
            const int x = layout.hero_info.x;
            const int y = layout.hero_info.y;
            check((48 - x) * (48 - x) + (48 - y) * (48 - y) <= 48 * 48,
                  "portrait stays within the rounded top-left corner");
        }
        for (int zoom = 1; zoom <= 3; ++zoom) {
            pd_layout_set_zoom(&layout, zoom);
            check(layout.tile_pixels == 16 * zoom && layout.zoom == zoom,
                  "world zoom uses integer tile scale");
            check(layout.map_w == layout.cols * layout.tile_pixels &&
                  layout.map_h == layout.rows * layout.tile_pixels,
                  "world viewport follows zoom");
            check(layout.cols == (width + layout.tile_pixels - 1) /
                                     layout.tile_pixels ||
                      layout.cols == PD_MAP_W,
                  "world viewport includes partially visible edge columns");
            check(layout.rows == (height + layout.tile_pixels - 1) /
                                     layout.tile_pixels ||
                      layout.rows == PD_MAP_H,
                  "world viewport includes partially visible edge rows");
            check(layout.hud_h == initial_hud_h &&
                  layout.bar_h == initial_bar_h,
                  "zoom never scales interface");
        }
        check(layout.map_x == (width - layout.map_w) / 2 &&
              layout.map_y == (height - layout.map_h) / 2,
              "world viewport stays centered independently of controls");
        check(layout.map_x <= layout.hero_info.x &&
              layout.map_y <= layout.hero_info.y &&
              layout.map_y + layout.map_h >=
                  layout.button[0].y + layout.button[0].h,
              "world continues behind the transparent HUD and toolbar");
        check(layout.cols <= PD_MAP_W && layout.rows <= PD_MAP_H,
              "viewport never exceeds level bounds");
        for (int index = 0; index < PD_BUTTON_COUNT; ++index)
            check(layout.button[index].x >= layout.safe_left &&
                  layout.button[index].x + layout.button[index].w <=
                      width - layout.safe_right &&
                  layout.button[index].y + layout.button[index].h <=
                      height - layout.safe_bottom,
                  "toolbar action fits safe area");
        for (int index = 0; index < PD_CLASS_COUNT; ++index)
            check(layout.class_button[index].x >= layout.safe_left &&
                  layout.class_button[index].x + layout.class_button[index].w <=
                      width - layout.safe_right &&
                  layout.class_button[index].y >= layout.safe_top &&
                  layout.class_button[index].y + layout.class_button[index].h <=
                      height - layout.safe_bottom,
                  "class card fits safe area");
        for (int index = 0; index < PD_BAG_ROWS; ++index)
            check(layout.bag_row[index].x >= layout.safe_left &&
                  layout.bag_row[index].x + layout.bag_row[index].w <=
                      width - layout.safe_right &&
                  layout.bag_row[index].y + layout.bag_row[index].h <=
                      height - layout.safe_bottom,
                  "inventory slot fits safe area");
    }
    {
        pd_layout_t layout;
        pd_layout_build(&layout, 410, 410, 60, 60, 60, 60);
        pd_layout_fit_display_shape(&layout, 2, NULL);
        check(pd_layout_save_page_size(&layout) == PD_SAVE_SLOTS,
              "large circles show all save slots without pagination");
        check(layout.menu_primary.h >= 44 && layout.title_rankings.h >= 44 &&
              layout.title_journal.h >= 44 && layout.title_settings.h >= 44,
              "large circle title targets remain easy to tap");
        check(layout.settings.w >= 44 && layout.journal.w >= 44 &&
              layout.settings_back.w >= 44 && layout.bag_close.w >= 44 &&
              layout.info_close.w >= 44 && layout.journal_close.w >= 44,
              "large circle menu and close controls use 44-pixel targets");
        check(layout.menu_pane.w >= 93 && layout.menu_pane.h >= 42 &&
              layout.journal.w >= 39 && layout.settings.w >= 36,
              "large circle menu fits integer-scaled original art");
        check(layout.button[0].w >= 44 && layout.button[0].h >= 44,
              "large circle toolbar actions remain easy to tap");
        check(layout.hero_info.w == 124 && layout.hero_info.h == 57 &&
              layout.depth.w == 28,
              "large circle status and floor remain readable");
        check(layout.class_button[0].h >= 63 &&
              layout.class_button[0].y >= layout.menu_back.y +
                  layout.menu_back.h + 4 &&
              layout.class_button[2].y + layout.class_button[2].h <
                  layout.height - layout.safe_bottom - 12,
              "round hero cards contain three lines below the back control");
        check(pd_layout_save_row(&layout, 5, 0).y >= layout.safe_top + 24 &&
              pd_layout_save_row(&layout, 5, 4).y + 44 <=
                  layout.height - layout.safe_bottom,
              "all five enlarged save slots fit the round display");
    }
    {
        static const int circles[][2] = {{176, 176}, {296, 240}};
        for (int screen = 0; screen < 2; ++screen) {
            const int width = circles[screen][0];
            const int height = circles[screen][1];
            const int diameter = width < height ? width : height;
            const pd_rect_t *controls[4];
            pd_layout_t layout;
            pd_layout_build(&layout, width, height, 8, 10, 8, 10);
            pd_layout_fit_display_shape(&layout, 2u, NULL);
            controls[0] = &layout.hero_info;
            controls[1] = &layout.menu_pane;
            controls[2] = &layout.depth;
            controls[3] = &layout.menu_back;
            if (width == 176) {
                check(layout.menu_back.y + layout.menu_back.h <=
                          layout.class_button[0].y &&
                      layout.class_button[2].y + layout.class_button[2].h <=
                          layout.height - layout.safe_bottom,
                      "tiny round class cards fit below their back control");
            }
            check(layout.menu_pane.y >= layout.hero_info.y +
                      layout.hero_info.h ||
                  layout.depth.x >= layout.hero_info.x + layout.hero_info.w,
                  "circle display separates the HUD and pause buttons");
            for (int index = 0; index < 4; ++index) {
                const pd_rect_t *rect = controls[index];
                for (int corner = 0; corner < 4; ++corner) {
                    const int x = rect->x + (corner & 1 ? rect->w - 1 : 0);
                    const int y = rect->y + (corner & 2 ? rect->h - 1 : 0);
                    const int64_t dx = 2 * (int64_t)x + 1 - width;
                    const int64_t dy = 2 * (int64_t)y + 1 - height;
                    check(dx * dx + dy * dy <= (int64_t)diameter * diameter,
                          "circular screen does not clip top controls");
                }
            }
            for (int index = 0; index < PD_BUTTON_COUNT; ++index) {
                const pd_rect_t *rect = &layout.button[index];
                for (int corner = 0; corner < 4; ++corner) {
                    const int x = rect->x + (corner & 1 ? rect->w - 1 : 0);
                    const int y = rect->y + (corner & 2 ? rect->h - 1 : 0);
                    const int64_t dx = 2 * (int64_t)x + 1 - width;
                    const int64_t dy = 2 * (int64_t)y + 1 - height;
                    check(dx * dx + dy * dy <= (int64_t)diameter * diameter,
                          "circular screen does not clip toolbar buttons");
                }
            }
        }
    }
    {
        pd_layout_t layout;
        pd_layout_build(&layout, 412, 412, 60, 60, 60, 60);
        pd_layout_fit_display_shape(&layout, 2, NULL);
        pd_layout_set_zoom(&layout, 1);
        check(layout.map_x <= 0 && layout.map_y <= 0 &&
              layout.map_x + layout.map_w >= layout.width &&
              layout.map_y + layout.map_h >= layout.height,
              "round screen map extends behind safe-area controls");
        check(layout.button[0].x >= layout.safe_left &&
              layout.button[PD_BUTTON_COUNT - 1].x +
                  layout.button[PD_BUTTON_COUNT - 1].w <=
                  layout.width - layout.safe_right &&
              layout.hero_info.x >= layout.safe_left,
              "round screen controls remain in the safe area");
        pd_layout_set_zoom(&layout, 2);
        check(layout.map_x <= 0 && layout.map_y <= 0 &&
              layout.map_x + layout.map_w >= layout.width &&
              layout.map_y + layout.map_h >= layout.height,
              "round screen zoom preserves the expanded world viewport");
    }
    {
        pd_layout_t layout;
        pd_layout_build(&layout, 296, 240, 8, 10, 8, 10);
        pd_layout_fit_display_shape(&layout, 2, NULL);
        pd_layout_set_zoom(&layout, 1);
        check(layout.map_x <= 0 && layout.map_y <= 0 &&
              layout.map_x + layout.map_w >= layout.width &&
              layout.map_y + layout.map_h >= layout.height,
              "small round screen also uses the area behind its controls");
    }
}

static void test_hud_digit_bounds(void) {
    struct {
        const char *text;
        int width;
        int scale;
    } cases[] = {
        {"0/11", 17, 1},
        {"255/1281", 17, 1},
        {"255/1281", 25, 2},
        {"32767/32767", 50, 1},
        {"9", 14, 1},
        {"10", 14, 1},
        {"255", 14, 1},
        {"10", 21, 2},
        {"255", 21, 2},
    };
    for (int index = 0; index < (int)(sizeof(cases) / sizeof(cases[0]));
         ++index) {
        const pd_hud_digits_t digits = pd_layout_hud_digits(
            100, cases[index].width, cases[index].text,
            cases[index].scale);
        check(digits.x >= 100 &&
              digits.x + digits.width <= 100 + cases[index].width,
              "single, double and maximum-length HUD values stay inside bars");
        check(abs(2 * (digits.x - 100) + digits.width -
                  cases[index].width) <= 1,
              "one- and two-digit levels remain centered in badge");
    }
    for (int level = 1; level <= 255; ++level) {
        char badge[8];
        snprintf(badge, sizeof(badge), "%d", level);
        for (int large = 0; large < 2; ++large) {
            const int badge_width = large ? 21 : 14;
            const int scale = large ? 2 : 1;
            const pd_hud_digits_t level_digits = pd_layout_hud_digits(
                100, badge_width, badge, scale);
            check(level_digits.x >= 100 &&
                  level_digits.x + level_digits.width <= 100 + badge_width &&
                  abs(2 * (level_digits.x - 100) + level_digits.width -
                      badge_width) <= 1,
                  "every one-, two- and three-digit level fits its badge");
        }
    }
}

static void test_hunger_and_font_pages(void) {
    pd_game_t game;
    pd_glyph_t glyph;
    char expected[PD_MESSAGE_TEXT];
    int extra_glyphs = 0;
    pd_game_reset(&game, 105);
    pd_game_start_run(&game, 0);
    for (int index = 0; index < PD_MOBS_MAX; ++index)
        game.mobs[index].type = 0xFF;

    game.hero.hunger = PD_HUNGER_WARN + 1;
    pd_game_hero_wait(&game);
    check(game.hero.hunger == PD_HUNGER_WARN &&
          strcmp(game.messages[(game.message_total - 1) % PD_MESSAGE_COUNT].text,
                 pd_str(PD_STR_HUNGRY)) == 0,
          "hunger warning appears when satiety reaches threshold");
    game.hero.hunger = 1;
    pd_game_hero_wait(&game);
    check(game.hero.hunger == 0 &&
          strcmp(game.messages[(game.message_total - 1) % PD_MESSAGE_COUNT].text,
                 pd_str(PD_STR_STARVING)) == 0,
          "starvation warning appears when satiety reaches zero");
    game.hero.hp = 10;
    game.turn = 5;
    game.sound_count = 0;
    pd_game_hero_wait(&game);
    pd_str_format(expected, sizeof(expected), PD_STR_STARVATION_DAMAGE, 1);
    check(game.hero.hp == 9 &&
          strcmp(game.messages[(game.message_total - 1) % PD_MESSAGE_COUNT].text,
                 expected) == 0 && game.sound_count == 0,
          "starvation deals damage without a monster-hit log or hit sound");
    game.hero.hp = 1;
    game.turn = 11;
    pd_game_hero_wait(&game);
    check(game.phase == PD_PHASE_DEAD &&
          strcmp(game.messages[(game.message_total - 1) % PD_MESSAGE_COUNT].text,
                 pd_str(PD_STR_STARVED)) == 0,
          "starvation death has its own message");
    check(pd_ui_rect[PD_UI_BUFF_HUNGRY][2] == 7 &&
          pd_ui_rect[PD_UI_BUFF_STARVING][2] == 7,
          "original hunger buff icons are available in the UI atlas");

    pd_strings_set_language(1);
    for (int id = PD_STR_STARVING; id <= PD_STR_STARVED; ++id) {
        const char *cursor = pd_str((pd_string_id_t)id);
        while (*cursor != '\0') {
            const int result = pd_font_next(&cursor, &glyph);
            check(result > 0, "starvation text has no missing glyphs");
            if (result > 0 && glyph.cjk == 2) ++extra_glyphs;
        }
    }
    check(extra_glyphs > 0 && PD_CJK_EXTRA_GLYPHS > 0,
          "overflow Chinese glyphs use the second texture page");
    pd_strings_set_language(0);
}

static void test_camera_top_pan(void) {
    pd_game_t game;
    pd_layout_t layout;
    int camera_x;
    int camera_y;
    pd_game_reset(&game, 812);
    pd_game_start_run(&game, 0);
    game.hero.x = 10;
    game.hero.y = 1;

    for (int profile = 0; profile < 3; ++profile) {
        const int width = profile == 0 ? 412 : profile == 1 ? 296 : 176;
        const int height = profile == 0 ? 412 : profile == 1 ? 240 : 176;
        pd_layout_build(&layout, width, height, 8, 10, 8, 10);
        pd_layout_fit_display_shape(&layout, 2, NULL);
        pd_layout_camera(&layout, game.hero.x, game.hero.y,
                         &camera_x, &camera_y);
        check(camera_y == 0, "camera starts at the top of the floor");
        pd_layout_pan(&layout, game.hero.x, game.hero.y, 0, -1);
        pd_layout_camera(&layout, game.hero.x, game.hero.y,
                         &camera_x, &camera_y);
        check(camera_y == -1, "one drag step moves beyond the top row");
        pd_layout_pan(&layout, game.hero.x, game.hero.y, 0, -100);
        pd_layout_camera_pixels(&layout, game.hero.x, game.hero.y,
                                &camera_x, &camera_y);
        const int hero_bottom = layout.hero_info.y + layout.hero_info.h;
        const int menu_bottom = layout.menu_pane.y + layout.menu_pane.h;
        const int top = hero_bottom > menu_bottom ? hero_bottom : menu_bottom;
        check(camera_y < 0 && layout.map_y - camera_y >= top,
              "top floor row can be moved below the HUD");
        const int limit = camera_y;
        pd_layout_pan(&layout, game.hero.x, game.hero.y, 0, -100);
        pd_layout_pan(&layout, game.hero.x, game.hero.y, 0, 1);
        pd_layout_camera_pixels(&layout, game.hero.x, game.hero.y,
                                &camera_x, &camera_y);
        check(camera_y == limit + layout.tile_pixels,
              "dragging back from the boundary has no hidden offset");
        if (profile == 0) {
            const int tile_x = 15;
            const int tile_y = 1;
            const int screen_x = layout.map_x + tile_x * layout.tile_pixels -
                                 camera_x + layout.tile_pixels / 2;
            const int screen_y = layout.map_y + tile_y * layout.tile_pixels -
                                 camera_y + layout.tile_pixels / 2;
            check(screen_y >= top &&
                  !pd_rect_contains(&layout.menu_pane, screen_x, screen_y),
                  "reframed top tile is outside the menu hitbox");
            pd_input_pointer(&game, &layout, screen_x, screen_y, 0,
                             PXA_POINTER_DOWN, 10);
            pd_input_pointer(&game, &layout, screen_x, screen_y, 0,
                             PXA_POINTER_UP, 11);
            check(game.walk_x == tile_x && game.walk_y == tile_y,
                  "reframed top tile accepts a world tap");
        }
    }
}

static void test_camera_smooth_edges(void) {
    pd_game_t game;
    pd_layout_t layout;
    int camera_x;
    int camera_y;
    pd_game_reset(&game, 813);
    pd_game_start_run(&game, 0);
    game.phase = PD_PHASE_PLAY;
    for (int profile = 0; profile < 3; ++profile) {
        const int width = profile == 0 ? 412 : profile == 1 ? 296 : 176;
        const int height = profile == 0 ? 412 : profile == 1 ? 240 : 176;
        pd_layout_build(&layout, width, height, 8, 10, 8, 10);
        pd_layout_fit_display_shape(&layout, 2, NULL);
        game.hero.x = 20;
        game.hero.y = 18;
        pd_layout_camera_pixels(&layout, 20, 18, &camera_x, &camera_y);
        const int start_x = camera_x;
        const int start_y = camera_y;
        const int center_x = width / 2;
        const int center_y = height / 2;
        pd_input_pointer(&game, &layout, center_x, center_y, 0,
                         PXA_POINTER_DOWN, 1);
        pd_input_pointer(&game, &layout, center_x - 11, center_y + 5, 0,
                         PXA_POINTER_MOVE, 2);
        pd_layout_camera_pixels(&layout, 20, 18, &camera_x, &camera_y);
        check(camera_x == start_x + 11 && camera_y == start_y - 5,
              "map drag follows every pixel on both axes");
        pd_input_pointer(&game, &layout, center_x - 11, center_y + 5, 0,
                         PXA_POINTER_UP, 3);
        pd_layout_camera_pixels(&layout, 20, 18, &camera_x, &camera_y);
        const int destination_x = camera_x;
        pd_layout_camera_visual_pixels(&layout, 20, 18, 19, 18, 6,
                                       &camera_x, &camera_y);
        check(camera_x == destination_x - layout.tile_pixels / 2,
              "camera follows the interpolated walking sprite");

        game.hero.x = 0;
        game.hero.y = 0;
        pd_layout_pan_pixels(&layout, 0, 0, -10000, 0);
        pd_layout_camera_pixels(&layout, 0, 0, &camera_x, &camera_y);
        check(camera_x < 0 && layout.map_x - camera_x >=
              layout.hero_info.x + layout.hero_info.w,
              "left floor edge clears the portrait HUD");
        const int left_limit = camera_x;
        pd_layout_pan_pixels(&layout, 0, 0, -10000, 0);
        pd_layout_pan_pixels(&layout, 0, 0, 1, 0);
        pd_layout_camera_pixels(&layout, 0, 0, &camera_x, &camera_y);
        check(camera_x == left_limit + 1,
              "left bound reverses without a hidden offset");

        game.hero.x = PD_MAP_W - 1;
        game.hero.y = PD_MAP_H - 1;
        pd_layout_pan_pixels(&layout, game.hero.x, game.hero.y,
                             10000, 10000);
        pd_layout_camera_pixels(&layout, game.hero.x, game.hero.y,
                                &camera_x, &camera_y);
        const int right = layout.map_x + PD_MAP_W * layout.tile_pixels -
                          camera_x;
        const int bottom = layout.map_y + PD_MAP_H * layout.tile_pixels -
                           camera_y;
        check(camera_x > (PD_MAP_W - layout.cols) * layout.tile_pixels &&
              right <= layout.menu_pane.x,
              "right floor edge clears the menu HUD");
        check(camera_y > (PD_MAP_H - layout.rows) * layout.tile_pixels &&
              bottom <= layout.button[0].y,
              "bottom floor edge clears the toolbar");
        const int right_limit = camera_x;
        const int bottom_limit = camera_y;
        pd_layout_pan_pixels(&layout, game.hero.x, game.hero.y,
                             10000, 10000);
        pd_layout_pan_pixels(&layout, game.hero.x, game.hero.y, -1, -1);
        pd_layout_camera_pixels(&layout, game.hero.x, game.hero.y,
                                &camera_x, &camera_y);
        check(camera_x == right_limit - 1 && camera_y == bottom_limit - 1,
              "right and bottom bounds reverse without a hidden offset");
    }
}

static void test_world_zoom_input(void) {
    pd_game_t game;
    pd_layout_t layout;
    int x;
    int y;
    pd_game_reset(&game, 706);
    pd_game_start_run(&game, 0);
    pd_layout_build(&layout, 400, 240, 8, 10, 8, 10);
    x = layout.map_x + 40;
    y = layout.map_y + 40;
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_DOWN, 1);
    pd_input_pointer(&game, &layout, x + 50, y, 1, PXA_POINTER_DOWN, 1);
    pd_input_pointer(&game, &layout, x + 80, y, 1, PXA_POINTER_MOVE, 1);
    check(layout.zoom == 2 && game.turn == 0,
          "pinch changes world zoom without taking a turn");
    pd_input_pointer(&game, &layout, x + 80, y, 1, PXA_POINTER_UP, 1);
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_UP, 1);
    check(game.phase == PD_PHASE_PLAY && game.turn == 0,
          "releasing pinch does not tap map");
    x = layout.settings.x + layout.settings.w / 2;
    y = layout.settings.y + layout.settings.h / 2;
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_DOWN, 1);
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_UP, 1);
    check(game.phase == PD_PHASE_PAUSE, "original menu button opens pause");
    x = layout.pause_settings.x + layout.pause_settings.w / 2;
    y = layout.pause_settings.y + layout.pause_settings.h / 2;
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_DOWN, 1);
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_UP, 1);
    check(game.phase == PD_PHASE_SETTINGS, "pause menu opens settings");
    x = layout.zoom_in.x + layout.zoom_in.w / 2;
    y = layout.zoom_in.y + layout.zoom_in.h / 2;
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_DOWN, 1);
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_UP, 1);
    check(layout.zoom == 3, "settings zooms map without scaling HUD");
    x = layout.settings_back.x + layout.settings_back.w / 2;
    y = layout.settings_back.y + layout.settings_back.h / 2;
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_DOWN, 1);
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_UP, 1);
    check(game.phase == PD_PHASE_PAUSE, "settings returns to pause");
    x = layout.pause_menu.x - 10;
    y = layout.pause_menu.y - 10;
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_DOWN, 1);
    pd_input_pointer(&game, &layout, x, y, 0, PXA_POINTER_UP, 1);
    check(game.phase == PD_PHASE_PLAY, "pause resumes without taking a turn");
}

static void test_grass_trample(void) {
    pd_game_t game;
    int leaf_count = 0;
    pd_game_reset(&game, 780);
    pd_game_start_run(&game, 0);
    for (int index = 0; index < PD_MOBS_MAX; ++index)
        game.mobs[index].type = 0xFF;
    for (int index = 0; index < PD_EFFECTS_MAX; ++index)
        game.effects[index].ttl = 0;
    game.ground_count = 0;
    game.sound_count = 0;
    const int grass_x = game.hero.x + 1;
    const int grass_y = game.hero.y;
    game.level.tiles[grass_y * PD_MAP_W + grass_x] =
        PD_TILE(game.level.theme, PD_TILEK_HIGH_GRASS);
    const int old_changes = game.change_count;
    pd_game_hero_step(&game, 1, 0);
    check(PD_TILE_KIND(pd_tile_at(&game.level, grass_x, grass_y)) ==
              PD_TILEK_GRASS, "high grass is trampled into short grass");
    check(game.change_count == old_changes + 1,
          "trampled grass survives save and restore");
    check(game.sound_count > 0 && game.sounds[0] == PD_SOUND_TRAMPLE,
          "trampling plays the original grass sound");
    for (int index = 0; index < PD_EFFECTS_MAX; ++index)
        if (game.effects[index].ttl > 0 &&
            game.effects[index].kind == PD_EFFECT_LEAF) ++leaf_count;
    check(leaf_count == 1, "high grass releases leaf particles");
    game.level.tiles[grass_y * PD_MAP_W + grass_x + 1] = game.level.floor_tile;
    game.sound_count = 0;
    pd_game_hero_step(&game, 1, 0);
    check(game.sound_count > 0 && game.sounds[0] == PD_SOUND_STEP,
          "plain floor plays footsteps without dust");
    for (int index = 0; index < PD_EFFECTS_MAX; ++index)
        if (game.effects[index].ttl > 0 &&
            game.effects[index].kind == PD_EFFECT_LEAF) --leaf_count;
    check(leaf_count == 0, "ordinary walking creates no extra particles");
}

static void test_wall_stitching(void) {
    pd_level_t level;
    const uint8_t wall = PD_TILE(4, PD_TILEK_WALL);
    const uint8_t floor = PD_TILE(4, PD_TILEK_FLOOR);
    const int center = 10 * PD_MAP_W + 10;
    const int neighbors[4] = {center + 1, center + PD_MAP_W + 1,
                              center + PD_MAP_W - 1, center - 1};
    memset(&level, 0, sizeof(level));
    level.theme = 4;
    for (int index = 0; index < PD_MAP_TILES; ++index)
        level.tiles[index] = wall;
    level.tiles[center + PD_MAP_W] = floor;
    check(!pd_wall_exposed(&level, 10, 10),
          "undiscovered floor does not expose an isolated wall");
    level.explored[center + PD_MAP_W] = 1;
    check(pd_wall_exposed(&level, 10, 10),
          "discovered adjacent floor exposes the room wall");
    level.tiles[center + PD_MAP_W] = wall;
    for (int mask = 0; mask < 16; ++mask) {
        for (int bit = 0; bit < 4; ++bit)
            level.tiles[neighbors[bit]] = mask & (1 << bit) ? floor : wall;
        check(pd_wall_upper(&level, 10, 10) ==
              PD_WALL(4, PD_WALLK_INTERNAL + mask),
              "internal wall joins match all four original neighbors");
    }
    level.tiles[center + PD_MAP_W] = floor;
    level.tiles[center - 1] = floor;
    level.tiles[center + 1] = wall;
    check(pd_wall_face(&level, 10, 10) ==
          PD_WALL(4, PD_WALLK_FACE + 2) ||
          pd_wall_face(&level, 10, 10) ==
          PD_WALL(4, PD_WALLK_FACE_ALT + 2),
          "raised wall joins on open left side");
    level.tiles[center + PD_MAP_W] = PD_TILE(4, PD_TILEK_DOOR);
    check(pd_wall_face(&level, 10, 10) ==
          PD_WALL(4, PD_WALLK_FACE_DOOR + 2),
          "raised wall behind doorway");
    check(pd_wall_upper(&level, 10, 10) ==
          PD_WALL(4, PD_WALLK_DOOR_SIDEWAYS),
          "sideways doorway has wall-top lintel");
    level.tiles[center] = floor;
    level.tiles[center + PD_MAP_W] = wall;
    for (int mask = 0; mask < 4; ++mask) {
        level.tiles[center + PD_MAP_W + 1] = mask & 1 ? floor : wall;
        level.tiles[center + PD_MAP_W - 1] = mask & 2 ? floor : wall;
        check(pd_wall_upper(&level, 10, 10) ==
              PD_WALL(4, PD_WALLK_OVERHANG + mask),
              "overhang joins with walls on both diagonal sides");
    }
    level.tiles[center] = PD_TILE(4, PD_TILEK_DOOR_OPEN);
    check(pd_wall_upper(&level, 10, 10) ==
          PD_WALL(4, PD_WALLK_SIDE_OVERHANG_OPEN + 3),
          "opened sideways doorway has its own wall overhang");
    level.tiles[center] = floor;
    level.tiles[center + PD_MAP_W] = PD_TILE(4, PD_TILEK_DOOR_OPEN);
    check(pd_wall_upper(&level, 10, 10) ==
          PD_WALL(4, PD_WALLK_DOOR_OVERHANG_OPEN),
          "open door has a separate lintel");
    check(pd_wall_upper(&level, 10, PD_MAP_H - 1) == -1,
          "last row cannot sample below map");
    level.tiles[center] = wall;
    level.tiles[center + PD_MAP_W] = PD_TILE_VOID;
    check(pd_wall_face(&level, 10, 10) == -1,
          "uncarved space cannot produce a wall face");
    level.tiles[center] = PD_TILE_VOID;
    level.tiles[center + PD_MAP_W] = wall;
    check(pd_wall_upper(&level, 10, 10) == -1,
          "uncarved space cannot produce a floating overhang");
    level.tiles[center] = wall;
    level.tiles[center + PD_MAP_W] = floor;
    level.tiles[center - 1] = PD_TILE_VOID;
    level.tiles[center + 1] = PD_TILE_VOID;
    check(pd_wall_face(&level, 10, 10) == PD_WALL(4, PD_WALLK_FACE) ||
          pd_wall_face(&level, 10, 10) == PD_WALL(4, PD_WALLK_FACE_ALT),
          "uncarved neighbors join like the original solid background");
}

static int same_sprite_cell(int first, int second) {
    for (int row = 0; row < PD_CELL; ++row) {
        const int first_offset = (first / PD_CELLS_PER_ROW * PD_CELL + row) *
                                 PD_ATLAS_WIDTH + first % PD_CELLS_PER_ROW * PD_CELL;
        const int second_offset = (second / PD_CELLS_PER_ROW * PD_CELL + row) *
                                  PD_ATLAS_WIDTH + second % PD_CELLS_PER_ROW * PD_CELL;
        if (memcmp(pd_sprite_atlas + first_offset,
                   pd_sprite_atlas + second_offset, PD_CELL) != 0) return 0;
    }
    return 1;
}

static void test_visual_features(void) {
    pd_game_t game;
    pd_layout_t layout;
    uint8_t water = PD_TILE(0, PD_TILEK_WATER_A);
    int search_count = 0;
    pd_game_reset(&game, 1827);
    pd_game_start_run(&game, 0);
    check(pd_hero_visual_tier(&game) == 1,
          "equipped starting cloth uses armor tier one artwork");
    check(!same_sprite_cell(PD_SPRITE_HERO(0, 0, 0),
                            PD_SPRITE_HERO(0, 1, 0)),
          "cloth tier is not the rag sprite painted twice");
    check(!same_sprite_cell(PD_SPRITE_HERO(0, 1, 0),
                            PD_SPRITE_HERO(0, 1, 1)),
          "idle animation includes the original second hand pose");
    game.bag[game.bag_count] = (pd_item_t){PD_ITEM_ARMOR, 4, 0, 0};
    pd_game_bag_use(&game, game.bag_count++);
    check(pd_hero_visual_tier(&game) == 4,
          "equipping armor updates the actual hero costume");
    game.hero.armor = -1;
    check(pd_hero_visual_tier(&game) == 0,
          "unequipping armor restores the unarmored artwork");

    for (int index = 0; index < PD_MAP_TILES; ++index)
        game.level.tiles[index] = PD_TILE(0, PD_TILEK_FLOOR);
    game.level.tiles[10 * PD_MAP_W + 10] = water;
    check(pd_water_shore(&game.level, 10, 10) == 15,
          "water island draws all four original shore edges");
    game.level.tiles[9 * PD_MAP_W + 10] = water;
    game.level.tiles[11 * PD_MAP_W + 10] = water;
    check(pd_water_shore(&game.level, 10, 10) == 10,
          "water shore mask follows the four cardinal neighbors");

    pd_level_update_fov(&game.level, game.hero.x, game.hero.y);
    pd_game_hero_search(&game);
    for (int index = 0; index < PD_EFFECTS_MAX; ++index)
        if (game.effects[index].ttl &&
            game.effects[index].kind == PD_EFFECT_SEARCH) ++search_count;
    check(search_count > 0, "search paints the scanned nearby cells");

    pd_layout_build(&layout, 480, 800, 0, 0, 0, 0);
    game.phase = PD_PHASE_DEAD;
    pd_input_pointer(&game, &layout, layout.menu_primary.x + 4,
                     layout.menu_primary.y + 4, 0, PXA_POINTER_DOWN, 1);
    pd_input_pointer(&game, &layout, layout.menu_primary.x + 4,
                     layout.menu_primary.y + 4, 0, PXA_POINTER_UP, 1);
    check(game.phase == PD_PHASE_CLASS,
          "game over new-game button returns to class selection");
    game.phase = PD_PHASE_DEAD;
    pd_input_pointer(&game, &layout, layout.menu_secondary.x + 4,
                     layout.menu_secondary.y + 4, 0, PXA_POINTER_DOWN, 1);
    pd_input_pointer(&game, &layout, layout.menu_secondary.x + 4,
                     layout.menu_secondary.y + 4, 0, PXA_POINTER_UP, 1);
    check(game.phase == PD_PHASE_TITLE,
          "game over back button returns to main menu");
}

int main(void) {
    test_back_navigation();
    test_menus_and_slots();
    test_generation();
    test_mob_death();
    test_mob_movement();
    test_spawn_and_fov();
    test_play_session();
    test_save_restore();
    test_keys_shop_and_camera();
    test_auto_walk();
    test_responsive_layout();
    test_hud_digit_bounds();
    test_hunger_and_font_pages();
    test_camera_top_pan();
    test_camera_smooth_edges();
    test_world_zoom_input();
    test_grass_trample();
    test_wall_stitching();
    test_visual_features();
    if (g_failures == 0) {
        printf("selftest: all checks passed\n");
        return 0;
    }
    printf("selftest: %d failures\n", g_failures);
    return 1;
}
