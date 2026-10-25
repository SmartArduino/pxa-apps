/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pxa_game_render_spans.h"
#define SPAN_PRECISION 6
#define mipShift(m) (4 - (m))
#define mipMask(m) ((1 << mipShift(m)) - 1)
#define ESP_PLATFORM 1
#define ESPANS_HAVE_V 0
#define ESP_LIT_SPAN 1
#define MAX_SPAN_COUNT 32
#define D_FIXED_Z(zi) (65536.0f * (1.0f / (zi)))
#define QDFLOAT float
#define WIN32 0
#define VID_GRADES 64
#define VID_HEIGHT 8
#define MSURFACE_HAS_TEXINFO_POINTER 1
#define WIDTH 320
#define HEIGHT 8
#define TARGET_BYTES (WIDTH * HEIGHT)
typedef uint8_t byte;
typedef int32_t fixed16_t;
typedef uint32_t BLOCKLIGHT_TYPE;
typedef struct { uint32_t width, height; byte *extmemdata[4]; } texture_t;
typedef struct { texture_t *texture; } mtexinfo_t;
typedef struct { mtexinfo_t *texinfo; int extents[2]; } msurface_t;
static struct { int surfmip; msurface_t *surf; texture_t *texture; } r_drawsurf;
typedef struct {
    const uint32_t *blocklights;
    int r_lightwidth, r_texture_width, r_texture_height;
    int r_surf_soffs, r_surf_toffs, textureType;
    const uint8_t *r_source, *colormap;
    uint32_t qcs, qct;
} surfDrawData_t;
static surfDrawData_t sdd;
static byte r_fullbright;
#include "quake_original_lit_span.h"
defineDrawLitSpan(0, 1) defineDrawLitSpan(1, 1)
defineDrawLitSpan(2, 1) defineDrawLitSpan(3, 1)
defineDrawLitSpan(0, 0) defineDrawLitSpan(1, 0)
defineDrawLitSpan(2, 0) defineDrawLitSpan(3, 0)
static void drawLitSpan(uint8_t *dest, int n, int s, int t, int ds, int dt) {
    typedef void (*fn)(uint8_t *, int, int, int, int, int);
    fn refs[2][4] = {{drawLitSpan0_1, drawLitSpan1_1, drawLitSpan2_1, drawLitSpan3_1},
        {drawLitSpan0_0, drawLitSpan1_0, drawLitSpan2_0, drawLitSpan3_0}};
    refs[!!sdd.textureType][r_drawsurf.surfmip](dest,n,s,t,ds,dt);
}
typedef struct { unsigned u, count, nextSpanIdx; } espan_t;
static struct {
    float d_sdivzorigin, d_sdivzstepu, d_sdivzstepv;
    float d_tdivzorigin, d_tdivzstepu, d_tdivzstepv;
    float d_ziorigin, d_zistepu, d_zistepv;
    int sadjust, tadjust, bbextents, bbextentt;
    byte *d_viewbuffer;
    short *d_pzbuffer;
    espan_t *basespan_p;
    uint8_t *basespanV_p;
    uint32_t blocklights[18*18];
} globals, *_g = &globals;
static const int screenwidth = WIDTH, d_zwidth = WIDTH;
static espan_t *getNextSpan(espan_t *s, int null_end) {
    return !s->nextSpanIdx && null_end ? NULL : _g->basespan_p + s->nextSpanIdx;
}
#include "quake_reciprocal.h"
#include "quake_original_projected.h"
static texture_t *R_TextureAnimation(texture_t *t) { return t; }
uint64_t port_game_render_context = 77;
#ifdef PXA_QUAKE_RENDER_PROBE
unsigned port_render_probe_mode;
#endif
static uint32_t packet_calls, replay_logs;
static union { uint64_t align; uint8_t bytes[153600]; } packet;
void *port_frame_scratch(size_t *size) { *size=sizeof(packet.bytes); return packet.bytes; }
void port_log(const char *fmt, ...) { (void)fmt; ++replay_logs; }
/* Every packet is rejected atomically. The adapter must render captured
 * materials with the real upstream Rows functions after live state changes. */
