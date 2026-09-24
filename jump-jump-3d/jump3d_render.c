#include "jump3d_render.h"

#include "jump3d_font.h"
#include "jump3d_math.h"
#include "jump3d_palette.h"

/* The original renders the scene with an OrthographicCamera at (-17, 30, 26)
 * looking at (13, 0, -4). Its screen basis is constant and +X travels to the
 * upper right while -Z travels to the upper left, so Jump Jump projects with a
 * plain dot product instead of a view matrix. */
#define J3_SEGMENTS 12
#define J3_HEAD_SEGMENTS 10
#define J3_LIGHT_TOP 31.0F
#define J3_LIGHT_LEFT 25.0F
#define J3_LIGHT_FRONT 23.0F
#define J3_LIGHT_INNER 27.0F
/* Unit vector from the scene towards the OrthographicCamera at (-17,30,26)
 * looking at (13,0,-4). A face is visible when its outward normal points along
 * it, which is the test every culled primitive below uses. */
#define J3_VIEW_X (-0.57735027F)
#define J3_VIEW_Y (0.57735027F)
#define J3_VIEW_Z (0.57735027F)

#if J3_SKIP_PROBE
/* Bitmask of renderer categories to omit; cycled by main.c, see the header. */
uint32_t j3_skip_probe_mask;
#endif

typedef struct {
    float x;
    float y;
    float light;
} j3_point_t;

static void draw_shadow_quad(j3_render_t *render, float x, float y, float z,
                             float scale);
static void draw_rect_on_top(j3_render_t *render, const j3_block_t *block,
                             float top, float offset_x, float offset_z,
                             float half_x, float half_z, uint8_t color,
                             float light);

/* ---------------------------------------------------------------- primitives */

static void store_vertex(pxa_raster_vertex_t *vertex, float x, float y,
                         float light) {
    x = j3_clamp(x, -2032.0F, 2032.0F);
    y = j3_clamp(y, -2032.0F, 2032.0F);
    vertex->x_q4 = (int16_t)(x * 16.0F + (x >= 0.0F ? 0.5F : -0.5F));
    vertex->y_q4 = (int16_t)(y * 16.0F + (y >= 0.0F ? 0.5F : -0.5F));
    vertex->u_q4 = 0;
    vertex->v_q4 = 0;
    vertex->light =
        (uint8_t)j3_clamp(light + 0.5F, 0.0F, (float)J3_LIGHT_FULL);
    vertex->depth_q8 = 0;
}

static void run_begin(j3_render_t *render) {
    render->run_count = 0;
    render->pool_used = 0;
}

/* Appends one triangle to the current colour run. Consecutive triangles that
 * share a palette index are batched into a single triangle record, which keeps
 * a whole block at two to five Host commands. */
static void run_triangle(j3_render_t *render, uint8_t color,
                         const j3_point_t *a, const j3_point_t *b,
                         const j3_point_t *c) {
    j3_run_t *run;
    pxa_raster_vertex_t *vertex;
    int fresh_run = 0;
    if (render->run_count == 0 ||
        render->runs[render->run_count - 1u].color != color) {
        if (render->run_count >= J3_RUN_MAX) {
            render->dropped = 1;
            return;
        }
        run = &render->runs[render->run_count++];
        run->color = color;
        run->triangles = 0;
        run->first = render->pool_used;
        fresh_run = 1;
    }
    run = &render->runs[render->run_count - 1u];
    if ((uint32_t)render->pool_used + 3u > (uint32_t)J3_POOL_VERTS) {
        render->dropped = 1;
        return;
    }
    vertex = &render->pool[render->pool_used];
    store_vertex(&vertex[0], a->x, a->y, a->light);
    store_vertex(&vertex[1], b->x, b->y, b->light);
    store_vertex(&vertex[2], c->x, c->y, c->light);
    /* The Host rejects zero-area polygons, and quarter-pixel quantisation can
     * collapse a sliver that looked valid in float. Drop it here instead. */
    if ((int32_t)(vertex[1].x_q4 - vertex[0].x_q4) *
            (int32_t)(vertex[2].y_q4 - vertex[0].y_q4) ==
        (int32_t)(vertex[1].y_q4 - vertex[0].y_q4) *
            (int32_t)(vertex[2].x_q4 - vertex[0].x_q4)) {
        if (fresh_run && run->triangles == 0) --render->run_count;
        return;
    }
    render->pool_used = (uint16_t)(render->pool_used + 3u);
    run->triangles = (uint16_t)(run->triangles + 1u);
}

static void run_quad(j3_render_t *render, uint8_t color,
                     const j3_point_t v[4]) {
    run_triangle(render, color, &v[0], &v[1], &v[2]);
    run_triangle(render, color, &v[0], &v[2], &v[3]);
}

static void run_flush(j3_render_t *render) {
    uint16_t index;
    for (index = 0; index < render->run_count; ++index) {
        const j3_run_t *run = &render->runs[index];
        if (run->triangles == 0) continue;
        (void)pxa_raster_triangle_batch_flags(
            &render->list, &render->pool[run->first], run->triangles, 0,
            PXA_RASTER_QUAD_PAINTER | PXA_RASTER_QUAD_SOLID_COLOR, run->color);
    }
    run_begin(render);
}

/* --------------------------------------------------------------- projection */

static j3_point_t project(const j3_render_t *render, float x, float y, float z,
                          float light) {
    const float dx = x - render->follow_x;
    const float dy = y - render->follow_y;
    const float dz = z - render->follow_z;
    j3_point_t point;
    point.x = render->anchor_x + (0.70710678F * (dx + dz)) * render->scale;
    point.y = render->anchor_y -
              (0.40824829F * dx + 0.81649658F * dy - 0.40824829F * dz) *
                  render->scale;
    point.light = light;
    return point;
}

static j3_point_t screen_point(float x, float y, float light) {
    j3_point_t point;
    point.x = x;
    point.y = y;
    point.light = light;
    return point;
}

static void screen_rect(j3_render_t *render, float x, float y, float width,
                        float height, uint8_t color) {
    j3_point_t v[4];
    v[0] = screen_point(x, y, J3_LIGHT_FULL);
    v[1] = screen_point(x + width, y, J3_LIGHT_FULL);
    v[2] = screen_point(x + width, y + height, J3_LIGHT_FULL);
    v[3] = screen_point(x, y + height, J3_LIGHT_FULL);
    run_quad(render, color, v);
}

static void textured_quad(j3_render_t *render, uint8_t slot,
                                uint8_t flags, const j3_point_t v[4],
                                float u0, float v0, float u1, float v1,
                                float light) {
    static const float k_u[4] = {0.0F, 1.0F, 1.0F, 0.0F};
    static const float k_v[4] = {0.0F, 0.0F, 1.0F, 1.0F};
    pxa_raster_vertex_t vertices[4];
    uint8_t index;
    for (index = 0; index < 4; ++index) {
        store_vertex(&vertices[index], v[index].x, v[index].y, light);
        vertices[index].u_q4 =
            (int16_t)((u0 + (u1 - u0) * k_u[index]) * 16.0F + 0.5F);
        vertices[index].v_q4 =
            (int16_t)((v0 + (v1 - v0) * k_v[index]) * 16.0F + 0.5F);
    }
    (void)pxa_raster_textured_quad_flags(
        &render->list, vertices, slot,
        PXA_RASTER_QUAD_PAINTER | PXA_RASTER_QUAD_TRANSPARENT_INDEX0 | flags);
}

/* ---------------------------------------------------------------- background */

/* Linear RGB565 mix, used by the interface pieces that need a shade between
 * two authored colours. */
