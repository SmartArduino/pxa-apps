#ifndef PXA_CANVAS_PIXELS_H
#define PXA_CANVAS_PIXELS_H
#include <stdint.h>

/* Compatibility for existing C raster games. Canvas input uses dp; a native
 * Surface HUD uses pixels. Keep density from the UI startup/environment event.
 * New C++ games use pxa::ui::canvas_to_surface_coordinate instead. */
static inline int32_t pxa_game_canvas_to_surface_coordinate(int32_t value,
                                                            uint32_t density_q16) {
    const int64_t scaled = (int64_t)value *
        (density_q16 != 0 ? density_q16 : UINT32_C(65536)) / INT64_C(65536);
    if (scaled > INT32_MAX) return INT32_MAX;
    if (scaled < INT32_MIN) return INT32_MIN;
    return (int32_t)scaled;
}
#endif
