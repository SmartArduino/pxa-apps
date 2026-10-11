#include "mesh.h"

#include "palette.h"
#include "textures.h"
#include "tomb_math.h"

typedef struct {
    tomb_vec3_t p;
    float u;
    float v;
    float light;
} tomb_clip_vertex_t;

typedef struct {
    float x, y;
    float u, v;
    float light;
    float depth;
} tomb_screen_vertex_t;

/* ---- vectors and transforms ------------------------------------------------ */

void tomb_vec3_set(tomb_vec3_t *out, float x, float y, float z) {
    out->x = x;
    out->y = y;
    out->z = z;
}

tomb_vec3_t tomb_vec3_sub(tomb_vec3_t a, tomb_vec3_t b) {
    tomb_vec3_t out;
    tomb_vec3_set(&out, a.x - b.x, a.y - b.y, a.z - b.z);
    return out;
}

tomb_vec3_t tomb_vec3_scale(tomb_vec3_t v, float scale) {
    tomb_vec3_t out;
    tomb_vec3_set(&out, v.x * scale, v.y * scale, v.z * scale);
    return out;
}

tomb_vec3_t tomb_vec3_add(tomb_vec3_t a, tomb_vec3_t b) {
    tomb_vec3_t out;
    tomb_vec3_set(&out, a.x + b.x, a.y + b.y, a.z + b.z);
    return out;
}

float tomb_vec3_dot(tomb_vec3_t a, tomb_vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

tomb_vec3_t tomb_vec3_cross(tomb_vec3_t a, tomb_vec3_t b) {
    tomb_vec3_t out;
    tomb_vec3_set(&out, a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x);
    return out;
}

float tomb_vec3_length(tomb_vec3_t v) {
    const float squared = v.x * v.x + v.y * v.y + v.z * v.z;
    return squared > 0.0f ? __builtin_sqrtf(squared) : 0.0f;
}

static void transform_from_rotation(tomb_transform_t *out) {
    out->m[0][0] = 1.0f;
    out->m[0][1] = 0.0f;
    out->m[0][2] = 0.0f;
    out->m[1][0] = 0.0f;
    out->m[1][1] = 1.0f;
    out->m[1][2] = 0.0f;
    out->m[2][0] = 0.0f;
    out->m[2][1] = 0.0f;
    out->m[2][2] = 1.0f;
}

void tomb_transform_identity(tomb_transform_t *out) {
    transform_from_rotation(out);
    tomb_vec3_set(&out->t, 0.0f, 0.0f, 0.0f);
}

void tomb_transform_translation(tomb_transform_t *out, tomb_vec3_t offset) {
    transform_from_rotation(out);
    out->t = offset;
}

void tomb_transform_rotation_x(tomb_transform_t *out, float radians) {
    const float c = tomb_cos(radians);
    const float s = tomb_sin(radians);
    transform_from_rotation(out);
    tomb_vec3_set(&out->t, 0.0f, 0.0f, 0.0f);
    out->m[1][1] = c;
    out->m[1][2] = s;
    out->m[2][1] = -s;
    out->m[2][2] = c;
}

void tomb_transform_rotation_y(tomb_transform_t *out, float radians) {
    const float c = tomb_cos(radians);
    const float s = tomb_sin(radians);
    transform_from_rotation(out);
    tomb_vec3_set(&out->t, 0.0f, 0.0f, 0.0f);
    out->m[0][0] = c;
    out->m[0][2] = s;
    out->m[2][0] = -s;
    out->m[2][2] = c;
}

void tomb_transform_rotation_z(tomb_transform_t *out, float radians) {
    const float c = tomb_cos(radians);
    const float s = tomb_sin(radians);
    transform_from_rotation(out);
    tomb_vec3_set(&out->t, 0.0f, 0.0f, 0.0f);
    out->m[0][0] = c;
    out->m[0][1] = -s;
    out->m[1][0] = s;
    out->m[1][1] = c;
}

void tomb_transform_uniform(tomb_transform_t *out, tomb_vec3_t translation,
                            float yaw, float pitch, float roll) {
    tomb_transform_t rotation_y;
    tomb_transform_t rotation_x;
    tomb_transform_t rotation_z;
    tomb_transform_t combined;
    tomb_transform_rotation_y(&rotation_y, yaw);
    tomb_transform_rotation_x(&rotation_x, pitch);
    tomb_transform_rotation_z(&rotation_z, roll);
    tomb_transform_mul(&combined, &rotation_y, &rotation_x);
    tomb_transform_mul(&combined, &combined, &rotation_z);
    *out = combined;
    out->t = translation;
}

void tomb_transform_mul(tomb_transform_t *out, const tomb_transform_t *parent,
                        const tomb_transform_t *child) {
    tomb_transform_t result;
    int row;
    int column;
    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column) {
            result.m[row][column] =
                parent->m[row][0] * child->m[0][column] +
                parent->m[row][1] * child->m[1][column] +
                parent->m[row][2] * child->m[2][column];
        }
    }
    tomb_transform_apply(parent, child->t, &result.t);
    *out = result;
}

