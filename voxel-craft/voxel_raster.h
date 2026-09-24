#ifndef VOXEL_CRAFT_RASTER_H
#define VOXEL_CRAFT_RASTER_H

#include <stdint.h>

#include "game.h"
#include "render.h"

typedef struct {
    uint32_t cached_quads;
    uint32_t rebuilt_chunks;
    uint32_t mesh_build_passes;
    uint32_t visible_chunks;
    uint32_t unloaded_chunks;
    uint32_t pending_mesh_chunks;
    uint32_t mesh_input_quads;
    uint32_t mesh_visited_quads;
    uint32_t mesh_overflow_quads;
    uint32_t candidate_quads;
    uint32_t candidate_limit;
    uint32_t chunk_base_quota;
    uint32_t candidate_budget_omitted;
    uint32_t projection_budget_dropped;
    uint32_t submitted_quads;
    uint32_t backface_culled;
    uint32_t frustum_culled;
    uint32_t clipped_quads;
    uint32_t dropped_quads;
    uint32_t command_budget_dropped;
    uint32_t degenerate_dropped;
    uint32_t append_failures;
    uint32_t affine_quads;
    uint32_t painter_quads;
    uint32_t depth_quads;
    uint32_t painter_splits;
    uint32_t sort_edges;
    uint32_t sort_edge_overflow;
    uint32_t sort_cycles;
    uint32_t draw_commands;
    uint32_t draw_list_bytes;
    uint32_t covered_pixel_budget;
    uint32_t fog_end_q8;
} voxel_raster_stats_t;

void voxel_raster_reset(void);
/* Full-detail horizon in blocks. The App tunes it from the measured frame
 * cost so one build keeps the same frame rate on panels and CPUs of very
 * different capacity; it is clamped to the range the fog ramp covers. */
void voxel_raster_set_view_distance(float blocks);
float voxel_raster_view_distance(void);
float voxel_raster_max_view_distance(void);
float voxel_raster_min_view_distance(void);
void voxel_raster_set_capabilities(uint32_t capabilities);
int voxel_raster_upload_assets(uint32_t surface_handle);
/* Draws the 3D scene plus either the play HUD or a menu. A menu without the
 * paint-overlay flag replaces the scene; a pause menu overlays the frozen
 * frame. Menus are drawn at the scene resolution (1x when the caller sizes the
 * Surface for the display). */
int32_t voxel_raster_render(uint32_t surface_handle, uint64_t frame_id,
                            const player_t *player, uint8_t quality,
                            const hud_state_t *hud, const menu_state_t *menu,
                            const ray_hit_t *target);
void voxel_raster_get_stats(voxel_raster_stats_t *stats);
/* Optional measurement hook: reports phase 0 after terrain projection and
 * phase 1 after the painter order, so a build can attribute Guest time. */
void voxel_raster_set_phase_marker(void (*marker)(uint8_t phase));

#endif
