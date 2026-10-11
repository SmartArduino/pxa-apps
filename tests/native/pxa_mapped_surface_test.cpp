#include "pxa_mapped_surface.h"

#include <assert.h>

int main(void) {
    pxa_mapped_surface_t state;

    pxa_mapped_surface_reset(&state);
    assert(state.writing_buffer == PXA_MAPPED_SURFACE_BUFFER_NONE);
    assert(pxa_mapped_surface_begin(&state, 1, 3, 10));
    assert(!pxa_mapped_surface_begin(&state, 2, 3, 11));
    assert(pxa_mapped_surface_presented(&state, 3));
    assert(state.host_owned_mask == (uint8_t)(1u << 1));
    assert(!pxa_mapped_surface_begin(&state, 1, 3, 11));
    assert(pxa_mapped_surface_begin(&state, 2, 3, 11));
    assert(pxa_mapped_surface_presented(&state, 3));
    assert(!pxa_mapped_surface_released(&state, 0, 3));
    assert(pxa_mapped_surface_released(&state, 1, 3));
    assert(pxa_mapped_surface_released(&state, 2, 3));
    assert(state.host_owned_mask == 0);

    assert(!pxa_mapped_surface_begin(&state, 3, 3, 12));
    assert(!pxa_mapped_surface_begin(&state, 0, 1, 12));
    assert(!pxa_mapped_surface_begin(&state, 0, 3, 0));
    return 0;
}