void tomb_transform_apply(const tomb_transform_t *transform, tomb_vec3_t point,
                          tomb_vec3_t *out) {
    tomb_vec3_set(out,
                  transform->m[0][0] * point.x + transform->m[0][1] * point.y +
                      transform->m[0][2] * point.z + transform->t.x,
                  transform->m[1][0] * point.x + transform->m[1][1] * point.y +
                      transform->m[1][2] * point.z + transform->t.y,
                  transform->m[2][0] * point.x + transform->m[2][1] * point.y +
                      transform->m[2][2] * point.z + transform->t.z);
}

void tomb_transform_rotate(const tomb_transform_t *transform, tomb_vec3_t point,
                           tomb_vec3_t *out) {
    tomb_vec3_set(out,
                  transform->m[0][0] * point.x + transform->m[0][1] * point.y +
                      transform->m[0][2] * point.z,
                  transform->m[1][0] * point.x + transform->m[1][1] * point.y +
                      transform->m[1][2] * point.z,
                  transform->m[2][0] * point.x + transform->m[2][1] * point.y +
                      transform->m[2][2] * point.z);
}

float tomb_camera_focal_length(float horizontal_fov_radians, int view_width) {
    const float half = tomb_clamp(horizontal_fov_radians, 0.1f, 3.0f) * 0.5f;
    const float c = tomb_cos(half);
    if (c <= 1e-4f) return (float)view_width;
    return (float)view_width * 0.5f * c / tomb_sin(half);
}

tomb_rect_t tomb_rect_intersect(tomb_rect_t a, tomb_rect_t b) {
    const int x0 = a.x > b.x ? a.x : b.x;
    const int y0 = a.y > b.y ? a.y : b.y;
    const int x1 = (a.x + a.width) < (b.x + b.width) ? (a.x + a.width)
                                                     : (b.x + b.width);
    const int y1 = (a.y + a.height) < (b.y + b.height) ? (a.y + a.height)
                                                       : (b.y + b.height);
    tomb_rect_t out;
    out.x = x0;
    out.y = y0;
    out.width = x1 > x0 ? x1 - x0 : 0;
    out.height = y1 > y0 ? y1 - y0 : 0;
    return out;
}

/* ---- lighting -------------------------------------------------------------- */

static float light_for(const tomb_renderer_t *renderer, float brightness,
                       float depth) {
    const tomb_lighting_t *lighting = &renderer->lighting;
    float attenuation = 1.0f;
    float top;
    if (depth >= lighting->dark_distance) {
        attenuation = 0.0f;
    } else if (depth > lighting->full_distance) {
        attenuation = (lighting->dark_distance - depth) /
                      (lighting->dark_distance - lighting->full_distance);
    }
    top = (float)(lighting->levels - 1u);
    return tomb_clamp(brightness * (1.0f / 255.0f) * top * attenuation,
                      (float)lighting->minimum, top);
}

/* ---- Host raster record emission ------------------------------------------ */

