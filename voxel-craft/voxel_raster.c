#include "voxel_raster.h"

#include <stddef.h>
#include <stdint.h>

#include "block_textures.h"
#include "pxa_log.h"
#include "pxa_raster.h"
#include "rc_math.h"
#include "voxel_font_data.h"
#include "voxel_sky.h"

#define VOXEL_MESH_QUADS_PER_CHUNK 640u
#define VOXEL_MESH_MAX_SPAN 16
#define VOXEL_MESH_SLICES_PER_STEP 4u
#define VOXEL_RASTER_CANDIDATES 1024u
#define VOXEL_RASTER_TEXTURED_BLOCKS 15u
#define VOXEL_RASTER_FONT_SLOT 15u
#define VOXEL_RASTER_FONT_SLOT_EXTENDED 45u
#define VOXEL_RASTER_FONT_SLOT_AA0 45u
#define VOXEL_RASTER_FONT_SLOT_AA1 46u
#define VOXEL_RASTER_FONT_SLOT_AA2 47u
#define VOXEL_RASTER_FONT_GLYPHS 42u
#define VOXEL_RASTER_FONT_WIDTH (VOXEL_RASTER_FONT_GLYPHS * 4u)
/* Set to 0 to force the legacy cut-out font: the antialiased tiers need
 * PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA and three free texture slots. */
#ifndef VOXEL_FONT_AA
#define VOXEL_FONT_AA 1
#endif
#define VOXEL_RASTER_NEAR 0.08F
#define VOXEL_RASTER_TAN_HALF 0.70F
#define VOXEL_RASTER_PARTICLE_LIMIT 32u
/* Terrain prefers the Host depth buffer over painter polygons when the Host
 * supports depth-tested textured quads. Depth resolves face order in hardware,
 * so distant trees stop bleeding through nearer water, textured faces get
 * perspective-correct UVs, and the Guest no longer pays for adaptive splitting
 * and the exact painter sort. The painter path stays for Hosts without it. */
/* Depth testing is the more correct path, but the Host's depth + perspective
 * textured triangle loop costs about five times the painter polygon per pixel
 * on esp32s31 (167 ms vs 35 ms for the same frame), so the painter path stays
 * the default. Build with -DVOXEL_RASTER_DEPTH_TERRAIN=1 to compare. */
#ifndef VOXEL_RASTER_DEPTH_TERRAIN
#define VOXEL_RASTER_DEPTH_TERRAIN 0u
#endif
/* Depth testing needs no order-dependent machinery, so the terrain budget can
 * cover the whole fog range instead of dropping the farthest faces. */
#define VOXEL_RASTER_DEPTH_CANDIDATES 640u
/* Enough headroom that the visible set rarely hits the terrain candidate
 * limit: a saturated limit drops the farthest faces, which pop in and out as
 * the camera turns. */
#define VOXEL_RASTER_PERFORMANCE_CANDIDATES 224u
#define VOXEL_RASTER_PERFORMANCE_FOG 26.0F
#define VOXEL_RASTER_HYBRID_DEPTH_Q8 0u
#define VOXEL_RASTER_PAINTER_SPLIT_DEPTH_Q8 (1u * 256u)
/* With a perspective-correct Host the split no longer bounds a UV error; it
 * only refines the painter sort key for faces that span a lot of depth, so a
 * far coarser threshold keeps the candidate budget for real geometry. */
#define VOXEL_RASTER_PAINTER_SORT_SPLIT_DEPTH_Q8 (5u * 256u)
#define VOXEL_RASTER_PAINTER_SPLIT_SCREEN_Q4 (8 * 16)
#define VOXEL_RASTER_PAINTER_SPLIT_MAX_PARTS 8u
#define VOXEL_RASTER_PAINTER_SPLIT_FAR_Q8 (12u * 256u)
#define VOXEL_RASTER_EXACT_CANDIDATES 448u
#define VOXEL_RASTER_COVERAGE_EXACT_CANDIDATES 448u
#define SORT_BUCKETS 256u
/* The exact painter order is an O(n^2) pair sweep. Keep it as a switch so the
 * measurement build can trade it for the cheaper depth-key sort. */
#ifndef VOXEL_RASTER_EXACT_SORT
#define VOXEL_RASTER_EXACT_SORT 0
#endif
#define VOXEL_RASTER_SORT_EDGES 4096u
#define VOXEL_RASTER_HUD_COMMAND_RESERVE 384u
/* The in-game HUD measures ~310 commands and under 9 KB. The old 16 KB
 * reserve made the draw list run out of bytes long before it ran out of
 * commands, which cost terrain for headroom nothing used. */
#define VOXEL_RASTER_HUD_BYTE_RESERVE 12288u
#define VOXEL_RASTER_UNDERWATER_TINT UINT16_C(0x008a)
#define VOXEL_RASTER_UNDERWATER_CLEAR UINT16_C(0x118d)

#define HUD_PANEL UINT16_C(0x2945)
#define HUD_PANEL_LIGHT UINT16_C(0x5aeb)
#define HUD_SHADOW UINT16_C(0x18c3)
#define HUD_TEXT UINT16_C(0xffff)
#define HUD_SELECT UINT16_C(0xfec8)
#define HUD_GREEN UINT16_C(0x5eea)
/* RGB565 of the menu background 0x142c50 used by the mapped renderer. */
#define MENU_BG UINT16_C(0x116a)
#define HUD_MUTED UINT16_C(0x94b2)
/* Round and rectangular UI buttons share this slate face; edges and the top
 * highlight are derived from it so one colour styles the whole control set. */
#define HUD_BTN_FACE UINT16_C(0x322c)
#define HUD_BTN_ACTIVE UINT16_C(0x4cac)

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t u_length;
    uint8_t v_length;
    uint8_t axis;
    int8_t sign;
    uint8_t block;
} mesh_quad_t;

typedef struct {
    const chunk_t *chunk;
    const chunk_t *build_chunk;
    uint32_t revision;
    uint32_t build_revision;
    int16_t chunk_cx;
    int16_t chunk_cz;
    int16_t build_chunk_cx;
    int16_t build_chunk_cz;
    uint16_t quad_count;
    uint16_t dropped;
    uint16_t build_quad_count;
    uint16_t build_dropped;
    uint8_t build_pass;
    uint8_t build_slice;
    uint8_t building;
    mesh_quad_t quads[VOXEL_MESH_QUADS_PER_CHUNK];
} chunk_mesh_t;

typedef struct {
    pxa_raster_vertex_t vertices[4];
    uint16_t nearest_depth_q8;
    uint16_t farthest_depth_q8;
    uint16_t sort_depth_q8;
    int16_t min_x_q4;
    int16_t min_y_q4;
    int16_t max_x_q4;
    int16_t max_y_q4;
    float inverse_a;
    float inverse_b;
    float inverse_c;
    uint8_t inverse_ready;
    uint16_t color;
    uint8_t texture_slot;
    uint8_t textured;
    uint8_t affine;
    uint8_t transparent_index0;
    uint8_t blend_75;
} projected_quad_t;

typedef struct {
    uint16_t ahead;
    uint16_t next;
} painter_sort_edge_t;

typedef struct {
    float fx;
    float fy;
    float fz;
    float rx;
    float rz;
    float ux;
    float uy;
    float uz;
    float x;
    float y;
    float z;
    float tan_x;
    float tan_y;
    uint16_t width;
    uint16_t height;
    float half_width;
    float half_height;
    float scale_x;
    float scale_y;
    float fog_start;
    float fog_end;
} raster_camera_t;

typedef struct {
    const chunk_t *chunk;
    chunk_mesh_t *mesh;
    float depth;
    uint8_t backface_mask;
} visible_chunk_t;

static chunk_mesh_t g_meshes[GRID_COUNT];
static chunk_mesh_t *g_building_mesh;
static mesh_quad_t g_mesh_build_quads[VOXEL_MESH_QUADS_PER_CHUNK];
static projected_quad_t g_candidates[VOXEL_RASTER_CANDIDATES];
static uint16_t g_sort_order[VOXEL_RASTER_CANDIDATES];
static uint16_t g_sort_window[VOXEL_RASTER_EXACT_CANDIDATES];
static uint16_t g_sort_indegree[VOXEL_RASTER_EXACT_CANDIDATES];
static uint16_t g_sort_edge_heads[VOXEL_RASTER_EXACT_CANDIDATES];
static uint16_t g_sort_heap[VOXEL_RASTER_EXACT_CANDIDATES];
static uint16_t g_sort_sweep[VOXEL_RASTER_EXACT_CANDIDATES];
static uint8_t g_sort_emitted[VOXEL_RASTER_EXACT_CANDIDATES];
_Alignas(uint32_t) static uint8_t g_draw_list[PXA_RASTER_MAX_DRAW_BYTES];
#define VOXEL_RASTER_UPLOAD_SCRATCH \
    (PXA_RASTER_UPLOAD_HEADER_BYTES + 256u * 66u)
static uint8_t g_upload[VOXEL_RASTER_UPLOAD_SCRATCH];
static uint8_t g_font_texture[VOXEL_RASTER_FONT_WIDTH * 5u];
/* Antialiased glyph tiers, selected when the Host blends per-texel coverage.
 * Without it the legacy 3x5 cut-out font keeps every panel working. */
static uint8_t g_font_aa;
static const uint8_t *g_font_aa_pixels[VOXEL_FONT_TIERS];
static uint8_t g_font_aa_slot[VOXEL_FONT_TIERS];
static uint8_t g_font_aa_width[VOXEL_FONT_TIERS];
static uint8_t g_font_aa_height[VOXEL_FONT_TIERS];
static uint8_t g_font_aa_columns[VOXEL_FONT_TIERS];
/* Painter polygons select one pre-lit palette row per pixel. */
#define VOXEL_RASTER_LIGHT_LEVELS 16u
static uint16_t
    g_lit_palette[VOXEL_RASTER_LIGHT_LEVELS * PXA_RASTER_PALETTE_COLORS];
static uint8_t g_palette_upload[PXA_RASTER_UPLOAD_HEADER_BYTES +
                                VOXEL_RASTER_LIGHT_LEVELS *
                                    PXA_RASTER_PALETTE_COLORS * 2u];
static voxel_raster_stats_t g_stats;
static void (*g_phase_marker)(uint8_t phase);
static uint32_t g_raster_capabilities;
static uint8_t g_mesh_cache_warmed;
static uint8_t g_painter_perspective;
/* Tuned by the App from the measured frame cost; see
 * voxel_raster_set_view_distance(). */
#define VOXEL_RASTER_VIEW_MIN 12.0F
#define VOXEL_RASTER_VIEW_MAX 32.0F
static float g_view_distance = VOXEL_RASTER_VIEW_MAX;
static int16_t g_water_uv_offset_q4;

static const char kFontCharacters[] =
    "0123456789.:-/+ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
static const uint8_t kFontRows[VOXEL_RASTER_FONT_GLYPHS][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7},
    {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7},
    {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7},
    {7, 5, 7, 1, 7}, {0, 0, 0, 0, 2}, {0, 2, 0, 2, 0},
    {0, 0, 7, 0, 0}, {1, 1, 2, 4, 4}, {0, 2, 7, 2, 0},
    {7, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {7, 4, 4, 4, 7},
    {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4},
    {7, 4, 5, 5, 7}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7},
    {1, 1, 1, 5, 7}, {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7},
    {5, 7, 7, 5, 5}, {5, 7, 7, 7, 5}, {7, 5, 5, 5, 7},
    {7, 5, 7, 4, 4}, {7, 5, 5, 7, 1}, {6, 5, 6, 5, 5},
    {7, 4, 7, 1, 7}, {7, 2, 2, 2, 2}, {5, 5, 5, 5, 7},
    {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5},
    {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7}, {0, 0, 0, 0, 0},
};

typedef struct {
    uint8_t slot;
    uint8_t width;
    uint8_t height;
    uint8_t columns;
    uint8_t rows;
    const uint8_t *pixels;
} font_tier_t;

/* Antialiased HUD tiers, drawn at their native texel size so no atlas is ever
 * upscaled. Selected by ui_text/font_logical_tier once the Host advertises the
 * per-texel blend; otherwise the legacy 3x5 cut-out font is used. */
static const font_tier_t kFontTiers[VOXEL_FONT_TIERS] = {
    {VOXEL_RASTER_FONT_SLOT_AA0, VOXEL_FONT_SMALL_WIDTH,
     VOXEL_FONT_SMALL_HEIGHT, VOXEL_FONT_SMALL_COLUMNS, VOXEL_FONT_SMALL_ROWS,
     voxel_font_small_pixels},
    {VOXEL_RASTER_FONT_SLOT_AA1, VOXEL_FONT_MEDIUM_WIDTH,
     VOXEL_FONT_MEDIUM_HEIGHT, VOXEL_FONT_MEDIUM_COLUMNS,
     VOXEL_FONT_MEDIUM_ROWS, voxel_font_medium_pixels},
    {VOXEL_RASTER_FONT_SLOT_AA2, VOXEL_FONT_LARGE_WIDTH,
     VOXEL_FONT_LARGE_HEIGHT, VOXEL_FONT_LARGE_COLUMNS, VOXEL_FONT_LARGE_ROWS,
     voxel_font_large_pixels},
};

static int block_uses_cutout(int block) {
    return block == BLOCK_LEAVES;
}

static int block_is_translucent(int block) {
    return block == BLOCK_WATER;
}

static int block_is_transparent(int block) {
    return block_uses_cutout(block) || block_is_translucent(block);
}

static int block_face_visible(int block, int neighbor) {
    const int transparent = block_is_transparent(block);
    const int neighbor_transparent = block_is_transparent(neighbor);
    if (block == BLOCK_AIR) return 0;
    if (neighbor == BLOCK_AIR) return 1;
    if (!transparent) return neighbor_transparent;
    if (!neighbor_transparent || block == neighbor) return 0;
    /* Emit only one side of a boundary between different transparent
     * materials. */
    return block < neighbor;
}

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return (uint16_t)(((uint16_t)(red >> 3) << 11) |
                      ((uint16_t)(green >> 2) << 5) | (blue >> 3));
}

/* Extended slots give every block its own top/side/bottom 16x16 tile. Older
 * Hosts only have 16 slots, so they fall back to one side tile per block. */
static uint8_t texture_slot_for(uint8_t block, uint8_t kind) {
    if ((g_raster_capabilities & PXA_RASTER_CAP_TEXTURE_SLOTS_48) != 0)
        return (uint8_t)((block - 1u) * 3u + kind);
    return (uint8_t)(block - 1u);
}

static uint8_t font_slot(void) {
    return (g_raster_capabilities & PXA_RASTER_CAP_TEXTURE_SLOTS_48) != 0
               ? VOXEL_RASTER_FONT_SLOT_EXTENDED
               : VOXEL_RASTER_FONT_SLOT;
}

void voxel_raster_set_view_distance(float blocks) {
    if (blocks < VOXEL_RASTER_VIEW_MIN) blocks = VOXEL_RASTER_VIEW_MIN;
    if (blocks > VOXEL_RASTER_VIEW_MAX) blocks = VOXEL_RASTER_VIEW_MAX;
    g_view_distance = blocks;
}

float voxel_raster_view_distance(void) { return g_view_distance; }

float voxel_raster_max_view_distance(void) { return VOXEL_RASTER_VIEW_MAX; }

float voxel_raster_min_view_distance(void) { return VOXEL_RASTER_VIEW_MIN; }

void voxel_raster_set_capabilities(uint32_t capabilities) {
    g_raster_capabilities = capabilities;
    g_painter_perspective = (uint8_t)(
        (capabilities & PXA_RASTER_CAP_PAINTER_PERSPECTIVE) != 0);
}

/* Same rounding as the Host's light_rgb565(), so the pre-lit rows match the
 * per-pixel shading of the depth path. */
