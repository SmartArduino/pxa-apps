#ifndef MERITOUS_COMPAT_SDL_IMAGE_H
#define MERITOUS_COMPAT_SDL_IMAGE_H

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Loads an embedded, tightly packed, grayscale INDEX8 image. */
SDL_Surface *IMG_Load(const char *filename);

#ifdef __cplusplus
}
#endif
#endif
