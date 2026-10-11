#include "pxa_control_capture.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pxa_raster.h"
#include "voxel_raster.h"

chunk_t *g_chunk_grid[GRID_W][GRID_W];
int16_t g_chunk_origin_cx;
int16_t g_chunk_origin_cz;
uint32_t g_world_seed;
particle_t g_particles[MAX_PARTICLES];
mob_t g_mobs[MAX_MOBS];

render_layout_t g_layout;
inventory_layout_t g_inv_layout;
menu_button_t g_menu_buttons[MENU_BUTTON_MAX];
int g_menu_button_count;
item_stack_t g_inventory[INV_SLOTS];
item_stack_t g_craft[CRAFT_SLOTS];
item_stack_t g_craft_result;
item_stack_t g_table_craft[TABLE_CRAFT_SLOTS];
item_stack_t g_table_result;
static int g_width;
static int g_height;
static uint32_t g_submit_count;
static uint32_t g_last_command_count;

int32_t pxa_test_control(const uint8_t *data, uint32_t length) {
    (void)data;
    (void)length;
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *data,
               uint32_t length) {
    assert(handle == UINT64_C(0x100000003) && data != NULL);
    if (operation == PXA_GAME_RENDER_IO_SUBMIT) {
        assert(pxa_read_u32(data) == PXA_RASTER_DRAW_MAGIC);
        g_last_command_count = pxa_read_u32(data + 16);
        ++g_submit_count;
    }
    return (int32_t)length;
}

int render_scene_width(void) { return g_width; }
int render_scene_height(void) { return g_height; }
int game_block(int x, int y, int z) {
    (void)x;
    (void)y;
    (void)z;
    return BLOCK_AIR;
}

uint16_t render_block_color(int block) {
    return (uint16_t)(UINT16_C(0x2104) +
                      (uint16_t)block * UINT16_C(0x0421));
}

void render_menu_layout(const menu_state_t *menu) { (void)menu; }

static void configure_layout(int width, int height) {
    const int scale = width >= 640 ? 2 : 1;
    const int slot = width < 280 ? 20 : 24 * scale;
    const int panel_x = 6 * scale;
    const int panel_y = 6 * scale;
    memset(&g_layout, 0, sizeof(g_layout));
    g_layout.screen_w = width;
    g_layout.screen_h = height;
    g_layout.view_w = width;
    g_layout.view_h = height;
    g_layout.ui_scale = scale;
    g_layout.hotbar_slot = slot;
    g_layout.hotbar_x = (width - HOTBAR_SLOTS * slot) / 2;
    g_layout.hotbar_y = height - 48 * scale;
    g_layout.jump_x = width - 34 * scale;
    g_layout.jump_y = height - 42 * scale;
    g_layout.jump_r = 16 * scale;
    g_layout.action_x = g_layout.jump_x;
    g_layout.action_y = g_layout.jump_y - 38 * scale;
    g_layout.action_r = 16 * scale;
    g_layout.place_x = g_layout.jump_x;
    g_layout.place_y = g_layout.jump_y - 76 * scale;
    g_layout.place_r = 16 * scale;
    g_layout.down_x = g_layout.jump_x;
    g_layout.down_y = g_layout.jump_y - 112 * scale;
    g_layout.down_r = 12 * scale;
    g_layout.fly_x = width - 20 * scale;
    g_layout.fly_y = 28 * scale;
    g_layout.fly_r = 11 * scale;
    g_layout.bag_x = width - 48 * scale;
    g_layout.bag_y = 28 * scale;
    g_layout.bag_r = 11 * scale;
    g_layout.menu_x = width - 76 * scale;
    g_layout.menu_y = 28 * scale;
    g_layout.menu_r = 11 * scale;
    g_inv_layout.panel_x = panel_x;
    g_inv_layout.panel_y = panel_y;
    g_inv_layout.panel_w = width - 12 * scale;
    g_inv_layout.panel_h = height - 12 * scale;
    g_inv_layout.main_x = panel_x + (g_inv_layout.panel_w - 9 * slot) / 2;
    g_inv_layout.main_y = panel_y + 112 * scale;
    g_inv_layout.inv_hotbar_x = g_inv_layout.main_x;
    g_inv_layout.inv_hotbar_y = g_inv_layout.main_y + 3 * slot + 8 * scale;
    g_inv_layout.craft_x = g_inv_layout.main_x + 30 * scale;
    g_inv_layout.craft_y = panel_y + 34 * scale;
    g_inv_layout.result_x = g_inv_layout.craft_x + 3 * slot + 16 * scale;
    g_inv_layout.result_y = g_inv_layout.craft_y + slot;
    g_inv_layout.close_x = panel_x + g_inv_layout.panel_w - 18 * scale;
    g_inv_layout.close_y = panel_y + 32 * scale;
    g_inv_layout.close_r = 10 * scale;
}