/* True while `bytes` more bytes and one more command fit in the draw list. */
static int record_fits(const tomb_renderer_t *renderer, uint32_t bytes) {
    const pxa_raster_draw_list_t *list = renderer->list;
    return list != 0 && list->status == PXA_STATUS_OK &&
           list->command_count < PXA_RASTER_MAX_COMMANDS &&
           list->length + bytes <= list->capacity;
}

/* The Host rejects zero-area triangles. Clipping can collapse a sliver onto a
 * line once the coordinates are quantised to quarter pixels, so check the
 * emitted fan triangles (a quad becomes (0,1,2) and (0,2,3)). */
static int64_t record_area2(const pxa_raster_vertex_t *a,
                            const pxa_raster_vertex_t *b,
                            const pxa_raster_vertex_t *c) {
    return (int64_t)(b->x_q4 - a->x_q4) * (int64_t)(c->y_q4 - a->y_q4) -
           (int64_t)(b->y_q4 - a->y_q4) * (int64_t)(c->x_q4 - a->x_q4);
}

static int record_degenerate(const pxa_raster_vertex_t *vertices,
                             uint32_t count) {
    if (record_area2(&vertices[0], &vertices[1], &vertices[2]) == 0) return 1;
    if (count == 4u &&
        record_area2(&vertices[0], &vertices[2], &vertices[3]) == 0)
        return 1;
    return 0;
}

static uint8_t light_level(const tomb_renderer_t *renderer, float light) {
    const float top = (float)(renderer->lighting.levels - 1u);
    return (uint8_t)tomb_clamp(light + 0.5f, 0.0f, top);
}

static void fill_record_vertices(const tomb_renderer_t *renderer,
                                 const tomb_screen_vertex_t *vertices,
                                 uint32_t count, const uint8_t *levels,
                                 pxa_raster_vertex_t out[4]) {
    float min_u = vertices[0].u;
    float min_v = vertices[0].v;
    float shift_u;
    float shift_v;
    uint32_t index;
    (void)renderer;
    for (index = 0; index < count; ++index) {
        if (vertices[index].u < min_u) min_u = vertices[index].u;
        if (vertices[index].v < min_v) min_v = vertices[index].v;
    }
    /* The wire holds texel coordinates; shift by a multiple of 256 so the
     * smallest corner lands in [0, 256). The original mesh wire wrapped on
     * 256 texels; PXA wraps on the texture size instead, which the generated
     * faces already respect. */
    shift_u = tomb_floor(min_u * (1.0f / 256.0f)) * 256.0f;
    shift_v = tomb_floor(min_v * (1.0f / 256.0f)) * 256.0f;
    for (index = 0; index < count; ++index) {
        const tomb_screen_vertex_t *vertex = &vertices[index];
        out[index].x_q4 = (int16_t)(vertex->x * 16.0f + 0.5f);
        out[index].y_q4 = (int16_t)(vertex->y * 16.0f + 0.5f);
        out[index].u_q4 = (int16_t)((vertex->u - shift_u) * 16.0f + 0.5f);
        out[index].v_q4 = (int16_t)((vertex->v - shift_v) * 16.0f + 0.5f);
        out[index].light = levels[index];
        out[index].depth_q8 = 0;
    }
}