static uint16_t lerp_rgb565(uint16_t from, uint16_t to, float t) {
    const int from_r = (from >> 11) & 31;
    const int from_g = (from >> 5) & 63;
    const int from_b = from & 31;
    const int to_r = (to >> 11) & 31;
    const int to_g = (to >> 5) & 63;
    const int to_b = to & 31;
    const int red = from_r + (int)((float)(to_r - from_r) * t + 0.5F);
    const int green = from_g + (int)((float)(to_g - from_g) * t + 0.5F);
    const int blue = from_b + (int)((float)(to_b - from_b) * t + 0.5F);
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

static void draw_background(j3_render_t *render, const j3_game_t *game) {
    const uint8_t scheme = (uint8_t)((game->jump_count / 15u) % J3_BG_SCHEMES);
    const uint8_t color = J3_SKY_INDEX(scheme);
    /* The gradient lives in the palette's light rows (see j3_palette_build), so
     * the whole sky is one painter run and every span fills with a single store
     * instead of the general triangle rasteriser's per pixel edge and attribute
     * work. The bands tile the target exactly, so no clear is needed either.
     * (The Host's band prefill is not used: on esp32s31-korvo-1 its 16 blocking
     * PPA fills measured 63 ms against 20 ms for this run.) */
    const int bands = j3_clampi(render->height / 6, 32, 40);
    int index;
    run_begin(render);
    for (index = 0; index < bands; ++index) {
        j3_point_t quad[4];
        const int y0 = (render->height * index) / bands;
        const int y1 = (render->height * (index + 1)) / bands;
        const float light =
            (float)J3_LIGHT_FULL * (float)index / (float)(bands - 1);
        quad[0] = screen_point(0.0F, (float)y0, light);
        quad[1] = screen_point((float)render->width, (float)y0, light);
        quad[2] = screen_point((float)render->width, (float)y1, light);
        quad[3] = screen_point(0.0F, (float)y1, light);
        run_quad(render, color, quad);
    }
    run_flush(render);
}

/* ------------------------------------------------------------------ shadows */

/* One soft shadow layer: the shape comes from the texture's alpha, the strength
 * from the vertex light row (lower is darker). */
static void shadow_layer(j3_render_t *render, uint8_t slot, float center_x,
                         float center_z, float half, float depth,
                         float light) {
    const float y = 0.002F;
    j3_point_t quad[4];
    if (!render->has_blend) return;
    quad[0] = project(render, center_x - half, y, center_z - depth, 0.0F);
    quad[1] = project(render, center_x + half, y, center_z - depth, 0.0F);
    quad[2] = project(render, center_x + half, y, center_z + depth, 0.0F);
    quad[3] = project(render, center_x - half, y, center_z + depth, 0.0F);
    textured_quad(render, slot, PXA_RASTER_QUAD_BLEND_75, quad, 0.0F,
                        0.0F, (float)J3_SHADOW_TEXTURE_SIZE,
                        (float)J3_SHADOW_TEXTURE_SIZE, light);
}

/* Solid fallback for a Host without the fixed alpha blend. */
static void shadow_fallback(j3_render_t *render, const j3_block_t *block,
                            const j3_block_t *shape) {
    const float center_x = block->x - 0.07F;
    const float center_z = block->z - 0.20F;
    const float half = block->radius * 1.05F + 0.04F;
    const float depth = half;
    j3_point_t v[4];
    const float y = 0.003F;
    const uint8_t color = J3_INDEX(J3_HUE_UI, 7);
    if (shape->kind == J3_KIND_ROUND || shape->kind == J3_KIND_WELL) {
        int index;
        const j3_point_t center =
            project(render, center_x, y, center_z, J3_LIGHT_FULL);
        for (index = 0; index < 12; ++index) {
            const float a0 = J3_TWO_PI * (float)index / 12.0F;
            const float a1 = J3_TWO_PI * (float)(index + 1) / 12.0F;
            j3_point_t a = project(render, center_x + j3_cos(a0) * half, y,
                                   center_z + j3_sin(a0) * depth,
                                   J3_LIGHT_FULL);
            j3_point_t b = project(render, center_x + j3_cos(a1) * half, y,
                                   center_z + j3_sin(a1) * depth,
                                   J3_LIGHT_FULL);
            run_triangle(render, color, &center, &a, &b);
        }
        return;
    }
    v[0] = project(render, center_x - half, y, center_z - depth,
                   J3_LIGHT_FULL);
    v[1] = project(render, center_x + half, y, center_z - depth,
                   J3_LIGHT_FULL);
    v[2] = project(render, center_x + half, y, center_z + depth,
                   J3_LIGHT_FULL);
    v[3] = project(render, center_x - half, y, center_z + depth,
                   J3_LIGHT_FULL);
    run_quad(render, color, v);
}

/* Ground shadow under a block: a wide soft spread plus a tighter, darker
 * contact core, matching the original's two-tone blobs. */
static void draw_ground_shadow(j3_render_t *render, const j3_block_t *block) {
    const int round_shape =
        block->kind == J3_KIND_ROUND || block->kind == J3_KIND_WELL;
    const uint8_t slot = round_shape ? J3_TEXTURE_SHADOW_ROUND
                                     : J3_TEXTURE_SHADOW_SQUARE;
    const float center_x = block->x - 0.07F;
    const float center_z = block->z - 0.20F;
    const float half = block->radius * 1.10F + 0.04F;
    const float depth = half * 1.02F;
    if (!render->has_blend) {
        shadow_fallback(render, block, block);
        return;
    }
    /* One baked layer: the texture already carries the falloff and the darker
     * core, and a second blended pass costs more than it shows. */
    shadow_layer(render, slot, center_x, center_z, half * 1.08F, depth * 1.08F,
                 24.0F);
}

/* The little man's shadow, scaled by how far he is above the surface. */
static void draw_shadow_quad(j3_render_t *render, float x, float y, float z,
                             float scale) {
    const float half = 0.30F * scale;
    const float depth = 0.22F * scale;
    j3_point_t quad[4];
    if (!render->has_blend) {
        const uint8_t color = J3_INDEX(J3_HUE_UI, 7);
        quad[0] = project(render, x - half * 0.7F, y + 0.003F,
                          z - depth * 0.7F, J3_LIGHT_FULL);
        quad[1] = project(render, x + half * 0.7F, y + 0.003F,
                          z - depth * 0.7F, J3_LIGHT_FULL);
        quad[2] = project(render, x + half * 0.7F, y + 0.003F,
                          z + depth * 0.7F, J3_LIGHT_FULL);
        quad[3] = project(render, x - half * 0.7F, y + 0.003F,
                          z + depth * 0.7F, J3_LIGHT_FULL);
        run_quad(render, color, quad);
        return;
    }
    quad[0] = project(render, x - half, y + 0.003F, z - depth, 0.0F);
    quad[1] = project(render, x + half, y + 0.003F, z - depth, 0.0F);
    quad[2] = project(render, x + half, y + 0.003F, z + depth, 0.0F);
    quad[3] = project(render, x - half, y + 0.003F, z + depth, 0.0F);
    textured_quad(render, J3_TEXTURE_SHADOW_ROUND,
                        PXA_RASTER_QUAD_BLEND_75, quad, 0.0F, 0.0F,
                        (float)J3_SHADOW_TEXTURE_SIZE,
                        (float)J3_SHADOW_TEXTURE_SIZE, 24.0F);
}

/* --------------------------------------------------------------- block parts */

static uint8_t block_body_color(const j3_block_t *block) {
    switch (block->kind) {
    case J3_KIND_MUSIC: return J3_MUSIC_BODY;
    case J3_KIND_STORE: return J3_STORE_WALL;
    case J3_KIND_WELL: return J3_WELL_MID;
    default: return J3_INDEX(J3_HUE_BLOCK, block->color);
    }
}

static uint8_t block_trim_color(const j3_block_t *block) {
    switch (block->kind) {
    case J3_KIND_MUSIC: return J3_MUSIC_BODY_LIT;
    case J3_KIND_STORE: return J3_STORE_AWNING;
    case J3_KIND_WELL: return J3_WELL_EDGE;
    default: return J3_TRIM_BAND;
    }
}

static void box_side(j3_render_t *render, float ax, float az, float bx,
                     float bz, float y_lo, float y_hi, uint8_t color,
                     float light) {
    j3_point_t quad[4];
    quad[0] = project(render, ax, y_lo, az, light - 2.0F);
    quad[1] = project(render, bx, y_lo, bz, light - 2.0F);
    quad[2] = project(render, bx, y_hi, bz, light);
    quad[3] = project(render, ax, y_hi, az, light);
    run_quad(render, color, quad);
}

static void draw_box(j3_render_t *render, const j3_block_t *block) {
    const float base = j3_block_offset(block);
    const float top = j3_block_top(block);
    const float low = base + (top - base) * 0.227F;
    const float high = base + (top - base) * 0.773F;
    const float x0 = block->x - block->radius;
    const float x1 = block->x + block->radius;
    const float z0 = block->z - block->radius;
    const float z1 = block->z + block->radius;
    const uint8_t body = block_body_color(block);
    const uint8_t trim = block_trim_color(block);
    const uint8_t lower = block->kind == J3_KIND_STORE ? J3_STORE_WALL : body;
    j3_point_t cap[4];

    /* Left face (-X). */
    box_side(render, x0, z0, x0, z1, base, low, lower, J3_LIGHT_LEFT);
    box_side(render, x0, z0, x0, z1, low, high, trim, J3_LIGHT_LEFT);
    box_side(render, x0, z0, x0, z1, high, top, body, J3_LIGHT_LEFT + 2.0F);
    /* Front face (+Z, slightly darker). */
    box_side(render, x0, z1, x1, z1, base, low, lower, J3_LIGHT_FRONT);
    box_side(render, x0, z1, x1, z1, low, high, trim, J3_LIGHT_FRONT);
    box_side(render, x0, z1, x1, z1, high, top, body, J3_LIGHT_FRONT + 2.0F);
    /* Top face. */
    cap[0] = project(render, x0, top, z0, J3_LIGHT_TOP);
    cap[1] = project(render, x1, top, z0, J3_LIGHT_TOP);
    cap[2] = project(render, x1, top, z1, J3_LIGHT_TOP);
    cap[3] = project(render, x0, top, z1, J3_LIGHT_TOP);
    run_quad(render, body, cap);
}

/* Light for a cylinder surface point whose outward normal is (sin? no) - the
 * caller passes the normal's z component; the host interpolates the per-vertex
 * light across the strip, so a 18 segment cylinder reads as a smooth body. */
static float cylinder_light(float normal_z, float light_base, float light_amp) {
    return light_base + light_amp * j3_clamp(normal_z, 0.0F, 1.0F);
}

static void draw_cylinder_side(j3_render_t *render, const j3_block_t *block,
                               float y_low, float y_high, float radius,
                               uint8_t color, float light_base,
                               float light_amp) {
    const float step = J3_TWO_PI / (float)J3_SEGMENTS;
    int index;
    for (index = 0; index < J3_SEGMENTS; ++index) {
        const float a0 = step * (float)index;
        const float a1 = step * (float)(index + 1);
        const float normal_x = j3_cos(a0 * 0.5F + a1 * 0.5F);
        const float normal_z = j3_sin(a0 * 0.5F + a1 * 0.5F);
        j3_point_t quad[4];
        const float light0 = cylinder_light(j3_sin(a0), light_base, light_amp);
        const float light1 = cylinder_light(j3_sin(a1), light_base, light_amp);
        if (normal_x * -0.548F + normal_z * 0.837F <= 0.0F) continue;
        quad[0] = project(render, block->x + j3_cos(a0) * radius, y_low,
                          block->z + j3_sin(a0) * radius, light0 - 2.0F);
        quad[1] = project(render, block->x + j3_cos(a1) * radius, y_low,
                          block->z + j3_sin(a1) * radius, light1 - 2.0F);
        quad[2] = project(render, block->x + j3_cos(a1) * radius, y_high,
                          block->z + j3_sin(a1) * radius, light1);
        quad[3] = project(render, block->x + j3_cos(a0) * radius, y_high,
                          block->z + j3_sin(a0) * radius, light0);
        run_quad(render, color, quad);
    }
}

static void draw_disc(j3_render_t *render, float cx, float cy, float cz,
                      float radius, uint8_t color, float light, int segments) {
    const j3_point_t center = project(render, cx, cy, cz, light);
    int index;
    for (index = 0; index < segments; ++index) {
        const float a0 = J3_TWO_PI * (float)index / (float)segments;
        const float a1 = J3_TWO_PI * (float)(index + 1) / (float)segments;
        j3_point_t a = project(render, cx + j3_cos(a0) * radius, cy,
                               cz + j3_sin(a0) * radius, light);
        j3_point_t b = project(render, cx + j3_cos(a1) * radius, cy,
                               cz + j3_sin(a1) * radius, light);
        run_triangle(render, color, &center, &a, &b);
    }
}

#define J3_RING_SEGMENTS 10

static void draw_ring(j3_render_t *render, float cx, float cy, float cz,
                      float inner, float outer, uint8_t color, float light,
                      int segments) {
    int index;
    for (index = 0; index < segments; ++index) {
        const float a0 = J3_TWO_PI * (float)index / (float)segments;
        const float a1 = J3_TWO_PI * (float)(index + 1) / (float)segments;
        j3_point_t quad[4];
        quad[0] = project(render, cx + j3_cos(a0) * inner, cy,
                          cz + j3_sin(a0) * inner, light);
        quad[1] = project(render, cx + j3_cos(a0) * outer, cy,
                          cz + j3_sin(a0) * outer, light);
        quad[2] = project(render, cx + j3_cos(a1) * outer, cy,
                          cz + j3_sin(a1) * outer, light);
        quad[3] = project(render, cx + j3_cos(a1) * inner, cy,
                          cz + j3_sin(a1) * inner, light);
        run_quad(render, color, quad);
    }
}

static void draw_rect_on_top(j3_render_t *render, const j3_block_t *block,
                             float top, float offset_x, float offset_z,
                             float half_x, float half_z, uint8_t color,
                             float light) {
    j3_point_t quad[4];
    const float y = top + 0.0006F;
    quad[0] = project(render, block->x + offset_x - half_x, y,
                      block->z + offset_z - half_z, light);
    quad[1] = project(render, block->x + offset_x + half_x, y,
                      block->z + offset_z - half_z, light);
    quad[2] = project(render, block->x + offset_x + half_x, y,
                      block->z + offset_z + half_z, light);
    quad[3] = project(render, block->x + offset_x - half_x, y,
                      block->z + offset_z + half_z, light);
    run_quad(render, color, quad);
}

static void draw_round_block(j3_render_t *render, const j3_block_t *block) {
    const float base = j3_block_offset(block);
    const float top = j3_block_top(block);
    const float band = base + (top - base) * 0.91F;
    const uint8_t body = block_body_color(block);
    const uint8_t trim = block_trim_color(block);
    draw_cylinder_side(render, block, base, band, block->radius, trim, 20.0F,
                       9.0F);
    draw_cylinder_side(render, block, band, top, block->radius, body, 24.0F,
                       7.0F);
    draw_disc(render, block->x, top, block->z, block->radius, body,
              J3_LIGHT_TOP, J3_SEGMENTS);
    draw_ring(render, block->x, top + 0.0005F, block->z,
              block->radius * 0.58F, block->radius * 0.82F, trim,
              J3_LIGHT_TOP - 2.0F, J3_RING_SEGMENTS);
}

static void draw_well_block(j3_render_t *render, const j3_block_t *block) {
    const float base = j3_block_offset(block);
    const float top = j3_block_top(block);
    const float band = base + (top - base) * 0.86F;
    draw_cylinder_side(render, block, base, band, block->radius, J3_WELL_MID,
                       22.0F, 8.0F);
    draw_cylinder_side(render, block, band, top, block->radius, J3_WELL_EDGE,
                       27.0F, 4.0F);
    draw_disc(render, block->x, top, block->z, block->radius, J3_WELL_LIGHT,
              J3_LIGHT_TOP, J3_SEGMENTS);
    draw_ring(render, block->x, top + 0.0006F, block->z,
              block->radius * 0.62F, block->radius * 0.74F, J3_WELL_MID,
              J3_LIGHT_TOP - 2.0F, J3_RING_SEGMENTS);
    draw_ring(render, block->x, top + 0.0007F, block->z,
              block->radius * 0.30F, block->radius * 0.40F, J3_WELL_DARK,
              J3_LIGHT_TOP - 3.0F, J3_RING_SEGMENTS);
    draw_disc(render, block->x, top + 0.0008F, block->z,
              block->radius * 0.16F, J3_WELL_DARK, J3_LIGHT_TOP - 2.0F, 8);
}

static void draw_music_block(j3_render_t *render, const j3_block_t *block) {
    const float top = j3_block_top(block);
    const float record = block->radius * 0.66F;
    const float arm = block->radius * 0.92F;
    j3_point_t quad[4];
    draw_box(render, block);
    draw_disc(render, block->x, top + 0.001F, block->z, record,
              J3_MUSIC_RECORD, J3_LIGHT_TOP - 3.0F, J3_SEGMENTS);
    draw_ring(render, block->x, top + 0.0015F, block->z, record * 0.42F,
              record * 0.55F, J3_MUSIC_LIGHT, J3_LIGHT_TOP - 4.0F,
              J3_RING_SEGMENTS);
    draw_ring(render, block->x, top + 0.0016F, block->z, record * 0.72F,
              record * 0.82F, J3_MUSIC_BODY_LIT, J3_LIGHT_TOP - 5.0F,
              J3_RING_SEGMENTS);
    draw_disc(render, block->x, top + 0.002F, block->z, record * 0.16F,
              J3_MUSIC_LIGHT, J3_LIGHT_TOP - 1.0F, 8);
    quad[0] = project(render, block->x + arm, top + 0.003F, block->z - arm,
                      J3_LIGHT_TOP - 2.0F);
    quad[1] = project(render, block->x + arm + 0.05F, top + 0.003F,
                      block->z - arm + 0.05F, J3_LIGHT_TOP - 2.0F);
    quad[2] = project(render, block->x + arm * 0.22F, top + 0.003F,
                      block->z + arm * 0.22F, J3_LIGHT_TOP - 2.0F);
    quad[3] = project(render, block->x + arm * 0.16F, top + 0.003F,
                      block->z + arm * 0.16F, J3_LIGHT_TOP - 2.0F);
    run_quad(render, J3_MUSIC_LIGHT, quad);
}

static void draw_store_block(j3_render_t *render, const j3_block_t *block) {
    const float top = j3_block_top(block);
    const float base = j3_block_offset(block);
    const float low = base + (top - base) * 0.32F;
    const float high = base + (top - base) * 0.76F;
    const float front_z = block->z + block->radius + 0.002F;
    const float left_x = block->x - block->radius - 0.002F;
    j3_point_t quad[4];
    draw_box(render, block);
    quad[0] = project(render, block->x - block->radius * 0.55F, low, front_z,
                      J3_LIGHT_FRONT + 3.0F);
    quad[1] = project(render, block->x + block->radius * 0.55F, low, front_z,
                      J3_LIGHT_FRONT + 3.0F);
    quad[2] = project(render, block->x + block->radius * 0.55F, high, front_z,
                      J3_LIGHT_FRONT + 3.0F);
    quad[3] = project(render, block->x - block->radius * 0.55F, high, front_z,
                      J3_LIGHT_FRONT + 3.0F);
    run_quad(render, J3_STORE_WINDOW, quad);

    quad[0] = project(render, left_x, low, block->z - block->radius * 0.55F,
                      J3_LIGHT_LEFT + 3.0F);
    quad[1] = project(render, left_x, low, block->z + block->radius * 0.55F,
                      J3_LIGHT_LEFT + 3.0F);
    quad[2] = project(render, left_x, high, block->z + block->radius * 0.55F,
                      J3_LIGHT_LEFT + 3.0F);
    quad[3] = project(render, left_x, high, block->z - block->radius * 0.55F,
                      J3_LIGHT_LEFT + 3.0F);
    run_quad(render, J3_STORE_WINDOW, quad);

    draw_rect_on_top(render, block, top, 0.0F, 0.0F, block->radius * 0.26F,
                     block->radius * 0.26F, J3_STORE_DARK, J3_LIGHT_TOP - 3.0F);
}

static void draw_rubik_block(j3_render_t *render, const j3_block_t *block) {
    static const uint8_t k_faces[9] = {
        J3_INDEX(J3_HUE_BLOCK, 7),  J3_INDEX(J3_HUE_BLOCK, 9),
        J3_INDEX(J3_HUE_BLOCK, 3),  J3_INDEX(J3_HUE_BLOCK, 4),
        J3_INDEX(J3_HUE_BLOCK, 0),  J3_INDEX(J3_HUE_BLOCK, 10),
        J3_INDEX(J3_HUE_BLOCK, 2),  J3_INDEX(J3_HUE_BLOCK, 1),
        J3_INDEX(J3_HUE_BLOCK, 13),
    };
    const float top = j3_block_top(block);
    const float pitch = block->radius * 0.55F;
    int row;
    int column;
    draw_box(render, block);
    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column) {
            const float offset_x = ((float)column - 1.0F) * pitch;
            const float offset_z = ((float)row - 1.0F) * pitch;
            draw_rect_on_top(render, block, top, offset_x, offset_z,
                             pitch * 0.44F, pitch * 0.44F,
                             k_faces[row * 3 + column], J3_LIGHT_TOP - 1.0F);
        }
    }
}