static uint16_t shade_rgb565(uint16_t color, uint32_t intensity) {
    uint32_t red = (color >> 11) * intensity + 127u;
    uint32_t green = ((color >> 5) & 63u) * intensity + 127u;
    uint32_t blue = (color & 31u) * intensity + 127u;
    red = (red + 1u + ((red + 1u) >> 8)) >> 8;
    green = (green + 1u + ((green + 1u) >> 8)) >> 8;
    blue = (blue + 1u + ((blue + 1u) >> 8)) >> 8;
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

static void build_lit_palette(void) {
    const uint16_t *base = block_texture_palette();
    uint32_t level;
    uint32_t index;
    for (level = 0; level < VOXEL_RASTER_LIGHT_LEVELS; ++level) {
        /* Row 0 stays the full-bright palette: textured sprites (item icons)
         * sample palette[texel] and have no light row of their own, while
         * painter polygons select darker rows through light_row(). */
        const uint32_t intensity =
            (VOXEL_RASTER_LIGHT_LEVELS - 1u - level) * 255u /
            (VOXEL_RASTER_LIGHT_LEVELS - 1u);
        uint16_t *row =
            &g_lit_palette[level * PXA_RASTER_PALETTE_COLORS];
        for (index = 0; index < PXA_RASTER_PALETTE_COLORS; ++index)
            row[index] = shade_rgb565(base[index], intensity);
    }
}

/* Maps a 0..255 face light to a pre-lit palette row index (bright faces use
 * row 0 so sprite sampling keeps the unlit palette). */
static uint8_t light_row(uint8_t light) {
    const uint32_t level =
        ((uint32_t)light * (VOXEL_RASTER_LIGHT_LEVELS - 1u) + 127u) / 255u;
    return (uint8_t)(VOXEL_RASTER_LIGHT_LEVELS - 1u - level);
}

int voxel_raster_upload_assets(uint32_t surface_handle) {
    const block_index_set_t *indices = block_texture_indices();
    const uint16_t *palette = block_texture_palette();
    const int extended =
        (g_raster_capabilities & PXA_RASTER_CAP_TEXTURE_SLOTS_48) != 0;
    uint8_t block;
    int32_t result;
    if ((g_raster_capabilities & PXA_RASTER_CAP_LIT_PALETTE_DEPTH) != 0) {
        build_lit_palette();
        result = pxa_raster_upload_lit_palette_rgb565(
            surface_handle, VOXEL_RASTER_LIGHT_LEVELS, g_lit_palette,
            g_palette_upload, sizeof(g_palette_upload));
        if (result !=
            (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                      VOXEL_RASTER_LIGHT_LEVELS *
                          PXA_RASTER_PALETTE_COLORS * 2u)) {
            (void)pxa_log_error("voxel: lit palette upload failed");
            return 0;
        }
    } else {
        result = pxa_raster_upload_palette_rgb565(
            surface_handle, palette, g_upload, sizeof(g_upload));
        if (result != (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + 512u)) {
            (void)pxa_log_error("voxel: palette upload failed");
            return 0;
        }
    }
    for (block = 0; block < VOXEL_RASTER_TEXTURED_BLOCKS; ++block) {
        const uint8_t kinds = extended ? 3u : 1u;
        uint8_t kind;
        for (kind = 0; kind < kinds; ++kind) {
            const uint8_t texture_kind =
                extended ? kind : BLOCK_TEXTURE_SIDE;
            const uint8_t slot =
                texture_slot_for((uint8_t)(block + 1u), texture_kind);
            result = pxa_raster_upload_texture_index8(
                surface_handle, slot, 16, 16, indices[block + 1][texture_kind],
                g_upload, sizeof(g_upload));
            if (result != (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + 256u)) {
                (void)pxa_log_error("voxel: block texture upload failed");
                return 0;
            }
        }
    }
    pxa_raster_zero_bytes(g_font_texture, sizeof(g_font_texture));
    for (block = 0; block < VOXEL_RASTER_FONT_GLYPHS; ++block) {
        uint8_t row;
        for (row = 0; row < 5; ++row) {
            uint8_t column;
            for (column = 0; column < 3; ++column) {
                if ((kFontRows[block][row] & (4u >> column)) != 0)
                    g_font_texture[(size_t)row * VOXEL_RASTER_FONT_WIDTH +
                                   (size_t)block * 4u + column] = 255;
            }
        }
    }
    result = pxa_raster_upload_texture_index8(
        surface_handle, font_slot(), VOXEL_RASTER_FONT_WIDTH, 5,
        g_font_texture, g_upload, sizeof(g_upload));
    if (result != (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                            VOXEL_RASTER_FONT_WIDTH * 5u)) {
        (void)pxa_log_error("voxel: font texture upload failed");
        return 0;
    }
    g_font_aa = (g_raster_capabilities &
                 PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA) != 0 &&
                (g_raster_capabilities & PXA_RASTER_CAP_TEXTURE_SLOTS_48) != 0;
    if (g_font_aa) {
        uint8_t tier;
        for (tier = 0; tier < VOXEL_FONT_TIERS; ++tier) {
            const font_tier_t *const face = &kFontTiers[tier];
            const uint16_t texture_width =
                (uint16_t)(face->width * face->columns);
            const uint16_t texture_height =
                (uint16_t)(face->height * face->rows);
            const uint32_t bytes =
                (uint32_t)texture_width * texture_height;
            result = pxa_raster_upload_texture_index8(
                surface_handle, face->slot, texture_width, texture_height,
                face->pixels, g_upload, sizeof(g_upload));
            if (result != (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + bytes)) {
                /* Missing glyph tiers are a cosmetic loss: keep the legacy
                 * cut-out font instead of failing the whole Surface. */
                (void)pxa_log_error("voxel: font tier upload failed");
                g_font_aa = 0;
                break;
            }
        }
    }
    return 1;
}

static int local_block(const chunk_t *chunk, int x, int y, int z) {
    if ((unsigned)x >= CHUNK_SIZE || (unsigned)y >= CHUNK_HEIGHT ||
        (unsigned)z >= CHUNK_SIZE)
        return BLOCK_AIR;
    return chunk->blocks[(y << 8) | (z << CHUNK_BITS) | x];
}

static int neighbor_block(const chunk_t *chunk, int x, int y, int z) {
    if ((unsigned)x < CHUNK_SIZE && (unsigned)y < CHUNK_HEIGHT &&
        (unsigned)z < CHUNK_SIZE)
        return local_block(chunk, x, y, z);
    return game_block_fast((int)chunk->cx * CHUNK_SIZE + x, y,
                           (int)chunk->cz * CHUNK_SIZE + z);
}

static void append_mesh_quad(chunk_mesh_t *mesh, int axis, int sign,
                             int slice, int u, int v, int u_length,
                             int v_length, int block) {
    mesh_quad_t *quad;
    if (mesh->build_quad_count >= VOXEL_MESH_QUADS_PER_CHUNK) {
        ++mesh->build_dropped;
        return;
    }
    quad = &g_mesh_build_quads[mesh->build_quad_count++];
    pxa_raster_zero_bytes(quad, sizeof(*quad));
    quad->axis = (uint8_t)axis;
    quad->sign = (int8_t)sign;
    quad->block = (uint8_t)block;
    quad->u_length = (uint8_t)u_length;
    quad->v_length = (uint8_t)v_length;
    if (axis == 0) {
        quad->x = (uint8_t)(slice + (sign > 0));
        quad->y = (uint8_t)v;
        quad->z = (uint8_t)u;
    } else if (axis == 1) {
        quad->x = (uint8_t)u;
        quad->y = (uint8_t)(slice + (sign > 0));
        quad->z = (uint8_t)v;
    } else {
        quad->x = (uint8_t)u;
        quad->y = (uint8_t)v;
        quad->z = (uint8_t)(slice + (sign > 0));
    }
}

static void append_mesh_rect(chunk_mesh_t *mesh, int axis, int sign,
                             int slice, int u, int v, int u_length,
                             int v_length, int block) {
    int v_offset;
    /* Bound UV ranges and clipping expansion while retaining most of greedy
     * meshing's command-count reduction. */
    for (v_offset = 0; v_offset < v_length; v_offset += VOXEL_MESH_MAX_SPAN) {
        int u_offset;
        const int rect_height = v_length - v_offset > VOXEL_MESH_MAX_SPAN
                                    ? VOXEL_MESH_MAX_SPAN
                                    : v_length - v_offset;
        for (u_offset = 0; u_offset < u_length;
             u_offset += VOXEL_MESH_MAX_SPAN) {
            const int rect_width = u_length - u_offset > VOXEL_MESH_MAX_SPAN
                                       ? VOXEL_MESH_MAX_SPAN
                                       : u_length - u_offset;
            append_mesh_quad(mesh, axis, sign, slice, u + u_offset,
                             v + v_offset, rect_width, rect_height, block);
        }
    }
}

static void build_axis_face_slices(chunk_mesh_t *mesh, const chunk_t *chunk,
                                   int axis, int sign, int first_slice,
                                   int last_slice) {
    uint8_t mask[CHUNK_SIZE * CHUNK_HEIGHT];
    const int axis_length = axis == 1 ? CHUNK_HEIGHT : CHUNK_SIZE;
    const int u_length = CHUNK_SIZE;
    const int v_length = axis == 1 ? CHUNK_SIZE : CHUNK_HEIGHT;
    int slice;
    if (last_slice > axis_length) last_slice = axis_length;
    for (slice = first_slice; slice < last_slice; ++slice) {
        int v;
        pxa_raster_zero_bytes(mask, sizeof(mask));
        for (v = 0; v < v_length; ++v) {
            int u;
            for (u = 0; u < u_length; ++u) {
                int x = axis == 0 ? slice : u;
                int y = axis == 1 ? slice : v;
                int z = axis == 2 ? slice : (axis == 1 ? v : u);
                const int block = local_block(chunk, x, y, z);
                if (axis == 0) x += sign;
                if (axis == 1) y += sign;
                if (axis == 2) z += sign;
                if (block_face_visible(
                        block, neighbor_block(chunk, x, y, z)))
                    mask[v * u_length + u] = (uint8_t)block;
            }
        }
        for (v = 0; v < v_length; ++v) {
            int u = 0;
            while (u < u_length) {
                const uint8_t block = mask[v * u_length + u];
                int width = 1;
                int height = 1;
                int row;
                int column;
                if (block == 0) {
                    ++u;
                    continue;
                }
                while (u + width < u_length &&
                       mask[v * u_length + u + width] == block)
                    ++width;
                while (v + height < v_length) {
                    for (column = 0; column < width; ++column) {
                        if (mask[(v + height) * u_length + u + column] != block)
                            break;
                    }
                    if (column != width) break;
                    ++height;
                }
                append_mesh_rect(mesh, axis, sign, slice, u, v, width,
                                 height, block);
                for (row = 0; row < height; ++row)
                    for (column = 0; column < width; ++column)
                        mask[(v + row) * u_length + u + column] = 0;
                u += width;
            }
        }
    }
}

static void begin_mesh_rebuild(chunk_mesh_t *mesh, const chunk_t *chunk) {
    if (g_building_mesh != NULL && g_building_mesh != mesh)
        g_building_mesh->building = 0;
    g_building_mesh = mesh;
    mesh->build_chunk = chunk;
    mesh->build_revision = chunk->revision;
    mesh->build_chunk_cx = chunk->cx;
    mesh->build_chunk_cz = chunk->cz;
    mesh->build_quad_count = 0;
    mesh->build_dropped = 0;
    mesh->build_pass = 0;
    mesh->build_slice = 0;
    mesh->building = 1;
}

static int advance_mesh_rebuild(chunk_mesh_t *mesh, const chunk_t *chunk) {
    int axis;
    int axis_length;
    int sign;
    if (g_building_mesh != mesh || !mesh->building ||
        mesh->build_chunk != chunk ||
        mesh->build_revision != chunk->revision ||
        mesh->build_chunk_cx != chunk->cx ||
        mesh->build_chunk_cz != chunk->cz) {
        begin_mesh_rebuild(mesh, chunk);
    }
    axis = mesh->build_pass >> 1;
    sign = (mesh->build_pass & 1u) != 0 ? 1 : -1;
    axis_length = axis == 1 ? CHUNK_HEIGHT : CHUNK_SIZE;
    build_axis_face_slices(
        mesh, chunk, axis, sign, mesh->build_slice,
        mesh->build_slice + VOXEL_MESH_SLICES_PER_STEP);
    mesh->build_slice = (uint8_t)(
        mesh->build_slice + VOXEL_MESH_SLICES_PER_STEP);
    if (mesh->build_slice < axis_length) return 0;
    mesh->build_slice = 0;
    if (++mesh->build_pass < 6u) return 0;
    mesh->chunk = mesh->build_chunk;
    mesh->revision = mesh->build_revision;
    mesh->chunk_cx = mesh->build_chunk_cx;
    mesh->chunk_cz = mesh->build_chunk_cz;
    mesh->quad_count = mesh->build_quad_count;
    mesh->dropped = mesh->build_dropped;
    {
        uint16_t group_count[6] = {0};
        uint16_t group_start[6];
        uint16_t output = 0;
        uint16_t max_group = 0;
        uint16_t index;
        for (index = 0; index < mesh->quad_count; ++index) {
            const mesh_quad_t *quad = &g_mesh_build_quads[index];
            const uint8_t group =
                (uint8_t)(quad->axis * 2u + (quad->sign > 0));
            ++group_count[group];
        }
        group_start[0] = 0;
        for (uint8_t group = 0; group < 6u; ++group) {
            if (group != 0)
                group_start[group] =
                    (uint16_t)(group_start[group - 1u] +
                               group_count[group - 1u]);
            if (group_count[group] > max_group)
                max_group = group_count[group];
        }
        /* Mesh construction emits six contiguous face-direction groups.
         * Interleave them so a per-chunk candidate quota cannot accidentally
         * retain only one side of a distant chunk. */
        for (uint16_t row = 0; row < max_group; ++row) {
            for (uint8_t group = 0; group < 6u; ++group) {
                if (row < group_count[group])
                    mesh->quads[output++] =
                        g_mesh_build_quads[group_start[group] + row];
            }
        }
    }
    mesh->building = 0;
    g_building_mesh = NULL;
    return 1;
}

static void rebuild_mesh(chunk_mesh_t *mesh, const chunk_t *chunk) {
    begin_mesh_rebuild(mesh, chunk);
    while (!advance_mesh_rebuild(mesh, chunk)) {
    }
}

static chunk_mesh_t *mesh_for_chunk(const chunk_t *chunk,
                                    uint8_t preferred_index) {
    chunk_mesh_t *empty = NULL;
    uint8_t index;
    for (index = 0; index < GRID_COUNT; ++index) {
        if (g_meshes[index].chunk == chunk ||
            (g_meshes[index].building &&
             g_meshes[index].build_chunk == chunk))
            return &g_meshes[index];
        if (empty == NULL && g_meshes[index].chunk == NULL)
            empty = &g_meshes[index];
    }
    /* The pool and cache have the same size, so this is only reachable for
     * synthetic callers that replace every resident chunk pointer at once. */
    return empty != NULL ? empty : &g_meshes[preferred_index];
}

void voxel_raster_reset(void) {
    pxa_raster_zero_bytes(g_meshes, sizeof(g_meshes));
    pxa_raster_zero_bytes(&g_stats, sizeof(g_stats));
    g_building_mesh = NULL;
    g_mesh_cache_warmed = 0;
}

static void build_camera(raster_camera_t *camera, const player_t *player,
                         uint8_t quality) {
    const float cy = rc_cos(player->yaw);
    const float sy = rc_sin(player->yaw);
    const float cp = rc_cos(player->pitch);
    const float sp = rc_sin(player->pitch);
    camera->fx = sy * cp;
    camera->fy = sp;
    camera->fz = cy * cp;
    camera->rx = cy;
    camera->rz = -sy;
    camera->ux = -sy * sp;
    camera->uy = cp;
    camera->uz = -cy * sp;
    camera->x = player->x;
    camera->y = player->y + EYE_HEIGHT;
    camera->z = player->z;
    camera->width = (uint16_t)render_scene_width();
    camera->height = (uint16_t)render_scene_height();
    camera->tan_y = VOXEL_RASTER_TAN_HALF;
    camera->tan_x = VOXEL_RASTER_TAN_HALF *
                    (float)camera->width / camera->height;
    camera->half_width = (float)camera->width * 0.5F;
    camera->half_height = (float)camera->height * 0.5F;
    camera->scale_x = camera->half_width / camera->tan_x;
    camera->scale_y = camera->half_height / camera->tan_y;
    /* The view distance is the main geometry lever: every extra chunk in range
     * costs projection work and Host fill time. A perspective-correct Host
     * spends no candidates on subdivision, so full detail can afford the
     * horizon the fog ramp was written for instead of ending the world two
     * chunks out. */
    camera->fog_end = quality >= QUALITY_PERFORMANCE
                          ? VOXEL_RASTER_PERFORMANCE_FOG
                      : quality >= QUALITY_BALANCED
                          ? (g_painter_perspective ? g_view_distance * 0.75F
                                                   : 15.0F)
                          : (g_painter_perspective ? g_view_distance : 16.0F);
    /* Submerged: pull the fog in so terrain fades like murky water. */
    if (game_block(rc_floor_int(player->x),
                   rc_floor_int(player->y + EYE_HEIGHT),
                   rc_floor_int(player->z)) == BLOCK_WATER)
        camera->fog_end = 14.0F;
    /* The ramp covers the outer 55% of the view whatever the horizon is, so
     * a horizon the controller pulled in still fades instead of cutting the
     * world off at a visible edge. */
    camera->fog_start = camera->fog_end * 0.45F;
}

static int chunk_visible(const raster_camera_t *camera, const chunk_t *chunk) {
    const float center_x = (float)chunk->cx * CHUNK_SIZE + CHUNK_SIZE * 0.5F;
    const float center_y = CHUNK_HEIGHT * 0.5F;
    const float center_z = (float)chunk->cz * CHUNK_SIZE + CHUNK_SIZE * 0.5F;
    const float dx = center_x - camera->x;
    const float dy = center_y - camera->y;
    const float dz = center_z - camera->z;
    const float depth = dx * camera->fx + dy * camera->fy + dz * camera->fz;
    const float right = dx * camera->rx + dz * camera->rz;
    const float up = dx * camera->ux + dy * camera->uy + dz * camera->uz;
    const float radius = 17.0F;
    return depth + radius > VOXEL_RASTER_NEAR &&
           depth - radius < camera->fog_end &&
           rc_fabs(right) <= depth * camera->tan_x + radius &&
           rc_fabs(up) <= depth * camera->tan_y + radius;
}

static float chunk_depth(const raster_camera_t *camera, const chunk_t *chunk) {
    const float center_x = (float)chunk->cx * CHUNK_SIZE + CHUNK_SIZE * 0.5F;
    const float center_y = CHUNK_HEIGHT * 0.5F;
    const float center_z = (float)chunk->cz * CHUNK_SIZE + CHUNK_SIZE * 0.5F;
    return (center_x - camera->x) * camera->fx +
           (center_y - camera->y) * camera->fy +
           (center_z - camera->z) * camera->fz;
}

static void sort_visible_chunks(visible_chunk_t *chunks, uint8_t count) {
    uint8_t index;
    /* Near chunks provide the first-person silhouettes. Prioritising them
     * before the draw-list budget is consumed avoids traversal-order holes. */
    for (index = 1; index < count; ++index) {
        visible_chunk_t value = chunks[index];
        uint8_t cursor = index;
        while (cursor != 0 && chunks[cursor - 1u].depth > value.depth) {
            chunks[cursor] = chunks[cursor - 1u];
            --cursor;
        }
        chunks[cursor] = value;
    }
}

static uint8_t project_quad(const raster_camera_t *camera,
                            const chunk_t *chunk, const mesh_quad_t *quad,
                            projected_quad_t *output, uint16_t capacity,
                            uint8_t adaptive_painter);

static void append_chunk_candidates(const raster_camera_t *camera,
                                    const visible_chunk_t *visible,
                                    uint16_t *quad_cursor,
                                    uint32_t stop_count,
                                    uint32_t mesh_limit,
                                    uint8_t adaptive_painter,
                                    uint32_t *candidate_count) {
    const float camera_x = camera->x -
        (float)visible->chunk->cx * CHUNK_SIZE;
    const float camera_z = camera->z -
        (float)visible->chunk->cz * CHUNK_SIZE;
    while (*quad_cursor < visible->mesh->quad_count &&
           *candidate_count < stop_count && *candidate_count < mesh_limit) {
        /* The per-chunk quota decides whether to begin another source quad.
         * Once begun, let clipping/splitting keep the whole face as long as
         * the global terrain budget has room. Otherwise a quota boundary can
         * leave a clipped polygon with one of its triangles missing. */
        const uint32_t remaining = mesh_limit - *candidate_count;
        ++g_stats.mesh_visited_quads;
        {
            const mesh_quad_t *quad =
                &visible->mesh->quads[(*quad_cursor)++];
            const uint8_t face_bit = (uint8_t)(1u <<
                (quad->axis * 2u + (quad->sign > 0)));
            if ((visible->backface_mask & face_bit) != 0 &&
                !(quad->axis == 1 && quad->block == BLOCK_WATER)) {
                ++g_stats.backface_culled;
                continue;
            }
            {
                const float base_depth =
                    ((float)quad->x - camera_x) * camera->fx +
                    ((float)quad->y - camera->y) * camera->fy +
                    ((float)quad->z - camera_z) * camera->fz;
                const float u_depth = quad->u_length *
                    (quad->axis == 0 ? camera->fz : camera->fx);
                const float v_depth = quad->v_length *
                    (quad->axis == 1 ? camera->fz : camera->fy);
                const float nearest = base_depth +
                    (u_depth < 0.0F ? u_depth : 0.0F) +
                    (v_depth < 0.0F ? v_depth : 0.0F);
                const float farthest = base_depth +
                    (u_depth > 0.0F ? u_depth : 0.0F) +
                    (v_depth > 0.0F ? v_depth : 0.0F);
                if (nearest > camera->fog_end + 0.25F ||
                    farthest < VOXEL_RASTER_NEAR - 0.25F) {
                    ++g_stats.frustum_culled;
                    continue;
                }
            }
            *candidate_count += project_quad(
                camera, visible->chunk, quad,
                &g_candidates[*candidate_count], (uint16_t)remaining,
                adaptive_painter);
        }
    }
}

typedef struct {
    float right;
    float up;
    float depth;
    float u_q4;
    float v_q4;
} clip_vertex_t;

enum {
    CLIP_NEAR = 0,
    CLIP_FAR,
    CLIP_LEFT,
    CLIP_RIGHT,
    CLIP_BOTTOM,
    CLIP_TOP,
    CLIP_PLANE_COUNT,
};

#define VOXEL_CLIP_VERTICES 12u

static float clip_distance(const raster_camera_t *camera,
                           const clip_vertex_t *vertex, uint8_t plane) {
    if (plane == CLIP_NEAR) return vertex->depth - VOXEL_RASTER_NEAR;
    if (plane == CLIP_FAR) return camera->fog_end - vertex->depth;
    if (plane == CLIP_LEFT)
        return vertex->right + vertex->depth * camera->tan_x;
    if (plane == CLIP_RIGHT)
        return vertex->depth * camera->tan_x - vertex->right;
    if (plane == CLIP_BOTTOM)
        return vertex->up + vertex->depth * camera->tan_y;
    return vertex->depth * camera->tan_y - vertex->up;
}

static uint8_t clip_outcode(const raster_camera_t *camera,
                            const clip_vertex_t *vertex) {
    const float horizontal = vertex->depth * camera->tan_x;
    const float vertical = vertex->depth * camera->tan_y;
    uint8_t code = 0;
    if (vertex->depth < VOXEL_RASTER_NEAR) code |= 1u << CLIP_NEAR;
    if (vertex->depth > camera->fog_end) code |= 1u << CLIP_FAR;
    if (vertex->right < -horizontal) code |= 1u << CLIP_LEFT;
    if (vertex->right > horizontal) code |= 1u << CLIP_RIGHT;
    if (vertex->up < -vertical) code |= 1u << CLIP_BOTTOM;
    if (vertex->up > vertical) code |= 1u << CLIP_TOP;
    return code;
}

static clip_vertex_t clip_intersection(const clip_vertex_t *a,
                                       const clip_vertex_t *b, float a_distance,
                                       float b_distance) {
    clip_vertex_t result;
    const float denominator = a_distance - b_distance;
    const float t = denominator == 0.0F ? 0.0F : a_distance / denominator;
    result.right = a->right + (b->right - a->right) * t;
    result.up = a->up + (b->up - a->up) * t;
    result.depth = a->depth + (b->depth - a->depth) * t;
    result.u_q4 = a->u_q4 + (b->u_q4 - a->u_q4) * t;
    result.v_q4 = a->v_q4 + (b->v_q4 - a->v_q4) * t;
    return result;
}

static uint8_t clip_polygon(const raster_camera_t *camera,
                            const clip_vertex_t *input, uint8_t input_count,
                            clip_vertex_t *output, uint8_t plane) {
    const clip_vertex_t *previous;
    float previous_distance;
    uint8_t output_count = 0;
    uint8_t index;
    if (input_count == 0) return 0;
    previous = &input[input_count - 1u];
    previous_distance = clip_distance(camera, previous, plane);
    for (index = 0; index < input_count; ++index) {
        const clip_vertex_t *current = &input[index];
        const float current_distance = clip_distance(camera, current, plane);
        const uint8_t previous_inside = previous_distance >= 0.0F;
        const uint8_t current_inside = current_distance >= 0.0F;
        if (previous_inside != current_inside &&
            output_count < VOXEL_CLIP_VERTICES) {
            output[output_count++] = clip_intersection(
                previous, current, previous_distance, current_distance);
        }
        if (current_inside && output_count < VOXEL_CLIP_VERTICES)
            output[output_count++] = *current;
        previous = current;
        previous_distance = current_distance;
    }
    return output_count;
}

static void prepare_projected_order(projected_quad_t *quad) {
    const pxa_raster_vertex_t *v0 = &quad->vertices[0];
    uint8_t index;
    quad->inverse_ready = 0;
    quad->min_x_q4 = quad->max_x_q4 = v0->x_q4;
    quad->min_y_q4 = quad->max_y_q4 = v0->y_q4;
    for (index = 1; index < 4; ++index) {
        const pxa_raster_vertex_t *vertex = &quad->vertices[index];
        if (vertex->x_q4 < quad->min_x_q4)
            quad->min_x_q4 = vertex->x_q4;
        if (vertex->x_q4 > quad->max_x_q4)
            quad->max_x_q4 = vertex->x_q4;
        if (vertex->y_q4 < quad->min_y_q4)
            quad->min_y_q4 = vertex->y_q4;
        if (vertex->y_q4 > quad->max_y_q4)
            quad->max_y_q4 = vertex->y_q4;
    }
}

static void prepare_projected_inverse(projected_quad_t *quad) {
    const pxa_raster_vertex_t *v0 = &quad->vertices[0];
    const pxa_raster_vertex_t *v1 = &quad->vertices[1];
    const pxa_raster_vertex_t *v2 = &quad->vertices[2];
    float x10;
    float y10;
    float x20;
    float y20;
    float denominator;
    float q0;
    float q1;
    float q2;
    if (quad->inverse_ready) return;
    quad->inverse_ready = 1;
    x10 = (float)(v1->x_q4 - v0->x_q4);
    y10 = (float)(v1->y_q4 - v0->y_q4);
    x20 = (float)(v2->x_q4 - v0->x_q4);
    y20 = (float)(v2->y_q4 - v0->y_q4);
    denominator = x10 * y20 - x20 * y10;
    q0 = v0->depth_q8 != 0 ? 1.0F / v0->depth_q8 : 0.0F;
    q1 = v1->depth_q8 != 0 ? 1.0F / v1->depth_q8 : q0;
    q2 = v2->depth_q8 != 0 ? 1.0F / v2->depth_q8 : q0;
    if (denominator == 0.0F) {
        quad->inverse_a = 0.0F;
        quad->inverse_b = 0.0F;
        quad->inverse_c = q0;
        return;
    }
    quad->inverse_a =
        ((q1 - q0) * y20 - (q2 - q0) * y10) / denominator;
    quad->inverse_b =
        (x10 * (q2 - q0) - x20 * (q1 - q0)) / denominator;
    quad->inverse_c = q0 - quad->inverse_a * v0->x_q4 -
                      quad->inverse_b * v0->y_q4;
}

static void finish_projected_primitive(
    const raster_camera_t *camera, const mesh_quad_t *quad,
    const pxa_raster_vertex_t *vertices, const uint8_t indices[4],
    uint8_t unique_vertices, projected_quad_t *projected) {
    float depth = 0.0F;
    uint16_t min_depth = UINT16_MAX;
    uint16_t max_depth = 0;
    int32_t min_x = INT32_MAX;
    int32_t max_x = INT32_MIN;
    int32_t min_y = INT32_MAX;
    int32_t max_y = INT32_MIN;
    uint8_t light;
    uint8_t index;
    for (index = 0; index < unique_vertices; ++index) {
        const uint16_t vertex_depth = vertices[indices[index]].depth_q8;
        depth += (float)vertex_depth / 256.0F;
        if (vertex_depth < min_depth) min_depth = vertex_depth;
        if (vertex_depth > max_depth) max_depth = vertex_depth;
    }
    depth /= unique_vertices;
    for (index = 0; index < 4; ++index) {
        const pxa_raster_vertex_t *vertex = &vertices[indices[index]];
        if (vertex->x_q4 < min_x) min_x = vertex->x_q4;
        if (vertex->x_q4 > max_x) max_x = vertex->x_q4;
        if (vertex->y_q4 < min_y) min_y = vertex->y_q4;
        if (vertex->y_q4 > max_y) max_y = vertex->y_q4;
    }
    light = quad->axis == 1 ? (quad->sign > 0 ? 255u : 116u)
                            : (quad->axis == 0 ? 190u : 160u);
    if (depth > camera->fog_start) {
        float fog = (depth - camera->fog_start) /
                    (camera->fog_end - camera->fog_start);
        if (fog > 1.0F) fog = 1.0F;
        light = (uint8_t)((float)light * (1.0F - fog * 0.15F));
    }
    for (index = 0; index < 4; ++index) {
        projected->vertices[index] = vertices[indices[index]];
        projected->vertices[index].light = light;
        if (quad->block == BLOCK_WATER) {
            projected->vertices[index].u_q4 = (int16_t)(
                projected->vertices[index].u_q4 + g_water_uv_offset_q4);
            projected->vertices[index].v_q4 = (int16_t)(
                projected->vertices[index].v_q4 +
                (g_water_uv_offset_q4 >> 1));
        }
    }
    projected->nearest_depth_q8 = min_depth;
    projected->farthest_depth_q8 = max_depth;
    projected->sort_depth_q8 =
        quad->axis == 1 &&
        (g_raster_capabilities & PXA_RASTER_CAP_COVERAGE_MASK) == 0
            ? max_depth : min_depth;
    projected->textured = quad->block <= VOXEL_RASTER_TEXTURED_BLOCKS;
    projected->transparent_index0 = block_uses_cutout(quad->block);
    projected->blend_75 = block_is_translucent(quad->block);
#ifndef VOXEL_OPAQUE_TEST
#define VOXEL_OPAQUE_TEST 0
#endif
#if VOXEL_OPAQUE_TEST
    /* Measurement build: render cut-out and blended faces as opaque so the
     * Host raster cost of the slow painter branches can be compared. */
    projected->transparent_index0 = 0;
    projected->blend_75 = 0;
#endif
    projected->color = render_block_color(quad->block);
    projected->affine = 0;
    prepare_projected_order(projected);
    if (projected->textured) {
        const uint8_t kind =
            quad->axis == 1
                ? (quad->sign > 0 ? BLOCK_TEXTURE_TOP : BLOCK_TEXTURE_BOTTOM)
                : BLOCK_TEXTURE_SIDE;
        projected->texture_slot = texture_slot_for(
            (uint8_t)quad->block, kind);
        /* Faces below roughly 4x4 screen pixels skip the texture and use the
         * block colour; the texel detail is invisible at that size. */
        if ((g_raster_capabilities & PXA_RASTER_CAP_COVERAGE_MASK) == 0 &&
            !projected->transparent_index0 && !projected->blend_75 &&
            (max_x - min_x < 4 * 16 || max_y - min_y < 4 * 16)) {
            projected->textured = 0;
            return;
        }
        /* Affine UV is only worth its warping when the depth range across the
         * face stays small relative to its merged texel span. The bound keeps
         * the worst-case affine error near half a texel. */
        {
            const uint8_t span = quad->u_length > quad->v_length
                                     ? quad->u_length
                                     : quad->v_length;
            if (min_depth != 0 &&
                (uint32_t)(max_depth - min_depth) * 8u * span <= min_depth)
                projected->affine = 1;
        }
    }
}

static uint8_t project_quad_raw(const raster_camera_t *camera,
                                const chunk_t *chunk,
                                const mesh_quad_t *quad,
                                projected_quad_t *output, uint16_t capacity,
                                uint8_t *clipped_out) {
    clip_vertex_t clip_a[VOXEL_CLIP_VERTICES];
    clip_vertex_t clip_b[VOXEL_CLIP_VERTICES];
    clip_vertex_t *input = clip_a;
    clip_vertex_t *clipped = clip_b;
    pxa_raster_vertex_t vertices[VOXEL_CLIP_VERTICES];
    float face_position;
    float normal_dot;
    uint8_t count = 4;
    uint8_t primitive_count;
    uint8_t was_clipped = 0;
    uint8_t outside_any = 0;
    uint8_t outside_all = (uint8_t)((1u << CLIP_PLANE_COUNT) - 1u);
    uint8_t plane;
    uint8_t index;
    if (clipped_out != NULL) *clipped_out = 0;
    if (capacity == 0) return 0;
    face_position = quad->axis == 0
                        ? (float)chunk->cx * CHUNK_SIZE + quad->x
                    : quad->axis == 1
                        ? quad->y
                        : (float)chunk->cz * CHUNK_SIZE + quad->z;
    normal_dot = quad->sign *
        (quad->axis == 0 ? camera->x - face_position
         : quad->axis == 1 ? camera->y - face_position
                           : camera->z - face_position);
    /* Water surface caps are two-sided so the lake surface is visible from
     * below. Vertical water walls stay culled: from inside a lake they would
     * otherwise turn into a flat blue wall at the far shore. */
    if (normal_dot <= 0.0F &&
        !(quad->block == BLOCK_WATER && quad->axis == 1)) {
        ++g_stats.backface_culled;
        return 0;
    }
    {
        const float dx = (float)chunk->cx * CHUNK_SIZE + quad->x - camera->x;
        const float dy = (float)quad->y - camera->y;
        const float dz = (float)chunk->cz * CHUNK_SIZE + quad->z - camera->z;
        const float du = (float)quad->u_length;
        const float dv = (float)quad->v_length;
        float one_right, one_up, one_depth;
        float three_right, three_up, three_depth;
        input[0].right = dx * camera->rx + dz * camera->rz;
        input[0].up = dx * camera->ux + dy * camera->uy +
                      dz * camera->uz;
        input[0].depth = dx * camera->fx + dy * camera->fy +
                         dz * camera->fz;
        if (quad->axis == 0) {
            one_right = 0.0F;
            one_up = dv * camera->uy;
            one_depth = dv * camera->fy;
            three_right = du * camera->rz;
            three_up = du * camera->uz;
            three_depth = du * camera->fz;
        } else if (quad->axis == 1) {
            one_right = du * camera->rx;
            one_up = du * camera->ux;
            one_depth = du * camera->fx;
            three_right = dv * camera->rz;
            three_up = dv * camera->uz;
            three_depth = dv * camera->fz;
        } else {
            one_right = 0.0F;
            one_up = dv * camera->uy;
            one_depth = dv * camera->fy;
            three_right = du * camera->rx;
            three_up = du * camera->ux;
            three_depth = du * camera->fx;
        }
        {
            const float radius = (du + dv) * 0.5F;
            const float center_depth = input[0].depth +
                (one_depth + three_depth) * 0.5F;
            const float center_right = input[0].right +
                (one_right + three_right) * 0.5F;
            const float center_up = input[0].up +
                (one_up + three_up) * 0.5F;
            const float horizontal =
                (center_depth + radius) * camera->tan_x + radius;
            const float vertical =
                (center_depth + radius) * camera->tan_y + radius;
            if (center_depth + radius < VOXEL_RASTER_NEAR ||
                center_depth - radius > camera->fog_end ||
                center_right < -horizontal || center_right > horizontal ||
                center_up < -vertical || center_up > vertical) {
                ++g_stats.frustum_culled;
                return 0;
            }
        }
        input[1].right = input[0].right + one_right;
        input[1].up = input[0].up + one_up;
        input[1].depth = input[0].depth + one_depth;
        input[3].right = input[0].right + three_right;
        input[3].up = input[0].up + three_up;
        input[3].depth = input[0].depth + three_depth;
        input[2].right = input[1].right + three_right;
        input[2].up = input[1].up + three_up;
        input[2].depth = input[1].depth + three_depth;
        for (index = 0; index < 4; ++index) {
            const uint8_t outcode = clip_outcode(camera, &input[index]);
            outside_any |= outcode;
            outside_all &= outcode;
        }
    }
    if (outside_all != 0) {
        ++g_stats.frustum_culled;
        return 0;
    }
    {
        /* Texture V grows downwards inside a tile, so V is flipped against
         * the world axis the face runs along: the highest world corner
         * samples row 0. This matches the ray caster's
         * `ty = 15 - frac(v)` and keeps grass fringes and table edges up.
         * Extended slots give every face kind its own 16x16 texture, so V
         * never leaves the tile when a face spans several blocks. */
        if (quad->axis == 0) {
            input[0].u_q4 = input[1].u_q4 = 0.0F;
            input[2].u_q4 = input[3].u_q4 = quad->u_length * 256.0F;
            input[0].v_q4 = input[3].v_q4 = quad->v_length * 256.0F;
            input[1].v_q4 = input[2].v_q4 = 0.0F;
        } else if (quad->axis == 1) {
            input[0].u_q4 = input[3].u_q4 = 0.0F;
            input[1].u_q4 = input[2].u_q4 = quad->u_length * 256.0F;
            input[0].v_q4 = input[1].v_q4 = quad->v_length * 256.0F;
            input[2].v_q4 = input[3].v_q4 = 0.0F;
        } else {
            input[0].u_q4 = input[3].u_q4 = 0.0F;
            input[1].u_q4 = input[2].u_q4 = quad->u_length * 256.0F;
            input[0].v_q4 = input[1].v_q4 = quad->v_length * 256.0F;
            input[2].v_q4 = input[3].v_q4 = 0.0F;
        }
    }
    for (plane = 0; outside_any != 0 && plane < CLIP_PLANE_COUNT; ++plane) {
        uint8_t needs_clip = 0;
        clip_vertex_t *swap;
        if ((outside_any & (1u << plane)) == 0) continue;
        for (index = 0; index < count; ++index) {
            if (clip_distance(camera, &input[index], plane) < 0.0F) {
                needs_clip = 1;
                break;
            }
        }
        if (!needs_clip) continue;
        was_clipped = 1;
        count = clip_polygon(camera, input, count, clipped, plane);
        if (count < 3) break;
        swap = input;
        input = clipped;
        clipped = swap;
    }
    if (count < 3) {
        ++g_stats.frustum_culled;
        return 0;
    }
    if (was_clipped) {
        ++g_stats.clipped_quads;
        if (clipped_out != NULL) *clipped_out = 1;
    }
    for (index = 0; index < count; ++index) {
        /* One reciprocal per vertex: the half-extent over the tangent is
         * constant for the frame, so the two divisions this loop used to run
         * per vertex collapse into multiplies. */
        const float inverse_depth = 1.0F / input[index].depth;
        float screen_x = camera->half_width +
            input[index].right * inverse_depth * camera->scale_x;
        float screen_y = camera->half_height -
            input[index].up * inverse_depth * camera->scale_y;
        if (screen_x < 0.0F) screen_x = 0.0F;
        if (screen_x > camera->width) screen_x = camera->width;
        if (screen_y < 0.0F) screen_y = 0.0F;
        if (screen_y > camera->height) screen_y = camera->height;
        vertices[index].x_q4 = (int16_t)(screen_x * 16.0F + 0.5F);
        vertices[index].y_q4 = (int16_t)(screen_y * 16.0F + 0.5F);
        vertices[index].u_q4 = (int16_t)(input[index].u_q4 + 0.5F);
        vertices[index].v_q4 = (int16_t)(input[index].v_q4 + 0.5F);
        vertices[index].depth_q8 =
            (uint16_t)(input[index].depth * 256.0F + 0.5F);
    }
    primitive_count = count <= 4 ? 1u : (uint8_t)(count - 2u);
    if (primitive_count > capacity) {
        g_stats.dropped_quads += primitive_count - capacity;
        g_stats.projection_budget_dropped += primitive_count - capacity;
        primitive_count = capacity;
    }
    if (count <= 4) {
        uint8_t indices[4] = {0, 1, 2, 2};
        if (count == 4) indices[3] = 3;
        finish_projected_primitive(camera, quad, vertices, indices, count,
                                   &output[0]);
    } else {
        for (index = 0; index < primitive_count; ++index) {
            const uint8_t indices[4] = {
                0, (uint8_t)(index + 1u), (uint8_t)(index + 2u),
                (uint8_t)(index + 2u)};
            finish_projected_primitive(camera, quad, vertices, indices, 3,
                                       &output[index]);
        }
    }
    return primitive_count;
}

static uint32_t painter_split_depth_q8(void) {
    return g_painter_perspective
               ? VOXEL_RASTER_PAINTER_SORT_SPLIT_DEPTH_Q8
               : VOXEL_RASTER_PAINTER_SPLIT_DEPTH_Q8;
}

static int projected_quad_needs_split(const projected_quad_t *quad) {
    int32_t min_x = INT32_MAX;
    int32_t max_x = INT32_MIN;
    int32_t min_y = INT32_MAX;
    int32_t max_y = INT32_MIN;
    uint8_t index;
#if VOXEL_RASTER_HYBRID_DEPTH_Q8 != 0
    if (quad->nearest_depth_q8 < VOXEL_RASTER_HYBRID_DEPTH_Q8)
        return 0;
#endif
    /* Splitting exists to bound the visible affine-UV error. Beyond this
     * distance the face is already small on screen, while splitting it can
     * consume most of the candidate budget and omit complete far chunks. */
    if (quad->nearest_depth_q8 >= VOXEL_RASTER_PAINTER_SPLIT_FAR_Q8)
        return 0;
    if ((uint32_t)quad->farthest_depth_q8 - quad->nearest_depth_q8 <=
            painter_split_depth_q8())
        return 0;
    for (index = 0; index < 4; ++index) {
        const pxa_raster_vertex_t *vertex = &quad->vertices[index];
        if (vertex->x_q4 < min_x) min_x = vertex->x_q4;
        if (vertex->x_q4 > max_x) max_x = vertex->x_q4;
        if (vertex->y_q4 < min_y) min_y = vertex->y_q4;
        if (vertex->y_q4 > max_y) max_y = vertex->y_q4;
    }
    return max_x - min_x >= VOXEL_RASTER_PAINTER_SPLIT_SCREEN_Q4 ||
           max_y - min_y >= VOXEL_RASTER_PAINTER_SPLIT_SCREEN_Q4;
}

static void painter_depth_spans(const raster_camera_t *camera,
                                const mesh_quad_t *quad, float *u_span,
                                float *v_span) {
    float u_forward;
    float v_forward;
    if (quad->axis == 0) {
        u_forward = camera->fz;
        v_forward = camera->fy;
    } else if (quad->axis == 1) {
        u_forward = camera->fx;
        v_forward = camera->fz;
    } else {
        u_forward = camera->fx;
        v_forward = camera->fy;
    }
    *u_span = rc_fabs(u_forward) * quad->u_length;
    *v_span = rc_fabs(v_forward) * quad->v_length;
}

static void painter_split_counts(const raster_camera_t *camera,
                                 const mesh_quad_t *quad,
                                 uint8_t max_parts, uint8_t *u_parts,
                                 uint8_t *v_parts) {
    float u_span;
    float v_span;
    painter_depth_spans(camera, quad, &u_span, &v_span);
    *u_parts = 1;
    *v_parts = 1;
    while ((uint8_t)(*u_parts * *v_parts) < max_parts &&
           u_span * *v_parts + v_span * *u_parts >
               ((float)painter_split_depth_q8() / 256.0F) *
                   *u_parts * *v_parts) {
        const uint8_t can_split_u =
            *u_parts < quad->u_length &&
            (uint8_t)((*u_parts + 1u) * *v_parts) <= max_parts;
        const uint8_t can_split_v =
            *v_parts < quad->v_length &&
            (uint8_t)(*u_parts * (*v_parts + 1u)) <= max_parts;
        if (!can_split_u && !can_split_v) break;
        /* Split the axis with the larger remaining depth contribution. The
         * cross multiplication avoids soft floating-point division. */
        if (can_split_u &&
            (!can_split_v ||
             u_span * *v_parts >= v_span * *u_parts)) {
            ++*u_parts;
        } else {
            ++*v_parts;
        }
    }
}

static mesh_quad_t mesh_quad_part(const mesh_quad_t *quad, uint8_t u_part,
                                  uint8_t u_parts, uint8_t v_part,
                                  uint8_t v_parts) {
    mesh_quad_t part = *quad;
    const uint8_t u_start =
        (uint8_t)((uint16_t)quad->u_length * u_part / u_parts);
    const uint8_t u_end =
        (uint8_t)((uint16_t)quad->u_length * (u_part + 1u) / u_parts);
    const uint8_t v_start =
        (uint8_t)((uint16_t)quad->v_length * v_part / v_parts);
    const uint8_t v_end =
        (uint8_t)((uint16_t)quad->v_length * (v_part + 1u) / v_parts);
    part.u_length = (uint8_t)(u_end - u_start);
    part.v_length = (uint8_t)(v_end - v_start);
    if (quad->axis == 0) {
        part.z = (uint8_t)(part.z + u_start);
        part.y = (uint8_t)(part.y + v_start);
    } else if (quad->axis == 1) {
        part.x = (uint8_t)(part.x + u_start);
        part.z = (uint8_t)(part.z + v_start);
    } else {
        part.x = (uint8_t)(part.x + u_start);
        part.y = (uint8_t)(part.y + v_start);
    }
    return part;
}

static uint8_t project_quad(const raster_camera_t *camera,
                            const chunk_t *chunk, const mesh_quad_t *quad,
                            projected_quad_t *output, uint16_t capacity,
                            uint8_t adaptive_painter) {
    uint8_t clipped = 0;
    uint8_t count = project_quad_raw(camera, chunk, quad, output, capacity,
                                     &clipped);
    uint8_t u_parts;
    uint8_t v_parts;
    uint8_t total = 0;
    uint8_t v_part;
    if (!adaptive_painter || clipped || count != 1u || capacity < 2u ||
        !projected_quad_needs_split(&output[0]))
        return count;
    painter_split_counts(
        camera, quad,
        capacity < VOXEL_RASTER_PAINTER_SPLIT_MAX_PARTS
            ? (uint8_t)capacity
            : VOXEL_RASTER_PAINTER_SPLIT_MAX_PARTS,
        &u_parts, &v_parts);
    if (u_parts == 1u && v_parts == 1u) return count;
    for (v_part = 0; v_part < v_parts; ++v_part) {
        uint8_t u_part;
        for (u_part = 0; u_part < u_parts; ++u_part) {
            const mesh_quad_t part = mesh_quad_part(
                quad, u_part, u_parts, v_part, v_parts);
            const uint8_t first = total;
            const uint8_t added = project_quad_raw(
                camera, chunk, &part, &output[total],
                (uint16_t)(capacity - total), NULL);
            uint8_t index;
            total = (uint8_t)(total + added);
            for (index = first; index < total; ++index) {
                projected_quad_t *piece = &output[index];
                piece->sort_depth_q8 = (uint16_t)(
                    piece->nearest_depth_q8 +
                    (piece->farthest_depth_q8 - piece->nearest_depth_q8) / 2u);
            }
        }
    }
    if (total > 1u) g_stats.painter_splits += total - 1u;
    return total != 0u ? total : count;
}

static int project_billboard(const raster_camera_t *camera, float x, float y,
                             float z, float width, float height,
                             uint16_t color, projected_quad_t *projected) {
    const float half_width = width * 0.5F;
    float corners[4][3];
    float min_depth = 1.0e9F;
    float min_x = 1.0e9F;
    float min_y = 1.0e9F;
    float max_x = -1.0e9F;
    float max_y = -1.0e9F;
    uint8_t index;
    corners[0][0] = corners[1][0] = x - camera->rx * half_width;
    corners[0][2] = corners[1][2] = z - camera->rz * half_width;
    corners[2][0] = corners[3][0] = x + camera->rx * half_width;
    corners[2][2] = corners[3][2] = z + camera->rz * half_width;
    corners[0][1] = corners[3][1] = y;
    corners[1][1] = corners[2][1] = y + height;
    for (index = 0; index < 4; ++index) {
        const float dx = corners[index][0] - camera->x;
        const float dy = corners[index][1] - camera->y;
        const float dz = corners[index][2] - camera->z;
        const float depth = dx * camera->fx + dy * camera->fy +
                            dz * camera->fz;
        const float right = dx * camera->rx + dz * camera->rz;
        const float up = dx * camera->ux + dy * camera->uy +
                         dz * camera->uz;
        float screen_x;
        float screen_y;
        if (depth < VOXEL_RASTER_NEAR || depth > camera->fog_end) return 0;
        screen_x = camera->width * 0.5F +
                   right / depth * (camera->width * 0.5F) / camera->tan_x;
        screen_y = camera->height * 0.5F -
                   up / depth * (camera->height * 0.5F) / camera->tan_y;
        projected->vertices[index].x_q4 = (int16_t)(screen_x * 16.0F);
        projected->vertices[index].y_q4 = (int16_t)(screen_y * 16.0F);
        projected->vertices[index].u_q4 = 0;
        projected->vertices[index].v_q4 = 0;
        projected->vertices[index].light = 255;
        projected->vertices[index].depth_q8 =
            (uint16_t)(depth * 256.0F + 0.5F);
        if (depth < min_depth) min_depth = depth;
        if (screen_x < min_x) min_x = screen_x;
        if (screen_x > max_x) max_x = screen_x;
        if (screen_y < min_y) min_y = screen_y;
        if (screen_y > max_y) max_y = screen_y;
    }
    if (max_x < 0.0F || max_y < 0.0F || min_x >= camera->width ||
        min_y >= camera->height)
        return 0;
    projected->nearest_depth_q8 =
        (uint16_t)(min_depth * 256.0F + 0.5F);
    projected->farthest_depth_q8 = projected->nearest_depth_q8;
    /* Keep billboards at their nearest depth so a mob immediately behind a
     * wall cannot move ahead of it in painter order. */
    projected->sort_depth_q8 = projected->nearest_depth_q8;
    projected->color = color;
    projected->texture_slot = 0;
    projected->textured = 0;
    projected->affine = 0;
    projected->transparent_index0 = 0;
    projected->blend_75 = 0;
    prepare_projected_order(projected);
    return 1;
}

static void append_entities(const raster_camera_t *camera,
                            uint32_t candidate_limit,
                            uint32_t *candidate_count) {
    static const uint8_t mob_red[MOB_KIND_COUNT] = {104, 232, 232, 110};
    static const uint8_t mob_green[MOB_KIND_COUNT] = {188, 232, 150, 80};
    static const uint8_t mob_blue[MOB_KIND_COUNT] = {104, 225, 160, 60};
    uint32_t particle_count = 0;
    int index;
    for (index = 0; index < MAX_MOBS && *candidate_count < candidate_limit;
         ++index) {
        const mob_t *mob = &g_mobs[index];
        const uint8_t kind = mob->kind < MOB_KIND_COUNT ? mob->kind : MOB_SLIME;
        uint16_t color;
        if (!mob->alive) continue;
        color = mob->hurt > 0.0F
                    ? rgb565(255, 72, 72)
                    : rgb565(mob_red[kind], mob_green[kind], mob_blue[kind]);
        if (project_billboard(camera, mob->x, mob->y, mob->z,
                              MOB_HALF * 2.0F, MOB_HEIGHT, color,
                              &g_candidates[*candidate_count]))
            ++*candidate_count;
    }
    for (index = 0; index < MAX_PARTICLES &&
                    particle_count < VOXEL_RASTER_PARTICLE_LIMIT &&
                    *candidate_count < candidate_limit;
         ++index) {
        const particle_t *particle = &g_particles[index];
        const uint16_t color = render_block_color(particle->block);
        if (particle->life <= 0.0F) continue;
        if (project_billboard(camera, particle->x, particle->y, particle->z,
                              0.10F, 0.10F, color,
                              &g_candidates[*candidate_count])) {
            ++*candidate_count;
            ++particle_count;
        }
    }
}

static int point_inside_projected(const projected_quad_t *quad,
                                  int32_t x, int32_t y) {
    int sign = 0;
    uint8_t edge_index;
    for (edge_index = 0; edge_index < 4; ++edge_index) {
        const pxa_raster_vertex_t *a = &quad->vertices[edge_index];
        const pxa_raster_vertex_t *b =
            &quad->vertices[(edge_index + 1u) & 3u];
        const int32_t dx = b->x_q4 - a->x_q4;
        const int32_t dy = b->y_q4 - a->y_q4;
        const int64_t side =
            (int64_t)dx * (y - a->y_q4) - (int64_t)dy * (x - a->x_q4);
        if (dx == 0 && dy == 0) continue;
        if (side == 0) return 0;
        if (sign == 0)
            sign = side > 0 ? 1 : -1;
        else if ((side > 0) != (sign > 0))
            return 0;
    }
    return sign != 0;
}

static int proper_edge_intersection(const pxa_raster_vertex_t *a,
                                    const pxa_raster_vertex_t *b,
                                    const pxa_raster_vertex_t *c,
                                    const pxa_raster_vertex_t *d,
                                    float *x, float *y) {
    const int32_t rx = b->x_q4 - a->x_q4;
    const int32_t ry = b->y_q4 - a->y_q4;
    const int32_t sx = d->x_q4 - c->x_q4;
    const int32_t sy = d->y_q4 - c->y_q4;
    const int64_t denominator = (int64_t)rx * sy - (int64_t)ry * sx;
    const int32_t qx = c->x_q4 - a->x_q4;
    const int32_t qy = c->y_q4 - a->y_q4;
    const int64_t t_numerator = (int64_t)qx * sy - (int64_t)qy * sx;
    const int64_t u_numerator = (int64_t)qx * ry - (int64_t)qy * rx;
    float t;
    if (denominator == 0) return 0;
    if (denominator > 0) {
        if (t_numerator <= 0 || t_numerator >= denominator ||
            u_numerator <= 0 || u_numerator >= denominator)
            return 0;
    } else if (t_numerator >= 0 || t_numerator <= denominator ||
               u_numerator >= 0 || u_numerator <= denominator) {
        return 0;
    }
    t = (float)t_numerator / (float)denominator;
    *x = a->x_q4 + (float)rx * t;
    *y = a->y_q4 + (float)ry * t;
    return 1;
}

static int projected_overlap_sample(const projected_quad_t *a,
                                    const projected_quad_t *b,
                                    float *x, float *y) {
    const int32_t left = a->min_x_q4 > b->min_x_q4
                             ? a->min_x_q4 : b->min_x_q4;
    const int32_t right = a->max_x_q4 < b->max_x_q4
                              ? a->max_x_q4 : b->max_x_q4;
    const int32_t top = a->min_y_q4 > b->min_y_q4
                            ? a->min_y_q4 : b->min_y_q4;
    const int32_t bottom = a->max_y_q4 < b->max_y_q4
                               ? a->max_y_q4 : b->max_y_q4;
    uint8_t index;
    if (left >= right || top >= bottom) return 0;
    {
        const int32_t center_x = left + (right - left) / 2;
        const int32_t center_y = top + (bottom - top) / 2;
        if (point_inside_projected(a, center_x, center_y) &&
            point_inside_projected(b, center_x, center_y)) {
            *x = center_x;
            *y = center_y;
            return 1;
        }
    }
    for (index = 0; index < 4; ++index) {
        const pxa_raster_vertex_t *vertex = &a->vertices[index];
        if (point_inside_projected(b, vertex->x_q4, vertex->y_q4)) {
            *x = vertex->x_q4;
            *y = vertex->y_q4;
            return 1;
        }
    }
    for (index = 0; index < 4; ++index) {
        const pxa_raster_vertex_t *vertex = &b->vertices[index];
        if (point_inside_projected(a, vertex->x_q4, vertex->y_q4)) {
            *x = vertex->x_q4;
            *y = vertex->y_q4;
            return 1;
        }
    }
    for (uint8_t edge_a = 0; edge_a < 4; ++edge_a) {
        for (uint8_t edge_b = 0; edge_b < 4; ++edge_b) {
            if (proper_edge_intersection(
                    &a->vertices[edge_a],
                    &a->vertices[(edge_a + 1u) & 3u],
                    &b->vertices[edge_b],
                    &b->vertices[(edge_b + 1u) & 3u], x, y))
                return 1;
        }
    }
    return 0;
}

/* Returns -1 when a is behind b, +1 when b is behind a. */
static int projected_painter_relation(projected_quad_t *a,
                                      projected_quad_t *b) {
    float x;
    float y;
    float inverse_a;
    float inverse_b;
    if (a->max_x_q4 <= b->min_x_q4 || b->max_x_q4 <= a->min_x_q4 ||
        a->max_y_q4 <= b->min_y_q4 || b->max_y_q4 <= a->min_y_q4)
        return 0;
    /* Adaptive splitting bounds most faces to a narrow depth interval. When
     * two intervals do not overlap their order is exact everywhere, so avoid
     * polygon containment, edge intersection and reciprocal-plane math. */
    if (a->nearest_depth_q8 >= b->farthest_depth_q8) return -1;
    if (b->nearest_depth_q8 >= a->farthest_depth_q8) return 1;
    if (!projected_overlap_sample(a, b, &x, &y)) return 0;
    prepare_projected_inverse(a);
    prepare_projected_inverse(b);
    inverse_a = a->inverse_a * x + a->inverse_b * y + a->inverse_c;
    inverse_b = b->inverse_a * x + b->inverse_b * y + b->inverse_c;
    if (inverse_a + 1.0e-7F < inverse_b) return -1;
    if (inverse_b + 1.0e-7F < inverse_a) return 1;
    return 0;
}

static int sort_farther(uint16_t a, uint16_t b) {
    const uint16_t depth_a = g_candidates[g_sort_window[a]].sort_depth_q8;
    const uint16_t depth_b = g_candidates[g_sort_window[b]].sort_depth_q8;
    return depth_a > depth_b || (depth_a == depth_b && a < b);
}

static void sort_heap_push(uint32_t *count, uint16_t value) {
    uint32_t cursor = (*count)++;
    while (cursor != 0) {
        const uint32_t parent = (cursor - 1u) / 2u;
        if (sort_farther(g_sort_heap[parent], value)) break;
        g_sort_heap[cursor] = g_sort_heap[parent];
        cursor = parent;
    }
    g_sort_heap[cursor] = value;
}

static uint16_t sort_heap_pop(uint32_t *count) {
    const uint16_t result = g_sort_heap[0];
    const uint16_t tail = g_sort_heap[--*count];
    uint32_t cursor = 0;
    while (cursor * 2u + 1u < *count) {
        uint32_t child = cursor * 2u + 1u;
        if (child + 1u < *count &&
            sort_farther(g_sort_heap[child + 1u], g_sort_heap[child]))
            ++child;
        if (sort_farther(tail, g_sort_heap[child])) break;
        g_sort_heap[cursor] = g_sort_heap[child];
        cursor = child;
    }
    if (*count != 0) g_sort_heap[cursor] = tail;
    return result;
}

static void sort_candidates(uint32_t count, uint8_t back_to_front,
                            uint8_t exact_painter) {
    uint32_t gap;
    uint32_t index;
    uint32_t exact_count;
    uint32_t exact_start;
    if (count > VOXEL_RASTER_CANDIDATES) count = VOXEL_RASTER_CANDIDATES;
    for (index = 0; index < count; ++index)
        g_sort_order[index] = (uint16_t)index;
    /* Chunk traversal already emits near chunks first. Native depth testing
     * makes exact polygon ordering unnecessary. Painter fallback uses Q8
     * integer keys so sorting does not add floating-point comparisons. */
    if (!back_to_front || count == 0) return;
    {
        uint16_t scratch[VOXEL_RASTER_CANDIDATES];
        uint16_t offsets[SORT_BUCKETS];
        for (uint8_t pass = 0; pass < 2u; ++pass) {
            const uint8_t shift = (uint8_t)(pass * 8u);
            const uint16_t *source = pass == 0 ? g_sort_order : scratch;
            uint16_t *destination = pass == 0 ? scratch : g_sort_order;
            uint16_t bucket;
            uint16_t offset = 0;
            pxa_raster_zero_bytes(offsets, sizeof(offsets));
            for (index = 0; index < count; ++index)
                ++offsets[(g_candidates[source[index]].sort_depth_q8 >>
                            shift) & 255u];
            for (bucket = SORT_BUCKETS; bucket != 0u; --bucket) {
                const uint16_t bucket_count = offsets[bucket - 1u];
                offsets[bucket - 1u] = offset;
                offset = (uint16_t)(offset + bucket_count);
            }
            for (index = 0; index < count; ++index) {
                const uint16_t candidate = source[index];
                const uint16_t key = (uint16_t)(
                    (g_candidates[candidate].sort_depth_q8 >> shift) & 255u);
                destination[offsets[key]++] = candidate;
            }
        }
    }
    if (!exact_painter) return;
    exact_count = exact_painter == 2u
                      ? VOXEL_RASTER_COVERAGE_EXACT_CANDIDATES
                      : VOXEL_RASTER_EXACT_CANDIDATES;
    if (exact_count > count) exact_count = count;
    exact_start = count - exact_count;
    for (index = 0; index < exact_count; ++index)
        g_sort_window[index] = g_sort_order[exact_start + index];
    {
        painter_sort_edge_t *const sort_edges =
            (painter_sort_edge_t *)g_draw_list;
        uint32_t edge_count = 0;
        uint32_t heap_count = 0;
        pxa_raster_zero_bytes(
            g_sort_indegree, (size_t)exact_count * sizeof(g_sort_indegree[0]));
        pxa_raster_zero_bytes(
            g_sort_emitted, (size_t)exact_count * sizeof(g_sort_emitted[0]));
        for (index = 0; index < exact_count; ++index)
            g_sort_edge_heads[index] = UINT16_MAX;
        for (index = 0; index < exact_count; ++index)
            g_sort_sweep[index] = (uint16_t)index;
        for (gap = exact_count / 2u; gap != 0; gap /= 2u) {
            for (index = gap; index < exact_count; ++index) {
                const uint16_t value = g_sort_sweep[index];
                uint32_t cursor = index;
                while (cursor >= gap &&
                       g_candidates[g_sort_window[
                           g_sort_sweep[cursor - gap]]].min_x_q4 >
                           g_candidates[g_sort_window[value]].min_x_q4) {
                    g_sort_sweep[cursor] = g_sort_sweep[cursor - gap];
                    cursor -= gap;
                }
                g_sort_sweep[cursor] = value;
            }
        }
        for (uint32_t sweep_a = 0; sweep_a < exact_count; ++sweep_a) {
            const uint32_t a = g_sort_sweep[sweep_a];
            for (uint32_t sweep_b = sweep_a + 1u; sweep_b < exact_count;
                 ++sweep_b) {
                const uint32_t b = g_sort_sweep[sweep_b];
                int relation;
                uint32_t behind;
                uint32_t ahead;
                if (g_candidates[g_sort_window[b]].min_x_q4 >=
                    g_candidates[g_sort_window[a]].max_x_q4)
                    break;
                relation = projected_painter_relation(
                    &g_candidates[g_sort_window[a]],
                    &g_candidates[g_sort_window[b]]);
                if (relation == 0) continue;
                behind = relation < 0 ? a : b;
                ahead = relation < 0 ? b : a;
                if (edge_count >= VOXEL_RASTER_SORT_EDGES) {
                    ++g_stats.sort_edge_overflow;
                    continue;
                }
                sort_edges[edge_count].ahead = (uint16_t)ahead;
                sort_edges[edge_count].next = g_sort_edge_heads[behind];
                g_sort_edge_heads[behind] = (uint16_t)edge_count++;
                ++g_sort_indegree[ahead];
            }
        }
        g_stats.sort_edges = edge_count;
        for (index = 0; index < exact_count; ++index)
            if (g_sort_indegree[index] == 0)
                sort_heap_push(&heap_count, (uint16_t)index);
        for (uint32_t output = 0; output < exact_count; ++output) {
            uint32_t selected = heap_count != 0
                                    ? sort_heap_pop(&heap_count)
                                    : UINT32_MAX;
            /* A painter cycle is possible when three large faces mutually
             * overlap. Adaptive splitting makes this rare; break it with the
             * stable farthest-depth key instead of adding a pixel z-buffer. */
            if (selected == UINT32_MAX) {
                ++g_stats.sort_cycles;
                for (index = 0; index < exact_count; ++index) {
                    if (!g_sort_emitted[index] &&
                        (selected == UINT32_MAX ||
                         sort_farther((uint16_t)index,
                                      (uint16_t)selected)))
                        selected = index;
                }
            }
            g_sort_order[exact_start + output] = g_sort_window[selected];
            g_sort_emitted[selected] = 1;
            for (uint16_t edge = g_sort_edge_heads[selected];
                 edge != UINT16_MAX; edge = sort_edges[edge].next) {
                const uint16_t ahead = sort_edges[edge].ahead;
                if (!g_sort_emitted[ahead] && g_sort_indegree[ahead] != 0 &&
                    --g_sort_indegree[ahead] == 0)
                    sort_heap_push(&heap_count, ahead);
            }
        }
    }
}

static int projected_quad_has_area(const projected_quad_t *quad) {
    int64_t area = 0;
    uint8_t corner;
    if (quad == NULL) return 0;
    for (corner = 0; corner < 4; ++corner) {
        const pxa_raster_vertex_t *a = &quad->vertices[corner];
        const pxa_raster_vertex_t *b = &quad->vertices[(corner + 1u) & 3u];
        area += (int64_t)a->x_q4 * b->y_q4 -
                (int64_t)b->x_q4 * a->y_q4;
    }
    return area != 0;
}

static int append_projected_quad(pxa_raster_draw_list_t *list,
                                 const projected_quad_t *quad,
                                 uint8_t painter,
                                 uint8_t lit_palette_depth,
                                 uint8_t coverage_mode) {
    if (quad->textured &&
        (g_raster_capabilities & PXA_RASTER_CAP_TEXTURED_QUAD) != 0) {
        pxa_raster_vertex_t vertices[4];
        uint8_t flags = 0;
        uint8_t corner;
        for (corner = 0; corner < 4; ++corner)
            vertices[corner] = quad->vertices[corner];
        if (painter) {
            const uint8_t row = light_row(vertices[0].light);
            /* The Host can map a painter face perspective-correctly from the
             * per-vertex depth. Faces whose depth barely changes keep the
             * cheaper screen-linear mapping; the rest carry their depth so a
             * near wall or floor stays straight without being subdivided. */
            const uint8_t perspective =
                (g_raster_capabilities &
                 PXA_RASTER_CAP_PAINTER_PERSPECTIVE) != 0 &&
                !quad->affine;
            flags = PXA_RASTER_QUAD_PAINTER;
            if (coverage_mode == 3u) {
                flags |= PXA_RASTER_QUAD_LIT_PALETTE;
            } else if (coverage_mode != 0) {
                flags |= PXA_RASTER_QUAD_COVERAGE_MASK;
                if (!perspective) flags |= PXA_RASTER_QUAD_AFFINE_UV;
                if (coverage_mode == 2u)
                    flags |= PXA_RASTER_QUAD_COVERAGE_RESOLVE;
            } else if (!perspective &&
                       (g_raster_capabilities &
                        PXA_RASTER_CAP_AFFINE_UV) != 0) {
                flags |= PXA_RASTER_QUAD_AFFINE_UV;
            }
            if (quad->transparent_index0)
                flags |= PXA_RASTER_QUAD_TRANSPARENT_INDEX0;
            if (quad->blend_75 &&
                (g_raster_capabilities &
                 PXA_RASTER_CAP_FIXED_ALPHA_BLEND) != 0)
                flags |= PXA_RASTER_QUAD_BLEND_75;
            for (corner = 0; corner < 4; ++corner) {
                vertices[corner].light = row;
                if (!perspective && coverage_mode != 3u)
                    vertices[corner].depth_q8 = 0;
            }
        } else {
            if (quad->affine &&
                (g_raster_capabilities & PXA_RASTER_CAP_AFFINE_UV) != 0)
                flags |= PXA_RASTER_QUAD_AFFINE_UV;
            if (lit_palette_depth) {
                const uint8_t row = light_row(vertices[0].light);
                flags |= PXA_RASTER_QUAD_LIT_PALETTE;
                for (corner = 0; corner < 4; ++corner)
                    vertices[corner].light = row;
            }
            if (quad->transparent_index0 &&
                (g_raster_capabilities & PXA_RASTER_CAP_DEPTH_CUTOUT) != 0)
                flags |= PXA_RASTER_QUAD_TRANSPARENT_INDEX0;
            if (quad->blend_75 &&
                (g_raster_capabilities &
                 PXA_RASTER_CAP_FIXED_ALPHA_BLEND) != 0)
                flags |= PXA_RASTER_QUAD_BLEND_75;
        }
        return pxa_raster_textured_quad_flags(
            list, vertices, quad->texture_slot, flags);
    }
    if (painter && coverage_mode == 3u)
        return pxa_raster_solid_painter_depth_quad(
            list, quad->vertices, quad->color);
    if (painter && coverage_mode != 0)
        return pxa_raster_solid_coverage_quad(
            list, quad->vertices, quad->color);
    if (!painter &&
        (g_raster_capabilities & PXA_RASTER_CAP_TEXTURED_QUAD) != 0)
        return pxa_raster_solid_depth_quad(list, quad->vertices, quad->color);
    {
        int16_t xy[8];
        uint8_t corner;
        for (corner = 0; corner < 4; ++corner) {
            xy[corner * 2u] = quad->vertices[corner].x_q4;
            xy[corner * 2u + 1u] = quad->vertices[corner].y_q4;
        }
        return pxa_raster_flat_quad(list, xy, quad->color);
    }
}

static int scene_command_fits(const pxa_raster_draw_list_t *list) {
    const uint32_t largest_scene_command = PXA_RASTER_TEXTURED_QUAD_BYTES;
    return list != NULL &&
           list->command_count + VOXEL_RASTER_HUD_COMMAND_RESERVE <
               PXA_RASTER_MAX_COMMANDS &&
           list->length + largest_scene_command +
                   VOXEL_RASTER_HUD_BYTE_RESERVE <=
               PXA_RASTER_MAX_DRAW_BYTES;
}

static uint8_t projected_quad_is_painter(const projected_quad_t *quad) {
#if VOXEL_RASTER_HYBRID_DEPTH_Q8 == 0
    (void)quad;
    return 1;
#else
    return quad->nearest_depth_q8 >= VOXEL_RASTER_HYBRID_DEPTH_Q8;
#endif
}

static int font_character_index(char character) {
    uint8_t index;
    for (index = 0; index < VOXEL_FONT_GLYPH_COUNT; ++index)
        if (VOXEL_FONT_GLYPHS[index] == character) return index;
    return (int)VOXEL_FONT_GLYPH_COUNT - 1;
}

_Static_assert(VOXEL_FONT_GLYPH_COUNT == VOXEL_RASTER_FONT_GLYPHS,
               "legacy and antialiased font tables must list the same glyphs");

static char *append_u32_text(char *output, uint32_t value) {
    char reverse[10];
    uint8_t count = 0;
    do {
        reverse[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(reverse));
    while (count != 0) *output++ = reverse[--count];
    return output;
}

static void append_rect(pxa_raster_draw_list_t *list, int x, int y, int width,
                        int height, uint16_t color) {
    int16_t quad[8];
    quad[0] = quad[6] = (int16_t)(x * 16);
    quad[1] = quad[3] = (int16_t)(y * 16);
    quad[2] = quad[4] = (int16_t)((x + width) * 16);
    quad[5] = quad[7] = (int16_t)((y + height) * 16);
    (void)pxa_raster_flat_quad(list, quad, color);
}

static void append_sky_rect(pxa_raster_draw_list_t *list,
                            const raster_camera_t *camera, int x, int y,
                            int width, int height, uint16_t color) {
    int right = x + width;
    int bottom = y + height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > camera->width) right = camera->width;
    if (bottom > camera->height) bottom = camera->height;
    if (x < right && y < bottom)
        append_rect(list, x, y, right - x, bottom - y, color);
}

static int project_sky_direction(const raster_camera_t *camera,
                                 float dx, float dy, float dz,
                                 int *screen_x, int *screen_y) {
    const float depth = dx * camera->fx + dy * camera->fy +
                        dz * camera->fz;
    const float right = dx * camera->rx + dz * camera->rz;
    const float up = dx * camera->ux + dy * camera->uy +
                     dz * camera->uz;
    if (depth <= 0.15F) return 0;
    *screen_x = (int)((float)camera->width * 0.5F +
                      right / depth * (float)camera->width *
                          0.5F / camera->tan_x);
    *screen_y = (int)((float)camera->height * 0.5F -
                      up / depth * (float)camera->height *
                          0.5F / camera->tan_y);
    return 1;
}

static void append_sky(pxa_raster_draw_list_t *list,
                       const raster_camera_t *camera, uint8_t gradient) {
    const int bands = 10;
    if (gradient) {
        for (int band = 0; band < bands; ++band) {
            const int top = band * camera->height / bands;
            const int bottom = (band + 1) * camera->height / bands;
            const float vertical =
                (1.0F - (float)(top + bottom) / camera->height) *
                camera->tan_y;
            const float elevation = (camera->fy + camera->uy * vertical) /
                                    rc_sqrt(1.0F + vertical * vertical);
            int r = elevation >= 0.0F ? 150 - (int)(elevation * 95.0F)
                                       : 150 - (int)(elevation * 30.0F);
            int g = elevation >= 0.0F ? 200 - (int)(elevation * 75.0F)
                                       : 200 + (int)(elevation * 25.0F);
            int b = elevation >= 0.0F ? 252 - (int)(elevation * 30.0F)
                                       : 240 + (int)(elevation * 30.0F);
            r = rc_clampi(r, 0, 255);
            g = rc_clampi(g, 0, 255);
            b = rc_clampi(b, 0, 255);
            append_sky_rect(list, camera, 0, top, camera->width,
                            bottom - top, rgb565((uint8_t)r, (uint8_t)g,
                                                (uint8_t)b));
        }
    }
    for (int index = 0; index < VOXEL_SKY_CLOUD_COUNT; ++index) {
        int x;
        int y;
        const float altitude = index & 1 ? 0.32F : 0.42F;
        if (!project_sky_direction(camera,
                                   kVoxelSkyCloudDirections[index][0],
                                   altitude,
                                   kVoxelSkyCloudDirections[index][1],
                                   &x, &y) || x < -40 ||
            x > camera->width + 40 || y < -15 ||
            y > camera->height + 15)
            continue;
        append_sky_rect(list, camera, x - 15, y, 30, 5,
                        UINT16_C(0xd75d));
        append_sky_rect(list, camera, x - 18, y - 4, 36, 5,
                        UINT16_C(0xf7de));
        append_sky_rect(list, camera, x - 9, y - 8, 18, 5,
                        UINT16_C(0xffff));
    }
    {
        int x;
        int y;
        if (project_sky_direction(camera, VOXEL_SKY_SUN_X,
                                  VOXEL_SKY_SUN_Y, VOXEL_SKY_SUN_Z,
                                  &x, &y)) {
            append_sky_rect(list, camera, x - 11, y - 11, 22, 22,
                            UINT16_C(0xff5a));
            append_sky_rect(list, camera, x - 7, y - 7, 14, 14,
                            UINT16_C(0xffdf));
            append_sky_rect(list, camera, x - 5, y - 5, 10, 10,
                            UINT16_C(0xffff));
        }
    }
}

typedef struct {
    uint16_t width;
    uint16_t height;
    const render_layout_t *layout;
} raster_ui_t;

static int ui_x(const raster_ui_t *ui, int value) {
    return value * (int)ui->width / ui->layout->screen_w;
}

static int ui_y(const raster_ui_t *ui, int value) {
    return value * (int)ui->height / ui->layout->screen_h;
}

static void append_ui_rect(pxa_raster_draw_list_t *list, const raster_ui_t *ui,
                           int x, int y, int rect_width, int rect_height,
                           uint16_t color) {
    int right;
    int bottom;
    if (rect_width <= 0 || rect_height <= 0) return;
    right = ui_x(ui, x + rect_width);
    bottom = ui_y(ui, y + rect_height);
    x = ui_x(ui, x);
    y = ui_y(ui, y);
    if (right <= x) right = x + 1;
    if (bottom <= y) bottom = y + 1;
    append_rect(list, x, y, right - x, bottom - y, color);
}

static void append_ui_outline(pxa_raster_draw_list_t *list,
                              const raster_ui_t *ui, int x, int y,
                              int rect_width, int rect_height,
                              int thickness, uint16_t color) {
    append_ui_rect(list, ui, x, y, rect_width, thickness, color);
    append_ui_rect(list, ui, x, y + rect_height - thickness, rect_width,
                   thickness, color);
    append_ui_rect(list, ui, x, y, thickness, rect_height, color);
    append_ui_rect(list, ui, x + rect_width - thickness, y, thickness,
                   rect_height, color);
}

/* The cut-out font is five logical units tall per scale step; the nearest
 * antialiased tier is picked from that height in surface pixels so the new
 * glyphs occupy the space the layout reserved for the old ones. */
static int font_tier_for(const raster_ui_t *ui, int scale) {
    const int screen_h = ui->layout->screen_h > 0 ? ui->layout->screen_h : 1;
    const int logical = 5 * (scale > 0 ? scale : 1);
    const int surface = logical * (int)ui->height / screen_h;
    if (surface <= 8) return 0;
    if (surface <= 16) return 1;
    return 2;
}

static int ui_measure_logical(const raster_ui_t *ui, int surface) {
    if (ui->width == 0) return surface;
    return (surface * ui->layout->screen_w + (int)ui->width - 1) /
           (int)ui->width;
}

static int font_logical_advance(const raster_ui_t *ui, int scale) {
    if (!g_font_aa) return 4 * (scale > 0 ? scale : 1);
    return ui_measure_logical(
        ui, (int)kFontTiers[font_tier_for(ui, scale)].width + 1);
}

static int font_logical_height(const raster_ui_t *ui, int scale) {
    if (!g_font_aa) return 5 * (scale > 0 ? scale : 1);
    return ui_measure_logical(
        ui, kFontTiers[font_tier_for(ui, scale)].height);
}

static void append_ui_text(pxa_raster_draw_list_t *list, const raster_ui_t *ui,
                           int x, int y, const char *text, uint16_t color,
                           int scale) {
    const int advance = font_logical_advance(ui, scale);
    const int mapped_y = ui_y(ui, y);
    int cursor_x = ui_x(ui, x);
    if (g_font_aa) {
        const font_tier_t *const face =
            &kFontTiers[font_tier_for(ui, scale)];
        while (text != NULL && *text != '\0') {
            const int glyph = font_character_index(*text++);
            const int next_x = ui_x(ui, x + advance);
            if (glyph != (int)VOXEL_FONT_GLYPH_COUNT - 1) {
                const uint16_t source_x =
                    (uint16_t)((glyph % face->columns) * face->width);
                const uint16_t source_y =
                    (uint16_t)((glyph / face->columns) * face->height);
                (void)pxa_raster_sprite(
                    list, face->slot,
                    PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 |
                        PXA_RASTER_SPRITE_SOLID_COLOR |
                        PXA_RASTER_SPRITE_TEXEL_ALPHA,
                    g_raster_capabilities, (int16_t)cursor_x,
                    (int16_t)mapped_y, face->width, face->height, source_x,
                    source_y, face->width, face->height, color);
            }
            {
                const int step = next_x - cursor_x;
                cursor_x = next_x > cursor_x ? next_x
                                             : cursor_x + (step < 1 ? 1 : 0);
            }
            x += advance;
        }
        return;
    }
    while (text != NULL && *text != '\0') {
        const int glyph = font_character_index(*text++);
        const int mapped_x = ui_x(ui, x);
        const int next_x = ui_x(ui, x + 3 * scale);
        const int next_y = ui_y(ui, y + 5 * scale);
        const uint16_t glyph_width = (uint16_t)(next_x - mapped_x >= 2
                                                     ? next_x - mapped_x
                                                     : 2);
        const uint16_t glyph_height = (uint16_t)(next_y - mapped_y >= 3
                                                      ? next_y - mapped_y
                                                      : 3);
        if (glyph != (int)VOXEL_FONT_GLYPH_COUNT - 1) {
            /* A 3x5 source glyph must not be reduced to a single sampled
             * column at 2x dynamic resolution: that turns most letters into
             * dots after the Host scales the scene back up. */
            (void)pxa_raster_sprite(
                list, font_slot(),
                PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 |
                    PXA_RASTER_SPRITE_SOLID_COLOR,
                g_raster_capabilities, (int16_t)cursor_x, (int16_t)mapped_y,
                glyph_width, glyph_height, (uint16_t)(glyph * 4), 0, 3, 5,
                color);
        }
        {
            const int next_cursor = ui_x(ui, x + 4 * scale);
            const int mapped = ui_x(ui, x);
            const int step = next_cursor - mapped;
            cursor_x += step >= (int)glyph_width ? step : glyph_width;
        }
        x += 4 * scale;
    }
}

static int ui_text_width(const raster_ui_t *ui, const char *text, int scale) {
    int width = 0;
    const int advance = font_logical_advance(ui, scale);
    while (text != NULL && *text++ != '\0') width += advance;
    return width;
}

/* Lightens (percent > 0) or darkens (percent < 0) a 5/6/5 colour. */
static uint16_t shade565(uint16_t color, int percent) {
    uint32_t red = (color >> 11) & 31u;
    uint32_t green = (color >> 5) & 63u;
    uint32_t blue = color & 31u;
    if (percent >= 0) {
        const uint32_t amount = (uint32_t)percent;
        red += (31u - red) * amount / 100u;
        green += (63u - green) * amount / 100u;
        blue += (31u - blue) * amount / 100u;
    } else {
        const uint32_t amount = (uint32_t)(-percent);
        red -= red * amount / 100u;
        green -= green * amount / 100u;
        blue -= blue * amount / 100u;
    }
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

static int button_half_width(int radius, int dy) {
    const int inner = radius * radius - dy * dy;
    return inner > 0 ? (int)(rc_sqrt((float)inner) + 0.5F) : 0;
}

/* A round HUD button: drop shadow, shaded face and a lighter top third, drawn
 * as horizontal spans so it stays smooth at every resolution. */
static void append_ui_button(pxa_raster_draw_list_t *list,
                             const raster_ui_t *ui, int cx, int cy,
                             int radius, uint16_t fill, uint8_t active) {
    const int mx = ui_x(ui, cx);
    const int my = ui_y(ui, cy);
    const uint16_t face = active ? HUD_BTN_ACTIVE : fill;
    const uint16_t edge = shade565(face, -58);
    int mr = ui_x(ui, cx + radius) - mx;
    int step;
    int dy;
    if (mr < 4) mr = 4;
    step = 1 + mr / 10;
    /* Drop shadow: the face silhouette offset down and right. */
    for (dy = -mr + step; dy <= mr + 1; dy += step) {
        const int half = button_half_width(mr, dy - 1 - step / 2);
        if (half <= 1) continue;
        append_rect(list, mx - half + 1, my + dy, half * 2, step, edge);
    }
    /* Flat face: the offset shadow alone gives the button its depth, so no
     * highlight band crosses it. */
    for (dy = -mr; dy <= mr; dy += step) {
        const int center = dy + step / 2;
        const int half = button_half_width(mr, center);
        if (half <= 1) continue;
        append_rect(list, mx - half, my + dy, half * 2, step, face);
    }
}

static void append_ui_arrow(pxa_raster_draw_list_t *list, const raster_ui_t *ui,
                            int cx, int cy, int down) {
    const int s = ui->layout->ui_scale;
    int index;
    for (index = 0; index < 6 * s; ++index) {
        const int half = down ? 6 * s - 1 - index : index;
        append_ui_rect(list, ui, cx - half, cy - 2 * s + index,
                       half * 2 + 1, s, HUD_TEXT);
    }
}

static void append_ui_item(pxa_raster_draw_list_t *list, const raster_ui_t *ui,
                           int x, int y, int size, uint8_t item) {
    const int mapped_x = ui_x(ui, x);
    const int mapped_y = ui_y(ui, y);
    const int mapped_right = ui_x(ui, x + size);
    const int mapped_bottom = ui_y(ui, y + size);
    const uint16_t width = (uint16_t)(mapped_right > mapped_x
                                          ? mapped_right - mapped_x
                                          : 1);
    const uint16_t height = (uint16_t)(mapped_bottom > mapped_y
                                           ? mapped_bottom - mapped_y
                                           : 1);
    if (item == BLOCK_AIR) return;
    if (item <= VOXEL_RASTER_TEXTURED_BLOCKS) {
        /* Inventory icons show the side tile of the item's block. */
        (void)pxa_raster_sprite(list,
                                texture_slot_for(item, BLOCK_TEXTURE_SIDE), 0,
                                g_raster_capabilities, (int16_t)mapped_x,
                                (int16_t)mapped_y, width, height, 0, 0, 16,
                                16, 0);
    } else if (item < BLOCK_TYPE_COUNT) {
        append_ui_rect(list, ui, x, y, size, size, render_block_color(item));
    } else {
        const uint16_t tool = item <= ITEM_WOOD_SWORD ? UINT16_C(0xaeaa)
                                                       : UINT16_C(0x9cf3);
        append_ui_rect(list, ui, x + size / 2 - 1, y + 2, 3, size - 4, tool);
        append_ui_rect(list, ui, x + 2, y + 3, size - 4, 3, tool);
    }
}

static void append_ui_count(pxa_raster_draw_list_t *list, const raster_ui_t *ui,
                            int x, int y, uint8_t count, int scale) {
    char number[4];
    char *out = number;
    if (count == 0) return;
    out = append_u32_text(out, count);
    *out = '\0';
    /* `y` is the bottom of the number, `x` its right edge. */
    append_ui_text(list, ui, x - ui_text_width(ui, number, scale),
                   y - font_logical_height(ui, scale), number, HUD_TEXT,
                   scale);
}

static void append_hud(pxa_raster_draw_list_t *list, const hud_state_t *hud,
                       uint16_t width, uint16_t height) {
    raster_ui_t ui;
    char status[32];
    char *out;
    int slot;
    const render_layout_t *layout;
    int s;
    int center_x;
    int center_y;
    if (hud == NULL || hud->layout.screen_w <= 0 || hud->layout.screen_h <= 0)
        return;
    layout = &hud->layout;
    s = layout->ui_scale;
    center_x = layout->view_x + layout->view_w / 2;
    center_y = layout->view_y + layout->view_h / 2;
    ui.width = width;
    ui.height = height;
    ui.layout = layout;

    /* Crosshair matches the mapped renderer, but is emitted as compact quads. */
    append_ui_rect(list, &ui, center_x - 8, center_y, 6, 1, HUD_SHADOW);
    append_ui_rect(list, &ui, center_x + 2, center_y, 7, 1, HUD_SHADOW);
    append_ui_rect(list, &ui, center_x, center_y - 8, 1, 6, HUD_SHADOW);
    append_ui_rect(list, &ui, center_x, center_y + 2, 1, 7, HUD_SHADOW);
    append_ui_rect(list, &ui, center_x - 7, center_y, 5, 1, HUD_TEXT);
    append_ui_rect(list, &ui, center_x + 3, center_y, 5, 1, HUD_TEXT);
    append_ui_rect(list, &ui, center_x, center_y - 7, 1, 5, HUD_TEXT);
    append_ui_rect(list, &ui, center_x, center_y + 3, 1, 5, HUD_TEXT);

    for (slot = 0; slot < HOTBAR_SLOTS; ++slot) {
        const int slot_size = layout->hotbar_slot;
        const int padding = 2 * layout->ui_scale;
        const int x = layout->hotbar_x + slot * slot_size;
        const int y = layout->hotbar_y;
        append_ui_rect(list, &ui, x, y, slot_size, slot_size, HUD_PANEL);
        append_ui_outline(list, &ui, x, y, slot_size, slot_size,
                          layout->ui_scale, HUD_SHADOW);
        if (slot == hud->hotbar_selected)
            append_ui_outline(list, &ui, x, y - 2 * layout->ui_scale,
                              slot_size,
                              slot_size + 2 * layout->ui_scale,
                              2 * layout->ui_scale, HUD_SELECT);
        append_ui_item(list, &ui, x + padding, y + padding,
                       slot_size - 2 * padding,
                       hud->hotbar_items[slot]);
        append_ui_count(list, &ui, x + slot_size - padding,
                        y + slot_size - padding,
                        hud->hotbar_counts[slot], layout->ui_scale);
    }

    append_ui_button(list, &ui, layout->jump_x, layout->jump_y,
                     layout->jump_r, HUD_BTN_FACE, hud->jump_held);
    append_ui_arrow(list, &ui, layout->jump_x, layout->jump_y - 3 * s, 0);
    append_ui_button(list, &ui, layout->action_x, layout->action_y,
                     layout->action_r, HUD_BTN_FACE,
                     (uint8_t)(hud->action_held || hud->action_mode != 0));
    if (hud->action_mode == 2) {
        append_ui_rect(list, &ui, layout->action_x - 7 * s,
                       layout->action_y - 5 * s, 15 * s, 3 * s, HUD_TEXT);
        append_ui_rect(list, &ui, layout->action_x - 6 * s,
                       layout->action_y - 2 * s, 3 * s, 7 * s, HUD_TEXT);
        append_ui_rect(list, &ui, layout->action_x + 4 * s,
                       layout->action_y - 2 * s, 3 * s, 7 * s, HUD_TEXT);
    } else {
        append_ui_rect(list, &ui, layout->action_x - 2 * s,
                       layout->action_y - 6 * s, 3 * s, 14 * s, HUD_TEXT);
        append_ui_rect(list, &ui, layout->action_x - 8 * s,
                       layout->action_y - 4 * s, 14 * s, 3 * s, HUD_TEXT);
    }
    append_ui_button(list, &ui, layout->place_x, layout->place_y,
                     layout->place_r, HUD_BTN_FACE, 0);
    append_ui_rect(list, &ui, layout->place_x - 7 * s,
                   layout->place_y - s, 15 * s, 3 * s, HUD_TEXT);
    append_ui_rect(list, &ui, layout->place_x - s,
                   layout->place_y - 7 * s, 3 * s, 15 * s, HUD_TEXT);
    if (hud->flying) {
        append_ui_button(list, &ui, layout->down_x, layout->down_y,
                         layout->down_r, HUD_BTN_FACE, hud->down_held);
        append_ui_arrow(list, &ui, layout->down_x, layout->down_y - 3 * s, 1);
    }
    append_ui_button(list, &ui, layout->fly_x, layout->fly_y, layout->fly_r,
                     HUD_BTN_FACE, hud->flying);
    append_ui_text(list, &ui,
                   layout->fly_x - ui_text_width(&ui, "F", s) / 2,
                   layout->fly_y - font_logical_height(&ui, s) / 2, "F",
                   HUD_TEXT, s);
    append_ui_button(list, &ui, layout->menu_x, layout->menu_y,
                     layout->menu_r, HUD_BTN_FACE, 0);
    append_ui_rect(list, &ui, layout->menu_x - 6 * s,
                   layout->menu_y - 5 * s, 13 * s, 2 * s, HUD_TEXT);
    append_ui_rect(list, &ui, layout->menu_x - 6 * s,
                   layout->menu_y - s, 13 * s, 2 * s, HUD_TEXT);
    append_ui_rect(list, &ui, layout->menu_x - 6 * s,
                   layout->menu_y + 3 * s, 13 * s, 2 * s, HUD_TEXT);
    append_ui_button(list, &ui, layout->bag_x, layout->bag_y, layout->bag_r,
                     HUD_BTN_FACE, hud->inventory_open);
    append_ui_outline(list, &ui, layout->bag_x - 6 * s,
                      layout->bag_y - 4 * s, 13 * s, 9 * s, s, HUD_TEXT);
    append_ui_rect(list, &ui, layout->bag_x - 3 * s,
                   layout->bag_y - 6 * s, 7 * s, 2 * s, HUD_TEXT);

    if (hud->move_active) {
        /* Virtual stick: a square ring in place of the Canvas ellipse. */
        const int reach = 36 * s;
        const int cx = hud->move_origin_x;
        const int cy = hud->move_origin_y;
        append_ui_rect(list, &ui, cx - reach, cy - reach, reach * 2, 2 * s,
                       HUD_SHADOW);
        append_ui_rect(list, &ui, cx - reach, cy + reach - 2 * s, reach * 2,
                       2 * s, HUD_SHADOW);
        append_ui_rect(list, &ui, cx - reach, cy - reach, 2 * s, reach * 2,
                       HUD_SHADOW);
        append_ui_rect(list, &ui, cx + reach - 2 * s, cy - reach, 2 * s,
                       reach * 2, HUD_SHADOW);
        append_ui_button(list, &ui, cx + hud->move_dx, cy + hud->move_dy,
                         10 * s, HUD_BTN_FACE, 0);
    }

    if (hud->show_performance) {
        out = status;
        if (ui.width * 3u < (uint32_t)layout->screen_w * 2u) {
            *out++ = 'F';
        } else {
            *out++ = 'G'; *out++ = 'F'; *out++ = 'P'; *out++ = 'S'; *out++ = ' ';
        }
        out = append_u32_text(out, (hud->fps_x10 + 5u) / 10u);
        *out++ = ' ';
        if (ui.width * 3u >= (uint32_t)layout->screen_w * 2u) {
            if (hud->quality_manual) { *out++ = 'M'; *out++ = 'A'; *out++ = 'N'; }
            else { *out++ = 'A'; *out++ = 'U'; *out++ = 'T'; *out++ = 'O'; }
            *out++ = ' ';
        }
        *out++ = (char)('0' + hud->quality);
        *out++ = 'X';
        *out = '\0';
        append_ui_text(list, &ui, center_x - ui_text_width(&ui, status, s) / 2,
                       layout->view_y + 4 * s, status, HUD_TEXT, s);
    }
    if (hud->mine_progress > 0.01F) {
        int progress = (int)(44.0F * hud->mine_progress);
        if (progress > 44) progress = 44;
        append_ui_rect(list, &ui, center_x - 23, center_y + 13, 46, 7,
                       HUD_SHADOW);
        append_ui_rect(list, &ui, center_x - 22, center_y + 14, progress, 5,
                       HUD_SELECT);
    }
    if (hud->toast != NULL && hud->toast[0] != '\0') {
        const int text_width = ui_text_width(&ui, hud->toast, s);
        const int text_height = font_logical_height(&ui, s);
        const int x = center_x - text_width / 2;
        const int y = layout->hotbar_y - text_height - 6;
        append_ui_rect(list, &ui, x - 4, y - 3, text_width + 8,
                       text_height + 6, HUD_SHADOW);
        append_ui_text(list, &ui, x, y, hud->toast, HUD_TEXT, s);
    }
}

/* Button captions for the main menu, pause and settings screens. */
static const char *menu_button_label(const menu_state_t *menu, int index,
                                     char *dynamic) {
    if (menu->screen == 0) {
        static const char *const labels[] = {"NEW GAME", "LOAD SAVE",
                                             "SETTINGS"};
        return labels[index];
    }
    if (menu->screen == 2) {
        static const char *const labels[] = {"RESUME", "SAVE GAME",
                                             "SETTINGS", "MAIN MENU"};
        return labels[index];
    }
    if (index == 0) {
        char *out = dynamic;
        *out++ = 'Q';
        *out++ = 'U';
        *out++ = 'A';
        *out++ = 'L';
        *out++ = 'I';
        *out++ = 'T';
        *out++ = 'Y';
        *out++ = ':';
        *out++ = ' ';
        if (menu->quality_manual == 0) {
            *out++ = 'A';
            *out++ = 'U';
            *out++ = 'T';
            *out++ = 'O';
        } else {
            *out++ = (char)('0' + menu->quality);
            *out++ = 'X';
        }
        *out = '\0';
        return dynamic;
    }
    if (index == 1)
        return menu->show_performance ? "PERFORMANCE: ON" : "PERFORMANCE: OFF";
    if (index == 2) return "SAVE GAME";
    if (index == 3) return "DELETE SAVE";
    return "BACK";
}

/* Menus are drawn into the raster draw list at the scene resolution, so the
 * main menu, pause and settings screens do not need a Canvas overlay. */
static void append_menu(pxa_raster_draw_list_t *list, const menu_state_t *menu,
                        uint16_t width, uint16_t height) {
    raster_ui_t ui;
    const render_layout_t *layout = &g_layout;
    const int s = layout->ui_scale;
    const int body_scale = s;
    const int title_scale = 2 * s;
    int center_x;
    int index;
    char label[32];
    if (menu == NULL) return;
    render_menu_layout(menu);
    center_x = layout->view_x + layout->view_w / 2;
    ui.width = width;
    ui.height = height;
    ui.layout = layout;
    if (!menu->overlay) {
        append_ui_rect(list, &ui, 0, 0, layout->screen_w, layout->screen_h,
                       MENU_BG);
    }
    {
        const char *title = menu->screen == 2   ? "PAUSED"
                            : menu->screen == 1 ? "SETTINGS"
                                                : "VOXEL CRAFT";
        const int title_width = ui_text_width(&ui, title, title_scale);
        const int title_x = center_x - title_width / 2;
        const int title_y = layout->view_y + 18 * s;
        if (menu->overlay) {
            append_ui_rect(list, &ui, title_x - 8 * s, title_y - 4 * s,
                           title_width + 16 * s,
                           font_logical_height(&ui, title_scale) + 8 * s,
                           HUD_SHADOW);
        }
        append_ui_text(list, &ui, title_x, title_y, title, HUD_SELECT,
                       title_scale);
    }
    if (menu->screen == 0) {
        char *out = label;
        char *end;
        *out++ = 'S';
        *out++ = 'E';
        *out++ = 'E';
        *out++ = 'D';
        *out++ = ' ';
        end = append_u32_text(out, menu->seed);
        *end = '\0';
        append_ui_text(list, &ui,
                       center_x - ui_text_width(&ui, label, body_scale) / 2,
                       layout->view_y + 54 * s, label, HUD_MUTED, body_scale);
    }
    for (index = 0; index < g_menu_button_count; ++index) {
        const menu_button_t *button = &g_menu_buttons[index];
        const char *text = menu_button_label(menu, index, label);
        const int text_width = ui_text_width(&ui, text, body_scale);
        const uint16_t face =
            button->enabled ? HUD_BTN_FACE : shade565(HUD_BTN_FACE, -45);
        const uint16_t ink = button->enabled ? HUD_TEXT : HUD_MUTED;
        /* Offset shadow plus a flat face and an accent outline: no highlight
         * band on the surface. */
        append_ui_rect(list, &ui, button->x + 2 * s, button->y + 3 * s,
                       button->w, button->h, shade565(face, -62));
        append_ui_rect(list, &ui, button->x, button->y, button->w,
                       button->h, face);
        append_ui_outline(list, &ui, button->x, button->y, button->w,
                          button->h, 2 * s,
                          button->enabled ? HUD_SELECT : HUD_SHADOW);
        append_ui_text(list, &ui,
                       button->x + (button->w - text_width) / 2,
                       button->y + (button->h -
                                    font_logical_height(&ui, body_scale)) /
                                       2,
                       text,
                       ink, body_scale);
    }
    if (menu->screen == 0) {
        static const char hint[] = "TAP TO SELECT";
        append_ui_text(list, &ui,
                       center_x - ui_text_width(&ui, hint, body_scale) / 2,
                       layout->view_y + layout->view_h - 26 * s, hint,
                       HUD_MUTED, body_scale);
    }
    if (menu->toast != NULL && menu->toast[0] != '\0') {
        const int text_width = ui_text_width(&ui, menu->toast, body_scale);
        const int x = center_x - text_width / 2;
        const int y = layout->view_y + layout->view_h - 28 * s;
        append_ui_rect(list, &ui, x - 4 * s, y - 3 * s, text_width + 8 * s,
                       font_logical_height(&ui, body_scale) + 6 * s,
                       HUD_SHADOW);
        append_ui_text(list, &ui, x, y, menu->toast, HUD_TEXT, body_scale);
    }
}

static void append_outline_line(pxa_raster_draw_list_t *list, float x0,
                                float y0, float x1, float y1) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float length = rc_sqrt(dx * dx + dy * dy);
    float nx;
    float ny;
    int16_t xy[8];
    if (length < 0.5F) return;
    nx = -dy / length * 0.55F;
    ny = dx / length * 0.55F;
    xy[0] = (int16_t)((x0 + nx) * 16.0F);
    xy[1] = (int16_t)((y0 + ny) * 16.0F);
    xy[2] = (int16_t)((x1 + nx) * 16.0F);
    xy[3] = (int16_t)((y1 + ny) * 16.0F);
    xy[4] = (int16_t)((x1 - nx) * 16.0F);
    xy[5] = (int16_t)((y1 - ny) * 16.0F);
    xy[6] = (int16_t)((x0 - nx) * 16.0F);
    xy[7] = (int16_t)((y0 - ny) * 16.0F);
    (void)pxa_raster_flat_quad(list, xy, UINT16_C(0x0000));
}

static int clip_block_line(const raster_camera_t *camera, float x0, float y0,
                           float x1, float y1, float *start, float *end) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float delta[4] = {-dx, dx, -dy, dy};
    const float limit[4] = {x0, camera->width - x0, y0,
                            camera->height - y0};
    *start = 0.0F;
    *end = 1.0F;
    for (uint8_t side = 0; side < 4; ++side) {
        if (delta[side] == 0.0F) {
            if (limit[side] < 0.0F) return 0;
            continue;
        }
        const float fraction = limit[side] / delta[side];
        if (delta[side] < 0.0F) {
            if (fraction > *start) *start = fraction;
        } else if (fraction < *end) {
            *end = fraction;
        }
        if (*start >= *end) return 0;
    }
    return 1;
}

