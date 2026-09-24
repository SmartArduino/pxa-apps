#ifndef TOMB_LEVEL_H
#define TOMB_LEVEL_H

#include <stdint.h>

/* Read-only level produced by tools/generate_level.py from tools/level.json.
 *
 * A room is a grid of 1 x 1 sectors in the x/z plane with a floor and a
 * ceiling height at each sector corner (so floors may slope), its static mesh
 * (floor, ceiling, wall and step quads with baked vertex light) and the
 * portals that open onto neighbouring rooms. World y is up; a sector column
 * with floor == ceiling is solid (a pillar or the room's outline). */

#define TOMB_NO_ROOM 0xFFu

typedef struct {
    float x, y, z;
} tomb_vec3_t;

typedef tomb_vec3_t tomb_vertex_t;

enum {
    /* Texel index 0 is not drawn. */
    TOMB_FACE_TRANSPARENT = 1u << 0u,
    /* No texture: palette entry u[0] at the interpolated light. */
    TOMB_FACE_FLAT_COLOR = 1u << 1u,
    /* Drawn from both sides (no back-face culling). */
    TOMB_FACE_DOUBLE_SIDED = 1u << 2u,
};

typedef struct {
    uint16_t vertex[4]; /* [3] == 0xFFFF for a triangle */
    uint16_t u[4];
    uint16_t v[4];
    uint8_t brightness[4];
    uint8_t texture;
    uint8_t flags;
} tomb_face_t;

typedef struct {
    /* Corner heights in the order north-west, north-east, south-east,
     * south-west, where north is -z and east is +x. */
    float floor[4];
    float ceiling[4];
    uint8_t solid;
} tomb_sector_t;

typedef struct {
    /* World-space quad, counter-clockwise as seen from inside the owning room. */
    tomb_vertex_t corners[4];
    uint8_t target_room;
} tomb_portal_t;

typedef struct {
    /* World position of the room's north-west sector corner. */
    float origin_x;
    float origin_z;
    uint8_t width;  /* sectors along +x */
    uint8_t depth;  /* sectors along +z */
    uint8_t ambient;
    uint8_t portal_count;
    uint16_t sector_count;
    const tomb_sector_t *sectors;
    uint16_t vertex_count;
    const tomb_vertex_t *vertices;
    uint16_t face_count;
    const tomb_face_t *faces;
    const tomb_portal_t *portals;
} tomb_room_t;

typedef struct {
    const tomb_room_t *rooms;
    uint8_t room_count;
    /* Player spawn. */
    tomb_vertex_t start;
    float start_yaw;
    uint8_t start_room;
} tomb_level_t;

const tomb_level_t *tomb_level(void);

#endif
