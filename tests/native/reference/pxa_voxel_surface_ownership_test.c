#include <assert.h>

#include "surface_ownership.h"

int main(void) {
    voxel_surface_ownership_t ownership;
    voxel_surface_ownership_reset(&ownership);
    assert(ownership.writing_buffer == VOXEL_SURFACE_BUFFER_NONE);
    assert(!voxel_surface_can_recreate(&ownership));

    assert(voxel_surface_begin_write(&ownership, 0, 3, 1));
    assert(!voxel_surface_begin_write(&ownership, 1, 3, 2));
    assert(voxel_surface_mark_presented(&ownership, 3));
    assert(ownership.host_owned_mask == 1u);

    assert(voxel_surface_begin_write(&ownership, 1, 3, 2));
    assert(voxel_surface_mark_presented(&ownership, 3));
    assert(ownership.host_owned_mask == 3u);
    assert(!voxel_surface_begin_write(&ownership, 0, 3, 3));

    voxel_surface_request_recreate(&ownership);
    assert(!voxel_surface_can_recreate(&ownership));
    assert(voxel_surface_mark_released(&ownership, 0, 3));
    assert(!voxel_surface_can_recreate(&ownership));
    assert(voxel_surface_mark_released(&ownership, 1, 3));
    assert(voxel_surface_can_recreate(&ownership));

    voxel_surface_ownership_reset(&ownership);
    assert(!voxel_surface_begin_write(&ownership, 3, 3, 1));
    assert(!voxel_surface_begin_write(&ownership, UINT8_MAX, 3, 1));
    assert(!voxel_surface_begin_write(&ownership, 7, 9, 1));
    assert(!voxel_surface_begin_write(&ownership, 0, 3, 0));
    assert(!voxel_surface_mark_released(&ownership, 0, 3));
    assert(!voxel_surface_mark_released(&ownership, UINT8_MAX, 3));
    ownership.writing_buffer = UINT8_MAX;
    ownership.writing_frame_id = 3;
    assert(!voxel_surface_mark_presented(&ownership, 3));
    return 0;
}