static void draw_block(j3_render_t *render, const j3_block_t *block) {
    switch (block->kind) {
    case J3_KIND_ROUND:
        draw_round_block(render, block);
        break;
    case J3_KIND_WELL:
        draw_well_block(render, block);
        break;
    case J3_KIND_MUSIC:
        draw_music_block(render, block);
        break;
    case J3_KIND_STORE:
        draw_store_block(render, block);
        break;
    case J3_KIND_RUBIK:
        draw_rubik_block(render, block);
        break;
    default:
        draw_box(render, block);
        break;
    }
    if (block->center_dot) {
        draw_disc(render, block->x, j3_block_top(block) + 0.002F, block->z,
                  block->radius * 0.14F, J3_WHITE, J3_LIGHT_FULL, 8);
    }
}

/* ----------------------------------------------------------------- the man */

static void rotate_axis(float *x, float *y, float *z, float ax, float ay,
                        float az, float angle) {
    const float c = j3_cos(angle);
    const float s = j3_sin(angle);
    const float dot = (*x) * ax + (*y) * ay + (*z) * az;
    const float cross_x = ay * (*z) - az * (*y);
    const float cross_y = az * (*x) - ax * (*z);
    const float cross_z = ax * (*y) - ay * (*x);
    *x = (*x) * c + cross_x * s + ax * dot * (1.0F - c);
    *y = (*y) * c + cross_y * s + ay * dot * (1.0F - c);
    *z = (*z) * c + cross_z * s + az * dot * (1.0F - c);
}

