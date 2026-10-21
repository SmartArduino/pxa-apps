/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#ifdef PXA_QUAKE_LIGHTMAP_CACHE
int Pxa_QuakeLightLoad(const void *surface);
int Pxa_QuakeLightPacked(void);
void Pxa_QuakeLightStore(void);
void Pxa_QuakeLightReset(void);
void Pxa_QuakeLightStats(void);
#else
#define Pxa_QuakeLightLoad(surface) 0
#define Pxa_QuakeLightPacked() 0
#define Pxa_QuakeLightStore() ((void)0)
#define Pxa_QuakeLightReset() ((void)0)
#define Pxa_QuakeLightStats() ((void)0)
#endif