static void queue(tomb_renderer_t *renderer,
                  const tomb_screen_vertex_t *vertices, uint32_t count,
                  const tomb_face_t *face, uint8_t group, float depth) {
    const int flat = (face->flags & TOMB_FACE_FLAT_COLOR) != 0u;
    uint32_t index;
    float area2 = 0.0f;
    uint8_t levels[4];
    tomb_polygon_slot_t *slot;
    uint32_t bucket;
    uint16_t *head;
    if (!flat && face->texture >= TOMB_TEXTURE_COUNT) {
        ++renderer->stats.dropped;
        return;
    }
    if (renderer->used >= TOMB_POLYGON_POOL) {
        ++renderer->stats.dropped;
        return;
    }
    for (index = 0; index < count; ++index) {
        const tomb_screen_vertex_t *a = &vertices[index];
        const tomb_screen_vertex_t *b = &vertices[index + 1u == count ? 0u : index + 1u];
        area2 += a->x * b->y - b->x * a->y;
        levels[index] = light_level(renderer, a->light);
    }
    if (area2 == 0.0f) {
        ++renderer->stats.culled;
        return;
    }
    slot = &renderer->polygons[renderer->used];
    fill_record_vertices(renderer, vertices, count, levels, slot->corners);
    if (record_degenerate(slot->corners, count)) {
        ++renderer->stats.culled;
        return;
    }
    slot->count = (uint8_t)count;
    slot->texture_slot = flat ? (uint8_t)face->u[0] : face->texture;
    slot->flags = face->flags;
    bucket = (uint32_t)(tomb_clamp(depth, 0.0f, renderer->config.far) *
                        renderer->bucket_scale);
    head = &renderer->buckets[(uint32_t)group * TOMB_RENDER_BUCKETS + bucket];
    slot->next = *head;
    *head = renderer->used++;
    ++renderer->stats.polygons;
    renderer->stats.pixel_estimate += (uint32_t)(tomb_fabs(area2) * 0.5f);
}

/* Exact clip against the scissor edges in screen space; attributes are affine
 * in screen space by construction. */
static void emit_screen_polygon(tomb_renderer_t *renderer,
                                tomb_screen_vertex_t *vertices, uint32_t count,
                                const tomb_face_t *face, uint8_t group,
                                float depth,
                                tomb_rect_t scissor) {
    const float edges[4] = {(float)scissor.x, (float)(scissor.x + scissor.width),
                            (float)scissor.y, (float)(scissor.y + scissor.height)};
    tomb_screen_vertex_t scratch[TOMB_MAX_CLIP_VERTICES];
    int inside = 1;
    uint32_t c;
    uint32_t next;
    for (c = 0u; c < count && inside; ++c) {
        inside = vertices[c].x >= edges[0] && vertices[c].x <= edges[1] &&
                 vertices[c].y >= edges[2] && vertices[c].y <= edges[3];
    }
    if (!inside) {
        tomb_screen_vertex_t *in = vertices;
        tomb_screen_vertex_t *out = scratch;
        uint32_t edge;
        ++renderer->stats.clipped;
        for (edge = 0u; edge < 4u && count >= 3u; ++edge) {
            const int horizontal = edge >= 2u; /* clipping against a y bound */
            const int keep_greater = (edge & 1u) == 0u;
            const float bound = edges[edge];
            uint32_t produced = 0u;
            for (c = 0u; c < count && produced + 2u <= TOMB_MAX_CLIP_VERTICES; ++c) {
                const tomb_screen_vertex_t *a = &in[c];
                const tomb_screen_vertex_t *b = &in[c + 1u == count ? 0u : c + 1u];
                const float da = (horizontal ? a->y : a->x) - bound;
                const float db = (horizontal ? b->y : b->x) - bound;
                const int a_in = keep_greater ? da >= 0.0f : da <= 0.0f;
                const int b_in = keep_greater ? db >= 0.0f : db <= 0.0f;
                if (a_in) out[produced++] = *a;
                if (a_in != b_in) {
                    const float t = da / (da - db);
                    tomb_screen_vertex_t *m = &out[produced++];
                    m->x = a->x + (b->x - a->x) * t;
                    m->y = a->y + (b->y - a->y) * t;
                    if (horizontal) {
                        m->y = bound;
                    } else {
                        m->x = bound;
                    }
                    m->u = a->u + (b->u - a->u) * t;
                    m->v = a->v + (b->v - a->v) * t;
                    m->light = a->light + (b->light - a->light) * t;
                    m->depth = a->depth + (b->depth - a->depth) * t;
                }
            }
            count = produced;
            {
                tomb_screen_vertex_t *swap = in;
                in = out;
                out = swap;
            }
        }
        vertices = in;
        if (count < 3u) {
            ++renderer->stats.culled;
            return;
        }
    }
    /* Fan from vertex 0: quads while three more corners remain, a triangle for
     * the last two. Every piece of a convex polygon is convex. */
    next = 1u;
    while (count - next >= 2u) {
        if (count - next >= 3u) {
            const tomb_screen_vertex_t quad[4] = {vertices[0], vertices[next],
                                                  vertices[next + 1u],
                                                  vertices[next + 2u]};
            queue(renderer, quad, 4u, face, group, depth);
            next += 2u;
        } else {
            const tomb_screen_vertex_t triangle[3] = {vertices[0], vertices[next],
                                                      vertices[next + 1u]};
            queue(renderer, triangle, 3u, face, group, depth);
            next += 1u;
        }
    }
}

