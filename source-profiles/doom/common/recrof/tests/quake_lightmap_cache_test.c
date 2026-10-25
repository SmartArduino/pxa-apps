/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "quakedef.h"
#include "lightmap_cache.h"
#include <assert.h>
#ifndef __wasm__
#include <setjmp.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quake_original_light_pack.h"
static globals_t state;
globals_t *_g = &state;
drawsurf_t r_drawsurf;
unsigned char nodeHadDlight[32];
int r_fullbright;
unsigned port_render_probe_mode = 5;
#ifndef __wasm__
static jmp_buf mismatch;
#endif
void port_error(const char *text) {
    assert(strstr(text,"differs"));
#ifdef __wasm__
    abort();
#else
    longjmp(mismatch,1);
#endif
}
void port_log(const char *text,...) { (void)text; }
static void fill(unsigned value, unsigned count) {
    for (unsigned i=0;i<count;++i) _g->blocklights[i]=value+i*1291;
}

#ifndef __wasm__
static void expect_mismatch(void) {
    if (!setjmp(mismatch)) { Pxa_QuakeLightStore(); abort(); }
}
#endif
int main(void) {
    /* Execute the current upstream packing loop. Odd live-row lengths read
     * one extra scratch word, but only alter an unsampled last-row halfword. */
    unsigned tail_cases = 0;
    for (unsigned width = 2; width <= 18; ++width) for (unsigned height = 2; height <= 18; ++height) {
        uint32_t a[325], b[325]; unsigned cells = width * height, live = cells - width;
        for (unsigned i=0;i<325;++i) a[i]=b[i]=i*13u+7u;
        a[cells]=0x1234; b[cells]=0xabcd;
        original_pack(a,width,cells); original_pack(b,width,cells);
        assert(!memcmp(a,b,live*sizeof(*a)));
        assert((memcmp(a,b,cells*sizeof(*a))!=0)==((live&1u)!=0));
        tail_cases += (live & 1u) != 0;
    }
    assert(tail_cases);
    brush_t brush={&brush}; model_t world={&brush}; _g->cl.worldmodel=&world;
    msurface_t surfaces[256];
    for (unsigned round=0;round<4;++round) {
        Pxa_QuakeLightReset();
        for (unsigned i=0;i<256;++i) {
            surfaces[i]=(msurface_t){{272,272},i}; r_drawsurf.surf=surfaces+i;
            r_drawsurf.surfmip=round;
            r_drawsurf.lightadj[0]=(int)i; r_drawsurf.lightadj[1]=(int)round;
            assert(!Pxa_QuakeLightLoad(surfaces+i)); assert(!Pxa_QuakeLightPacked());
            fill(i+round,324); Pxa_QuakeLightStore();
            memset(_g->blocklights,0,sizeof(_g->blocklights));
            assert(Pxa_QuakeLightLoad(surfaces+i)); assert(Pxa_QuakeLightPacked());
            assert(!Pxa_QuakeLightPacked());
            for(unsigned j=0;j<324;++j) assert(_g->blocklights[j]==i+round+j*1291);
            nodeHadDlight[i/8]|=1u<<(i%8);
            assert(!Pxa_QuakeLightLoad(surfaces+i)); fill(999,324); Pxa_QuakeLightStore();
            nodeHadDlight[i/8]=0;
            assert(Pxa_QuakeLightLoad(surfaces+i)); assert(Pxa_QuakeLightPacked());
            assert(_g->blocklights[0]==i+round); /* dynamic output never entered cache */
            _g->r_refdef.ambientlight++;
            assert(!Pxa_QuakeLightLoad(surfaces+i)); fill(17,324); Pxa_QuakeLightStore();
            r_drawsurf.lightadj[3]++;
            assert(!Pxa_QuakeLightLoad(surfaces+i)); fill(23,324); Pxa_QuakeLightStore();
            port_render_probe_mode=13;
            assert(!Pxa_QuakeLightLoad(surfaces+i)); assert(!Pxa_QuakeLightPacked());
            fill(23,324); Pxa_QuakeLightStore();
            assert(!Pxa_QuakeLightLoad(surfaces+i));
            _g->blocklights[306] ^= 0xabcd0000u; /* last packed row is never sampled */
            Pxa_QuakeLightStore();
#ifndef __wasm__
            assert(!Pxa_QuakeLightLoad(surfaces+i)); fill(24,324);
            expect_mismatch();
#endif
            port_render_probe_mode=5;
        }
    }
    /* Revisit older keys after pool exhaustion, collisions and style changes.
     * A stale tag pointing into reused storage must never produce a hit. */
    Pxa_QuakeLightReset();
    unsigned seed = 0x473921u;
    for (unsigned n = 0; n < 20000; ++n) {
        seed = seed * 1664525u + 1013904223u;
        unsigned i = (seed >> 16) & 255u, phase = n / 1000u;
        unsigned width = 2u + i % 17u, height = 2u + (i / 3u) % 17u;
        surfaces[i] = (msurface_t){{(int)((width-1u)*16u),(int)((height-1u)*16u)},i};
        r_drawsurf.surf = surfaces + i; r_drawsurf.surfmip = i % 4u;
        r_drawsurf.lightadj[0] = phase; r_drawsurf.lightadj[1] = i % 7u;
        _g->r_refdef.ambientlight = 0;
        unsigned value = i + phase * 256u, cells = width * height;
        if (Pxa_QuakeLightLoad(surfaces+i)) {
            assert(Pxa_QuakeLightPacked());
            for (unsigned j=0;j<cells;++j) assert(_g->blocklights[j] == value+j*1291u);
        } else { fill(value,cells); Pxa_QuakeLightStore(); }
    }
    Pxa_QuakeLightReset(); assert(!Pxa_QuakeLightLoad(r_drawsurf.surf));
    fill(23,324); Pxa_QuakeLightStore();
    r_fullbright=1; assert(!Pxa_QuakeLightLoad(r_drawsurf.surf)); r_fullbright=0;
    brush.lightdata=NULL; assert(!Pxa_QuakeLightLoad(r_drawsurf.surf)); brush.lightdata=&brush;
    r_drawsurf.surf->extents[0]=288; assert(!Pxa_QuakeLightLoad(r_drawsurf.surf));
    port_render_probe_mode=1; assert(!Pxa_QuakeLightLoad(r_drawsurf.surf));
    printf("PASS: 1024 surface/mip cycles, 20000 pool revisits, invalidation and sampled-cell verification; original packing has %u unused-tail cases in 289 grid shapes\n",tail_cases);
}
