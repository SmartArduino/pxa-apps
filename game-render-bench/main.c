#include <stdint.h>

#include "pxa.h"
#include "pxa_game_render.h"
#include "pxa_log.h"
#include "pxa_raster.h"

#define WIDTH 296
#define HEIGHT 240
#ifndef PXA_GAME_RENDER_SCALE_SHIFT
#define PXA_GAME_RENDER_SCALE_SHIFT 1
#endif
#if PXA_GAME_RENDER_SCALE_SHIFT < 0 || PXA_GAME_RENDER_SCALE_SHIFT > 3
#error "PXA_GAME_RENDER_SCALE_SHIFT must be between 0 and 3"
#endif
#define RENDER_WIDTH (WIDTH >> PXA_GAME_RENDER_SCALE_SHIFT)
#define RENDER_HEIGHT (HEIGHT >> PXA_GAME_RENDER_SCALE_SHIFT)
#define CREATE_REQUEST UINT32_C(1)
#define FRAME_PERIOD_MS 16u
#define SPRITE_COUNT 192u
#define QUAD_COUNT 96u
#define TEXTURED_QUAD_COUNT 24u
#define MAX_CUBE_COUNT 12u
#define TRIANGLES_PER_GROUP (MAX_CUBE_COUNT * 4u)
#define VERTICES_PER_GROUP (TRIANGLES_PER_GROUP * 3u)
#define TRIANGLE_GROUPS 2u
#define FONT_SLOT 1u
#define FONT_GLYPHS 42u
#define FONT_WIDTH (FONT_GLYPHS * 4u)
#define HUD_SCALE 2
#define RESULT_TEXT_LENGTH 18u
#define BENCH_GROUPS 11u
#define MICRO_BENCH_DURATION_US UINT64_C(4000000)
#define GAME_BENCH_DURATION_US UINT64_C(6000000)
#define TELEMETRY_TICK_PERIOD 12u
#define REQUIRED_CAPABILITIES                                           \
    (PXA_RASTER_CAP_FLAT_QUAD | PXA_RASTER_CAP_TEXTURED_QUAD |         \
     PXA_RASTER_CAP_ADDITIVE_SPRITE | PXA_RASTER_CAP_SPRITE_BATCH |    \
     PXA_RASTER_CAP_TRIANGLE_BATCH)

static uint8_t g_packet[64];
static uint8_t g_upload[PXA_RASTER_UPLOAD_HEADER_BYTES + FONT_WIDTH * 5u];
static uint8_t g_draw[PXA_RASTER_MAX_DRAW_BYTES];
static uint8_t g_texture[16 * 16];
static uint8_t g_font[FONT_WIDTH * 5u];
static uint16_t g_palette[256];
static pxa_raster_sprite_instance_t g_sprites[SPRITE_COUNT];
static pxa_raster_vertex_t g_triangles[TRIANGLE_GROUPS][VERTICES_PER_GROUP];
static uint16_t g_triangle_counts[TRIANGLE_GROUPS];
static uint32_t g_context;
static uint32_t g_capabilities;
static uint32_t g_tick;
static uint32_t g_group_tick;
static uint64_t g_frame_id;
static uint8_t g_group;
static uint8_t g_finished;
static uint8_t g_have_group_baseline;
static uint64_t g_group_started_us;
static uint64_t g_group_visible_start;
static uint64_t g_group_rendered_start;
static uint64_t g_group_raster_start_us;

typedef enum {
    BENCH_CLEAR,
    BENCH_SPRITES_NATIVE,
    BENCH_SPRITES_SCALED,
    BENCH_SPRITES_ADDITIVE,
    BENCH_QUADS,
    BENCH_TEXTURED_QUADS,
    BENCH_GAME_2D,
    BENCH_CUBES_3,
    BENCH_CUBES_6,
    BENCH_CUBES_12,
    BENCH_GAME_3D,
} bench_kind_t;

typedef struct {
    const char *label;
    bench_kind_t kind;
    uint64_t duration_us;
} bench_group_t;

typedef struct {
    uint32_t visible_fps;
    uint32_t rendered_fps;
    uint32_t raster_ms;
} bench_result_t;

