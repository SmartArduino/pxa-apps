/* PXA Game Boy video renderer. MPL-2.0 (gnuboy frames, PXA frontend). */
#pragma once
#include <stdint.h>

#include "core.h"
#include "display.h"

typedef struct {
    unsigned width, height;
    unsigned stride;            /* Surface row stride in pixels. */
    unsigned view_x, view_y, view_width, view_height;
    uint16_t sx[GB_DISPLAY_MAX_WIDTH];
    uint16_t sy[GB_DISPLAY_MAX_HEIGHT];
    unsigned fit;
    /* A separate source snapshot for each acquired Surface buffer. */
    unsigned cache_valid;
    uint16_t previous[2][GB_VIDEO_WIDTH*GB_VIDEO_HEIGHT];
} gb_video_renderer_t;

void gb_video_init(gb_video_renderer_t *r, unsigned width, unsigned height,
                   gb_display_rect_t area);
void gb_video_set_stride(gb_video_renderer_t *r, unsigned stride);
void gb_video_set_fit(gb_video_renderer_t *r, gb_display_rect_t area, unsigned fit);
/* Core already supplies RGB565: no conversion buffer or frame upload. */
void gb_video_render(gb_video_renderer_t *r, uint16_t *out, const uint16_t *rgb565);
unsigned gb_video_render_cached(gb_video_renderer_t *r,uint16_t *out,const uint16_t *rgb565,
                                unsigned buffer,unsigned force,uint8_t *dirty_rows);
/* Restore selected output rows underneath changed translucent controls. */
unsigned gb_video_render_cached_rows(gb_video_renderer_t *r,uint16_t *out,const uint16_t *rgb565,
                                     unsigned buffer,unsigned force,const uint8_t *restore_rows,
                                     uint8_t *dirty_rows);
