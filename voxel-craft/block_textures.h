#ifndef VOXEL_CRAFT_BLOCK_TEXTURES_H
#define VOXEL_CRAFT_BLOCK_TEXTURES_H

#include <stdint.h>

/* Face kinds of the shared block texture set. Every block owns a 16x16 tile
 * per kind. */
#define BLOCK_TEXTURE_TOP 0
#define BLOCK_TEXTURE_SIDE 1
#define BLOCK_TEXTURE_BOTTOM 2

typedef uint16_t block_texture_tile_t[256];
typedef block_texture_tile_t block_texture_set_t[3];
typedef uint8_t block_index_tile_t[256];
typedef block_index_tile_t block_index_set_t[3];

/* Shared procedurally generated block textures, built on first use.
 * block_textures() returns RGB565 tiles indexed by block, kind and pixel.
 * block_texture_indices() returns the same tiles quantised into the
 * block_texture_palette() 256-colour palette; palette index 0 is transparent
 * for cutout materials and index 255 is reserved for the HUD font. */
const block_texture_set_t *block_textures(void);
const uint16_t *block_texture_palette(void);
const block_index_set_t *block_texture_indices(void);

#endif