static const bench_group_t kBenchGroups[BENCH_GROUPS] = {
    {"2D-CL", BENCH_CLEAR, MICRO_BENCH_DURATION_US},
    {"2D-SN", BENCH_SPRITES_NATIVE, MICRO_BENCH_DURATION_US},
    {"2D-SS", BENCH_SPRITES_SCALED, MICRO_BENCH_DURATION_US},
    {"2D-AD", BENCH_SPRITES_ADDITIVE, MICRO_BENCH_DURATION_US},
    {"2D-QD", BENCH_QUADS, MICRO_BENCH_DURATION_US},
    {"2D-TX", BENCH_TEXTURED_QUADS, MICRO_BENCH_DURATION_US},
    {"GM-2D", BENCH_GAME_2D, GAME_BENCH_DURATION_US},
    {"3D-03", BENCH_CUBES_3, MICRO_BENCH_DURATION_US},
    {"3D-06", BENCH_CUBES_6, MICRO_BENCH_DURATION_US},
    {"3D-12", BENCH_CUBES_12, MICRO_BENCH_DURATION_US},
    {"GM-3D", BENCH_GAME_3D, GAME_BENCH_DURATION_US},
};
static bench_result_t g_results[BENCH_GROUPS];

static const int16_t kSinQ10[16] = {
    0, 392, 724, 946, 1024, 946, 724, 392,
    0, -392, -724, -946, -1024, -946, -724, -392,
};

static const char kFontCharacters[] =
    "0123456789.:-/+ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
static const uint8_t kFontRows[FONT_GLYPHS][5] = {
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

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return (uint16_t)(((uint16_t)(red >> 3) << 11) |
                      ((uint16_t)(green >> 2) << 5) | (blue >> 3));
}

static void prepare_resources(void) {
    uint32_t index;
    for (index = 0; index < 256; ++index) {
        const uint8_t red = (uint8_t)((index * 37u) & 255u);
        const uint8_t green = (uint8_t)((index * 73u) & 255u);
        const uint8_t blue = (uint8_t)((index * 19u) & 255u);
        g_palette[index] = rgb565(red, green, blue);
    }
    g_palette[0] = 0;
    for (index = 0; index < sizeof(g_texture); ++index) {
        const uint32_t x = index & 15u;
        const uint32_t y = index >> 4;
        g_texture[index] = ((x + y * 3u) & 7u) == 0
                               ? 0
                               : (uint8_t)(1u +
                                           ((x >> 2) + (y >> 2) * 4u) * 11u);
    }
    for (index = 0; index < FONT_GLYPHS; ++index) {
        uint8_t row;
        for (row = 0; row < 5; ++row) {
            uint8_t column;
            for (column = 0; column < 3; ++column) {
                if ((kFontRows[index][row] & (4u >> column)) != 0)
                    g_font[(size_t)row * FONT_WIDTH + index * 4u + column] = 255;
            }
        }
    }
}

static int upload_resources(void) {
    return pxa_raster_upload_palette_rgb565(
               g_context, g_palette, g_upload,
               PXA_RASTER_UPLOAD_HEADER_BYTES + 512u) ==
               (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + 512u) &&
           pxa_raster_upload_texture_index8(
               g_context, 0, 16, 16, g_texture, g_upload,
               PXA_RASTER_UPLOAD_HEADER_BYTES + sizeof(g_texture)) ==
               (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + sizeof(g_texture)) &&
           pxa_raster_upload_texture_index8(
               g_context, FONT_SLOT, FONT_WIDTH, 5, g_font, g_upload,
               sizeof(g_upload)) == (int32_t)sizeof(g_upload);
}

static void append_rect(pxa_raster_draw_list_t *list, int x, int y,
                        int width, int height, uint16_t color) {
    int16_t xy[8];
    xy[0] = xy[6] = (int16_t)(x << 4);
    xy[1] = xy[3] = (int16_t)(y << 4);
    xy[2] = xy[4] = (int16_t)((x + width) << 4);
    xy[5] = xy[7] = (int16_t)((y + height) << 4);
    (void)pxa_raster_flat_quad(list, xy, color);
}

static int font_index(char character) {
    uint8_t index;
    for (index = 0; index < FONT_GLYPHS; ++index)
        if (kFontCharacters[index] == character) return index;
    return FONT_GLYPHS - 1u;
}

static void write_decimal(char *output, uint32_t value, uint8_t digits) {
    while (digits != 0) {
        output[--digits] = (char)('0' + value % 10u);
        value /= 10u;
    }
}

