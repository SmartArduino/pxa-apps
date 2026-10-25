/* SPDX-License-Identifier: GPL-2.0-or-later
 * Cache the engine's exact packed light grid, never its rendered pixels.
 * Storage lives in Guest linear memory (PSRAM on ESP), with no allocation in
 * the render loop. Two ways bound lookup and replacement cost. */
#include "quakedef.h"
#include "r_local.h"
#include "lightmap_cache.h"
#include "port.h"
#include <stdint.h>
#include <string.h>
#ifdef PXA_QUAKE_COLOR_CACHE
#include "color_cache.h"
#endif

#define SETS 256u
#define POOL_CELLS (96u * 1024u / sizeof(uint32_t))
typedef struct {
    const void *surface;
    int lightadj[4], ambient, mip;
    unsigned cells, live_cells, age;
    unsigned offset, capacity;
    uint32_t serial;
} light_entry_t;
static light_entry_t entries[SETS][2];
static uint32_t pool[POOL_CELLS];
static unsigned used, pool_resets;
static light_entry_t *pending;
static unsigned tick, hits, misses, bypass, verified, packed, unused_tail_changes;
static int verify;
static uint32_t current_token,next_token;
uint32_t Pxa_QuakeLightToken(void) {return current_token;}
static uint32_t new_token(void) {
    if(next_token==UINT32_MAX) {
#ifdef PXA_QUAKE_COLOR_CACHE
        Pxa_QuakeColorReset();
#endif
        for(unsigned i=0;i<SETS;++i)
            for(unsigned j=0;j<2;++j)entries[i][j].serial=0;
        next_token=0;
    }
    return ++next_token;
}

static void clear_pool(void) {
    for (unsigned i = 0; i < SETS; ++i)
        for (unsigned j = 0; j < 2; ++j) {
            entries[i][j].surface = NULL;
            entries[i][j].capacity = 0;
        }
    used = 0;
}

void Pxa_QuakeLightReset(void) {
#ifdef PXA_QUAKE_COLOR_CACHE
    Pxa_QuakeColorReset();
#endif
    current_token=next_token=0;
    clear_pool();
    pending = NULL; packed = 0; tick = 0;
}

int Pxa_QuakeLightLoad(const void *surface) {
    current_token=0;
    pending = NULL; packed = 0; verify = 0;
#ifdef PXA_QUAKE_RENDER_PROBE
    extern unsigned port_render_probe_mode;
    if (!(port_render_probe_mode & 4u)) return 0;
    verify = (port_render_probe_mode & 8u) != 0;
#endif
    msurface_t *surf = r_drawsurf.surf;
    if (surf != surface || r_fullbright || !_g->cl.worldmodel->brushModelData->lightdata ||
        (nodeHadDlight[surf->surfNodeIndex / 8] & (1u << (surf->surfNodeIndex % 8)))) {
        ++bypass; return 0;
    }
    unsigned width = (surf->extents[0] >> 4) + 1u, height = (surf->extents[1] >> 4) + 1u;
    if (width < 2 || height < 2 || width > 18 || height > 18) { ++bypass; return 0; }
    unsigned cells = width * height, live_cells = width * (height - 1u);
    uintptr_t hash = ((uintptr_t)surf >> 3) ^ ((uintptr_t)surf >> 11) ^ (unsigned)r_drawsurf.surfmip;
    light_entry_t *set = entries[hash & (SETS - 1u)];
    light_entry_t *same_surface = NULL;
    for (unsigned i = 0; i < 2; ++i) {
        light_entry_t *e = set + i;
        if (e->surface == surf && e->mip == r_drawsurf.surfmip) same_surface = e;
        if (e->surface == surf && e->mip == r_drawsurf.surfmip && e->cells == cells && e->live_cells == live_cells &&
            e->ambient == _g->r_refdef.ambientlight &&
            !memcmp(e->lightadj, r_drawsurf.lightadj, sizeof(e->lightadj))) {
            e->age = ++tick; ++hits; pending = e;
            if(!e->serial)e->serial=new_token();
            current_token=e->serial;
            if (verify) return 0;
            port_copy_read(_g->blocklights, pool + e->offset, cells * sizeof(uint32_t));
            packed = 1;
            return 1;
        }
    }
    ++misses;
    pending = same_surface ? same_surface : !set[0].surface ? set : !set[1].surface ? set + 1 :
        ((uint32_t)(tick - set[0].age) >= (uint32_t)(tick - set[1].age) ? set : set + 1);
    pending->surface = NULL;
    pending->mip = r_drawsurf.surfmip; pending->cells = cells; pending->live_cells = live_cells;
    pending->ambient = _g->r_refdef.ambientlight;
    memcpy(pending->lightadj, r_drawsurf.lightadj, sizeof(pending->lightadj));
    return 0;
}