static void char_point(const j3_game_t *game, float lx, float ly, float lz,
                       float *ox, float *oy, float *oz) {
    float x = lx;
    float z = lz;
    float y;
    const float widen = 1.0F + (1.0F - game->body_scale) * 0.4F;
    if (game->root_yaw > 2.0F) {
        x = -lx;
        z = -lz;
    } else if (game->root_yaw > 0.5F) {
        x = lz;
        z = -lx;
    } else if (game->root_yaw < -0.5F) {
        x = -lz;
        z = lx;
    }
    x *= widen;
    z *= widen;
    y = ly * game->body_scale;
    {
        const float ax = game->dir_z;
        const float az = -game->dir_x;
        if (ax * ax + az * az > 0.25F) {
            const float angle = game->spin + game->tilt;
            if (angle != 0.0F) rotate_axis(&x, &y, &z, ax, 0.0F, az, angle);
        }
    }
    *ox = game->px + x;
    *oy = game->py + y;
    *oz = game->pz + z;
}

/* Direction counterpart of char_point: a model-space direction goes through
 * the same heading turn, widening and tumble rotation. `outward` inverts the
 * squash scales, which is what converts a model-space surface normal into the
 * world-space normal the shading and culling need. */
static void char_direction(const j3_game_t *game, float nx, float ny, float nz,
                           int outward, float *ox, float *oy, float *oz) {
    float x = nx;
    float y = ny;
    float z = nz;
    const float widen = 1.0F + (1.0F - game->body_scale) * 0.4F;
    if (game->root_yaw > 2.0F) {
        x = -nx;
        z = -nz;
    } else if (game->root_yaw > 0.5F) {
        x = nz;
        z = -nx;
    } else if (game->root_yaw < -0.5F) {
        x = -nz;
        z = nx;
    }
    if (outward) {
        if (widen > 0.001F) {
            x /= widen;
            z /= widen;
        }
        if (game->body_scale > 0.001F) y /= game->body_scale;
    }
    {
        const float ax = game->dir_z;
        const float az = -game->dir_x;
        if (ax * ax + az * az > 0.25F) {
            const float angle = game->spin + game->tilt;
            if (angle != 0.0F) rotate_axis(&x, &y, &z, ax, 0.0F, az, angle);
        }
    }
    *ox = x;
    *oy = y;
    *oz = z;
}

/* True when the model-space normal faces the camera in the man's current
 * pose. The earlier code culled against the untransformed normal, so any
 * heading other than the default one (and every tumble frame) hid faces that
 * should have been visible. */
static int char_visible(const j3_game_t *game, float nx, float ny, float nz) {
    float wx;
    float wy;
    float wz;
    char_direction(game, nx, ny, nz, 1, &wx, &wy, &wz);
    return wx * J3_VIEW_X + wy * J3_VIEW_Y + wz * J3_VIEW_Z > 0.0F;
}

/* The light rig does not turn with the little man, so his facets brighten and
 * dim as he faces different ways, exactly like the original bottle. */
static float char_light_shade(const j3_game_t *game, float nx, float ny,
                              float nz) {
    float wx;
    float wy;
    float wz;
    char_direction(game, nx, ny, nz, 0, &wx, &wy, &wz);
    return 23.0F + 8.0F * j3_clamp(wz, 0.0F, 1.0F) +
           5.0F * j3_clamp(-wx, 0.0F, 1.0F) + 6.0F * j3_clamp(wy, 0.0F, 1.0F);
}