int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *data, uint32_t size) {
    assert(handle==77 && operation==PXA_GAME_RENDER_IO_EXECUTE_FRAME_SPANS);
    assert(data==packet.bytes && size==sizeof(packet.bytes));
    ++packet_calls;
    return PXA_STATUS_WOULD_BLOCK;
}
#include "host_frame_spans.h"
static uint32_t rng = 129;
static uint32_t random32(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static union { uint64_t align; uint8_t bytes[TARGET_BYTES]; } target;
static union { uint64_t align; uint8_t bytes[2*TARGET_BYTES]; } depth;
static uint8_t expected[TARGET_BYTES], expected_depth[2*TARGET_BYTES];
static uint8_t texture_pixels[64*80], colormaps[2][VID_GRADES*256];
static espan_t spans[5]; static byte ys[5];
int main(void) {
    texture_t texture={64,64,{0}}; mtexinfo_t info={&texture};
    msurface_t surface={&info,{272,272}};
    r_drawsurf.surf=&surface;r_drawsurf.texture=&texture;
    for(unsigned i=0;i<sizeof(colormaps);++i)((uint8_t*)colormaps)[i]=random32();
    for(unsigned iteration=0;iteration<256;++iteration) {
#ifdef PXA_QUAKE_RENDER_PROBE
        port_render_probe_mode=iteration&1u;
        pxa_game_render_frame_batch_t before_mode_test=pxa_frame_batch;
        assert(!Pxa_BeginProjectedFrame());
        assert(!Pxa_QueueProjectedSurface(spans+1,1));
        assert(!memcmp(&before_mode_test,&pxa_frame_batch,sizeof(pxa_frame_batch)));
        port_render_probe_mode=2u|(iteration&1u);
#endif
        memset(target.bytes,0xa5,sizeof(target.bytes));memset(depth.bytes,0x5a,sizeof(depth.bytes));
        _g->d_viewbuffer=target.bytes;_g->d_pzbuffer=(short*)(void*)depth.bytes;
        _g->basespan_p=spans;_g->basespanV_p=ys;
        for(unsigned material=0;material<3;++material) {
            r_drawsurf.surfmip=(iteration+material)&3;
            unsigned shift=4-r_drawsurf.surfmip;
            r_fullbright=(iteration>>3)&1;
            sdd.textureType=(iteration+material)&1;
            sdd.r_texture_width=sdd.textureType?48:64;sdd.r_texture_height=sdd.textureType?80:64;
            texture.extmemdata[r_drawsurf.surfmip]=(byte*)(uintptr_t)(100+material);
            sdd.r_source=texture_pixels;sdd.colormap=colormaps[material&1];sdd.r_lightwidth=18;
            sdd.r_surf_soffs=((int)(iteration%3)-1)*16;sdd.r_surf_toffs=((int)(iteration%5)-2)*16;
            sdd.qcs=UINT32_MAX/sdd.r_texture_width+1;sdd.qct=UINT32_MAX/sdd.r_texture_height+1;
            sdd.blocklights=(uint32_t*)((uintptr_t)_g->blocklights-
                (uintptr_t)((sdd.r_surf_soffs>>shift)+(sdd.r_surf_toffs>>shift)*18)*4u);
            for(unsigned i=0;i<sizeof(texture_pixels);++i)texture_pixels[i]=random32();
            for(unsigned i=0;i<18*18;++i)_g->blocklights[i]=(random32()&127)|((random32()&127)<<16);
            _g->d_ziorigin=.025f;_g->d_zistepu=.000001f;_g->d_zistepv=-.000001f;
            _g->d_sdivzorigin=(4+random32()%8)*.025f;_g->d_tdivzorigin=(4+random32()%8)*.025f;
            _g->d_sdivzstepu=.001f;_g->d_tdivzstepu=-.001f;
            _g->d_sdivzstepv=.0001f;_g->d_tdivzstepv=.0001f;
            _g->sadjust=1234;_g->tadjust=-4321;_g->bbextents=_g->bbextentt=(16<<16)-1;
            for(unsigned i=0;i<4;++i){spans[i+1]=(espan_t){random32()%20,1+random32()%288,i==3?0:i+2};ys[i+1]=random32()%HEIGHT;}
            D_DrawSpans8Rows(spans+1,-1);
            if(material&1)D_DrawZSpansRows(spans+1,-1);
            assert(Pxa_QueueProjectedSurface(spans+1,material&1));
        }
        memcpy(expected,target.bytes,sizeof(expected));memcpy(expected_depth,depth.bytes,sizeof(expected_depth));
        memset(target.bytes,0xa5,sizeof(target.bytes));memset(depth.bytes,0x5a,sizeof(depth.bytes));
        /* Recycle every borrowed engine source and all compact span records. */
        memset(texture_pixels,17,sizeof(texture_pixels));memset(_g->blocklights,0,sizeof(_g->blocklights));
        memset(spans,0,sizeof(spans));memset(ys,7,sizeof(ys));
        _g->d_sdivzorigin=123;_g->d_tdivzstepu=45;_g->sadjust=789;
        r_drawsurf.surfmip=3;sdd.textureType=1;sdd.r_source=texture_pixels+1;r_fullbright=1;
        typeof(globals) saved_globals=globals;surfDrawData_t saved_sdd=sdd;
        Pxa_FlushProjectedFrame();
        assert(!memcmp(expected,target.bytes,sizeof(expected)));
        assert(!memcmp(expected_depth,depth.bytes,sizeof(expected_depth)));
        assert(!memcmp(&saved_globals,&globals,sizeof(globals)) && !memcmp(&saved_sdd,&sdd,sizeof(sdd)));
        assert(r_drawsurf.surfmip==3 && r_fullbright==1 && !pxa_frame_batch.count);
    }
    assert(packet_calls==256 && replay_logs==4);
    printf("PASS: 256 rejected WASM frame batches, 768 captured materials replay original colour/depth; recycled texture/light/span storage and restored live state\n");
    return 0;
}
