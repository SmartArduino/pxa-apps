#include "pxa_control_capture.h"
#include "voxel_raster.h"

#include <assert.h>
#include <string.h>

#include "block_textures.h"
#include "pxa_raster.h"

chunk_t *g_chunk_grid[GRID_W][GRID_W];
int16_t g_chunk_origin_cx;
int16_t g_chunk_origin_cz;
uint32_t g_world_seed;
particle_t g_particles[MAX_PARTICLES];
mob_t g_mobs[MAX_MOBS];
item_stack_t g_inventory[INV_SLOTS];
item_stack_t g_craft[CRAFT_SLOTS];
item_stack_t g_craft_result;
item_stack_t g_table_craft[TABLE_CRAFT_SLOTS];
item_stack_t g_table_result;
render_layout_t g_layout;
inventory_layout_t g_inv_layout;
menu_button_t g_menu_buttons[MENU_BUTTON_MAX];
int g_menu_button_count;
static chunk_t chunks[GRID_COUNT];
static uint32_t submitted_bytes;
static uint32_t submitted_commands;
static uint32_t sky_band_count;
static uint32_t sun_count;
static uint32_t cloud_count;
static uint32_t painter_command_count;
static uint32_t painter_cutout_command_count;
static uint32_t painter_blend_command_count;
static uint32_t depth_command_count;
static uint32_t depth_cutout_command_count;
static uint32_t depth_blend_command_count;
static uint32_t coverage_command_count;
static uint32_t coverage_resolve_command_count;
static uint32_t coverage_solid_command_count;
static uint32_t coverage_perspective_command_count;
static uint32_t painter_depth_command_count;
static uint8_t painter_after_depth;
static uint8_t depth_blend_seen;
static uint8_t opaque_after_depth_blend;
static uint16_t first_sky_color;
static uint8_t sky_colors_differ;
static uint32_t outline_commands;
static uint8_t outline_out_of_bounds;

int32_t pxa_test_control(const uint8_t *data, uint32_t length) {
    (void)data;
    (void)length;
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *data,
               uint32_t length) {
    assert(handle == UINT64_C(0x100000003) && data != NULL);
    if (operation == PXA_GAME_RENDER_IO_SUBMIT) {
        uint32_t offset = PXA_RASTER_DRAW_HEADER_BYTES;
        assert(pxa_read_u32(data) == PXA_RASTER_DRAW_MAGIC);
        submitted_bytes = length;
        submitted_commands = pxa_read_u32(data + 16);
        sky_band_count = 0;
        sun_count = 0;
        cloud_count = 0;
        painter_command_count = 0;
        painter_cutout_command_count = 0;
        painter_blend_command_count = 0;
        depth_command_count = 0;
        depth_cutout_command_count = 0;
        depth_blend_command_count = 0;
        coverage_command_count = 0;
        coverage_resolve_command_count = 0;
        coverage_solid_command_count = 0;
        coverage_perspective_command_count = 0;
        painter_depth_command_count = 0;
        painter_after_depth = 0;
        depth_blend_seen = 0;
        opaque_after_depth_blend = 0;
        sky_colors_differ = 0;
        for (uint32_t index = 0; index < submitted_commands; ++index) {
            const uint16_t size = pxa_read_u16(data + offset + 2);
            assert(size >= 4u &&
                   offset + size <= length);
            if (data[offset] == PXA_RASTER_RECORD_FLAT_QUAD) {
                const uint16_t color = pxa_read_u16(data + offset + 4);
                if (color == 0) {
                    ++outline_commands;
                    for (uint8_t corner = 0; corner < 4; ++corner) {
                        const int16_t vertex_x = (int16_t)pxa_read_u16(
                            data + offset + 8 + corner * 4u);
                        const int16_t vertex_y = (int16_t)pxa_read_u16(
                            data + offset + 10 + corner * 4u);
                        if (vertex_x < -16 || vertex_x > 149 * 16 ||
                            vertex_y < -16 || vertex_y > 121 * 16)
                            outline_out_of_bounds = 1;
                    }
                }
                if (sky_band_count == 0) first_sky_color = color;
                else if (color != first_sky_color) sky_colors_differ = 1;
                ++sky_band_count;
                if (color == UINT16_C(0xffff)) ++sun_count;
                if (color == UINT16_C(0xf7de)) ++cloud_count;
            } else if (data[offset] == PXA_RASTER_RECORD_TEXTURED_QUAD) {
                if ((data[offset + 1] & PXA_RASTER_QUAD_PAINTER) != 0) {
                    if ((data[offset + 1] &
                         PXA_RASTER_QUAD_LIT_PALETTE) != 0)
                        ++painter_depth_command_count;
                    if (depth_command_count != 0) painter_after_depth = 1;
                    ++painter_command_count;
                    if ((data[offset + 1] &
                         PXA_RASTER_QUAD_TRANSPARENT_INDEX0) != 0)
                        ++painter_cutout_command_count;
                    if ((data[offset + 1] & PXA_RASTER_QUAD_BLEND_75) != 0)
                        ++painter_blend_command_count;
                    if ((data[offset + 1] &
                         PXA_RASTER_QUAD_COVERAGE_MASK) != 0) {
                        ++coverage_command_count;
                        if ((data[offset + 1] &
                             PXA_RASTER_QUAD_SOLID_COLOR) != 0)
                            ++coverage_solid_command_count;
                        if ((data[offset + 1] &
                             PXA_RASTER_QUAD_AFFINE_UV) == 0)
                            ++coverage_perspective_command_count;
                    }
                    if ((data[offset + 1] &
                         PXA_RASTER_QUAD_COVERAGE_RESOLVE) != 0)
                        ++coverage_resolve_command_count;
                } else {
                    ++depth_command_count;
                    if ((data[offset + 1] &
                         PXA_RASTER_QUAD_TRANSPARENT_INDEX0) != 0)
                        ++depth_cutout_command_count;
                    if ((data[offset + 1] &
                         PXA_RASTER_QUAD_BLEND_75) != 0) {
                        ++depth_blend_command_count;
                        depth_blend_seen = 1;
                    } else if (depth_blend_seen) {
                        opaque_after_depth_blend = 1;
                    }
                }
            }
            offset += size;
        }
        assert(offset == length);
    }
    return (int32_t)length;
}

