#ifndef TOMB_WORLD_H
#define TOMB_WORLD_H

#include <stdint.h>

#include "level.h"
#include "mesh.h"

/* Visibility and collision queries over the static level, ported from
 * micropixel guest/apps/tomb-explorer/world/room_world.cpp: which rooms to
 * draw through which portal rectangles, and floor/ceiling heights for
 * movement. */

#define TOMB_MAX_VISIBLE TOMB_MAX_GROUPS
#define TOMB_MAX_PORTAL_DEPTH 4u

/* A room the camera can see this frame. Rooms are returned far to near, so
 * `group` simply counts up along the list; dynamic objects standing in the
 * room submit with the same group and scissor. */
typedef struct {
    uint8_t room;
    uint8_t group;
    uint8_t depth; /* portals crossed from the camera room */
    tomb_rect_t scissor;
} tomb_visible_room_t;

/* Walks the portals from `camera_room` with the renderer's current camera
 * (after tomb_renderer_begin). `view` is the buffer rectangle. Returns the
 * number of rooms written, farthest first. */
uint32_t tomb_world_compute_visible(const tomb_level_t *level,
                                    const tomb_renderer_t *renderer,
                                    uint8_t camera_room, tomb_rect_t view,
                                    tomb_visible_room_t *out, uint32_t capacity);

/* Room whose footprint holds (x, z), trying `hint` first. TOMB_NO_ROOM outside
 * every room. */
uint8_t tomb_world_room_at(const tomb_level_t *level, float x, float z,
                           uint8_t hint);
/* Floor and ceiling height under (x, z) in `room`. Returns 0 on a solid sector
 * or outside the room. */
int tomb_world_heights_at(const tomb_level_t *level, uint8_t room, float x,
                          float z, float *floor_out, float *ceiling_out);

#endif