static int block_outline_point_visible(const raster_camera_t *camera,
                                       const ray_hit_t *target,
                                       uint8_t first_corner,
                                       uint8_t second_corner, float fraction,
                                       float depth_a, float depth_b) {
    const float world_fraction = fraction * depth_a /
        (depth_b * (1.0F - fraction) + depth_a * fraction);
    const float bx = (float)target->x;
    const float by = (float)target->y;
    const float bz = (float)target->z;
    const float world_x = bx + 0.01F + 0.98F *
        ((float)(first_corner & 1u) +
         ((float)(second_corner & 1u) - (float)(first_corner & 1u)) *
             world_fraction);
    const float world_y = by + 0.01F + 0.98F *
        ((float)((first_corner >> 1) & 1u) +
         ((float)((second_corner >> 1) & 1u) -
          (float)((first_corner >> 1) & 1u)) *
             world_fraction);
    const float world_z = bz + 0.01F + 0.98F *
        ((float)((first_corner >> 2) & 1u) +
         ((float)((second_corner >> 2) & 1u) -
          (float)((first_corner >> 2) & 1u)) *
             world_fraction);
    const float dx = world_x - camera->x;
    const float dy = world_y - camera->y;
    const float dz = world_z - camera->z;
    float extent = rc_fabs(dx);
    uint16_t steps;
    float inverse_steps;
    float step_x;
    float step_y;
    float step_z;
    float sample_x = camera->x;
    float sample_y = camera->y;
    float sample_z = camera->z;
    if (rc_fabs(dy) > extent) extent = rc_fabs(dy);
    if (rc_fabs(dz) > extent) extent = rc_fabs(dz);
    steps = (uint16_t)(extent * 8.0F + 1.0F);
    if (steps > 128u) steps = 128u;
    inverse_steps = 1.0F / steps;
    step_x = dx * inverse_steps;
    step_y = dy * inverse_steps;
    step_z = dz * inverse_steps;
    for (uint16_t step = 0; step < steps; ++step) {
        const int cell_x = rc_floor_int(sample_x += step_x);
        const int cell_y = rc_floor_int(sample_y += step_y);
        const int cell_z = rc_floor_int(sample_z += step_z);
        const int block = game_block_fast(cell_x, cell_y, cell_z);
        if (block == BLOCK_AIR || block == BLOCK_WATER) continue;
        return cell_x == target->x && cell_y == target->y &&
               cell_z == target->z;
    }
    return 0;
}

