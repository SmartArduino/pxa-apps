#ifndef MAZE_EVIL_FONT_H
#define MAZE_EVIL_FONT_H

#include <stdint.h>

#include "raster.h"

#define GLYPH_WIDTH 5
#define GLYPH_HEIGHT 7
#define GLYPH_CELL 8
#define GLYPH_ATLAS_WIDTH 128
#define GLYPH_ATLAS_HEIGHT 32

/* Builds the 128x32 column-major INDEX8 glyph atlas (texel 1 where set). */
void font_build_atlas(void);

/* Atlas as a sprite for raster_solid_sprite. */
const sprite_t *font_atlas(void);

/* Top-left atlas texel of `symbol`; 0 for a symbol without a glyph. */
int font_glyph_cell(char symbol, int *u0, int *v0);

int font_text_width(const char *text, int scale);

#endif