static tomb_clip_vertex_t clip_midpoint(const tomb_clip_vertex_t *a,
                                        const tomb_clip_vertex_t *b) {
    tomb_clip_vertex_t out;
    out.p = tomb_vec3_scale(tomb_vec3_add(a->p, b->p), 0.5f);
    out.u = (a->u + b->u) * 0.5f;
    out.v = (a->v + b->v) * 0.5f;
    out.light = (a->light + b->light) * 0.5f;
    return out;
}

static void emit_view_polygon(tomb_renderer_t *renderer,
                              const tomb_clip_vertex_t *vertices, uint32_t count,
                              const tomb_face_t *face,
                              const tomb_submit_options_t *options,
                              tomb_rect_t scissor, float depth, uint8_t level) {
    tomb_screen_vertex_t screen[TOMB_MAX_CLIP_VERTICES];
    float min_x = 1e9f;
    float max_x = -1e9f;
    float min_y = 1e9f;
    float max_y = -1e9f;
    float min_z = 1e9f;
    float max_z = -1e9f;
    float area2 = 0.0f;
    uint32_t c;
    const int double_sided = (face->flags & TOMB_FACE_DOUBLE_SIDED) != 0u;
    for (c = 0u; c < count; ++c) {
        const tomb_clip_vertex_t *cv = &vertices[c];
        const float scale = renderer->camera.focal_length / cv->p.z;
        tomb_screen_vertex_t *sv = &screen[c];
        sv->x = renderer->center_x + cv->p.x * scale;
        sv->y = renderer->center_y - cv->p.y * scale;
        sv->u = cv->u;
        sv->v = cv->v;
        sv->light = cv->light;
        sv->depth = cv->p.z;
        if (sv->x < min_x) min_x = sv->x;
        if (sv->x > max_x) max_x = sv->x;
        if (sv->y < min_y) min_y = sv->y;
        if (sv->y > max_y) max_y = sv->y;
        if (cv->p.z < min_z) min_z = cv->p.z;
        if (cv->p.z > max_z) max_z = cv->p.z;
    }
    /* Screen-space winding (y down): a face listed counter-clockwise from its
     * front projects with negative doubled area when the front faces the camera. */
    for (c = 0u; c < count; ++c) {
        const tomb_screen_vertex_t *a = &screen[c];
        const tomb_screen_vertex_t *b = &screen[c + 1u == count ? 0u : c + 1u];
        area2 += a->x * b->y - b->x * a->y;
    }
    if (area2 == 0.0f || (!double_sided && area2 > 0.0f)) {
        ++renderer->stats.culled;
        return;
    }
    if (max_x <= (float)scissor.x ||
        min_x >= (float)(scissor.x + scissor.width) ||
        max_y <= (float)scissor.y ||
        min_y >= (float)(scissor.y + scissor.height)) {
        ++renderer->stats.culled;
        return;
    }
    if (level < renderer->config.subdivide_levels && count <= 4u &&
        max_z > min_z * renderer->config.subdivide_depth_ratio &&
        (max_x - min_x > (float)renderer->config.subdivide_min_pixels ||
         max_y - min_y > (float)renderer->config.subdivide_min_pixels)) {
        const uint8_t next = (uint8_t)(level + 1u);
        ++renderer->stats.subdivided;
        if (count == 4u) {
            const tomb_clip_vertex_t m01 = clip_midpoint(&vertices[0], &vertices[1]);
            const tomb_clip_vertex_t m12 = clip_midpoint(&vertices[1], &vertices[2]);
            const tomb_clip_vertex_t m23 = clip_midpoint(&vertices[2], &vertices[3]);
            const tomb_clip_vertex_t m30 = clip_midpoint(&vertices[3], &vertices[0]);
            const tomb_clip_vertex_t center = clip_midpoint(&m01, &m23);
            const tomb_clip_vertex_t quads[4][4] = {
                {vertices[0], m01, center, m30},
                {m01, vertices[1], m12, center},
                {center, m12, vertices[2], m23},
                {m30, center, m23, vertices[3]},
            };
            uint32_t q;
            for (q = 0; q < 4u; ++q)
                emit_view_polygon(renderer, quads[q], 4u, face, options,
                                  scissor, depth, next);
        } else {
            const tomb_clip_vertex_t m01 = clip_midpoint(&vertices[0], &vertices[1]);
            const tomb_clip_vertex_t m12 = clip_midpoint(&vertices[1], &vertices[2]);
            const tomb_clip_vertex_t m20 = clip_midpoint(&vertices[2], &vertices[0]);
            const tomb_clip_vertex_t triangles[4][3] = {
                {vertices[0], m01, m20},
                {m01, vertices[1], m12},
                {m20, m12, vertices[2]},
                {m01, m12, m20},
            };
            uint32_t triangle;
            for (triangle = 0; triangle < 4u; ++triangle)
                emit_view_polygon(renderer, triangles[triangle], 3u, face,
                                  options, scissor, depth, next);
        }
        return;
    }
    emit_screen_polygon(renderer, screen, count, face, options->group, depth,
                        scissor);
}

