/* GPL-2.0-or-later */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "controls.h"
#define NES_VIDEO_HISTORY_BYTES (256u*240u)
/* Synchronous resolve into an acquired Surface. Returning zero selects the
 * portable renderer; no pointer may be retained after this call. */
typedef int (*nes_video_resolve_t)(uint16_t *target,unsigned target_pitch,
    unsigned width,unsigned height,const uint8_t *source,unsigned source_pitch);
typedef struct {
    unsigned width,height;
    unsigned view_x,view_y,view_width,view_height;
    uint8_t sx[NES_DISPLAY_MAX_WIDTH],sy[NES_DISPLAY_MAX_HEIGHT];
    uint16_t xedge[33],yedge[31];
    uint32_t dirty[2][30],overlay[30];
    uint32_t button_overlay[NES_CONTROLS_COUNT][30];
    uint8_t valid[2],alpha[2],pressed[2],history_valid;
    uint8_t *history;
    unsigned painted_pixels,painted_tiles;
    nes_video_resolve_t resolve;
} nes_video_renderer_t;
/* History can use the unused tail of the reserved Surface array. A missing
 * history disables reuse without changing the output or allocating memory. */
void nes_video_init(nes_video_renderer_t *r,unsigned width,unsigned height,
                    void *history,size_t bytes,const nes_controls_t *controls);
void nes_video_init_in_rect(nes_video_renderer_t *r,unsigned width,unsigned height,
                    nes_display_rect_t area,void *history,size_t bytes,const nes_controls_t *controls);
void nes_video_reset(nes_video_renderer_t *r);
void nes_video_invalidate(nes_video_renderer_t *r,unsigned buffer);
void nes_video_render(nes_video_renderer_t *r,uint16_t *out,unsigned buffer,
                      const uint8_t *video,unsigned pitch,
                      nes_controls_t *controls,uint8_t pressed);
