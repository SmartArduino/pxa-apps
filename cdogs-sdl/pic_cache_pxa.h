/* Pic values are copied by the upstream renderer. Cache by owned pixel data,
 * rather than the address or Tex field of a temporary Pic. GPL-3.0-or-later. */
#pragma once
#include <SDL.h>
SDL_Texture *pxa_pic_texture(const void *data);
int pxa_pic_store(const void *data,SDL_Texture *texture);
void pxa_pic_release(const void *data);