/* The tube shading keeps the original 25 + 6 z ramp, on the turned normal. */
static float char_light_side(const j3_game_t *game, float nx, float nz) {
    float wx;
    float wy;
    float wz;
    char_direction(game, nx, 0.0F, nz, 0, &wx, &wy, &wz);
    return 25.0F + 6.0F * j3_clamp(wz, 0.0F, 1.0F);
}

/* A flat disc in the little man's model space that closes one end of the
 * tube. It is only drawn while its outward normal faces the camera, which is
 * what keeps the open tube solid once the man tumbles. */
static void draw_man_cap(j3_render_t *render, const j3_game_t *game,
                         float local_y, float radius, float normal_y,
                         uint8_t color) {
    int index;
    const int segments = J3_HEAD_SEGMENTS;
    float ox;
    float oy;
    float oz;
    j3_point_t center;
    float light;
    if (!char_visible(game, 0.0F, normal_y, 0.0F)) return;
    light = char_light_shade(game, 0.0F, normal_y, 0.0F);
    char_point(game, 0.0F, local_y, 0.0F, &ox, &oy, &oz);
    center = project(render, ox, oy, oz, light);
    for (index = 0; index < segments; ++index) {
        const float a0 = J3_TWO_PI * (float)index / (float)segments;
        const float a1 = J3_TWO_PI * (float)(index + 1) / (float)segments;
        j3_point_t a;
        j3_point_t b;
        char_point(game, j3_cos(a0) * radius, local_y, j3_sin(a0) * radius, &ox,
                   &oy, &oz);
        a = project(render, ox, oy, oz, light);
        char_point(game, j3_cos(a1) * radius, local_y, j3_sin(a1) * radius, &ox,
                   &oy, &oz);
        b = project(render, ox, oy, oz, light);
        run_triangle(render, color, &center, &a, &b);
    }
}

static void draw_man(j3_render_t *render, const j3_game_t *game) {
    static const float k_rings[4][2] = {
        {0.000F, 0.1200F},
        {0.253F, 0.0832F},
        {0.300F, 0.0920F},
        {0.372F, 0.0945F},
    };
    const int segments = J3_HEAD_SEGMENTS;
    const float step = J3_TWO_PI / (float)segments;
    const float head_y = 0.4605F;
    const float head_radius = 0.0945F;
    const int bands = 4;
    int ring;
    int index;

    /* Close the underside of the bottle as well: once the man tumbles the
     * bottom faces the camera and an open tube would show the background
     * through the body. Drawn before the walls so they still cover its rim. */
    draw_man_cap(render, game, k_rings[0][0], k_rings[0][1], -1.0F,
                 J3_CHAR_BODY_DARK);

    for (ring = 0; ring < 3; ++ring) {
        for (index = 0; index < segments; ++index) {
            const float a0 = step * (float)index;
            const float a1 = step * (float)(index + 1);
            const float a_mid = (a0 + a1) * 0.5F;
            j3_point_t quad[4];
            float light0;
            float light1;
            float ox;
            float oy;
            float oz;
            if (!char_visible(game, j3_cos(a_mid), 0.0F, j3_sin(a_mid))) {
                continue;
            }
            light0 = char_light_side(game, j3_cos(a0), j3_sin(a0));
            light1 = char_light_side(game, j3_cos(a1), j3_sin(a1));
            char_point(game, j3_cos(a0) * k_rings[ring][1], k_rings[ring][0],
                       j3_sin(a0) * k_rings[ring][1], &ox, &oy, &oz);
            quad[0] = project(render, ox, oy, oz, light0);
            char_point(game, j3_cos(a1) * k_rings[ring][1], k_rings[ring][0],
                       j3_sin(a1) * k_rings[ring][1], &ox, &oy, &oz);
            quad[1] = project(render, ox, oy, oz, light1);
            char_point(game, j3_cos(a1) * k_rings[ring + 1][1],
                       k_rings[ring + 1][0],
                       j3_sin(a1) * k_rings[ring + 1][1], &ox, &oy, &oz);
            quad[2] = project(render, ox, oy, oz, light1);
            char_point(game, j3_cos(a0) * k_rings[ring + 1][1],
                       k_rings[ring + 1][0],
                       j3_sin(a0) * k_rings[ring + 1][1], &ox, &oy, &oz);
            quad[3] = project(render, ox, oy, oz, light0);
            run_quad(render, ring == 0 ? J3_CHAR_BODY_DARK : J3_CHAR_BODY, quad);
        }
    }

    /* Cap the open top of the body so the shoulders stay solid under the
     * floating ball head, like the original bottle. */
    draw_man_cap(render, game, k_rings[3][0], k_rings[3][1], 1.0F,
                 J3_CHAR_BODY);

    for (ring = 0; ring < bands; ++ring) {
        const float phi0 = J3_PI * (float)ring / (float)bands;
        const float phi1 = J3_PI * (float)(ring + 1) / (float)bands;
        const float phi_mid = (phi0 + phi1) * 0.5F;
        for (index = 0; index < segments; ++index) {
            const float a0 = step * (float)index;
            const float a1 = step * (float)(index + 1);
            const float a_mid = (a0 + a1) * 0.5F;
            const float normal_x = j3_sin(phi_mid) * j3_cos(a_mid);
            const float normal_y = j3_cos(phi_mid);
            const float normal_z = j3_sin(phi_mid) * j3_sin(a_mid);
            j3_point_t quad[4];
            float light[4];
            float ox;
            float oy;
            float oz;
            uint8_t corner;
            if (!char_visible(game, normal_x, normal_y, normal_z)) continue;
            for (corner = 0; corner < 4u; ++corner) {
                /* Corner order: (phi0,a0) (phi0,a1) (phi1,a1) (phi1,a0). */
                const float phi = (corner == 0u || corner == 1u) ? phi0 : phi1;
                const float angle = (corner == 0u || corner == 3u) ? a0 : a1;
                const float vertex_x = j3_sin(phi) * j3_cos(angle);
                const float vertex_y = j3_cos(phi);
                const float vertex_z = j3_sin(phi) * j3_sin(angle);
                light[corner] =
                    char_light_shade(game, vertex_x, vertex_y, vertex_z);
            }
            char_point(game, head_radius * j3_sin(phi0) * j3_cos(a0),
                       head_y + head_radius * j3_cos(phi0),
                       head_radius * j3_sin(phi0) * j3_sin(a0), &ox, &oy, &oz);
            quad[0] = project(render, ox, oy, oz, light[0]);
            char_point(game, head_radius * j3_sin(phi0) * j3_cos(a1),
                       head_y + head_radius * j3_cos(phi0),
                       head_radius * j3_sin(phi0) * j3_sin(a1), &ox, &oy, &oz);
            quad[1] = project(render, ox, oy, oz, light[1]);
            char_point(game, head_radius * j3_sin(phi1) * j3_cos(a1),
                       head_y + head_radius * j3_cos(phi1),
                       head_radius * j3_sin(phi1) * j3_sin(a1), &ox, &oy, &oz);
            quad[2] = project(render, ox, oy, oz, light[2]);
            char_point(game, head_radius * j3_sin(phi1) * j3_cos(a0),
                       head_y + head_radius * j3_cos(phi1),
                       head_radius * j3_sin(phi1) * j3_sin(a0), &ox, &oy, &oz);
            quad[3] = project(render, ox, oy, oz, light[3]);
            if (ring == 0) {
                /* Top cap: the quad collapses onto the pole. */
                run_triangle(render, J3_CHAR_HEAD, &quad[0], &quad[2],
                             &quad[3]);
            } else if (ring == bands - 1) {
                run_triangle(render, J3_CHAR_HEAD, &quad[0], &quad[1],
                             &quad[2]);
            } else {
                run_quad(render, J3_CHAR_HEAD, quad);
            }
        }
    }
}

static void draw_man_shadow(j3_render_t *render, const j3_game_t *game) {
    if (game->state == J3_STATE_FLYING) {
        const j3_block_t *target = &game->blocks[1];
        const float top = j3_block_top(target);
        const float height = game->py - top;
        if (height < 0.0F || height > 1.6F) return;
        draw_shadow_quad(render, game->px, top, game->pz,
                         j3_clamp(1.0F - height * 0.45F, 0.45F, 1.0F));
        return;
    }
    if (game->state == J3_STATE_FALL || game->state == J3_STATE_TIP) {
        const j3_block_t *block = &game->blocks[0];
        const float top = j3_block_top(block);
        const float height = game->py - top;
        if (height < 0.0F || height > 0.9F) return;
        draw_shadow_quad(render, game->px, top, game->pz,
                         j3_clamp(1.0F - height * 0.5F, 0.4F, 1.0F));
        return;
    }
    draw_shadow_quad(render, game->px, j3_block_top(&game->blocks[0]),
                     game->pz, 1.0F);
}

