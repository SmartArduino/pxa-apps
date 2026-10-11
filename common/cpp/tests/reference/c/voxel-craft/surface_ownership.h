#ifndef VOXEL_CRAFT_SURFACE_OWNERSHIP_H
#define VOXEL_CRAFT_SURFACE_OWNERSHIP_H

#include <stdint.h>

#define VOXEL_SURFACE_BUFFER_NONE UINT8_MAX

typedef struct {
    uint8_t writing_buffer;
    uint8_t host_owned_mask;
    uint8_t recreate_pending;
    uint64_t writing_frame_id;
} voxel_surface_ownership_t;

static inline void voxel_surface_ownership_reset(
    voxel_surface_ownership_t *ownership) {
    ownership->writing_buffer = VOXEL_SURFACE_BUFFER_NONE;
    ownership->host_owned_mask = 0;
    ownership->recreate_pending = 0;
    ownership->writing_frame_id = 0;
}

static inline int voxel_surface_begin_write(
    voxel_surface_ownership_t *ownership, uint8_t buffer_index,
    uint8_t buffer_count, uint64_t frame_id) {
    uint8_t bit;
    if (buffer_index >= buffer_count || buffer_index >= 8 ||
        buffer_count > 8 || frame_id == 0 ||
        ownership->writing_buffer != VOXEL_SURFACE_BUFFER_NONE ||
        ownership->host_owned_mask == UINT8_MAX) {
        return 0;
    }
    bit = (uint8_t)(1u << buffer_index);
    if ((ownership->host_owned_mask & bit) != 0) return 0;
    ownership->writing_buffer = buffer_index;
    ownership->writing_frame_id = frame_id;
    return 1;
}

static inline int voxel_surface_mark_presented(
    voxel_surface_ownership_t *ownership, uint8_t buffer_count) {
    const uint8_t buffer_index = ownership->writing_buffer;
    if (buffer_index >= buffer_count || buffer_index >= 8 ||
        buffer_count > 8 ||
        ownership->writing_frame_id == 0) {
        return 0;
    }
    ownership->host_owned_mask |= (uint8_t)(1u << buffer_index);
    ownership->writing_buffer = VOXEL_SURFACE_BUFFER_NONE;
    ownership->writing_frame_id = 0;
    return 1;
}

static inline int voxel_surface_mark_released(
    voxel_surface_ownership_t *ownership, uint8_t buffer_index,
    uint8_t buffer_count) {
    uint8_t bit;
    if (buffer_index >= buffer_count || buffer_index >= 8 ||
        buffer_count > 8) {
        return 0;
    }
    bit = (uint8_t)(1u << buffer_index);
    if ((ownership->host_owned_mask & bit) == 0) return 0;
    ownership->host_owned_mask &= (uint8_t)~bit;
    return 1;
}

static inline void voxel_surface_request_recreate(
    voxel_surface_ownership_t *ownership) {
    ownership->recreate_pending = 1;
}

static inline int voxel_surface_can_recreate(
    const voxel_surface_ownership_t *ownership) {
    return ownership->recreate_pending && ownership->host_owned_mask == 0;
}

#endif