/* The targeted block outline. Host flat quads have no depth, so only the
 * faces that point at the camera are outlined; that reads as a solid 3D box
 * instead of a see-through wireframe. */
static void append_block_outline(pxa_raster_draw_list_t *list,
                                 const raster_camera_t *camera,
                                 const ray_hit_t *target) {
    static const uint8_t kFaceCorners[6][4] = {
        {0, 2, 6, 4}, {1, 3, 7, 5}, {0, 1, 5, 4},
        {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 5, 7, 6},
    };
    float screen_x[8];
    float screen_y[8];
    float depths[8];
    uint8_t projectable[8];
    uint64_t drawn_edges = 0;
    uint8_t index;
    float bx;
    float by;
    float bz;
    if (target == NULL || !target->hit) return;
    bx = (float)target->x;
    by = (float)target->y;
    bz = (float)target->z;
    for (index = 0; index < 8; ++index) {
        const float wx = bx + (float)(index & 1u);
        const float wy = by + (float)((index >> 1) & 1u);
        const float wz = bz + (float)((index >> 2) & 1u);
        const float dx = wx - camera->x;
        const float dy = wy - camera->y;
        const float dz = wz - camera->z;
        const float depth = dx * camera->fx + dy * camera->fy + dz * camera->fz;
        const float right = dx * camera->rx + dz * camera->rz;
        const float up = dx * camera->ux + dy * camera->uy + dz * camera->uz;
        if (depth < VOXEL_RASTER_NEAR) {
            projectable[index] = 0;
            continue;
        }
        projectable[index] = 1;
        depths[index] = depth;
        screen_x[index] = camera->width * 0.5F +
                          right / depth * (camera->width * 0.5F) /
                              camera->tan_x;
        screen_y[index] = camera->height * 0.5F -
                          up / depth * (camera->height * 0.5F) /
                              camera->tan_y;
    }
    for (index = 0; index < 6; ++index) {
        uint8_t visible;
        uint8_t edge;
        switch (index) {
        case 0: visible = camera->x < bx; break;
        case 1: visible = camera->x > bx + 1.0F; break;
        case 2: visible = camera->y < by; break;
        case 3: visible = camera->y > by + 1.0F; break;
        case 4: visible = camera->z < bz; break;
        default: visible = camera->z > bz + 1.0F; break;
        }
        if (!visible) continue;
        for (edge = 0; edge < 4; ++edge) {
            const uint8_t a = kFaceCorners[index][edge];
            const uint8_t b = kFaceCorners[index][(edge + 1u) & 3u];
            const uint8_t low = a < b ? a : b;
            const uint8_t high = a < b ? b : a;
            const uint64_t edge_bit = UINT64_C(1) << (low * 8u + high);
            float length;
            float clipped_start;
            float clipped_end;
            uint16_t segments;
            uint16_t segment;
            int32_t visible_start = -1;
            if (!projectable[a] || !projectable[b]) continue;
            if ((drawn_edges & edge_bit) != 0) continue;
            drawn_edges |= edge_bit;
            if (!clip_block_line(camera, screen_x[a], screen_y[a],
                                 screen_x[b], screen_y[b], &clipped_start,
                                 &clipped_end))
                continue;
            length = rc_sqrt((screen_x[b] - screen_x[a]) *
                                 (screen_x[b] - screen_x[a]) +
                             (screen_y[b] - screen_y[a]) *
                                 (screen_y[b] - screen_y[a])) *
                     (clipped_end - clipped_start);
            segments = (uint16_t)(length / 12.0F + 1.0F);
            if (segments > 64u) segments = 64u;
            for (segment = 0; segment <= segments; ++segment) {
                const int visible = segment < segments &&
                    block_outline_point_visible(
                        camera, target, a, b,
                        clipped_start + (clipped_end - clipped_start) *
                            ((float)segment + 0.5F) / segments,
                        depths[a], depths[b]);
                if (visible && visible_start < 0)
                    visible_start = segment;
                if (!visible && visible_start >= 0) {
                    const float start = clipped_start +
                        (clipped_end - clipped_start) *
                            (float)visible_start / segments;
                    const float end = clipped_start +
                        (clipped_end - clipped_start) *
                            (float)segment / segments;
                    append_outline_line(
                        list, screen_x[a] + (screen_x[b] - screen_x[a]) * start,
                        screen_y[a] + (screen_y[b] - screen_y[a]) * start,
                        screen_x[a] + (screen_x[b] - screen_x[a]) * end,
                        screen_y[a] + (screen_y[b] - screen_y[a]) * end);
                    visible_start = -1;
                }
            }
        }
    }
}

