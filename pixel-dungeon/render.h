#ifndef PD_RENDER_H
#define PD_RENDER_H

#include <stdint.h>

#include "game.h"
#include "layout.h"
#include "pxa_raster.h"

/* Texture slots. Terrain and stitched walls each contain all five regions;
 * remembered terrain is darkened with a painter overlay. */
#define PD_TEXTURE_TILES 0
#define PD_TEXTURE_SPRITES 1
#define PD_TEXTURE_FONT_ASCII 2
#define PD_TEXTURE_FONT_CJK 3
#define PD_TEXTURE_FOG 4
#define PD_TEXTURE_UI 5
#define PD_TEXTURE_TITLE 6
#define PD_TEXTURE_WALLS 7
#define PD_TEXTURE_SEARCH 8
#define PD_TEXTURE_COUNT 9

#define PD_MAX_DRAW_BYTES PXA_RASTER_MAX_DRAW_BYTES

/* Builds and submits one complete frame. `now_ms` drives tile and actor
 * animation. Returns 1 when the frame was accepted (a dropped frame is not an
 * error), 0 on a hard failure. */
int pd_render_present(uint32_t context_handle, uint32_t capabilities,
                      const pd_game_t *game, const pd_layout_t *layout,
                      uint8_t *buffer, uint32_t capacity, uint64_t frame_id,
                      uint32_t now_ms);

#endif