/* ---------------------------------------------------------------- feedback */

static void draw_waves(j3_render_t *render, const j3_game_t *game) {
    const float top = j3_block_top(&game->blocks[0]);
    int index;
    for (index = 0; index < J3_WAVE_MAX; ++index) {
        const j3_wave_t *wave = &game->waves[index];
        float t;
        float radius;
        uint8_t color;
        float light;
        if (!wave->active) continue;
        t = wave->t / 0.62F;
        radius = (0.22F + 0.34F * t) *
                 (1.0F + (float)(wave->power - 1) * 0.16F);
        if (t > 0.5F) {
            color = J3_INDEX(J3_HUE_NEUTRAL, 13);
            light = J3_LIGHT_FULL - (t - 0.5F) * 34.0F;
        } else {
            color = J3_WHITE;
            light = J3_LIGHT_FULL;
        }
        draw_ring(render, game->px, top + 0.0012F + (float)index * 0.0004F,
                  game->pz, radius * 0.74F, radius, color, light, 14);
    }
}

static void format_u32(char *output, uint32_t value, int *length) {
    char reverse[12];
    int count = 0;
    int index;
    do {
        reverse[count++] = (char)('0' + (char)(value % 10u));
        value /= 10u;
    } while (value != 0u && count < (int)sizeof(reverse));
    for (index = 0; index < count; ++index)
        output[index] = reverse[count - index - 1];
    output[count] = '\0';
    *length = count;
}

static uint16_t fade_to_white(uint8_t index, float t) {
    return lerp_rgb565(j3_color_rgb565(index), 0xFFFFu, t);
}

/* --------------------------------------------------------------------- hud */

static void draw_crown(j3_render_t *render, float x, float y, float size) {
    const uint8_t gold = J3_SOLID_GOLD;
    j3_point_t v[4];
    int index;
    v[0] = screen_point(x, y + size * 0.42F, J3_LIGHT_FULL);
    v[1] = screen_point(x + size, y + size * 0.42F, J3_LIGHT_FULL);
    v[2] = screen_point(x + size, y + size * 0.72F, J3_LIGHT_FULL);
    v[3] = screen_point(x, y + size * 0.72F, J3_LIGHT_FULL);
    run_quad(render, gold, v);
    for (index = 0; index < 3; ++index) {
        const float cx = x + size * (0.18F + (float)index * 0.32F);
        const j3_point_t a = screen_point(cx, y, J3_LIGHT_FULL);
        const j3_point_t b =
            screen_point(cx - size * 0.11F, y + size * 0.5F, J3_LIGHT_FULL);
        const j3_point_t c =
            screen_point(cx + size * 0.11F, y + size * 0.5F, J3_LIGHT_FULL);
        run_triangle(render, gold, &a, &b, &c);
    }
}

static void draw_badge(j3_render_t *render, const j3_game_t *game) {
    char text[12];
    int length;
    const float panel_x = render->badge_h * 0.25F;
    const float panel_y = render->badge_h * 0.25F;
    const float panel_w = render->badge_w;
    const float panel_h = render->badge_h;
    run_begin(render);
    screen_rect(render, panel_x, panel_y, panel_w, panel_h, J3_UI_PANEL);
    screen_rect(render, panel_x, panel_y, panel_w, 1.0F,
                J3_INDEX(J3_HUE_UI, 14));
    screen_rect(render, panel_x, panel_y + panel_h - 1.0F, panel_w, 1.0F,
                J3_INDEX(J3_HUE_UI, 7));
    draw_crown(render, panel_x + panel_h * 0.2F,
               panel_y + panel_h * 0.25F, render->crown_size);
    run_flush(render);
    format_u32(text, game->best, &length);
    j3_font_draw(&render->list, render->capabilities, J3_FONT_SMALL,
                 (int)(panel_x + panel_h * 1.05F),
                 (int)(panel_y + (panel_h - (float)j3_font_cell_height(J3_FONT_SMALL)) * 0.5F),
                 1, text, j3_color_rgb565(J3_UI_INK), J3_FONT_RAMP, J3_RAMP_INK_PANEL);
}

static void draw_score(j3_render_t *render, const j3_game_t *game) {
    char text[12];
    int length;
    int width;
    int x;
    format_u32(text, game->score, &length);
    width = j3_font_width(J3_FONT_BIG, 1, text);
    x = (render->width - width) / 2;
    {
        const int shadow = (int)(render->big_cell_h * 0.12F) + 1;
        j3_font_draw(&render->list, render->capabilities, J3_FONT_BIG,
                     x + shadow, (int)render->score_y + shadow + 1, 1, text,
                     j3_color_rgb565(J3_INDEX(J3_HUE_UI, 8)), J3_FONT_CRISP, 0);
    }
    j3_font_draw(&render->list, render->capabilities, J3_FONT_BIG, x,
                 (int)render->score_y, 1, text,
                 j3_color_rgb565(J3_UI_WHITE), J3_FONT_RAMP, J3_RAMP_SKY);
}

static void draw_popup(j3_render_t *render, const j3_game_t *game) {
    char text[12];
    int length;
    float rise;
    float alpha;
    j3_point_t anchor;
    float x;
    float y;
    if (game->popup_timer <= 0.0F) return;
    rise = (0.9F - game->popup_timer) / 0.9F;
    alpha = rise < 0.6F ? 0.0F : (rise - 0.6F) / 0.4F;
    anchor = project(render, game->px, game->py + 0.66F, game->pz, 0.0F);
    x = anchor.x;
    y = anchor.y - rise * 26.0F;
    if (game->popup_kind == 2u || game->popup_kind == 3u) {
        static const char plus[] = "+";
        static const char very_good[] = "\xe5\xbe\x88\xe5\xa5\xbd"; /* 很好 */
        static const char so_quick[] = "\xe5\xa5\xbd\xe5\xbf\xab";  /* 好快 */
        const char *label = game->popup_kind == 3u ? so_quick : very_good;
        const uint8_t base = game->popup_kind == 2u ? J3_SOLID_ORANGE
                                                    : J3_SOLID_GOLD;
        const uint16_t color = fade_to_white(base, alpha);
        int cx;
        const int scale = 1;
        const int cy = (int)(y - render->big_cell_h * 0.5F);
        format_u32(text, game->popup_points, &length);
        cx = (int)(x - (float)(j3_font_width(J3_FONT_BIG, scale, plus) +
                               j3_font_width(J3_FONT_BIG, scale, text)) *
                           0.5F);
        j3_font_draw(&render->list, render->capabilities, J3_FONT_BIG, cx,
                     cy, scale, plus, color, J3_FONT_RAMP, J3_RAMP_SKY);
        cx += j3_font_width(J3_FONT_BIG, scale, plus);
        j3_font_draw(&render->list, render->capabilities, J3_FONT_BIG, cx,
                     cy, scale, text, color, J3_FONT_RAMP, J3_RAMP_SKY);
        j3_font_draw(&render->list, render->capabilities, J3_FONT_CJK,
                     (int)(x - (float)j3_font_width(J3_FONT_CJK, 1, label) *
                                     0.5F),
                     (int)(y + render->big_cell_h * 0.35F), 1, label, color, J3_FONT_RAMP, J3_RAMP_SKY);
    } else {
        const uint16_t color = fade_to_white(J3_UI_WHITE, alpha);
        format_u32(text, game->popup_points, &length);
        j3_font_draw(&render->list, render->capabilities, J3_FONT_BIG,
                     (int)(x - (float)j3_font_width(J3_FONT_BIG, 1, text) *
                                     0.5F),
                     (int)(y - render->big_cell_h * 0.5F), 1, text, color, J3_FONT_RAMP, J3_RAMP_SKY);
    }
}