static void append_inv_slot(pxa_raster_draw_list_t *list,
                            const raster_ui_t *ui, int x, int y,
                            const item_stack_t *slot, int highlight) {
    const render_layout_t *layout = ui->layout;
    const int size = layout->hotbar_slot;
    const int pad = 2 * layout->ui_scale;
    append_ui_rect(list, ui, x, y, size, size, HUD_PANEL);
    append_ui_outline(list, ui, x, y, size, size, layout->ui_scale,
                      highlight ? HUD_SELECT : HUD_SHADOW);
    if (slot->item != BLOCK_AIR) {
        append_ui_item(list, ui, x + pad, y + pad, size - 2 * pad, slot->item);
        append_ui_count(list, ui, x + size - pad, y + size - pad, slot->count,
                        layout->ui_scale);
    }
}

/* Inventory and crafting table, drawn at the real scene resolution. */
static void append_inventory(pxa_raster_draw_list_t *list,
                             const hud_state_t *hud, uint16_t width,
                             uint16_t height) {
    raster_ui_t ui;
    const render_layout_t *layout = &g_layout;
    const int s = layout->ui_scale;
    const int slot_size = layout->hotbar_slot;
    const int padding = 2 * s;
    const int craft = hud->craft_table ? 3 : 2;
    int row;
    int column;
    char title[24];
    if (hud == NULL) return;
    ui.width = width;
    ui.height = height;
    ui.layout = layout;
    append_ui_rect(list, &ui, 0, 0, layout->screen_w, layout->screen_h,
                   MENU_BG);
    append_ui_rect(list, &ui, g_inv_layout.panel_x, g_inv_layout.panel_y,
                   g_inv_layout.panel_w, g_inv_layout.panel_h, HUD_PANEL);
    append_ui_outline(list, &ui, g_inv_layout.panel_x, g_inv_layout.panel_y,
                      g_inv_layout.panel_w, g_inv_layout.panel_h, 2 * s,
                      HUD_PANEL_LIGHT);
    {
        const char *label = hud->craft_table ? "CRAFTING" : "INVENTORY";
        static const char hint[] = "TAP MOVE  HOLD SPLIT";
        int index = 0;
        while (label[index] != '\0' && index < (int)sizeof(title) - 1) {
            title[index] = label[index];
            ++index;
        }
        title[index] = '\0';
        append_ui_text(list, &ui,
                       g_inv_layout.panel_x +
                           (g_inv_layout.panel_w -
                            ui_text_width(&ui, title, 2 * s)) /
                               2,
                       g_inv_layout.panel_y + 8 * s, title, HUD_TEXT, 2 * s);
        append_ui_text(list, &ui,
                       g_inv_layout.panel_x +
                           (g_inv_layout.panel_w -
                            ui_text_width(&ui, hint, s)) /
                               2,
                       g_inv_layout.panel_y + 22 * s, hint, HUD_MUTED, s);
    }
    for (row = 0; row < craft; ++row) {
        for (column = 0; column < craft; ++column) {
            const int x = g_inv_layout.craft_x + column * slot_size;
            const int y = g_inv_layout.craft_y + row * slot_size;
            const item_stack_t *slot =
                hud->craft_table ? &g_table_craft[row * craft + column]
                                 : &g_craft[row * craft + column];
            append_inv_slot(list, &ui, x, y, slot, 0);
        }
    }
    {
        const item_stack_t *result =
            hud->craft_table ? &g_table_result : &g_craft_result;
        const int arrow_x = g_inv_layout.craft_x + craft * slot_size + 4 * s;
        const int arrow_y = g_inv_layout.craft_y + (craft * slot_size) / 2;
        append_outline_line(list, (float)ui_x(&ui, arrow_x),
                            (float)ui_y(&ui, arrow_y),
                            (float)ui_x(&ui, arrow_x + 10 * s),
                            (float)ui_y(&ui, arrow_y));
        append_outline_line(list, (float)ui_x(&ui, arrow_x + 6 * s),
                            (float)ui_y(&ui, arrow_y - 4 * s),
                            (float)ui_x(&ui, arrow_x + 10 * s),
                            (float)ui_y(&ui, arrow_y));
        append_outline_line(list, (float)ui_x(&ui, arrow_x + 6 * s),
                            (float)ui_y(&ui, arrow_y + 4 * s),
                            (float)ui_x(&ui, arrow_x + 10 * s),
                            (float)ui_y(&ui, arrow_y));
        append_inv_slot(list, &ui, g_inv_layout.result_x,
                        g_inv_layout.result_y, result,
                        result->item != BLOCK_AIR ? 1 : 0);
    }
    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 9; ++column) {
            append_inv_slot(list, &ui,
                            g_inv_layout.main_x + column * slot_size,
                            g_inv_layout.main_y + row * slot_size,
                            &g_inventory[HOTBAR_SLOTS + row * 9 + column], 0);
        }
    }
    for (column = 0; column < 9; ++column) {
        append_inv_slot(list, &ui,
                        g_inv_layout.inv_hotbar_x + column * slot_size,
                        g_inv_layout.inv_hotbar_y, &g_inventory[column],
                        column == hud->hotbar_selected ? 1 : 0);
    }
    append_outline_line(list, (float)ui_x(&ui, g_inv_layout.close_x - 7 * s),
                        (float)ui_y(&ui, g_inv_layout.close_y - 7 * s),
                        (float)ui_x(&ui, g_inv_layout.close_x + 7 * s),
                        (float)ui_y(&ui, g_inv_layout.close_y + 7 * s));
    append_outline_line(list, (float)ui_x(&ui, g_inv_layout.close_x - 7 * s),
                        (float)ui_y(&ui, g_inv_layout.close_y + 7 * s),
                        (float)ui_x(&ui, g_inv_layout.close_x + 7 * s),
                        (float)ui_y(&ui, g_inv_layout.close_y - 7 * s));
    if (hud->cursor_item != BLOCK_AIR && hud->cursor_count != 0) {
        const int x = hud->pointer_x - slot_size / 2;
        const int y = hud->pointer_y - slot_size - 6 * s;
        append_ui_rect(list, &ui, x, y, slot_size, slot_size, HUD_PANEL);
        append_ui_item(list, &ui, x + padding, y + padding,
                       slot_size - 2 * padding, hud->cursor_item);
        append_ui_count(list, &ui, x + slot_size - padding,
                        y + slot_size - padding, hud->cursor_count, s);
    }
}

