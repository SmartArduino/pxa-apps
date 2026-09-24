#ifndef MAZE_EVIL_ASSETS_H
#define MAZE_EVIL_ASSETS_H

#include <stdint.h>

#include "raster.h"

#define TEX_SIZE 128
#define TEX_SHIFT 7
#define TEX_COUNT 9

enum {
    TEX_BRICK = 0,
    TEX_STONE,
    TEX_TECH,
    TEX_FLESH,
    TEX_WOOD,
    TEX_DOOR,
    TEX_EXIT,
    TEX_FLOOR,
    TEX_CEILING,
};

extern const uint8_t kTextureData[TEX_COUNT][TEX_SIZE * TEX_SIZE];
extern const texture_t kTextures[TEX_COUNT];

#endif
