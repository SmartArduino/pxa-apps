#ifndef PXA_MAPPED_SURFACE_H
#define PXA_MAPPED_SURFACE_H

#include <stdint.h>

#define PXA_MAPPED_SURFACE_BUFFER_NONE UINT8_MAX

typedef struct {
    uint8_t writing_buffer;
    uint8_t host_owned_mask;
    uint64_t writing_frame_id;
} pxa_mapped_surface_t;

static inline void pxa_mapped_surface_reset(pxa_mapped_surface_t *state) {
    state->writing_buffer = PXA_MAPPED_SURFACE_BUFFER_NONE;
    state->host_owned_mask = 0;
    state->writing_frame_id = 0;
}

static inline int pxa_mapped_surface_begin(pxa_mapped_surface_t *state,
                                           uint8_t buffer_index,
                                           uint8_t buffer_count,
                                           uint64_t frame_id) {
    uint8_t bit;
    if (state == 0 || buffer_count < 2 || buffer_count > 8 ||
        buffer_index >= buffer_count || frame_id == 0 ||
        state->writing_buffer != PXA_MAPPED_SURFACE_BUFFER_NONE) {
        return 0;
    }
    bit = (uint8_t)(1u << buffer_index);
    if ((state->host_owned_mask & bit) != 0) return 0;
    state->writing_buffer = buffer_index;
    state->writing_frame_id = frame_id;
    return 1;
}

static inline int pxa_mapped_surface_presented(pxa_mapped_surface_t *state,
                                               uint8_t buffer_count) {
    uint8_t bit;
    if (state == 0 || buffer_count < 2 || buffer_count > 8 ||
        state->writing_buffer >= buffer_count ||
        state->writing_frame_id == 0) {
        return 0;
    }
    bit = (uint8_t)(1u << state->writing_buffer);
    if ((state->host_owned_mask & bit) != 0) return 0;
    state->host_owned_mask |= bit;
    state->writing_buffer = PXA_MAPPED_SURFACE_BUFFER_NONE;
    state->writing_frame_id = 0;
    return 1;
}

static inline int pxa_mapped_surface_released(pxa_mapped_surface_t *state,
                                              uint8_t buffer_index,
                                              uint8_t buffer_count) {
    uint8_t bit;
    if (state == 0 || buffer_count < 2 || buffer_count > 8 ||
        buffer_index >= buffer_count) {
        return 0;
    }
    bit = (uint8_t)(1u << buffer_index);
    if ((state->host_owned_mask & bit) == 0) return 0;
    state->host_owned_mask &= (uint8_t)~bit;
    return 1;
}

#endif