static void append_text(pxa_raster_draw_list_t *list, int x, int y,
                        const char *text, uint16_t color) {
    while (*text != '\0') {
        const int glyph = font_index(*text++);
        (void)pxa_raster_sprite(
            list, FONT_SLOT,
            PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 |
                PXA_RASTER_SPRITE_SOLID_COLOR,
            g_capabilities, (int16_t)x, (int16_t)y, 3u * HUD_SCALE,
            5u * HUD_SCALE, (uint16_t)(glyph * 4), 0, 3, 5, color);
        x += 4 * HUD_SCALE;
    }
}

static void set_sprite(pxa_raster_sprite_instance_t *sprite, int x, int y,
                       uint16_t width, uint16_t height) {
    sprite->x = (int16_t)x;
    sprite->y = (int16_t)y;
    sprite->width = width;
    sprite->height = height;
    sprite->source_x = 0;
    sprite->source_y = 0;
    sprite->source_width = 16;
    sprite->source_height = 16;
}

static int render_sprites(pxa_raster_draw_list_t *list, uint8_t flags,
                          uint8_t scaled) {
    uint32_t index;
    for (index = 0; index < SPRITE_COUNT; ++index) {
        const uint32_t column = index % 16u;
        const uint32_t row = index / 16u;
        const int wave = (int)((g_tick * (1u + row % 3u) + index * 7u) % 22u);
        pxa_raster_sprite_instance_t *sprite = &g_sprites[index];
        const uint16_t size = scaled ? (uint16_t)(10u + index % 11u) : 16u;
        sprite->x = (int16_t)(column * 18u + (row & 1u) * 8u - 2u);
        sprite->y = (int16_t)(18u + row * 18u +
                              (wave < 11 ? wave : 21 - wave) - 5);
        sprite->width = size;
        sprite->height = size;
        sprite->source_x = 0;
        sprite->source_y = 0;
        sprite->source_width = 16;
        sprite->source_height = 16;
    }
    return pxa_raster_sprite_batch(
        list, 0, flags, g_capabilities, g_sprites, SPRITE_COUNT, 0);
}

static int append_sprite_group(pxa_raster_draw_list_t *list, uint16_t count,
                               uint8_t flags, uint16_t color) {
    return pxa_raster_sprite_batch(list, 0, flags, g_capabilities, g_sprites,
                                   count, color);
}

static int render_game_2d(pxa_raster_draw_list_t *list) {
    const uint8_t solid_flags =
        PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 | PXA_RASTER_SPRITE_SOLID_COLOR;
    const uint8_t additive_flags = solid_flags | PXA_RASTER_SPRITE_ADDITIVE;
    uint32_t index;

    append_rect(list, 0, 105, WIDTH, 30, rgb565(18, 39, 46));
    for (index = 0; index < 10; ++index)
        append_rect(list, (int)index * 32 - (int)(g_group_tick % 32u), 119,
                    22, 2, rgb565(35, 70, 75));

    for (index = 0; index < 32; ++index) {
        const uint32_t hit_tick = 12u + index * 9u;
        const int x = 76 + (int)(index % 8u) * 18;
        const int y = 31 + (int)(index / 8u) * 45;
        if (g_group_tick < hit_tick) {
            append_rect(list, x, y, 13, 19,
                        rgb565(88, (uint8_t)(108u + index % 3u * 18u), 102));
        } else if (g_group_tick - hit_tick < 46u) {
            const uint32_t age = g_group_tick - hit_tick;
            append_rect(list, x - (int)(age / 7u),
                        y + 10 + (int)(age / 5u), 5, 5,
                        rgb565(125, 142, 118));
            append_rect(list, x + 8 + (int)(age / 8u),
                        y + 5 + (int)(age / 4u), 4, 4,
                        rgb565(79, 96, 82));
        }
    }

    for (index = 0; index < 6; ++index) {
        const int lane = 25 + (int)index * 34;
        const int wave = kSinQ10[(g_group_tick / 3u + index * 2u) & 15u] / 160;
        set_sprite(&g_sprites[index], 18 + wave, lane, 18, 18);
        append_rect(list, 15, lane - 5, 25, 3, rgb565(28, 52, 55));
        append_rect(list, 15, lane - 5,
                    8 + (int)((index * 5u + g_group_tick / 12u) % 18u), 3,
                    rgb565(67, 235, 153));
    }
    if (!append_sprite_group(list, 6, solid_flags, rgb565(54, 196, 255)))
        return 0;

    for (index = 0; index < 6; ++index) {
        const int lane = 25 + (int)index * 34;
        const int wave = kSinQ10[(g_group_tick / 3u + index * 2u + 8u) & 15u] /
                         160;
        set_sprite(&g_sprites[index], 260 + wave, lane, 18, 18);
        append_rect(list, 257, lane - 5, 25, 3, rgb565(55, 36, 39));
        append_rect(list, 257, lane - 5,
                    8 + (int)((index * 7u + g_group_tick / 10u) % 18u), 3,
                    rgb565(255, 102, 91));
    }
    if (!append_sprite_group(list, 6, solid_flags, rgb565(255, 78, 91)))
        return 0;

    for (index = 0; index < 24; ++index) {
        const int x = 38 + (int)((g_group_tick * (3u + index % 3u) +
                                  index * 23u) % 224u);
        const int y = 30 + (int)(index % 6u) * 34;
        set_sprite(&g_sprites[index], x, y, 7, 4);
    }
    if (!append_sprite_group(list, 24, solid_flags, rgb565(92, 224, 255)))
        return 0;
    for (index = 0; index < 24; ++index) {
        const int x = 251 - (int)((g_group_tick * (2u + index % 4u) +
                                   index * 19u) % 218u);
        const int y = 30 + (int)(index % 6u) * 34;
        set_sprite(&g_sprites[index], x, y, 7, 4);
    }
    if (!append_sprite_group(list, 24, solid_flags, rgb565(255, 194, 75)))
        return 0;

    for (index = 0; index < 12; ++index) {
        const uint32_t pulse = (g_group_tick + index * 11u) % 36u;
        const uint16_t size = (uint16_t)(5u + (pulse < 18u ? pulse : 35u - pulse));
        set_sprite(&g_sprites[index], 65 + (int)(index * 37u % 170u),
                   24 + (int)(index * 29u % 185u), size, size);
    }
    return append_sprite_group(list, 12, additive_flags, rgb565(255, 214, 78));
}