static void submit_face(tomb_renderer_t *renderer, const tomb_face_t *face,
                        const tomb_submit_options_t *options, tomb_rect_t scissor) {
    const uint32_t corners = face->vertex[3] == TOMB_FACE_TRIANGLE ? 3u : 4u;
    tomb_clip_vertex_t input[4];
    tomb_clip_vertex_t output[TOMB_MAX_CLIP_VERTICES];
    uint32_t behind = 0u;
    uint32_t count = 0u;
    uint32_t c;
    float depth_sum = 0.0f;
    float depth;
    for (c = 0u; c < corners; ++c) {
        tomb_clip_vertex_t *cv = &input[c];
        const float *vertex = &renderer->view_vertices[(size_t)face->vertex[c] * 3u];
        tomb_vec3_set(&cv->p, vertex[0], vertex[1], vertex[2]);
        cv->u = (float)face->u[c];
        cv->v = (float)face->v[c];
        cv->light = light_for(renderer, (float)face->brightness[c], cv->p.z);
        depth_sum += cv->p.z;
        if (cv->p.z < renderer->camera.near) ++behind;
    }
    if (behind == corners) {
        ++renderer->stats.culled;
        return;
    }
    depth = depth_sum / (float)corners + options->depth_bias;
    if (behind == 0u) {
        emit_view_polygon(renderer, input, corners, face, options, scissor,
                          depth, 0u);
        return;
    }
    /* Sutherland-Hodgman against z = near; a convex n-gon gains at most one corner. */
    {
        const float near = renderer->camera.near;
        for (c = 0u; c < corners; ++c) {
            const tomb_clip_vertex_t *a = &input[c];
            const tomb_clip_vertex_t *b = &input[c + 1u == corners ? 0u : c + 1u];
            const int a_in = a->p.z >= near;
            const int b_in = b->p.z >= near;
            if (a_in) output[count++] = *a;
            if (a_in != b_in) {
                const float t = (near - a->p.z) / (b->p.z - a->p.z);
                tomb_clip_vertex_t *m = &output[count++];
                m->p = tomb_vec3_add(a->p, tomb_vec3_scale(tomb_vec3_sub(b->p, a->p), t));
                m->p.z = near;
                m->u = a->u + (b->u - a->u) * t;
                m->v = a->v + (b->v - a->v) * t;
                m->light = a->light + (b->light - a->light) * t;
            }
        }
    }
    ++renderer->stats.clipped;
    if (count < 3u) {
        ++renderer->stats.culled;
        return;
    }
    emit_view_polygon(renderer, output, count, face, options, scissor, depth,
                      renderer->config.subdivide_levels);
}