/* One-shot diagnostics: a rejected draw list is otherwise invisible from the
 * App side, and a silent submit failure looks like a frozen frame. */
static uint8_t g_submit_error_logged;
static uint32_t g_submit_frames;

static int32_t submit_list(uint32_t surface_handle,
                           pxa_raster_draw_list_t *list) {
    const int32_t result = pxa_raster_submit(surface_handle, list);
    ++g_submit_frames;
    if (result < PXA_STATUS_OK && !g_submit_error_logged) {
        char message[48];
        char *out = message;
        static const char prefix[] = "voxel: raster submit failed rc=";
        size_t index;
        g_submit_error_logged = 1;
        for (index = 0; index < sizeof(prefix) - 1u; ++index)
            *out++ = prefix[index];
        out = append_u32_text(out, (uint32_t)(result < 0 ? -result : result));
        *out++ = result < 0 ? '-' : '+';
        *out = '\0';
        (void)pxa_log_error(message);
    }
    return result;
}

int32_t voxel_raster_render(uint32_t surface_handle, uint64_t frame_id,
                            const player_t *player, uint8_t quality,
                            const hud_state_t *hud, const menu_state_t *menu,
                            const ray_hit_t *target) {
    raster_camera_t camera;
    pxa_raster_draw_list_t list;
    visible_chunk_t visible_chunks[GRID_COUNT];
    uint16_t visible_chunk_cursors[GRID_COUNT] = {0};
    uint32_t candidate_count = 0;
    uint32_t candidate_limit;
    uint32_t mesh_limit;
    uint8_t mesh_build_budget = 1;
    /* World edits invalidate the previous mesh; rebuild those synchronously so
     * a placed or broken block appears at once instead of after the
     * incremental streaming budget finishes the whole chunk. */
    uint8_t edit_build_budget = 3;
    uint8_t visible_count = 0;
    int depth_terrain;
    int depth_painter;
    int lit_palette_depth;
    int coverage_mask;
    int hybrid_painter;
    int exact_painter;
    int grid_z;
    if (surface_handle == 0 || frame_id == 0 || player == NULL)
        return PXA_STATUS_INVALID_ARGUMENT;
    g_water_uv_offset_q4 = (int16_t)((frame_id * 2u) & 255u);
    pxa_raster_zero_bytes(&g_stats, sizeof(g_stats));
    if (menu != NULL && !menu->overlay) {
        /* Full-screen menu: no 3D scene behind it, just the UI. */
        const uint16_t width = (uint16_t)render_scene_width();
        const uint16_t height = (uint16_t)render_scene_height();
        pxa_raster_draw_list_begin(&list, g_draw_list, sizeof(g_draw_list),
                                   frame_id);
        (void)pxa_raster_clear(&list, MENU_BG);
        append_menu(&list, menu, width, height);
        g_stats.draw_list_bytes = list.length;
        g_stats.covered_pixel_budget = (uint32_t)width * height;
        return submit_list(surface_handle, &list);
    }
    if (hud != NULL && hud->inventory_open) {
        /* The inventory is a full-screen panel; drawing it without the 3D
         * scene keeps it sharp at the real resolution. */
        const uint16_t width = (uint16_t)render_scene_width();
        const uint16_t height = (uint16_t)render_scene_height();
        pxa_raster_draw_list_begin(&list, g_draw_list, sizeof(g_draw_list),
                                   frame_id);
        (void)pxa_raster_clear(&list, MENU_BG);
        append_inventory(&list, hud, width, height);
        g_stats.draw_list_bytes = list.length;
        g_stats.covered_pixel_budget = (uint32_t)width * height;
        return submit_list(surface_handle, &list);
    }
    build_camera(&camera, player, quality);
    g_stats.fog_end_q8 = (uint32_t)(camera.fog_end * 256.0F + 0.5F);
    lit_palette_depth =
        (g_raster_capabilities & PXA_RASTER_CAP_LIT_PALETTE_DEPTH) != 0;
    depth_painter =
        (g_raster_capabilities & PXA_RASTER_CAP_PAINTER_DEPTH) != 0 &&
        (g_raster_capabilities & PXA_RASTER_CAP_PAINTER_PERSPECTIVE) != 0 &&
        (g_raster_capabilities & PXA_RASTER_CAP_LIT_PALETTE_DEPTH) != 0 &&
        (g_raster_capabilities & PXA_RASTER_CAP_PAINTER_POLYGON) != 0;
    coverage_mask = !depth_painter &&
        (g_raster_capabilities & PXA_RASTER_CAP_COVERAGE_MASK) != 0 &&
        (g_raster_capabilities & PXA_RASTER_CAP_AFFINE_UV) != 0 &&
        (g_raster_capabilities & PXA_RASTER_CAP_PAINTER_POLYGON) != 0 &&
        (g_raster_capabilities & PXA_RASTER_CAP_FIXED_ALPHA_BLEND) != 0;
    depth_terrain =
        VOXEL_RASTER_DEPTH_TERRAIN != 0 && !coverage_mask &&
        (g_raster_capabilities & PXA_RASTER_CAP_TEXTURED_QUAD) != 0 &&
        (g_raster_capabilities & PXA_RASTER_CAP_DEPTH_CUTOUT) != 0 &&
        (g_raster_capabilities & PXA_RASTER_CAP_LIT_PALETTE_DEPTH) != 0;
    if (depth_terrain || depth_painter) {
        candidate_limit = quality >= QUALITY_PERFORMANCE
                              ? VOXEL_RASTER_PERFORMANCE_CANDIDATES
                          : quality >= QUALITY_BALANCED ? 768u
                                                        : 1024u;
    } else {
        candidate_limit = quality >= QUALITY_PERFORMANCE
                              ? VOXEL_RASTER_PERFORMANCE_CANDIDATES
                          : quality >= QUALITY_BALANCED
                              ? (g_painter_perspective ? 640u : 480u)
                              : (g_painter_perspective ? 896u : 620u);
    }
    hybrid_painter =
        !depth_terrain && !coverage_mask && lit_palette_depth &&
        (g_raster_capabilities & PXA_RASTER_CAP_PAINTER_POLYGON) != 0;
    exact_painter = hybrid_painter && VOXEL_RASTER_HYBRID_DEPTH_Q8 == 0u &&
                    VOXEL_RASTER_EXACT_SORT != 0;
    /* A full-resolution frame makes both Guest projection and Host fill cost
     * substantially more expensive. Keep near terrain responsive instead of
     * spending the frame budget on distant faces. */
    if (camera.width >= 240u) {
        /* Without adaptive splitting one source face costs one candidate, so
         * the same budget covers three to four times the terrain it did when
         * every near face was subdivided to bound the affine error. */
        const uint32_t full_resolution_limit =
            depth_terrain || depth_painter
                ? VOXEL_RASTER_DEPTH_CANDIDATES
                : (exact_painter ? VOXEL_RASTER_EXACT_CANDIDATES
                                 : (g_painter_perspective ? 768u : 320u));
        if (candidate_limit > full_resolution_limit)
            candidate_limit = full_resolution_limit;
    }
    {
        /* Cap the candidate budget at what the draw list can actually carry.
         * Without this the pass loop runs out of transport part way through
         * and drops whatever is left - and because it walks far to near, the
         * faces it drops are the nearest ones. Budgeting up front moves the
         * loss to the per-chunk fair share instead, which thins the distance. */
        const uint32_t byte_capacity =
            (PXA_RASTER_MAX_DRAW_BYTES - VOXEL_RASTER_HUD_BYTE_RESERVE) /
            PXA_RASTER_TEXTURED_QUAD_BYTES;
        const uint32_t command_capacity =
            PXA_RASTER_MAX_COMMANDS - VOXEL_RASTER_HUD_COMMAND_RESERVE;
        uint32_t transport_limit =
            byte_capacity < command_capacity ? byte_capacity
                                             : command_capacity;
        /* Sky bands, the block outline and the entity billboards share it. */
        transport_limit = transport_limit > 48u ? transport_limit - 48u : 1u;
        if (candidate_limit > transport_limit)
            candidate_limit = transport_limit;
    }
    g_stats.candidate_limit = candidate_limit;
    if (g_mesh_cache_warmed && g_building_mesh != NULL) {
        const chunk_t *building_chunk = g_building_mesh->build_chunk;
        if (building_chunk == NULL || !building_chunk->loaded ||
            g_building_mesh->build_revision != building_chunk->revision ||
            g_building_mesh->build_chunk_cx != building_chunk->cx ||
            g_building_mesh->build_chunk_cz != building_chunk->cz) {
            g_building_mesh->building = 0;
            g_building_mesh = NULL;
        } else {
            --mesh_build_budget;
            ++g_stats.mesh_build_passes;
            if (advance_mesh_rebuild(g_building_mesh, building_chunk))
                ++g_stats.rebuilt_chunks;
        }
    }
    for (grid_z = 0; grid_z < GRID_W; ++grid_z) {
        int grid_x;
        for (grid_x = 0; grid_x < GRID_W; ++grid_x) {
            const int mesh_index = grid_z * GRID_W + grid_x;
            const chunk_t *chunk = g_chunk_grid[grid_z][grid_x];
            chunk_mesh_t *mesh;
            if (chunk == NULL || !chunk->loaded) {
                ++g_stats.unloaded_chunks;
                continue;
            }
            if (!chunk_visible(&camera, chunk)) {
                ++g_stats.frustum_culled;
                continue;
            }
            mesh = mesh_for_chunk(chunk, (uint8_t)mesh_index);
            if (mesh->chunk != chunk || mesh->revision != chunk->revision ||
                mesh->chunk_cx != chunk->cx || mesh->chunk_cz != chunk->cz) {
                if (!g_mesh_cache_warmed) {
                    rebuild_mesh(mesh, chunk);
                    ++g_stats.rebuilt_chunks;
                } else if (mesh->chunk == chunk && mesh->revision != 0 &&
                           edit_build_budget != 0) {
                    /* An edit invalidated a mesh that was already built:
                     * rebuild it in one go so the change shows this frame. */
                    rebuild_mesh(mesh, chunk);
                    --edit_build_budget;
                    ++g_stats.rebuilt_chunks;
                } else if (mesh->chunk != chunk &&
                           chunk_depth(&camera, chunk) < 28.0F &&
                           edit_build_budget != 0) {
                    /* A chunk streamed in right in front of the player:
                     * build it now instead of over dozens of frames. */
                    rebuild_mesh(mesh, chunk);
                    --edit_build_budget;
                    ++g_stats.rebuilt_chunks;
                } else if (g_building_mesh == NULL &&
                           mesh_build_budget != 0) {
                    begin_mesh_rebuild(mesh, chunk);
                    --mesh_build_budget;
                    ++g_stats.mesh_build_passes;
                    if (advance_mesh_rebuild(mesh, chunk))
                        ++g_stats.rebuilt_chunks;
                }
                /* A revision rebuild keeps the previous mesh visible. A cache
                 * slot reassigned to another chunk has no valid fallback. */
                if (mesh->chunk != chunk || mesh->chunk_cx != chunk->cx ||
                    mesh->chunk_cz != chunk->cz) {
                    ++g_stats.pending_mesh_chunks;
                    continue;
                }
            }
            g_stats.cached_quads += mesh->quad_count;
            g_stats.dropped_quads += mesh->dropped;
            g_stats.mesh_overflow_quads += mesh->dropped;
            g_stats.mesh_input_quads += mesh->quad_count;
            visible_chunks[visible_count].chunk = chunk;
            visible_chunks[visible_count].mesh = mesh;
            visible_chunks[visible_count].depth = chunk_depth(&camera, chunk);
            {
                uint8_t mask = 0;
                const float min_x = (float)chunk->cx * CHUNK_SIZE;
                const float min_z = (float)chunk->cz * CHUNK_SIZE;
                if (camera.x < min_x) mask |= 1u << 1;
                if (camera.x > min_x + CHUNK_SIZE) mask |= 1u << 0;
                if (camera.y < 0.0F) mask |= 1u << 3;
                if (camera.y > CHUNK_HEIGHT) mask |= 1u << 2;
                if (camera.z < min_z) mask |= 1u << 5;
                if (camera.z > min_z + CHUNK_SIZE) mask |= 1u << 4;
                visible_chunks[visible_count].backface_mask = mask;
            }
            ++visible_count;
        }
    }
    g_stats.visible_chunks = visible_count;
    g_mesh_cache_warmed = 1;
    sort_visible_chunks(visible_chunks, visible_count);
    /* Keep space for actors and particles. Without this reservation, a dense
     * terrain mesh consumes the whole transport budget before entities run. */
    mesh_limit = candidate_limit > 20u ? candidate_limit - 20u : candidate_limit;
    if (visible_count != 0) {
        const uint32_t fair_budget = mesh_limit * 2u / 3u;
        uint32_t base_quota = fair_budget / visible_count;
        if (base_quota == 0) base_quota = 1;
        g_stats.chunk_base_quota = base_quota;
        /* First retain a representative set from every visible chunk. */
        for (uint8_t chunk_index = 0;
             chunk_index < visible_count && candidate_count < mesh_limit;
             ++chunk_index) {
            uint32_t stop_count = candidate_count + base_quota;
            if (stop_count > mesh_limit) stop_count = mesh_limit;
            append_chunk_candidates(
                &camera, &visible_chunks[chunk_index],
                &visible_chunk_cursors[chunk_index], stop_count, mesh_limit,
                (uint8_t)(hybrid_painter || coverage_mask), &candidate_count);
        }
        /* Spend the remaining third near-to-far for close silhouettes. */
        for (uint8_t chunk_index = 0;
             chunk_index < visible_count && candidate_count < mesh_limit;
             ++chunk_index) {
            append_chunk_candidates(
                &camera, &visible_chunks[chunk_index],
                &visible_chunk_cursors[chunk_index], mesh_limit, mesh_limit,
                (uint8_t)(hybrid_painter || coverage_mask), &candidate_count);
        }
    }
    if (g_stats.mesh_visited_quads < g_stats.mesh_input_quads) {
        g_stats.candidate_budget_omitted =
            g_stats.mesh_input_quads - g_stats.mesh_visited_quads;
        g_stats.dropped_quads += g_stats.candidate_budget_omitted;
    }
    if (g_phase_marker != NULL) g_phase_marker(0u);
    append_entities(&camera, candidate_limit, &candidate_count);
    g_stats.candidate_quads = candidate_count;
    if (coverage_mask) exact_painter = 2u;
    sort_candidates(
        candidate_count,
        depth_terrain || depth_painter || coverage_mask || hybrid_painter ||
            (g_raster_capabilities & PXA_RASTER_CAP_FIXED_ALPHA_BLEND) != 0 ||
            (g_raster_capabilities & PXA_RASTER_CAP_TEXTURED_QUAD) == 0,
        (uint8_t)exact_painter);
    if (g_phase_marker != NULL) g_phase_marker(1u);
    pxa_raster_draw_list_begin(&list, g_draw_list, sizeof(g_draw_list), frame_id);
    /* The clear record also drops the depth buffer, but only when the list
     * declares depth cut-out support. Terrain always depth-tests, so request
     * the reset even on frames without any cut-out face. */
    if (depth_terrain)
        list.required_capabilities |= PXA_RASTER_CAP_DEPTH_CUTOUT;
    {
        /* A submerged camera sees a deep water backdrop instead of sky. */
        const int submerged =
            game_block(rc_floor_int(player->x),
                       rc_floor_int(player->y + EYE_HEIGHT),
                       rc_floor_int(player->z)) == BLOCK_WATER;
        const uint16_t clear = submerged ? VOXEL_RASTER_UNDERWATER_CLEAR
                                         : UINT16_C(0x9e5f);
        /* At balanced/full detail the sky bands cover every pixel before any
         * world geometry. Avoid writing the complete PSRAM framebuffer twice. */
        if (submerged || quality >= QUALITY_PERFORMANCE)
            (void)pxa_raster_clear(&list, clear);
#ifndef VOXEL_SKIP_SKY
#define VOXEL_SKIP_SKY 0
#endif
#if VOXEL_SKIP_SKY == 2
        /* Measurement: one full-screen flat quad instead of the sky bands, to
         * price the Host's axis-aligned fill path on this target. */
        if (!submerged &&
            (g_raster_capabilities & PXA_RASTER_CAP_FLAT_QUAD) != 0)
            append_sky_rect(&list, &camera, 0, 0, camera.width,
                            camera.height, UINT16_C(0x9e5f));
#elif VOXEL_SKIP_SKY == 0
        if (!submerged &&
            (g_raster_capabilities & PXA_RASTER_CAP_FLAT_QUAD) != 0)
            append_sky(&list, &camera, quality < QUALITY_PERFORMANCE);
#endif
    }
    {
        uint8_t list_full = 0;
        const uint8_t fixed_blend =
            (g_raster_capabilities & PXA_RASTER_CAP_FIXED_ALPHA_BLEND) != 0;
        if (coverage_mask) {
            /* Opaque/cutout faces claim a one-bit front-to-back coverage
             * mask. Water first marks pixels that had no nearer opaque face;
             * a second near-to-far pass blends exactly the nearest marked
             * surface after the terrain behind it has been drawn. */
            for (uint8_t pass = 0; pass < 2u && !list_full; ++pass) {
                for (uint32_t order = 0; order < candidate_count; ++order) {
                    const uint32_t draw_order = candidate_count - 1u - order;
                    projected_quad_t *quad =
                        &g_candidates[g_sort_order[draw_order]];
                    const uint8_t textured = quad->textured;
                    const uint8_t target_pass =
                        quad->blend_75 ? 1u : 0u;
                    const uint8_t coverage_mode = pass == 1u ? 2u : 1u;
                    int added;
                    if (pass != target_pass ||
                        (pass == 1u && !quad->blend_75))
                        continue;
                    if (!projected_quad_has_area(quad)) {
                        if (pass != 1u) {
                            ++g_stats.dropped_quads;
                            ++g_stats.degenerate_dropped;
                        }
                        continue;
                    }
                    if (!scene_command_fits(&list)) {
                        const uint32_t omitted = candidate_count - order;
                        g_stats.dropped_quads += omitted;
                        g_stats.command_budget_dropped += omitted;
                        list_full = 1;
                        break;
                    }
                    added = append_projected_quad(
                        &list, quad, 1, (uint8_t)lit_palette_depth,
                        coverage_mode);
                    if (!added) {
                        ++g_stats.dropped_quads;
                        ++g_stats.append_failures;
                        list_full = 1;
                        break;
                    }
                    if (pass == 1u) continue;
                    ++g_stats.submitted_quads;
                    ++g_stats.painter_quads;
                    if (textured) ++g_stats.affine_quads;
                }
            }
        } else {
        const uint8_t passes =
            depth_painter ? (fixed_blend ? 2u : 1u)
            : exact_painter ? 1u
            : hybrid_painter ? (fixed_blend ? 3u : 2u)
                           : (fixed_blend ? 2u : 1u);
        /* Exact painter mode interleaves water and opaque faces in dependency
         * order. The legacy hybrid path keeps water after depth-tested opaque
         * faces because blended fragments do not write depth. */
        for (uint8_t pass = 0; pass < passes && !list_full; ++pass) {
            /* The order list runs far to near: the blend pass needs that
             * order, while the opaque pass draws near to far so depth
             * rejection discards hidden fragments early. */
            const uint8_t near_to_far =
                (depth_terrain || depth_painter) && pass == 0u;
            for (uint32_t step = 0; step < candidate_count; ++step) {
                const uint32_t order =
                    near_to_far ? candidate_count - 1u - step : step;
                projected_quad_t *quad =
                    &g_candidates[g_sort_order[order]];
                const uint8_t painter = depth_painter ||
                    (hybrid_painter && projected_quad_is_painter(quad));
                uint8_t target_pass;
                int added;
                if (depth_painter)
                    target_pass = fixed_blend && quad->blend_75 ? 1u : 0u;
                else if (painter)
                    target_pass = 0u;
                else if (fixed_blend && quad->blend_75)
                    target_pass = hybrid_painter ? 2u : 1u;
                else
                    target_pass = hybrid_painter ? 1u : 0u;
                if (pass != target_pass) continue;
                if (!projected_quad_has_area(quad)) {
                    ++g_stats.dropped_quads;
                    ++g_stats.degenerate_dropped;
                    continue;
                }
                if (!scene_command_fits(&list)) {
                    const uint32_t omitted = candidate_count - order;
                    g_stats.dropped_quads += omitted;
                    g_stats.command_budget_dropped += omitted;
                    list_full = 1;
                    break;
                }
                added = append_projected_quad(
                    &list, quad, painter, (uint8_t)lit_palette_depth,
                    depth_painter ? 3u : 0u);
                if (!added) {
                    ++g_stats.dropped_quads;
                    ++g_stats.append_failures;
                    list_full = 1;
                    break;
                }
                ++g_stats.submitted_quads;
                if (depth_painter)
                    ++g_stats.depth_quads;
                else if (painter)
                    ++g_stats.painter_quads;
                else if ((g_raster_capabilities &
                          PXA_RASTER_CAP_TEXTURED_QUAD) != 0)
                    ++g_stats.depth_quads;
                if (!painter && quad->textured && quad->affine &&
                    (g_raster_capabilities & PXA_RASTER_CAP_AFFINE_UV) != 0)
                    ++g_stats.affine_quads;
            }
        }
        }
    }
    /* Underwater: a two-sided water volume alone still looks like clear air.
     * Add a fullscreen additive blue tint when the eye is inside water so the
     * submersion reads like the ray caster's water fill. */
    if ((g_raster_capabilities & PXA_RASTER_CAP_ADDITIVE_SPRITE) != 0 &&
        game_block(rc_floor_int(player->x),
                   rc_floor_int(player->y + EYE_HEIGHT),
                   rc_floor_int(player->z)) == BLOCK_WATER) {
        (void)pxa_raster_sprite(
            &list, 0,
            PXA_RASTER_SPRITE_SOLID_COLOR | PXA_RASTER_SPRITE_ADDITIVE,
            g_raster_capabilities, 0, 0, camera.width, camera.height, 0, 0, 1,
            1, VOXEL_RASTER_UNDERWATER_TINT);
    }
    if ((g_raster_capabilities & PXA_RASTER_CAP_FLAT_QUAD) != 0 &&
        menu == NULL)
        append_block_outline(&list, &camera, target);
    if (menu != NULL)
        append_menu(&list, menu, camera.width, camera.height);
    else
        append_hud(&list, hud, camera.width, camera.height);
    g_stats.draw_commands = list.command_count;
    g_stats.draw_list_bytes = list.length;
    g_stats.covered_pixel_budget = (uint32_t)camera.width * camera.height;
    if (g_phase_marker != NULL) g_phase_marker(2u);
    {
        const int32_t result = submit_list(surface_handle, &list);
        if (g_phase_marker != NULL) g_phase_marker(3u);
        return result;
    }
}

void voxel_raster_set_phase_marker(void (*marker)(uint8_t phase)) {
    g_phase_marker = marker;
}

void voxel_raster_get_stats(voxel_raster_stats_t *stats) {
    if (stats != NULL) *stats = g_stats;
}
