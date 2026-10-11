#include "world.h"

/* Camera distance to a portal plane below which the portal covers the view. */
#define TOMB_DOORWAY_DISTANCE 0.6f
#define TOMB_SECTOR_SIZE 1.0f

static int32_t floor_int(float value) {
    const int32_t truncated = (int32_t)value;
    return (float)truncated > value ? truncated - 1 : truncated;
}

static int portal_screen_rect(const tomb_renderer_t *renderer,
                              const tomb_portal_t *portal, tomb_rect_t scissor,
                              tomb_rect_t *rect_out) {
    /* The portal faces into its room; a camera on or beyond its plane cannot
     * look through it from this side. */
    const tomb_vec3_t edge_right =
        tomb_vec3_sub(portal->corners[1], portal->corners[0]);
    const tomb_vec3_t edge_up =
        tomb_vec3_sub(portal->corners[3], portal->corners[0]);
    const tomb_vec3_t normal = tomb_vec3_cross(edge_right, edge_up);
    const float signed_distance =
        tomb_vec3_dot(tomb_vec3_sub(renderer->camera.position, portal->corners[0]),
                      normal) /
        tomb_vec3_length(normal);
    tomb_vec3_t view[4];
    tomb_vec3_t clipped[8];
    uint32_t count = 0u;
    float focal;
    float center_x;
    float center_y;
    float min_x = 1e9f;
    float max_x = -1e9f;
    float min_y = 1e9f;
    float max_y = -1e9f;
    float limit = 1e6f;
    tomb_rect_t bounds;
    uint32_t c;
    if (signed_distance >= 0.0f) return 0;
    if (-signed_distance < TOMB_DOORWAY_DISTANCE) {
        /* The camera is about to pass through: the near plane may already cut
         * into the next room, so it gets the whole current view. */
        *rect_out = scissor;
        return 1;
    }
    /* Clip the quad against the near plane in view space, then project what is
     * left: a doorway the camera stands in still yields a tight rectangle. */
    for (c = 0u; c < 4u; ++c) {
        tomb_renderer_to_view(renderer, portal->corners[c], &view[c]);
    }
    for (c = 0u; c < 4u; ++c) {
        const tomb_vec3_t *a = &view[c];
        const tomb_vec3_t *b = &view[(c + 1u) & 3u];
        const int a_in = a->z >= renderer->camera.near;
        const int b_in = b->z >= renderer->camera.near;
        if (a_in) clipped[count++] = *a;
        if (a_in != b_in) {
            const float t =
                (renderer->camera.near - a->z) / (b->z - a->z);
            tomb_vec3_t m =
                tomb_vec3_add(*a, tomb_vec3_scale(tomb_vec3_sub(*b, *a), t));
            m.z = renderer->camera.near;
            clipped[count++] = m;
        }
    }
    if (count < 3u) return 0;
    focal = renderer->camera.focal_length;
    center_x = (float)renderer->config.width * 0.5f;
    center_y = (float)renderer->config.height * 0.5f;
    for (c = 0u; c < count; ++c) {
        const float scale = focal / clipped[c].z;
        const float x = center_x + clipped[c].x * scale;
        const float y = center_y - clipped[c].y * scale;
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    /* Clamp before converting: a corner at the near plane can project far out. */
    if (min_x < -limit) min_x = -limit;
    if (min_y < -limit) min_y = -limit;
    if (max_x > limit) max_x = limit;
    if (max_y > limit) max_y = limit;
    bounds.x = floor_int(min_x);
    bounds.y = floor_int(min_y);
    bounds.width = floor_int(max_x) - floor_int(min_x) + 1;
    bounds.height = floor_int(max_y) - floor_int(min_y) + 1;
    *rect_out = tomb_rect_intersect(bounds, scissor);
    return rect_out->width > 0 && rect_out->height > 0;
}

uint32_t tomb_world_compute_visible(const tomb_level_t *level,
                                    const tomb_renderer_t *renderer,
                                    uint8_t camera_room, tomb_rect_t view,
                                    tomb_visible_room_t *out,
                                    uint32_t capacity) {
    struct frame {
        uint8_t room;
        uint8_t from;
        uint8_t depth;
        tomb_rect_t scissor;
    } frames[TOMB_MAX_VISIBLE];
    uint32_t count = 1u;
    uint32_t head;
    uint32_t i;
    if (level == 0 || camera_room >= level->room_count || capacity == 0u) return 0u;
    if (capacity > TOMB_MAX_VISIBLE) capacity = TOMB_MAX_VISIBLE;
    /* Breadth-first over portals: `frames` doubles as the queue and the visit
     * list, so each room is entered once, through the nearest portal chain. */
    frames[0].room = camera_room;
    frames[0].from = TOMB_NO_ROOM;
    frames[0].depth = 0u;
    frames[0].scissor = view;
    for (head = 0u; head < count && count < capacity; ++head) {
        const struct frame frame = frames[head];
        const tomb_room_t *room = &level->rooms[frame.room];
        if (frame.depth >= TOMB_MAX_PORTAL_DEPTH) continue;
        for (i = 0u; i < room->portal_count; ++i) {
            const tomb_portal_t *portal = &room->portals[i];
            uint32_t visited_index;
            int visited = 0;
            tomb_rect_t rect;
            if (count >= capacity) break;
            for (visited_index = 0u; visited_index < count; ++visited_index) {
                if (frames[visited_index].room == portal->target_room) visited = 1;
            }
            if (visited) continue;
            if (!portal_screen_rect(renderer, portal, frame.scissor, &rect)) continue;
            frames[count].room = portal->target_room;
            frames[count].from = frame.room;
            frames[count].depth = (uint8_t)(frame.depth + 1u);
            frames[count].scissor = rect;
            ++count;
        }
    }
    /* Farthest first: reverse breadth-first order (depth never decreases along
     * the queue), so every room gets a group after the rooms seen through it. */
    for (i = 0u; i < count; ++i) {
        const struct frame *frame = &frames[count - 1u - i];
        out[i].room = frame->room;
        out[i].group = (uint8_t)i;
        out[i].depth = frame->depth;
        out[i].scissor = frame->scissor;
    }
    return count;
}

uint8_t tomb_world_room_at(const tomb_level_t *level, float x, float z,
                           uint8_t hint) {
    uint32_t index;
    if (level == 0) return TOMB_NO_ROOM;
    if (hint < level->room_count) {
        const tomb_room_t *room = &level->rooms[hint];
        if (x >= room->origin_x && z >= room->origin_z &&
            x < room->origin_x + (float)room->width * TOMB_SECTOR_SIZE &&
            z < room->origin_z + (float)room->depth * TOMB_SECTOR_SIZE)
            return hint;
    }
    for (index = 0u; index < level->room_count; ++index) {
        const tomb_room_t *room = &level->rooms[index];
        if (x >= room->origin_x && z >= room->origin_z &&
            x < room->origin_x + (float)room->width * TOMB_SECTOR_SIZE &&
            z < room->origin_z + (float)room->depth * TOMB_SECTOR_SIZE)
            return (uint8_t)index;
    }
    return TOMB_NO_ROOM;
}

int tomb_world_heights_at(const tomb_level_t *level, uint8_t room_index,
                          float x, float z, float *floor_out,
                          float *ceiling_out) {
    const tomb_room_t *room;
    const tomb_sector_t *sector;
    float local_x;
    float local_z;
    float fx;
    float fz;
    if (level == 0 || room_index >= level->room_count) return 0;
    room = &level->rooms[room_index];
    if (x < room->origin_x || z < room->origin_z ||
        x >= room->origin_x + (float)room->width * TOMB_SECTOR_SIZE ||
        z >= room->origin_z + (float)room->depth * TOMB_SECTOR_SIZE)
        return 0;
    local_x = (x - room->origin_x) / TOMB_SECTOR_SIZE;
    local_z = (z - room->origin_z) / TOMB_SECTOR_SIZE;
    sector = &room->sectors[(size_t)(int)local_z * room->width + (size_t)(int)local_x];
    if (sector->solid) return 0;
    fx = local_x - (float)floor_int(local_x);
    fz = local_z - (float)floor_int(local_z);
    {
        const float north = sector->floor[0] * (1.0f - fx) + sector->floor[1] * fx;
        const float south = sector->floor[3] * (1.0f - fx) + sector->floor[2] * fx;
        const float north_c =
            sector->ceiling[0] * (1.0f - fx) + sector->ceiling[1] * fx;
        const float south_c =
            sector->ceiling[3] * (1.0f - fx) + sector->ceiling[2] * fx;
        *floor_out = north * (1.0f - fz) + south * fz;
        *ceiling_out = north_c * (1.0f - fz) + south_c * fz;
    }
    return 1;
}