static int render_quads(pxa_raster_draw_list_t *list) {
    uint32_t index;
    for (index = 0; index < QUAD_COUNT; ++index) {
        const uint32_t column = index % 12u;
        const uint32_t row = index / 12u;
        const int wave =
            (int)((g_tick * (1u + row % 4u) + index * 5u) % 20u);
        const int x = (int)(column * 25u) + (wave < 10 ? wave : 19 - wave);
        const int y = 24 + (int)(row * 27u);
        append_rect(list, x, y, 19, 20,
                    rgb565((uint8_t)(35u + column * 14u),
                           (uint8_t)(80u + row * 18u),
                           (uint8_t)(220u - row * 16u)));
    }
    return list->status == PXA_STATUS_OK;
}

static int render_textured_quads(pxa_raster_draw_list_t *list) {
    uint32_t index;
    for (index = 0; index < TEXTURED_QUAD_COUNT; ++index) {
        const uint32_t column = index % 6u;
        const uint32_t row = index / 6u;
        const int phase = (int)((g_tick / 2u + index * 3u) & 15u);
        const int offset = kSinQ10[phase] / 256;
        const int x = 5 + (int)column * 49 + offset;
        const int y = 20 + (int)row * 53 - offset;
        const uint16_t depth = (uint16_t)(320u + row * 48u);
        pxa_raster_vertex_t vertices[4] = {
            {.x_q4 = (int16_t)(x * 16),
             .y_q4 = (int16_t)(y * 16),
             .u_q4 = 0,
             .v_q4 = 0,
             .light = 255,
             .depth_q8 = depth},
            {.x_q4 = (int16_t)((x + 42) * 16),
             .y_q4 = (int16_t)((y + 2) * 16),
             .u_q4 = 15 * 16,
             .v_q4 = 0,
             .light = 230,
             .depth_q8 = (uint16_t)(depth + 64u)},
            {.x_q4 = (int16_t)((x + 40) * 16),
             .y_q4 = (int16_t)((y + 44) * 16),
             .u_q4 = 15 * 16,
             .v_q4 = 15 * 16,
             .light = 205,
             .depth_q8 = (uint16_t)(depth + 96u)},
            {.x_q4 = (int16_t)((x - 2) * 16),
             .y_q4 = (int16_t)((y + 42) * 16),
             .u_q4 = 0,
             .v_q4 = 15 * 16,
             .light = 230,
             .depth_q8 = (uint16_t)(depth + 32u)},
        };
        if (!pxa_raster_textured_quad(list, vertices, 0)) return 0;
    }
    return 1;
}

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} point3_t;