static void draw_bonus_popup(j3_render_t *render, const j3_game_t *game) {
    char text[12];
    int length;
    float rise;
    float alpha;
    j3_point_t anchor;
    if (game->bonus_popup_timer <= 0.0F) return;
    rise = (1.1F - game->bonus_popup_timer) / 1.1F;
    alpha = rise < 0.6F ? 0.0F : (rise - 0.6F) / 0.4F;
    anchor = project(render, game->blocks[0].x,
                     j3_block_top(&game->blocks[0]) + 0.5F,
                     game->blocks[0].z, 0.0F);
    format_u32(text, game->bonus_points, &length);
    j3_font_draw(&render->list, render->capabilities, J3_FONT_BIG,
                 (int)(anchor.x -
                       (float)j3_font_width(J3_FONT_BIG, 1, text) * 0.5F),
                 (int)(anchor.y - rise * 22.0F), 1, text,
                 fade_to_white(J3_SOLID_GOLD, alpha), J3_FONT_ALPHA, 0);
}

static void draw_result(j3_render_t *render, const j3_game_t *game) {
    static const char score_label[] = "\xe6\x9c\xac\xe6\xac\xa1\xe5\xbe\x97\xe5\x88\x86";
    static const char best_label[] = "\xe6\x9c\x80\xe9\xab\x98\xe5\x88\x86";
    static const char again[] = "\xe9\x87\x8d\xe6\x96\xb0\xe5\xbc\x80\xe5\xa7\x8b";
    const float panel_w = render->panel_w;
    const float panel_h = render->panel_h;
    const float x = ((float)render->width - panel_w) * 0.5F;
    const float y = ((float)render->height - panel_h) * 0.5F;
    const float button_w = render->button_w;
    const float button_h = render->button_h;
    const float button_x = x + (panel_w - button_w) * 0.5F;
    const float button_y = y + panel_h * 0.79F;
    char text[12];
    int length;
    int width;

    run_begin(render);
    if (render->has_blend) {
        j3_point_t quad[4];
        quad[0] = screen_point(0.0F, 0.0F, J3_LIGHT_FULL);
        quad[1] = screen_point((float)render->width, 0.0F, J3_LIGHT_FULL);
        quad[2] =
            screen_point((float)render->width, (float)render->height,
                         J3_LIGHT_FULL);
        quad[3] = screen_point(0.0F, (float)render->height, J3_LIGHT_FULL);
        {
            pxa_raster_vertex_t vertices[4];
            uint8_t index;
            for (index = 0; index < 4; ++index) {
                store_vertex(&vertices[index], quad[index].x, quad[index].y,
                             16.0F);
                vertices[index].u_q4 = 8;
                vertices[index].v_q4 = 8;
            }
            (void)pxa_raster_textured_quad_flags(
                &render->list, vertices, J3_TEXTURE_SOLID,
                PXA_RASTER_QUAD_PAINTER | PXA_RASTER_QUAD_BLEND_75);
        }
    }
    screen_rect(render, x - 3.0F, y - 3.0F, panel_w + 6.0F, panel_h + 6.0F,
                J3_INDEX(J3_HUE_UI, 8));
    screen_rect(render, x, y, panel_w, panel_h, J3_SOLID_WHITE);
    screen_rect(render, x, y, panel_w, 2.0F, J3_INDEX(J3_HUE_UI, 14));
    screen_rect(render, x, y + panel_h - 2.0F, panel_w, 2.0F,
                J3_INDEX(J3_HUE_UI, 8));
    screen_rect(render, button_x, button_y + 2.0F, button_w, button_h,
                J3_INDEX(J3_HUE_UI, 7));
    screen_rect(render, button_x, button_y, button_w, button_h,
                J3_SOLID_GREEN);
    run_flush(render);

    j3_font_draw(&render->list, render->capabilities, J3_FONT_CJK,
                 (int)(x + (panel_w -
                            (float)j3_font_width(J3_FONT_CJK, 1,
                                                 score_label)) *
                               0.5F),
                 (int)(y + panel_h * 0.06F), 1, score_label,
                 j3_color_rgb565(J3_UI_TEXT_DIM), J3_FONT_RAMP, J3_RAMP_DIM_PANEL);
    format_u32(text, game->score, &length);
    width = j3_font_width(J3_FONT_BIG, 1, text);
    j3_font_draw(&render->list, render->capabilities, J3_FONT_BIG,
                 (int)(x + (panel_w - (float)width) * 0.5F),
                 (int)(y + panel_h * 0.20F), 1, text,
                 j3_color_rgb565(J3_UI_INK), J3_FONT_RAMP, J3_RAMP_INK_PANEL);
    j3_font_draw(&render->list, render->capabilities, J3_FONT_CJK,
                 (int)(x + panel_w * 0.09F), (int)(y + panel_h * 0.59F), 1,
                 best_label, j3_color_rgb565(J3_UI_TEXT_DIM), J3_FONT_RAMP, J3_RAMP_DIM_PANEL);
    format_u32(text, game->best, &length);
    j3_font_draw(&render->list, render->capabilities, J3_FONT_SMALL,
                 (int)(x + panel_w * 0.91F -
                       (float)j3_font_width(J3_FONT_SMALL, 1, text)),
                 (int)(y + panel_h * 0.60F), 1, text,
                 j3_color_rgb565(J3_UI_INK), J3_FONT_RAMP, J3_RAMP_INK_PANEL);
    j3_font_draw(&render->list, render->capabilities, J3_FONT_CJK,
                 (int)(button_x + (button_w -
                                   (float)j3_font_width(J3_FONT_CJK, 1,
                                                        again)) *
                                      0.5F),
                 (int)(button_y + (button_h -
                                   (float)j3_font_cell_height(J3_FONT_CJK)) *
                                      0.5F), 1, again,
                 j3_color_rgb565(J3_UI_WHITE), J3_FONT_ALPHA, 0);
}

/* -------------------------------------------------------------------- frame */

void j3_render_configure(j3_render_t *render, int width, int height,
                         uint32_t capabilities) {
    float ui = j3_clamp((float)height / 240.0F, 0.4F, 2.0F);
    float width_f = (float)width;
    float height_f = (float)height;
    uint8_t tier;
    render->width = width;
    render->height = height;
    render->capabilities = capabilities;
    render->has_blend =
        (capabilities & PXA_RASTER_CAP_FIXED_ALPHA_BLEND) != 0u ? 1u : 0u;
    render->scale = (float)height * 0.228F;
    render->anchor_x = (float)width * 0.5F;
    render->anchor_y = (float)height * 0.575F;

    /* Glyph tiers are designed for a target-height class, so the interface
     * picks the one that matches instead of scaling an atlas. */
    if (height <= 170)
        tier = 0u;
    else if (height <= 330)
        tier = 1u;
    else
        tier = 2u;
    j3_font_set_tier(tier);
    render->big_cell_h = j3_font_cell_height(J3_FONT_BIG);
    if (render->big_cell_h <= 0.0F) render->big_cell_h = 12.0F;
    render->badge_h = j3_clamp(20.0F * ui, 11.0F, 30.0F);
    render->badge_w = j3_clamp(78.0F * ui, 42.0F, 120.0F);
    render->crown_size = render->badge_h * 0.55F;
    render->score_y = height_f * 0.03F;
    render->panel_w = j3_clamp(width_f * 0.82F, 120.0F, 300.0F);
    render->panel_h = j3_clamp(height_f * 0.66F, 88.0F, 190.0F);
    render->button_w = render->panel_w * 0.56F;
    render->button_h = j3_clamp(render->panel_h * 0.18F, 14.0F, 34.0F);
}

/* Depth along the orthographic view direction: larger is farther. */
static float block_depth(const j3_block_t *block) {
    return 0.57735027F * (block->x - block->z);
}

