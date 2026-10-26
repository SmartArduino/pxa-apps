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

/* Match the centered integer-scaled Surface, then map its pixels to the
 * game's 320x240 controls. Outside presses must not activate edge buttons;
 * clamped coordinates still let an existing drag release outside the view. */
static inline int pxa_game_pointer_coordinates(int32_t x,int32_t y,uint32_t density,
    uint32_t display_w,uint32_t display_h,uint32_t surface_w,uint32_t surface_h,
    unsigned scale,int *logical_x,int *logical_y) {
    if(!scale || !surface_w || !surface_h) {*logical_x=*logical_y=0;return 0;}
    int64_t w=(int64_t)surface_w*scale,h=(int64_t)surface_h*scale;
    int64_t px=pxa_game_canvas_to_surface_coordinate(x,density)-((int64_t)display_w-w)/2;
    int64_t py=pxa_game_canvas_to_surface_coordinate(y,density)-((int64_t)display_h-h)/2;
    int inside=px>=0 && py>=0 && px<w && py<h;
    *logical_x=px<0?0:px>=w?319:(int)(px*320/w);
    *logical_y=py<0?0:py>=h?239:(int)(py*240/h);
    return inside;
}
#endif
