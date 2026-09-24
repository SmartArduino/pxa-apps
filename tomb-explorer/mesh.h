#ifndef TOMB_MESH_H
#define TOMB_MESH_H

#include <stdint.h>

#include "level.h"
#include "pxa_raster.h"

/* PS1-style polygon front end over the Host GameRender raster records, ported
 * from micropixel guest/runtime/mesh_renderer.cpp. The App owns meshes
 * (vertices + textured, per-corner lit faces) and their transforms; the
 * renderer runs the per-frame geometry: view transform, near-plane clipping,
 * back-face culling, distance lighting, exact clipping to a scissor rectangle
 * and subdivision of large near faces, then sorts whole polygons far to near
 * in an ordering table. The Host consumes painter polygons with affine UVs and
 * no depth buffer, matching the original renderer.
 *
 * Coordinates: world and view space are x right, y up, z forward (into the
 * screen). Faces list their corners counter-clockwise as seen from the front.
 * A camera at the origin with yaw 0 and pitch 0 looks along +z. */

#define TOMB_MAX_MESH_VERTICES 1024u
#define TOMB_MAX_GROUPS 8u
#define TOMB_MAX_CLIP_VERTICES 12u
#define TOMB_FACE_TRIANGLE 0xFFFFu
#define TOMB_RENDER_BUCKETS 512u
#define TOMB_POLYGON_POOL 2048u
#define TOMB_NO_POLYGON 0xFFFFu

/* Rigid transform: world = rotation * local + translation. */
typedef struct {
    float m[3][3];
    tomb_vec3_t t;
} tomb_transform_t;

typedef struct {
    tomb_vec3_t position;
    float yaw;   /* radians about +y; 0 looks along +z, positive turns towards +x */
    float pitch; /* radians; positive looks up */
    float focal_length;
    float near;
} tomb_camera_t;

typedef struct {
    uint8_t levels;
    uint8_t minimum;
    float full_distance;
    float dark_distance;
} tomb_lighting_t;

typedef struct {
    int x, y;
    int width, height;
} tomb_rect_t;

typedef struct {
    int width;
    int height;
    float far;
    float subdivide_depth_ratio;
    int subdivide_min_pixels;
    uint8_t subdivide_levels;
} tomb_render_config_t;

typedef struct {
    /* Ordering group; rooms are submitted far to near and dynamic objects use
     * the group of the room they stand in. */
    uint8_t group;
    /* Added to the depth key before emission: negative draws nearer. */
    float depth_bias;
    /* Polygons are clipped exactly to this rectangle (buffer pixels); an empty
     * rectangle means the whole view. Portals set their screen bounds here. */
    tomb_rect_t scissor;
} tomb_submit_options_t;

typedef struct {
    uint32_t faces;          /* faces submitted */
    uint32_t culled;         /* back-facing, behind the camera or outside the scissor */
    uint32_t clipped;        /* faces that touched the near plane or scissor */
    uint32_t subdivided;     /* faces split to reduce affine texture warp */
    uint32_t polygons;       /* records queued */
    uint32_t dropped;        /* polygons lost to a full draw list */
    uint32_t pixel_estimate; /* screen-space area of the queued polygons */
} tomb_render_stats_t;

typedef struct {
    pxa_raster_vertex_t corners[4];
    uint16_t next;
    uint8_t count;
    uint8_t texture_slot;
    uint8_t flags;
} tomb_polygon_slot_t;

typedef struct {
    pxa_raster_draw_list_t *list;
    tomb_render_config_t config;
    tomb_lighting_t lighting;
    tomb_camera_t camera;
    tomb_transform_t world_to_view;
    float center_x;
    float center_y;
    float bucket_scale;
    uint16_t used;
    uint8_t groups;
    tomb_render_stats_t stats;
    tomb_polygon_slot_t polygons[TOMB_POLYGON_POOL];
    uint16_t buckets[TOMB_MAX_GROUPS * TOMB_RENDER_BUCKETS];
    float view_vertices[TOMB_MAX_MESH_VERTICES * 3u];
} tomb_renderer_t;

void tomb_vec3_set(tomb_vec3_t *out, float x, float y, float z);
tomb_vec3_t tomb_vec3_sub(tomb_vec3_t a, tomb_vec3_t b);
tomb_vec3_t tomb_vec3_scale(tomb_vec3_t v, float scale);
tomb_vec3_t tomb_vec3_add(tomb_vec3_t a, tomb_vec3_t b);
float tomb_vec3_dot(tomb_vec3_t a, tomb_vec3_t b);
tomb_vec3_t tomb_vec3_cross(tomb_vec3_t a, tomb_vec3_t b);
float tomb_vec3_length(tomb_vec3_t v);

void tomb_transform_identity(tomb_transform_t *out);
void tomb_transform_translation(tomb_transform_t *out, tomb_vec3_t offset);
void tomb_transform_rotation_x(tomb_transform_t *out, float radians);
void tomb_transform_rotation_y(tomb_transform_t *out, float radians);
void tomb_transform_rotation_z(tomb_transform_t *out, float radians);
void tomb_transform_uniform(tomb_transform_t *out, tomb_vec3_t translation,
                            float yaw, float pitch, float roll);
/* out = parent * child (the child is applied first). */
void tomb_transform_mul(tomb_transform_t *out, const tomb_transform_t *parent,
                        const tomb_transform_t *child);
void tomb_transform_apply(const tomb_transform_t *transform, tomb_vec3_t point,
                          tomb_vec3_t *out);
void tomb_transform_rotate(const tomb_transform_t *transform, tomb_vec3_t point,
                           tomb_vec3_t *out);

float tomb_camera_focal_length(float horizontal_fov_radians, int view_width);

void tomb_renderer_init(tomb_renderer_t *renderer, uint8_t groups);
void tomb_renderer_begin(tomb_renderer_t *renderer, pxa_raster_draw_list_t *list,
                         const tomb_camera_t *camera,
                         const tomb_lighting_t *lighting,
                         const tomb_render_config_t *config);
/* Queues every visible face of a mesh. Returns the number of faces submitted,
 * or 0 when the mesh is too large or the group is out of range. */
uint32_t tomb_renderer_submit(tomb_renderer_t *renderer,
                              const tomb_vertex_t *vertices, uint32_t vertex_count,
                              const tomb_face_t *faces, uint32_t face_count,
                              const tomb_transform_t *transform,
                              const tomb_submit_options_t *options);
/* Appends queued polygons in painter order (groups ascending, depth far to
 * near). Returns zero if the draw list ran out of room. */
int tomb_renderer_flush(tomb_renderer_t *renderer);

void tomb_renderer_to_view(const tomb_renderer_t *renderer, tomb_vec3_t world,
                           tomb_vec3_t *out);
/* Returns 0 when the point is not in front of the near plane. */
int tomb_renderer_project(const tomb_renderer_t *renderer, tomb_vec3_t world,
                          float *x_out, float *y_out, float *depth_out);

tomb_rect_t tomb_rect_intersect(tomb_rect_t a, tomb_rect_t b);

#endif