int j3_render_frame(j3_render_t *render, const j3_game_t *game,
                    uint32_t context, uint8_t *bytes, uint32_t capacity,
                    uint64_t frame_id) {
    uint8_t order[J3_BLOCK_MAX];
    uint8_t count = game->block_count;
    uint8_t index;
    uint8_t i;
    uint8_t j;

    if (count > J3_BLOCK_MAX) count = J3_BLOCK_MAX;
    for (index = 0; index < count; ++index) order[index] = index;
    /* Insertion sort by descending depth: the far block is drawn first. */
    for (i = 1; i < count; ++i) {
        const uint8_t value = order[i];
        for (j = i; j > 0u; --j) {
            if (block_depth(&game->blocks[order[j - 1u]]) >=
                block_depth(&game->blocks[value]))
                break;
            order[j] = order[j - 1u];
        }
        order[j] = value;
    }

    render->dropped = 0;
    render->follow_x = game->cam_x;
    render->follow_y = 0.0F;
    render->follow_z = game->cam_z;
    pxa_raster_draw_list_begin(&render->list, bytes, capacity, frame_id);
    if (render->list.status != PXA_STATUS_OK) return 0;
    /* Deliberately no full-frame clear: the background bands tile the target
     * exactly, and every pixel written into a GameRender surface costs the Host
     * a PSRAM store, so a clear would only be overwritten one command later. */
#if J3_SKIP_PROBE
#define J3_PROBE_SKIP(bit) ((j3_skip_probe_mask & (bit)) != 0u)
#else
#define J3_PROBE_SKIP(bit) 0
#endif
#if J3_SKIP_PROBE
    if ((j3_skip_probe_mask & 0x40u) != 0u) {
        /* Probe: four full-screen painter quads (constant light) so the Host's
         * span-fill throughput can be measured on the real target. */
        int band;
        run_begin(render);
        for (band = 0; band < 4; ++band) {
            j3_point_t quad[4];
            const int y0 = render->height * band / 4;
            const int y1 = render->height * (band + 1) / 4;
            quad[0] = screen_point(0.0F, (float)y0, J3_LIGHT_FULL);
            quad[1] = screen_point((float)render->width, (float)y0, J3_LIGHT_FULL);
            quad[2] = screen_point((float)render->width, (float)y1, J3_LIGHT_FULL);
            quad[3] = screen_point(0.0F, (float)y1, J3_LIGHT_FULL);
            run_quad(render, J3_INDEX(J3_HUE_BLOCK, (uint8_t)band), quad);
        }
        run_flush(render);
        return pxa_raster_submit(context, &render->list) > 0;
    }
    if ((j3_skip_probe_mask & 0x80u) != 0u) {
        /* Probe: the same coverage through flat RGB565 quads, which the Host
         * rasterises through the general triangle path. */
        int band;
        for (band = 0; band < 4; ++band) {
            int16_t xy[8];
            const int y0 = render->height * band / 4;
            const int y1 = render->height * (band + 1) / 4;
            xy[0] = xy[6] = 0;
            xy[1] = xy[3] = (int16_t)(y0 << 4);
            xy[2] = xy[4] = (int16_t)(render->width << 4);
            xy[5] = xy[7] = (int16_t)(y1 << 4);
            (void)pxa_raster_flat_quad(&render->list, xy,
                                       (uint16_t)(0x1082u + band));
        }
        return pxa_raster_submit(context, &render->list) > 0;
    }
    if ((j3_skip_probe_mask & 0x100u) != 0u) {
        /* Probe: the same coverage through fractional flat quads. The edges miss
         * the pixel grid, so these stay on the general triangle path and one
         * build can compare both raster paths on the same board. */
        int band;
        for (band = 0; band < 4; ++band) {
            int16_t xy[8];
            const int y0 = render->height * band / 4;
            const int y1 = render->height * (band + 1) / 4;
            xy[0] = xy[6] = 2;
            xy[1] = xy[3] = (int16_t)((y0 << 4) + 2);
            xy[2] = xy[4] = (int16_t)((render->width << 4) - 2);
            xy[5] = xy[7] = (int16_t)((y1 << 4) - 2);
            (void)pxa_raster_flat_quad(&render->list, xy,
                                       (uint16_t)(0x1082u + band));
        }
        return pxa_raster_submit(context, &render->list) > 0;
    }
#endif
    if (!J3_PROBE_SKIP(0x01u)) draw_background(render, game);

    for (i = 0; i < count; ++i) {
        const j3_block_t *block = &game->blocks[order[i]];
        run_begin(render);
        if (!J3_PROBE_SKIP(0x02u)) draw_ground_shadow(render, block);
        if (!J3_PROBE_SKIP(0x04u)) draw_block(render, block);
        run_flush(render);
    }

    if (!J3_PROBE_SKIP(0x08u)) {
        run_begin(render);
        draw_waves(render, game);
        run_flush(render);
    }

    if (!J3_PROBE_SKIP(0x10u)) {
        run_begin(render);
        if (game->state != J3_STATE_OVER) {
            draw_man_shadow(render, game);
            draw_man(render, game);
        }
        run_flush(render);
    }

    if (!J3_PROBE_SKIP(0x20u)) {
        draw_popup(render, game);
        draw_bonus_popup(render, game);
        draw_badge(render, game);
        draw_score(render, game);
        if (game->state == J3_STATE_OVER) draw_result(render, game);
    }
#undef J3_PROBE_SKIP

    if (render->dropped) return 0;
    /* An empty list is not a present failure: the ladder must not drop a scale
     * because one frame had nothing to draw. */
    if (render->list.command_count == 0u) return 1;
    return pxa_raster_submit(context, &render->list) > 0;
}

/* Builds one shadow shape. `rounded_square` selects the box shadow (a square
 * with rounded corners) over the ellipse used by round blocks; both fall off to
 * fully transparent at the rim. The 8x8 ordered dither between neighbouring
 * palette greys keeps the gradient smooth after the Host's 75% blend. */
static void build_shadow_shape(uint8_t *pixels, int size, int rounded_square) {
    static const uint8_t bayer[8][8] = {
        {0, 32, 8, 40, 2, 34, 10, 42},     {48, 16, 56, 24, 50, 18, 58, 26},
        {12, 44, 4, 36, 14, 46, 6, 38},    {60, 28, 52, 20, 62, 30, 54, 22},
        {3, 35, 11, 43, 1, 33, 9, 41},     {51, 19, 59, 27, 49, 17, 57, 25},
        {15, 47, 7, 39, 13, 45, 5, 37},    {63, 31, 55, 23, 61, 29, 53, 21},
    };
    const float half = (float)size * 0.5F;
    int y;
    int x;
    for (y = 0; y < size; ++y) {
        for (x = 0; x < size; ++x) {
            const float normalized_x =
                ((float)x + 0.5F - half) / (half - 1.0F);
            const float normalized_z =
                ((float)y + 0.5F - half) / (half - 1.0F);
            float distance;
            float value;
            float fraction;
            uint8_t texel;
            if (rounded_square) {
                /* Chebyshev distance with a rounded corner. */
                const float ax = j3_fabs(normalized_x);
                const float az = j3_fabs(normalized_z);
                const float inset = 0.58F;
                const float cx = ax > inset ? ax - inset : 0.0F;
                const float cz = az > inset ? az - inset : 0.0F;
                distance = inset + j3_sqrt(cx * cx + cz * cz);
                distance /= 1.0F + inset * 0.4142F;
            } else {
                distance = j3_sqrt(normalized_x * normalized_x +
                                   normalized_z * normalized_z);
            }
            if (distance >= 1.0F) {
                pixels[y * size + x] = 0u; /* cut out */
                continue;
            }
            /* Index 7 at the centre (darkest) fading to 15 at the rim. */
            value = 7.0F + 8.0F * (distance * distance);
            fraction = value - j3_floor(value);
            texel = (uint8_t)j3_floor(value);
            if (fraction * 64.0F > (float)bayer[y & 7][x & 7])
                texel = (uint8_t)(texel + 1u);
            if (texel > 15u) texel = 15u;
            pixels[y * size + x] = texel;
        }
    }
}

int j3_render_upload_resources(uint32_t context, uint8_t *scratch,
                               uint32_t scratch_capacity) {
    static uint8_t pixels[J3_SHADOW_TEXTURE_SIZE * J3_SHADOW_TEXTURE_SIZE];
    static const uint8_t solid = 15u;
    const uint32_t payload = (uint32_t)sizeof(pixels);
    if (scratch == NULL ||
        scratch_capacity < PXA_RASTER_UPLOAD_HEADER_BYTES + payload)
        return 0;
    build_shadow_shape(pixels, (int)J3_SHADOW_TEXTURE_SIZE, 1);
    if (pxa_raster_upload_texture_index8(
            context, J3_TEXTURE_SHADOW_SQUARE, (uint16_t)J3_SHADOW_TEXTURE_SIZE,
            (uint16_t)J3_SHADOW_TEXTURE_SIZE, pixels, scratch,
            scratch_capacity) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + payload))
        return 0;
    build_shadow_shape(pixels, (int)J3_SHADOW_TEXTURE_SIZE, 0);
    if (pxa_raster_upload_texture_index8(
            context, J3_TEXTURE_SHADOW_ROUND, (uint16_t)J3_SHADOW_TEXTURE_SIZE,
            (uint16_t)J3_SHADOW_TEXTURE_SIZE, pixels, scratch,
            scratch_capacity) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + payload))
        return 0;
    return pxa_raster_upload_texture_index8(
               context, J3_TEXTURE_SOLID, 1, 1, &solid, scratch,
               scratch_capacity) ==
           (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + 1u);
}