static void assert_draws(const hud_state_t *hud, const menu_state_t *menu) {
    const player_t player = {0};
    const uint32_t previous_submits = g_submit_count;
    assert(voxel_raster_render(UINT64_C(0x100000003), g_submit_count + 1u, &player,
                               QUALITY_BALANCED, hud, menu, NULL) > 0);
    assert(g_submit_count == previous_submits + 1u);
    assert(g_last_command_count != 0);
}

static void test_resolution(int width, int height) {
    hud_state_t hud = {0};
    menu_state_t menu = {0};
    int index;
    g_width = width;
    g_height = height;
    configure_layout(width, height);
    hud.layout = g_layout;
    hud.flying = 1;
    hud.move_active = 1;
    hud.move_origin_x = 42;
    hud.move_origin_y = (int16_t)(height - 50);
    hud.move_dx = 8;
    hud.move_dy = -6;
    hud.mine_progress = 0.75F;
    hud.show_performance = 1;
    hud.fps_x10 = 300;
    hud.quality = 4;
    hud.toast = "BLOCK PLACED";
    for (index = 0; index < HOTBAR_SLOTS; ++index) {
        hud.hotbar_items[index] = (uint8_t)(BLOCK_GRASS + index);
        hud.hotbar_counts[index] = 64;
    }
    assert_draws(&hud, NULL);

    for (index = 0; index < INV_SLOTS; ++index) {
        g_inventory[index].item = (uint8_t)(BLOCK_GRASS + index % 12);
        g_inventory[index].count = 64;
    }
    for (index = 0; index < TABLE_CRAFT_SLOTS; ++index) {
        g_table_craft[index].item = BLOCK_WOOD;
        g_table_craft[index].count = 64;
    }
    g_table_result.item = BLOCK_TABLE;
    g_table_result.count = 64;
    hud.inventory_open = 1;
    hud.craft_table = 1;
    hud.cursor_item = BLOCK_STONE;
    hud.cursor_count = 64;
    hud.pointer_x = (int16_t)(width / 2);
    hud.pointer_y = (int16_t)(height / 2);
    assert_draws(&hud, NULL);

    menu.seed = UINT32_C(4294967295);
    menu.screen = 0;
    g_menu_button_count = 3;
    for (index = 0; index < g_menu_button_count; ++index) {
        g_menu_buttons[index].x = width / 4;
        g_menu_buttons[index].y = 88 * g_layout.ui_scale +
                                  index * 42 * g_layout.ui_scale;
        g_menu_buttons[index].w = width / 2;
        g_menu_buttons[index].h = 34 * g_layout.ui_scale;
        g_menu_buttons[index].enabled = 1;
    }
    assert_draws(NULL, &menu);
}

int main(void) {
    voxel_raster_reset();
    voxel_raster_set_capabilities(PXA_RASTER_CAP_FLAT_QUAD |
                                  PXA_RASTER_CAP_TEXTURED_QUAD |
                                  PXA_RASTER_CAP_AFFINE_UV |
                                  PXA_RASTER_CAP_PAINTER_POLYGON |
                                  PXA_RASTER_CAP_LIT_PALETTE_DEPTH |
                                  PXA_RASTER_CAP_DEPTH_CUTOUT |
                                  PXA_RASTER_CAP_FIXED_ALPHA_BLEND);
    test_resolution(256, 240);
    test_resolution(800, 480);
    return 0;
}