int Pxa_QuakeLightPacked(void) {
    int ready = packed != 0;
    packed = 0;
    if (ready) pending = NULL;
    return ready;
}

void Pxa_QuakeLightStore(void) {
    if (!pending) return;
    const uint32_t *cached = pool + pending->offset;
    if (verify && pending->surface) {
        /* Only the first height-1 packed rows are sampled: their upper
         * halfwords already contain the next row. The upstream two-at-a-time
         * packing loop can write one unused upper halfword in the last row
         * using scratch outside this surface's grid when width*(height-1)
         * is odd. That unused tail is not an input to either sampler. */
        if (memcmp(cached, _g->blocklights, pending->live_cells * sizeof(uint32_t))) {
            unsigned at = 0;
            while (at < pending->live_cells && cached[at] == _g->blocklights[at]) ++at;
            port_log("PXA QLIGHT mismatch at=%u live=%u cells=%u mip=%d cached=%u actual=%u",
                at, pending->live_cells, pending->cells, pending->mip,
                cached[at], _g->blocklights[at]);
            port_error("Cached lighting differs from original engine.");
        }
        if (memcmp(cached + pending->live_cells, _g->blocklights + pending->live_cells,
            (pending->cells - pending->live_cells) * sizeof(uint32_t))) ++unused_tail_changes;
        ++verified;
    } else {
        /* Reuse each way's allocation on style changes and replacement. A
         * bounded bump pool stores only the actual grid dimensions. On
         * exhaustion invalidate all tags before reusing any byte; no heap,
         * overlapping live blocks or render-loop compaction is required. */
        if (pending->capacity < pending->cells) {
            if (pending->cells > POOL_CELLS - used) { clear_pool(); ++pool_resets; }
            pending->offset = used;
            pending->capacity = pending->cells;
            used += pending->cells;
        }
        port_copy_read(pool + pending->offset, _g->blocklights, pending->cells * sizeof(uint32_t));
        pending->surface = r_drawsurf.surf;
        pending->age = ++tick;
        pending->serial=new_token();
    }
    current_token=pending->serial;
    pending = NULL;
}

void Pxa_QuakeLightStats(void) {
    port_log("PXA QLIGHT hit=%u miss=%u bypass=%u verified=%u unused_tail=%u pool_resets=%u pool_used=%u bytes=%u"
#ifdef PXA_QUAKE_COLOR_CACHE
        " color_hit=%u color_miss=%u"
#endif
        ,
        hits, misses, bypass, verified, unused_tail_changes, pool_resets, used * (unsigned)sizeof(uint32_t),
        (unsigned)(sizeof(entries) + sizeof(pool))
#ifdef PXA_QUAKE_COLOR_CACHE
        ,pxa_quake_color_hits,pxa_quake_color_misses
#endif
        );
#ifdef PXA_QUAKE_COLOR_CACHE
    pxa_quake_color_hits=pxa_quake_color_misses=0;
#endif
    hits = misses = bypass = verified = unused_tail_changes = pool_resets = 0;
}
