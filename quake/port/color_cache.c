/* SPDX-License-Identifier: GPL-2.0-or-later
 * Lazy 8x8 texel tiles. A light-grid generation and animated texture identity
 * make each cached byte the exact original shader result. No render-loop
 * allocation, pixel approximation or eager off-screen shading is used. */
#include "color_cache.h"
#include <string.h>
#define SETS 1024u
static pxa_quake_color_tile_t tiles[SETS][2];
static uint32_t tick;
uint32_t pxa_quake_color_hits, pxa_quake_color_misses;
void Pxa_QuakeColorReset(void) {
    memset(tiles,0,sizeof(tiles));tick=0;
    pxa_quake_color_hits=pxa_quake_color_misses=0;
}
pxa_quake_color_tile_t *Pxa_QuakeColorTile(uint32_t light,uintptr_t texture,
                                         int x,int y) {
    uint32_t hash=light*UINT32_C(2654435761) ^ (uint32_t)(texture>>3) ^
        (uint32_t)x*UINT32_C(2246822519) ^ (uint32_t)y*UINT32_C(3266489917);
    pxa_quake_color_tile_t *set=tiles[hash&(SETS-1u)],*entry;
    for(unsigned i=0;i<2;++i) {
        entry=set+i;
        if(entry->light==light && entry->texture==texture && entry->x==x && entry->y==y) {
            entry->age=++tick;return entry;
        }
    }
    entry=!set[0].light?set:!set[1].light?set+1:
        ((uint32_t)(tick-set[0].age)>=(uint32_t)(tick-set[1].age)?set:set+1);
    entry->light=light;entry->texture=texture;entry->x=x;entry->y=y;
    entry->age=++tick;memset(entry->valid,0,sizeof(entry->valid));return entry;
}
