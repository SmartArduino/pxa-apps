/* Dynamic console geometry and mapped RGB565 surfaces. MPL-2.0. */
#pragma once
#include "core.h"
#include "display.h"
typedef struct {
    unsigned width,height,stride,view_x,view_y,view_width,view_height,fit;
    unsigned source_width,source_height,source_stride,aspect_num,aspect_den;
    uint16_t sx[EMU_DISPLAY_MAX_WIDTH],sy[EMU_DISPLAY_MAX_HEIGHT];
    uint16_t *previous[2];size_t previous_pixels;
    unsigned cache_valid;
    emu_display_rect_t area;
} emu_video_renderer_t;
void emu_video_init(emu_video_renderer_t *,unsigned,unsigned,emu_display_rect_t);
void emu_video_deinit(emu_video_renderer_t *);
void emu_video_set_stride(emu_video_renderer_t *,unsigned);
void emu_video_set_fit(emu_video_renderer_t *,emu_display_rect_t,unsigned);
void emu_video_sync(emu_video_renderer_t *);
unsigned emu_video_render_cached(emu_video_renderer_t *,uint16_t *,const uint16_t *,unsigned,unsigned,uint8_t *);
void emu_video_render(emu_video_renderer_t *,uint16_t *,const uint16_t *);