static void project_vertex(pxa_raster_vertex_t *out, point3_t point,
                           int center_x, int center_y, uint8_t light) {
    int32_t depth = point.z + 640;
    int32_t x = center_x + (int32_t)point.x * 400 / depth;
    int32_t y = center_y - (int32_t)point.y * 400 / depth;
    if (x < -512) x = -512;
    if (x > 511) x = 511;
    if (y < -512) y = -512;
    if (y > 511) y = 511;
    out->x_q4 = (int16_t)(x << 4);
    out->y_q4 = (int16_t)(y << 4);
    out->u_q4 = 0;
    out->v_q4 = 0;
    out->light = light;
    out->depth_q8 = (uint16_t)depth;
}

static int face_visible(const pxa_raster_vertex_t vertices[4]) {
    const int64_t ax = vertices[1].x_q4 - vertices[0].x_q4;
    const int64_t ay = vertices[1].y_q4 - vertices[0].y_q4;
    const int64_t bx = vertices[2].x_q4 - vertices[0].x_q4;
    const int64_t by = vertices[2].y_q4 - vertices[0].y_q4;
    return ax * by - ay * bx < 0;
}

static void append_cube_at(uint8_t phase, int center_x, int center_y,
                           int scale, int depth_offset) {
    static const int8_t corners[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
    };
    static const uint8_t faces[4][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2},
    };
    point3_t points[8];
    const int32_t sine = kSinQ10[phase & 15u];
    const int32_t cosine = kSinQ10[(phase + 4u) & 15u];
    uint8_t corner;
    uint8_t face;
    for (corner = 0; corner < 8; ++corner) {
        const int32_t x = corners[corner][0] * scale;
        const int32_t y = corners[corner][1] * scale;
        const int32_t z = corners[corner][2] * scale;
        points[corner].x = (int16_t)((x * cosine + z * sine) / 1024);
        points[corner].y = (int16_t)y;
        points[corner].z =
            (int16_t)((z * cosine - x * sine) / 1024 + depth_offset);
    }
    for (face = 0; face < 4; ++face) {
        const uint8_t group = face >> 1;
        pxa_raster_vertex_t projected[4];
        pxa_raster_vertex_t *vertices;
        const uint8_t *quad = faces[face];
        project_vertex(&projected[0], points[quad[0]], center_x, center_y,
                       (uint8_t)(245u - group * 45u));
        project_vertex(&projected[1], points[quad[1]], center_x, center_y,
                       (uint8_t)(245u - group * 45u));
        project_vertex(&projected[2], points[quad[2]], center_x, center_y,
                       (uint8_t)(245u - group * 45u));
        project_vertex(&projected[3], points[quad[3]], center_x, center_y,
                       (uint8_t)(245u - group * 45u));
        if (!face_visible(projected)) continue;
        vertices = &g_triangles[group][g_triangle_counts[group] * 3u];
        vertices[0] = projected[0];
        vertices[1] = projected[1];
        vertices[2] = projected[2];
        vertices[3] = projected[0];
        vertices[4] = projected[2];
        vertices[5] = projected[3];
        g_triangle_counts[group] += 2u;
    }
}

static int render_3d(pxa_raster_draw_list_t *list, uint32_t cube_count) {
    static const uint16_t colors[TRIANGLE_GROUPS] = {
        UINT16_C(0x35bf), UINT16_C(0xfd08)};
    uint32_t cube;
    uint8_t group;
    for (group = 0; group < TRIANGLE_GROUPS; ++group)
        g_triangle_counts[group] = 0;
    for (cube = 0; cube < cube_count; ++cube) {
        append_cube_at((uint8_t)(g_tick / 3u + cube),
                       38 + (int)(cube % 4u) * 73,
                       48 + (int)(cube / 4u) * 72, 52,
                       (int)(cube % 3u) * 22);
    }
    for (group = 0; group < TRIANGLE_GROUPS; ++group) {
        if (g_triangle_counts[group] == 0) continue;
        if (!pxa_raster_triangle_batch(
                list, g_triangles[group], g_triangle_counts[group], 0, 1,
                colors[group]))
            return 0;
    }
    return 1;
}

static void set_depth_vertex(pxa_raster_vertex_t *vertex, int x, int y,
                             int u, int v, uint8_t light, uint16_t depth) {
    vertex->x_q4 = (int16_t)(x << 4);
    vertex->y_q4 = (int16_t)(y << 4);
    vertex->u_q4 = (int16_t)(u << 4);
    vertex->v_q4 = (int16_t)(v << 4);
    vertex->light = light;
    vertex->depth_q8 = depth;
}