int render_scene_width(void) { return 148; }
int render_scene_height(void) { return 120; }
int game_block(int x, int y, int z) {
    (void)x;
    (void)y;
    (void)z;
    return BLOCK_AIR;
}
uint16_t render_block_color(int block) {
    return block == BLOCK_STONE ? UINT16_C(0x8410) : UINT16_C(0xffff);
}
void render_menu_layout(const menu_state_t *menu) { (void)menu; }

int main(void) {
    player_t player = {8.0F, 6.0F, -2.0F, 0, 0, 0, 0, 0, 0, 0, 0};
    hud_state_t hud;
    voxel_raster_stats_t stats;
    uint32_t world_candidates;
    int grid_z;
    {
        const block_index_set_t *indices = block_texture_indices();
        uint32_t leaf_holes = 0;
        uint32_t water_holes = 0;
        uint32_t stone_holes = 0;
        for (int kind = 0; kind < 3; ++kind) {
            for (int pixel = 0; pixel < 256; ++pixel) {
                leaf_holes += indices[BLOCK_LEAVES][kind][pixel] == 0;
                water_holes += indices[BLOCK_WATER][kind][pixel] == 0;
                stone_holes += indices[BLOCK_STONE][kind][pixel] == 0;
            }
        }
        assert(leaf_holes != 0 && water_holes == 0);
        assert(stone_holes == 0);
        assert(block_texture_palette()[0] == 0);
    }
    memset(chunks, 0, sizeof(chunks));
    memset(&hud, 0, sizeof(hud));
    g_chunk_origin_cx = -3;
    g_chunk_origin_cz = -3;
    for (grid_z = 0; grid_z < GRID_W; ++grid_z) {
        int grid_x;
        for (grid_x = 0; grid_x < GRID_W; ++grid_x) {
            chunk_t *chunk = &chunks[grid_z * GRID_W + grid_x];
            chunk->cx = (int16_t)(g_chunk_origin_cx + grid_x);
            chunk->cz = (int16_t)(g_chunk_origin_cz + grid_z);
            g_chunk_grid[grid_z][grid_x] = chunk;
        }
    }
    chunks[3 * GRID_W + 3].loaded = 1;
    chunks[3 * GRID_W + 3].revision = 1;
    for (int y = 0; y < 4; ++y)
        for (int z = 0; z < CHUNK_SIZE; ++z)
            for (int x = 0; x < CHUNK_SIZE; ++x)
                chunks[3 * GRID_W + 3]
                    .blocks[(y << 8) | (z << CHUNK_BITS) | x] = BLOCK_STONE;

    voxel_raster_reset();
    voxel_raster_set_capabilities(PXA_RASTER_CAP_FLAT_QUAD |
                                  PXA_RASTER_CAP_TEXTURED_QUAD |
                                  PXA_RASTER_CAP_AFFINE_UV |
                                  PXA_RASTER_CAP_PAINTER_POLYGON |
                                  PXA_RASTER_CAP_LIT_PALETTE_DEPTH |
                                  PXA_RASTER_CAP_DEPTH_CUTOUT |
                                  PXA_RASTER_CAP_FIXED_ALPHA_BLEND);
    /* Resource preparation is now asynchronous voxel_assets_begin/on_event.
     * This transport mock checks command generation; the signed-AOT resource
     * integration test verifies real file loading, bindings and output pixels. */
    assert(voxel_raster_render(UINT64_C(0x100000003), 1, &player, QUALITY_BALANCED, &hud, NULL,
                               NULL) >
           0);
    voxel_raster_get_stats(&stats);
    world_candidates = stats.candidate_quads;
    /* Host depth testing requires bounded sub-quads rather than one large
     * greedy face per side; a 16x4 slab produces at most 24 8x8 faces. */
    assert(stats.cached_quads <= 24);
    assert(stats.candidate_quads != 0 && stats.submitted_quads != 0);
    assert(stats.painter_quads != 0 && stats.depth_quads == 0);
    assert(painter_command_count != 0 && depth_command_count == 0);
    assert(!painter_after_depth);
    assert(stats.submitted_quads < CHUNK_SIZE * CHUNK_SIZE);
    assert(submitted_commands > stats.submitted_quads &&
           submitted_commands <= stats.submitted_quads + 64);
    assert(sky_band_count >= 10 && sky_colors_differ);
    assert(sun_count != 0 && cloud_count != 0);
    assert(submitted_bytes == stats.draw_list_bytes &&
           submitted_bytes < 4096);
    g_mobs[0].x = 8.0F;
    g_mobs[0].y = 6.0F;
    g_mobs[0].z = 3.0F;
    g_mobs[0].alive = 1;
    g_mobs[0].kind = MOB_SLIME;
    assert(voxel_raster_render(UINT64_C(0x100000003), 2, &player, QUALITY_BALANCED, &hud, NULL,
                               NULL) >
           0);
    voxel_raster_get_stats(&stats);
    assert(stats.candidate_quads == world_candidates + 1u);
    memset(g_mobs, 0, sizeof(g_mobs));
    player.x = 8.0F;
    player.y = 2.5F;
    player.z = 8.0F;
    player.yaw = 0.0F;
    player.pitch = -0.55F;
    assert(voxel_raster_render(UINT64_C(0x100000003), 3, &player, QUALITY_BALANCED, &hud, NULL,
                               NULL) >
           0);
    voxel_raster_get_stats(&stats);
    assert(stats.clipped_quads != 0);
    assert(stats.candidate_quads != 0 && stats.submitted_quads != 0);

    /* Large greedy surfaces are subdivided for painter ordering. The fixed
     * cap bounds projection and command growth while reducing the depth
     * interval represented by one sort key. */
    player.x = 8.0F;
    player.y = 6.0F;
    player.z = -10.0F;
    player.yaw = 0.0F;
    player.pitch = -0.35F;
    assert(voxel_raster_render(UINT64_C(0x100000003), 4, &player, QUALITY_BALANCED, &hud, NULL,
                               NULL) >
           0);
    voxel_raster_get_stats(&stats);
    assert(stats.painter_splits != 0);
    assert(stats.painter_splits <= stats.cached_quads * 3u);

    chunks[3 * GRID_W + 3]
        .blocks[(4 << 8) | (4 << CHUNK_BITS) | 8] = BLOCK_LEAVES;
    ++chunks[3 * GRID_W + 3].revision;
    player.x = 8.0F;
    player.y = 4.0F;
    player.z = 0.0F;
    player.pitch = -0.2F;
    assert(voxel_raster_render(UINT64_C(0x100000003), 5, &player, QUALITY_BALANCED, &hud, NULL,
                               NULL) > 0);
    assert(painter_cutout_command_count != 0);
    assert(depth_cutout_command_count == 0);
    chunks[3 * GRID_W + 3]
        .blocks[(4 << 8) | (5 << CHUNK_BITS) | 8] = BLOCK_WATER;
    ++chunks[3 * GRID_W + 3].revision;
    assert(voxel_raster_render(UINT64_C(0x100000003), 6, &player, QUALITY_BALANCED, &hud, NULL,
                               NULL) > 0);
    assert(painter_blend_command_count != 0);
    assert(depth_blend_command_count == 0);
    assert(!opaque_after_depth_blend);
    voxel_raster_set_capabilities(PXA_RASTER_CAP_FLAT_QUAD |
                                  PXA_RASTER_CAP_TEXTURED_QUAD |
                                  PXA_RASTER_CAP_AFFINE_UV |
                                  PXA_RASTER_CAP_PAINTER_POLYGON |
                                  PXA_RASTER_CAP_LIT_PALETTE_DEPTH |
                                  PXA_RASTER_CAP_DEPTH_CUTOUT |
                                  PXA_RASTER_CAP_FIXED_ALPHA_BLEND |
                                  PXA_RASTER_CAP_COVERAGE_MASK |
                                  PXA_RASTER_CAP_PAINTER_PERSPECTIVE);
    g_mobs[0].x = 8.0F;
    g_mobs[0].y = 5.0F;
    g_mobs[0].z = 8.0F;
    g_mobs[0].alive = 1;
    assert(voxel_raster_render(UINT64_C(0x100000003), 7, &player, QUALITY_BALANCED, &hud, NULL,
                               NULL) > 0);
    assert(coverage_command_count != 0);
    assert(coverage_resolve_command_count != 0);
    assert(coverage_solid_command_count != 0);
    assert(coverage_perspective_command_count != 0);
    assert(depth_command_count == 0);
    voxel_raster_get_stats(&stats);
    assert(stats.candidate_quads <= 192u && stats.sort_edges != 0);
    {
        ray_hit_t target = {0};
        target.hit = 1;
        target.x = 8;
        target.y = 6;
        target.z = 8;
        chunks[3 * GRID_W + 3]
            .blocks[(6 << 8) | (8 << CHUNK_BITS) | 8] = BLOCK_STONE;
        ++chunks[3 * GRID_W + 3].revision;
        player.x = 8.5F;
        player.y = 5.0F;
        player.z = 6.0F;
        player.yaw = 0.0F;
        player.pitch = 0.0F;
        outline_commands = 0;
        outline_out_of_bounds = 0;
        assert(voxel_raster_render(UINT64_C(0x100000003), 8, &player, QUALITY_BALANCED, &hud,
                                   NULL, &target) > 0);
        assert(outline_commands != 0 && !outline_out_of_bounds);
        chunks[3 * GRID_W + 3]
            .blocks[(6 << 8) | (7 << CHUNK_BITS) | 8] = BLOCK_STONE;
        ++chunks[3 * GRID_W + 3].revision;
        outline_commands = 0;
        assert(voxel_raster_render(UINT64_C(0x100000003), 9, &player, QUALITY_BALANCED, &hud,
                                   NULL, &target) > 0);
        assert(outline_commands == 0);
        chunks[3 * GRID_W + 3]
            .blocks[(6 << 8) | (7 << CHUNK_BITS) | 8] = BLOCK_AIR;
        ++chunks[3 * GRID_W + 3].revision;
        player.z = 7.2F;
        outline_commands = 0;
        outline_out_of_bounds = 0;
        assert(voxel_raster_render(UINT64_C(0x100000003), 10, &player, QUALITY_BALANCED, &hud,
                                   NULL, &target) > 0);
        assert(outline_commands != 0 && !outline_out_of_bounds);
    }
    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            if (((x + z) & 1) != 0)
                chunks[3 * GRID_W + 3]
                    .blocks[(4 << 8) | (z << CHUNK_BITS) | x] = BLOCK_STONE;
        }
    }
    ++chunks[3 * GRID_W + 3].revision;
    player.x = 8.0F;
    player.y = 6.0F;
    player.z = -4.0F;
    player.pitch = -0.35F;
    assert(voxel_raster_render(UINT64_C(0x100000003), 11, &player, QUALITY_BALANCED, &hud, NULL,
                               NULL) > 0);
    voxel_raster_get_stats(&stats);
    assert(stats.candidate_quads > 192u);
    assert(stats.sort_edges != 0);
    assert(stats.sort_cycles == 0);
    voxel_raster_set_capabilities(PXA_RASTER_CAP_FLAT_QUAD |
                                  PXA_RASTER_CAP_TEXTURED_QUAD |
                                  PXA_RASTER_CAP_AFFINE_UV |
                                  PXA_RASTER_CAP_PAINTER_POLYGON |
                                  PXA_RASTER_CAP_LIT_PALETTE_DEPTH |
                                  PXA_RASTER_CAP_DEPTH_CUTOUT |
                                  PXA_RASTER_CAP_FIXED_ALPHA_BLEND |
                                  PXA_RASTER_CAP_PAINTER_PERSPECTIVE |
                                  PXA_RASTER_CAP_PAINTER_DEPTH);
    assert(voxel_raster_render(UINT64_C(0x100000003), 12, &player, QUALITY_BALANCED, &hud, NULL,
                               NULL) > 0);
    voxel_raster_get_stats(&stats);
    assert(painter_depth_command_count != 0);
    assert(coverage_command_count == 0);
    assert(stats.depth_quads != 0 && stats.sort_edges == 0);
    return 0;
}
