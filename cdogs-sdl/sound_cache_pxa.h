/* Stable SDL chunk descriptors with a bounded decoded-sample cache. GPL-3.0-or-later. */
#pragma once
#include <SDL_mixer.h>
Mix_Chunk *pxa_sound_register(const char *path);
int pxa_sound_resolve(Mix_Chunk *chunk);
void pxa_sound_cache_stats(void);