static int render_game_3d(pxa_raster_draw_list_t *list) {
    static const uint16_t colors[TRIANGLE_GROUPS] = {
        UINT16_C(0x35bf), UINT16_C(0xfd08)};
    const uint8_t shot_flags = PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 |
                               PXA_RASTER_SPRITE_SOLID_COLOR |
                               PXA_RASTER_SPRITE_ADDITIVE;
    const uint32_t destroyed = g_group_tick / 72u > 4u ? 4u : g_group_tick / 72u;
    uint32_t index;
    uint8_t group;

    for (index = 0; index < 7; ++index) {
        const int y0 = 92 + (int)index * 20;
        const int y1 = y0 + 23;
        const int inset0 = (int)(6u - index) * 14;
        const int inset1 = (int)(5u - (index < 6u ? index : 5u)) * 14;
        pxa_raster_vertex_t floor_quad[4];
        set_depth_vertex(&floor_quad[0], inset0, y0, 0, 0, 130,
                         (uint16_t)(820u - index * 70u));
        set_depth_vertex(&floor_quad[1], WIDTH - inset0, y0, 15, 0, 155,
                         (uint16_t)(820u - index * 70u));
        set_depth_vertex(&floor_quad[2], WIDTH - inset1, y1, 15, 15, 210,
                         (uint16_t)(760u - index * 70u));
        set_depth_vertex(&floor_quad[3], inset1, y1, 0, 15, 180,
                         (uint16_t)(760u - index * 70u));
        if (!pxa_raster_textured_quad(list, floor_quad, 0)) return 0;
    }

    for (group = 0; group < TRIANGLE_GROUPS; ++group)
        g_triangle_counts[group] = 0;
    for (index = 0; index < 8; ++index) {
        const int row = (int)(index / 4u);
        const int wave = kSinQ10[(g_group_tick / 4u + index * 3u) & 15u] / 90;
        append_cube_at((uint8_t)(g_group_tick / 4u + index),
                       48 + (int)(index % 4u) * 66 + wave,
                       76 + row * 83, 35 - row * 4, 40 + row * 130);
    }
    for (index = destroyed; index < 4; ++index) {
        append_cube_at((uint8_t)(index * 2u),
                       112 + (int)(index % 2u) * 72,
                       104 + (int)(index / 2u) * 62, 29, 115);
    }
    for (group = 0; group < TRIANGLE_GROUPS; ++group) {
        if (g_triangle_counts[group] != 0 &&
            !pxa_raster_triangle_batch(list, g_triangles[group],
                                       g_triangle_counts[group], 0, 1,
                                       colors[group]))
            return 0;
    }

    for (index = 0; index < destroyed * 2u; ++index) {
        const uint32_t age = (g_group_tick - (index / 2u + 1u) * 72u) % 80u;
        const int x = 102 + (int)(index % 4u) * 27 +
                      (index & 1u ? (int)(age / 5u) : -(int)(age / 6u));
        const int y = 112 + (int)(index / 4u) * 56 + (int)(age / 3u);
        pxa_raster_vertex_t fragment[4];
        set_depth_vertex(&fragment[0], x, y, 0, 0, 220, 610);
        set_depth_vertex(&fragment[1], x + 8, y + 2, 0, 0, 210, 612);
        set_depth_vertex(&fragment[2], x + 7, y + 9, 0, 0, 190, 608);
        set_depth_vertex(&fragment[3], x - 1, y + 7, 0, 0, 200, 606);
        if (!pxa_raster_solid_depth_quad(list, fragment,
                                         rgb565(226, 136, 68)))
            return 0;
    }

    for (index = 0; index < 24; ++index) {
        const int x = 35 + (int)((g_group_tick * (2u + index % 3u) +
                                  index * 29u) % 228u);
        const int y = 52 + (int)(index % 6u) * 25;
        const uint16_t size = (uint16_t)(4u + (index & 3u));
        set_sprite(&g_sprites[index], x, y, size, size);
    }
    return append_sprite_group(list, 24, shot_flags, rgb565(255, 220, 82));
}