/* ---- public API ------------------------------------------------------------ */

void tomb_renderer_init(tomb_renderer_t *renderer, uint8_t groups) {
    uint32_t index;
    renderer->list = 0;
    renderer->groups = groups == 0u ? 1u : (groups > TOMB_MAX_GROUPS ? TOMB_MAX_GROUPS : groups);
    renderer->config.width = 0;
    renderer->config.height = 0;
    renderer->center_x = 0.0f;
    renderer->center_y = 0.0f;
    renderer->bucket_scale = 0.0f;
    renderer->used = 0u;
    renderer->stats = (tomb_render_stats_t){0};
    renderer->camera = (tomb_camera_t){0};
    tomb_transform_identity(&renderer->world_to_view);
    for (index = 0u; index < TOMB_MAX_MESH_VERTICES * 3u; ++index) {
        renderer->view_vertices[index] = 0.0f;
    }
    for (index = 0u; index < TOMB_MAX_GROUPS * TOMB_RENDER_BUCKETS; ++index)
        renderer->buckets[index] = TOMB_NO_POLYGON;
}

void tomb_renderer_begin(tomb_renderer_t *renderer, pxa_raster_draw_list_t *list,
                         const tomb_camera_t *camera,
                         const tomb_lighting_t *lighting,
                         const tomb_render_config_t *config) {
    tomb_transform_t orientation;
    tomb_transform_t rotation_y;
    tomb_transform_t rotation_x;
    renderer->list = list;
    renderer->config = *config;
    renderer->lighting = *lighting;
    renderer->center_x = (float)config->width * 0.5f;
    renderer->center_y = (float)config->height * 0.5f;
    renderer->bucket_scale =
        (float)(TOMB_RENDER_BUCKETS - 1u) / renderer->config.far;
    renderer->camera = *camera;
    if (renderer->camera.near <= 0.0f) renderer->camera.near = 0.01f;
    if (renderer->camera.focal_length <= 0.0f) renderer->camera.focal_length = 1.0f;
    /* Camera local -> world is Ry(yaw) * Rx(pitch); world -> view is its
     * transpose applied to (world - position). */
    tomb_transform_rotation_y(&rotation_y, renderer->camera.yaw);
    tomb_transform_rotation_x(&rotation_x, renderer->camera.pitch);
    tomb_transform_mul(&orientation, &rotation_y, &rotation_x);
    {
        int row;
        int column;
        for (row = 0; row < 3; ++row) {
            for (column = 0; column < 3; ++column) {
                renderer->world_to_view.m[row][column] = orientation.m[column][row];
            }
        }
    }
    {
        const tomb_vec3_t negative = tomb_vec3_scale(renderer->camera.position, -1.0f);
        tomb_transform_rotate(&renderer->world_to_view, negative,
                              &renderer->world_to_view.t);
    }
    renderer->stats = (tomb_render_stats_t){0};
    renderer->used = 0u;
    {
        uint32_t index;
        for (index = 0u;
             index < (uint32_t)renderer->groups * TOMB_RENDER_BUCKETS;
             ++index)
            renderer->buckets[index] = TOMB_NO_POLYGON;
    }
}

uint32_t tomb_renderer_submit(tomb_renderer_t *renderer,
                              const tomb_vertex_t *vertices, uint32_t vertex_count,
                              const tomb_face_t *faces, uint32_t face_count,
                              const tomb_transform_t *transform,
                              const tomb_submit_options_t *options) {
    tomb_transform_t local_to_view;
    tomb_rect_t view;
    tomb_rect_t scissor;
    uint32_t index;
    if (vertex_count > TOMB_MAX_MESH_VERTICES ||
        options->group >= renderer->groups) {
        return 0u;
    }
    tomb_transform_mul(&local_to_view, &renderer->world_to_view, transform);
    for (index = 0u; index < vertex_count; ++index) {
        tomb_vec3_t point;
        float *out = &renderer->view_vertices[(size_t)index * 3u];
        tomb_vec3_set(&point, vertices[index].x, vertices[index].y, vertices[index].z);
        tomb_transform_apply(&local_to_view, point, &point);
        out[0] = point.x;
        out[1] = point.y;
        out[2] = point.z;
    }
    view.x = 0;
    view.y = 0;
    view.width = renderer->config.width;
    view.height = renderer->config.height;
    scissor = view;
    if (options->scissor.width > 0 && options->scissor.height > 0) {
        scissor = tomb_rect_intersect(options->scissor, view);
        if (scissor.width <= 0 || scissor.height <= 0) {
            renderer->stats.faces += face_count;
            renderer->stats.culled += face_count;
            return face_count;
        }
    }
    for (index = 0u; index < face_count; ++index) {
        const tomb_face_t *face = &faces[index];
        const uint32_t corners = face->vertex[3] == TOMB_FACE_TRIANGLE ? 3u : 4u;
        uint32_t c;
        int valid = 1;
        ++renderer->stats.faces;
        for (c = 0u; c < corners; ++c) {
            if (face->vertex[c] >= vertex_count) valid = 0;
        }
        if (!valid) {
            ++renderer->stats.culled;
            continue;
        }
        submit_face(renderer, face, options, scissor);
    }
    return face_count;
}

int tomb_renderer_flush(tomb_renderer_t *renderer) {
    uint32_t group;
    int ok = 1;
    for (group = 0u; group < renderer->groups; ++group) {
        uint32_t bucket = TOMB_RENDER_BUCKETS;
        while (bucket-- > 0u) {
            uint16_t slot_index =
                renderer->buckets[group * TOMB_RENDER_BUCKETS + bucket];
            while (slot_index != TOMB_NO_POLYGON) {
                const tomb_polygon_slot_t *slot =
                    &renderer->polygons[slot_index];
                const int flat =
                    (slot->flags & TOMB_FACE_FLAT_COLOR) != 0u;
                const uint8_t transparent =
                    (slot->flags & TOMB_FACE_TRANSPARENT) != 0u
                        ? PXA_RASTER_QUAD_TRANSPARENT_INDEX0
                        : 0u;
                const uint32_t record_bytes =
                    slot->count == 4u
                        ? PXA_RASTER_TEXTURED_QUAD_BYTES
                        : PXA_RASTER_TRIANGLE_BATCH_HEADER_BYTES +
                              PXA_RASTER_VERTEX_BYTES * 3u;
                if (!record_fits(renderer, record_bytes)) {
                    ++renderer->stats.dropped;
                    ok = 0;
                } else if (slot->count == 4u) {
                    if (flat) {
                        ok = pxa_raster_solid_painter_quad(
                                 renderer->list, slot->corners,
                                 slot->texture_slot) && ok;
                    } else {
                        ok = pxa_raster_textured_quad_flags(
                                 renderer->list, slot->corners,
                                 slot->texture_slot,
                                 PXA_RASTER_QUAD_PAINTER | transparent) && ok;
                    }
                } else {
                    ok = pxa_raster_triangle_batch_flags(
                             renderer->list, slot->corners, 1u,
                             flat ? 0u : slot->texture_slot,
                             PXA_RASTER_QUAD_PAINTER | transparent |
                                 (flat ? PXA_RASTER_QUAD_SOLID_COLOR : 0u),
                             flat ? slot->texture_slot : 0u) && ok;
                }
                slot_index = slot->next;
            }
        }
    }
    return ok;
}

void tomb_renderer_to_view(const tomb_renderer_t *renderer, tomb_vec3_t world,
                           tomb_vec3_t *out) {
    tomb_transform_apply(&renderer->world_to_view, world, out);
}

int tomb_renderer_project(const tomb_renderer_t *renderer, tomb_vec3_t world,
                          float *x_out, float *y_out, float *depth_out) {
    tomb_vec3_t view;
    float scale;
    tomb_renderer_to_view(renderer, world, &view);
    if (view.z < renderer->camera.near) return 0;
    scale = renderer->camera.focal_length / view.z;
    *x_out = renderer->center_x + view.x * scale;
    *y_out = renderer->center_y - view.y * scale;
    *depth_out = view.z;
    return 1;
}