static void append_results(pxa_raster_draw_list_t *list) {
    char line[RESULT_TEXT_LENGTH + 1] = "01 2D-CL 00/00/000";
    const int header_width = 13 * 4 * HUD_SCALE;
    uint8_t index;
    append_text(list, (WIDTH - header_width) / 2, 5, "RESULT V/R/MS",
                rgb565(245, 245, 250));
    for (index = 0; index < BENCH_GROUPS; ++index) {
        uint32_t visible_fps = g_results[index].visible_fps;
        uint32_t rendered_fps = g_results[index].rendered_fps;
        uint32_t raster_ms = g_results[index].raster_ms;
        if (visible_fps > 99u) visible_fps = 99u;
        if (rendered_fps > 99u) rendered_fps = 99u;
        if (raster_ms > 999u) raster_ms = 999u;
        write_decimal(line, index + 1u, 2);
        line[3] = kBenchGroups[index].label[0];
        line[4] = kBenchGroups[index].label[1];
        line[5] = kBenchGroups[index].label[2];
        line[6] = kBenchGroups[index].label[3];
        line[7] = kBenchGroups[index].label[4];
        write_decimal(line + 9, visible_fps, 2);
        write_decimal(line + 12, rendered_fps, 2);
        write_decimal(line + 15, raster_ms, 3);
        append_text(list,
                    (WIDTH - RESULT_TEXT_LENGTH * 4 * HUD_SCALE) / 2,
                    24 + index * 19, line,
                    index < 7 ? rgb565(80, 230, 235)
                              : rgb565(255, 185, 70));
    }
}

static int render_frame(void) {
    pxa_raster_draw_list_t list;
    const bench_group_t *group;
    pxa_raster_draw_list_begin_scaled(
        &list, g_draw, sizeof(g_draw), ++g_frame_id,
        PXA_GAME_RENDER_SCALE_SHIFT);
    if (g_finished) {
        (void)pxa_raster_clear(&list, rgb565(8, 12, 20));
        append_results(&list);
        return pxa_raster_submit(g_context, &list) > 0;
    }
    group = &kBenchGroups[g_group];
    (void)pxa_raster_clear(
        &list, group->kind <= BENCH_GAME_2D ? rgb565(7, 18, 27)
                                            : rgb565(18, 12, 20));
    if (group->kind == BENCH_SPRITES_NATIVE &&
        !render_sprites(&list, 0, 0))
        return 0;
    if (group->kind == BENCH_SPRITES_SCALED &&
        !render_sprites(&list, 0, 1))
        return 0;
    if (group->kind == BENCH_SPRITES_ADDITIVE &&
        !render_sprites(&list, PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 |
                                   PXA_RASTER_SPRITE_ADDITIVE,
                        0))
        return 0;
    if (group->kind == BENCH_QUADS && !render_quads(&list)) return 0;
    if (group->kind == BENCH_TEXTURED_QUADS &&
        !render_textured_quads(&list))
        return 0;
    if (group->kind == BENCH_GAME_2D && !render_game_2d(&list)) return 0;
    if (group->kind == BENCH_CUBES_3 && !render_3d(&list, 3)) return 0;
    if (group->kind == BENCH_CUBES_6 && !render_3d(&list, 6)) return 0;
    if (group->kind == BENCH_CUBES_12 && !render_3d(&list, 12)) return 0;
    if (group->kind == BENCH_GAME_3D && !render_game_3d(&list)) return 0;
    return pxa_raster_submit(g_context, &list) > 0;
}

static void log_result(uint8_t index) {
    char line[29] = "BENCH 01 2D-CL V00 R00 MS000";
    uint32_t visible_fps = g_results[index].visible_fps;
    uint32_t rendered_fps = g_results[index].rendered_fps;
    uint32_t raster_ms = g_results[index].raster_ms;
    if (visible_fps > 99u) visible_fps = 99u;
    if (rendered_fps > 99u) rendered_fps = 99u;
    if (raster_ms > 999u) raster_ms = 999u;
    write_decimal(line + 6, index + 1u, 2);
    line[9] = kBenchGroups[index].label[0];
    line[10] = kBenchGroups[index].label[1];
    line[11] = kBenchGroups[index].label[2];
    line[12] = kBenchGroups[index].label[3];
    line[13] = kBenchGroups[index].label[4];
    write_decimal(line + 16, visible_fps, 2);
    write_decimal(line + 20, rendered_fps, 2);
    write_decimal(line + 25, raster_ms, 3);
    if (raster_ms >= 50u)
        (void)pxa_log_warn(line);
    else
        (void)pxa_log_info(line);
}

static void finish_group(uint64_t timestamp_us,
                         const pxa_raster_telemetry_t *telemetry) {
    const uint64_t elapsed_us = timestamp_us - g_group_started_us;
    const uint64_t visible_frames =
        telemetry->visible_frames - g_group_visible_start;
    const uint64_t rendered_frames =
        telemetry->rendered_frames - g_group_rendered_start;
    const uint64_t raster_us = telemetry->host_raster_us -
                               g_group_raster_start_us;
    g_results[g_group].visible_fps =
        (uint32_t)(visible_frames * UINT64_C(1000000) / elapsed_us);
    g_results[g_group].rendered_fps =
        (uint32_t)(rendered_frames * UINT64_C(1000000) / elapsed_us);
    g_results[g_group].raster_ms = rendered_frames == 0
                                       ? 0
                                       : (uint32_t)((raster_us / rendered_frames +
                                                    UINT64_C(500)) /
                                                   UINT64_C(1000));
    log_result(g_group);
    ++g_group;
    g_group_tick = 0;
    g_have_group_baseline = 0;
    if (g_group == BENCH_GROUPS) {
        g_finished = 1;
        (void)pxa_log_info("GameRender benchmark complete");
    }
}

static void sample_telemetry(uint64_t timestamp_us) {
    pxa_raster_telemetry_t telemetry;
    if (pxa_raster_query_telemetry(g_context, &telemetry) !=
        (int32_t)PXA_RASTER_TELEMETRY_BYTES)
        return;
    if (g_finished) return;
    if (!g_have_group_baseline) {
        g_group_started_us = timestamp_us;
        g_group_visible_start = telemetry.visible_frames;
        g_group_rendered_start = telemetry.rendered_frames;
        g_group_raster_start_us = telemetry.host_raster_us;
        g_have_group_baseline = 1;
    } else if (timestamp_us - g_group_started_us >=
               kBenchGroups[g_group].duration_us) {
        finish_group(timestamp_us, &telemetry);
    }
}

int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
    (void)config;
    (void)length;
    prepare_resources();
    g_tick = 0;
    g_group_tick = 0;
    g_frame_id = 0;
    g_group = 0;
    g_finished = 0;
    g_have_group_baseline = 0;
    pxa_raster_zero_bytes(g_results, sizeof(g_results));
    if (!pxa_game_render_create(CREATE_REQUEST, RENDER_WIDTH, RENDER_HEIGHT,
                                3, 1,
                                g_packet, sizeof(g_packet))) {
        (void)pxa_log_error("GameRender create request failed");
        return PXA_STATUS_INTERNAL;
    }
#if PXA_GAME_RENDER_SCALE_SHIFT == 0
    (void)pxa_log_info("GameRender benchmark start: 11 groups, native");
#elif PXA_GAME_RENDER_SCALE_SHIFT == 1
    (void)pxa_log_info("GameRender benchmark start: 11 groups, 2x upscale");
#else
    (void)pxa_log_info("GameRender benchmark start: 11 groups, scaled");
#endif
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t length) {
    pxa_event_t event;
    uint64_t timestamp_us;
    if (!pxa_parse_event(bytes, length, &event)) return PXA_EVENT_UNHANDLED;
    if (event.request_id == CREATE_REQUEST) {
        pxa_game_render_create_result_t created;
        if (!pxa_game_render_parse_create(&event, &created))
            return PXA_EVENT_UNHANDLED;
        if (created.status != PXA_STATUS_OK) {
            (void)pxa_log_error("GameRender context creation failed");
            return PXA_EVENT_HANDLED;
        }
        if ((created.capabilities & REQUIRED_CAPABILITIES) !=
            REQUIRED_CAPABILITIES) {
            (void)pxa_log_error("GameRender capabilities missing");
            return PXA_EVENT_HANDLED;
        }
        g_context = created.context_handle;
        g_capabilities = created.capabilities;
        if (!upload_resources()) {
            (void)pxa_log_error("GameRender resource upload failed");
            return PXA_EVENT_HANDLED;
        }
        (void)render_frame();
        (void)pxa_clock_set_period(FRAME_PERIOD_MS);
        return PXA_EVENT_HANDLED;
    }
    if (!pxa_clock_tick_timestamp_us(&event, &timestamp_us) || g_context == 0)
        return PXA_EVENT_UNHANDLED;
    ++g_tick;
    ++g_group_tick;
    if ((g_tick % TELEMETRY_TICK_PERIOD) == 0)
        sample_telemetry(timestamp_us);
    (void)render_frame();
    return PXA_EVENT_HANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
    if (g_context != 0) (void)pxa_close_handle(g_context);
    g_context = 0;
}
