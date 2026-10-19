/* SPDX-License-Identifier: GPL-2.0-or-later
 * Snapshot cross-material inputs into an acquired RGB565 buffer. No output
 * pointer or resource survives the synchronous execution or final present. */
#include "host_material.h"
extern uint64_t port_game_render_context;
static void D_DrawSpans8Rows(espan_t *, int);
static void D_DrawZSpansRows(espan_t *, int);
static pxa_game_render_frame_batch_t pxa_frame_batch;
static pxa_game_render_row_wire_t pxa_frame_rows[256];
static struct { uint32_t identity, bytes, offset; } pxa_frame_textures[64];
static uint32_t pxa_frame_texture_count, pxa_frame_colormap;
static const void *pxa_frame_colormap_source;
static int pxa_frame_unavailable;
static unsigned pxa_frame_replays;
#ifndef PXA_FRAME_BULK_COPY
#define PXA_FRAME_BULK_COPY 1
#endif

/* Only large snapshot copies use the bulk-memory instruction. Keep the rest
 * of Quake's small libc calls under the ordinary compiler settings. */
#if PXA_FRAME_BULK_COPY && defined(__wasm__) && defined(__clang__)
__attribute__((target("bulk-memory"), noinline))
#endif
static void Pxa_CopyProjectedSource(void *destination, const void *source, uint32_t bytes) {
#if PXA_FRAME_BULK_COPY && defined(__wasm__) && defined(__clang__)
    __builtin_memmove(destination, source, bytes);
#else
    memmove(destination, source, bytes);
#endif
}
static uint32_t Pxa_StoreProjectedSource(const void *source, uint32_t bytes) {
    uint32_t offset = pxa_game_render_frame_reserve_source(&pxa_frame_batch, bytes);
    if (offset) Pxa_CopyProjectedSource(pxa_frame_batch.bytes + offset, source, bytes);
    return offset;
}

static float Pxa_BitsFloat(uint32_t bits) {
    float value; memcpy(&value, &bits, sizeof(value)); return value;
}
static void Pxa_SetProjection(const pxa_game_render_projection_wire_t *p) {
    _g->d_sdivzorigin = Pxa_BitsFloat(p->s_origin_bits);
    _g->d_sdivzstepu = Pxa_BitsFloat(p->s_du_bits);
    _g->d_sdivzstepv = Pxa_BitsFloat(p->s_dv_bits);
    _g->d_tdivzorigin = Pxa_BitsFloat(p->t_origin_bits);
    _g->d_tdivzstepu = Pxa_BitsFloat(p->t_du_bits);
    _g->d_tdivzstepv = Pxa_BitsFloat(p->t_dv_bits);
    _g->d_ziorigin = Pxa_BitsFloat(p->z_origin_bits);
    _g->d_zistepu = Pxa_BitsFloat(p->z_du_bits);
    _g->d_zistepv = Pxa_BitsFloat(p->z_dv_bits);
    _g->sadjust = p->s_adjust; _g->tadjust = p->t_adjust;
    _g->bbextents = p->s_max; _g->bbextentt = p->t_max;
}
static void Pxa_SaveProjection(pxa_game_render_projection_wire_t *p) {
    p->s_origin_bits = Pxa_FloatBits(_g->d_sdivzorigin);
    p->s_du_bits = Pxa_FloatBits(_g->d_sdivzstepu);
    p->s_dv_bits = Pxa_FloatBits(_g->d_sdivzstepv);
    p->t_origin_bits = Pxa_FloatBits(_g->d_tdivzorigin);
    p->t_du_bits = Pxa_FloatBits(_g->d_tdivzstepu);
    p->t_dv_bits = Pxa_FloatBits(_g->d_tdivzstepv);
    p->z_origin_bits = Pxa_FloatBits(_g->d_ziorigin);
    p->z_du_bits = Pxa_FloatBits(_g->d_zistepu);
    p->z_dv_bits = Pxa_FloatBits(_g->d_zistepv);
    p->s_adjust = _g->sadjust; p->t_adjust = _g->tadjust;
    p->s_max = _g->bbextents; p->t_max = _g->bbextentt;
}

static void Pxa_ReplayProjectedFrame(void) {
    pxa_game_render_projection_wire_t saved;
    Pxa_SaveProjection(&saved);
    void *saved_view = _g->d_viewbuffer;
    short *saved_depth = _g->d_pzbuffer;
    espan_t *saved_spans = _g->basespan_p;
    byte *saved_v = _g->basespanV_p;
    surfDrawData_t saved_sdd = sdd;
    int saved_mip = r_drawsurf.surfmip;
    byte saved_fullbright = r_fullbright;
    /* The original compact span iterator uses index zero as its sentinel. */
    static espan_t spans[257];
    static byte ys[257];
    const uint8_t *record = pxa_frame_batch.bytes + PXA_GAME_RENDER_FRAME_HEADER_BYTES;
    for (uint32_t i = 0; i < pxa_frame_batch.count; ++i) {
        pxa_game_render_spans_header_wire_t h;
        pxa_game_render_projection_wire_t p;
        pxa_game_render_spans_header_decode(record, 72, &h);
        pxa_game_render_projection_decode(record + 72, 80, &p);
        Pxa_SetProjection(&p);
        _g->d_viewbuffer = (void *)(uintptr_t)h.target_offset;
        _g->d_pzbuffer = (short *)(uintptr_t)p.depth_offset;
        _g->basespan_p = spans; _g->basespanV_p = ys;
        r_drawsurf.surfmip = 4 - h.light_shift;
        r_fullbright = !!(h.flags & PXA_GAME_RENDER_SPANS_FULLBRIGHT);
        sdd.r_source = pxa_frame_batch.bytes + h.texture_offset;
        sdd.colormap = pxa_frame_batch.bytes + h.colormap_offset;
        sdd.r_texture_width = h.texture_width; sdd.r_texture_height = h.texture_height;
        sdd.r_lightwidth = h.light_width; sdd.qcs = h.wrap_s; sdd.qct = h.wrap_t;
        sdd.r_surf_soffs = p.s_offset; sdd.r_surf_toffs = p.t_offset;
        sdd.textureType = !(h.flags & PXA_GAME_RENDER_SPANS_POW2);
        /* Form the reference's coordinate-biased pointer as an integer; only
         * the final indexed addresses are sampled inside the light snapshot. */
        int64_t origin = (p.s_offset >> h.light_shift) +
            (int64_t)(p.t_offset >> h.light_shift) * h.light_width;
        sdd.blocklights = (BLOCKLIGHT_TYPE *)((uintptr_t)pxa_frame_batch.bytes +
            h.light_offset - (uintptr_t)(origin * 4));
        for (uint32_t j = 0; j < h.count; ++j) {
            pxa_game_render_row_wire_t row;
            pxa_game_render_row_decode(record + 152 + j*8, 8, &row);
            spans[j + 1].u = row.x; spans[j + 1].count = row.count;
            spans[j + 1].nextSpanIdx = j + 1 == h.count ? 0 : j + 2;
            ys[j + 1] = row.y;
        }
        D_DrawSpans8Rows(spans + 1, -1);
        if (p.flags & PXA_GAME_RENDER_PROJECTED_WRITE_DEPTH) D_DrawZSpansRows(spans + 1, -1);
        record += 152 + h.count * 8;
    }
    Pxa_SetProjection(&saved);
    _g->d_viewbuffer = saved_view; _g->d_pzbuffer = saved_depth;
    _g->basespan_p = saved_spans; _g->basespanV_p = saved_v;
    sdd = saved_sdd; r_drawsurf.surfmip = saved_mip; r_fullbright = saved_fullbright;
}

void Pxa_FlushProjectedFrame(void) {
    if (pxa_frame_batch.count) {
        int32_t result = pxa_game_render_execute_frame_spans(port_game_render_context, &pxa_frame_batch);
        if (result < 0) {
            Pxa_ReplayProjectedFrame();
            if (pxa_frame_replays++ < 4)
                port_log("PXA GameRender frame replay=%d materials=%u", result, pxa_frame_batch.count);
        }
    }
    pxa_frame_batch.bytes = NULL; pxa_frame_batch.count = 0;
    pxa_frame_texture_count = pxa_frame_colormap = 0;
    pxa_frame_colormap_source = NULL; pxa_frame_unavailable = 0;
}
static int Pxa_BeginProjectedFrame(void) {
    if (!port_game_render_context || pxa_frame_unavailable) return 0;
    if (pxa_frame_batch.bytes) return !pxa_frame_batch.status;
    size_t capacity;
    uint8_t *storage = port_frame_scratch(&capacity);
    if (!storage) { pxa_frame_unavailable = 1; return 0; }
    pxa_game_render_frame_begin(&pxa_frame_batch, storage, (uint32_t)capacity);
    return !pxa_frame_batch.status;
}
/* Called before getSurfBuffer can reuse the texture/light scratch. Flushing
 * here preserves the original order of depth writes versus scratch loading. */
void Pxa_ReserveProjectedSurface(msurface_t *surface, int mip) {
    if (!Pxa_BeginProjectedFrame()) return;
#if MSURFACE_HAS_TEXINFO_POINTER
    mtexinfo_t *info = surface->texinfo;
#else
    mtexinfo_t *info = getMsurfaceTexinfo(surface);
#endif
    texture_t *texture = R_TextureAnimation(info->texture);
    uint64_t needed = 152 + 256*8 + 12 + VID_GRADES*256;
    needed += (uint64_t)(texture->width >> mip) * (texture->height >> mip);
    needed += (uint64_t)((surface->extents[0] >> 4) + 1) * ((surface->extents[1] >> 4) + 1) * 4;
    if (pxa_frame_batch.count && (needed > pxa_game_render_frame_remaining(&pxa_frame_batch) ||
        pxa_frame_texture_count == 64)) {
        Pxa_FlushProjectedFrame();
        Pxa_BeginProjectedFrame();
    }
}

static int Pxa_QueueProjectedSurface(espan_t *first, int write_depth) {
    if (!first || !Pxa_BeginProjectedFrame()) return 0;
    unsigned count = 0; uint32_t color_end = 0, depth_end = 0;
    pxa_game_render_spans_header_wire_t h;
    pxa_game_render_projection_wire_t p;
    Pxa_DescribeProjectedSurface(&h, &p, write_depth);
    if ((p.s_offset >> h.light_shift) != h.light_origin_s ||
        (p.t_offset >> h.light_shift) != h.light_origin_t) return 0;
    espan_t *span = first;
    do {
        if (count == 256) return 0;
        unsigned y = _g->basespanV_p[span - _g->basespan_p];
        pxa_frame_rows[count++] = (pxa_game_render_row_wire_t){span->u, y, span->count, 0};
        uint32_t end = y * p.pitch + span->u + span->count;
        uint32_t zend = y * p.depth_pitch + 2u * (span->u + span->count);
        if (end > color_end) color_end = end;
        if (zend > depth_end) depth_end = zend;
    } while ((span = getNextSpan(span, 1)) != NULL);
    h.target_bytes = color_end;
    if (write_depth) p.depth_bytes = depth_end;
    uint32_t identity = (uint32_t)(uintptr_t)r_drawsurf.texture->extmemdata[r_drawsurf.surfmip];
    uint32_t texture = 0;
    for (unsigned i = 0; i < pxa_frame_texture_count; ++i)
        if (pxa_frame_textures[i].identity == identity && pxa_frame_textures[i].bytes == h.texture_bytes)
            { texture = pxa_frame_textures[i].offset; break; }
    uint32_t colormap = pxa_frame_colormap_source == sdd.colormap ? pxa_frame_colormap : 0;
    uint64_t needed = 152 + count*8 + h.light_bytes + 12u;
    if (!texture) needed += h.texture_bytes;
    if (!colormap) needed += h.colormap_bytes;
    if (needed > pxa_game_render_frame_remaining(&pxa_frame_batch)) return 0;
    pxa_game_render_frame_batch_t previous = pxa_frame_batch;
    if (!texture) texture = Pxa_StoreProjectedSource(sdd.r_source, h.texture_bytes);
    if (!colormap) colormap = Pxa_StoreProjectedSource(sdd.colormap, h.colormap_bytes);
    h.texture_offset = texture; h.colormap_offset = colormap;
    h.light_offset = Pxa_StoreProjectedSource(_g->blocklights, h.light_bytes);
    if (!pxa_game_render_frame_append(&pxa_frame_batch, &h, &p, pxa_frame_rows, count)) {
        pxa_frame_batch = previous; return 0;
    }
    if (pxa_frame_texture_count < 64) {
        unsigned i;
        for (i = 0; i < pxa_frame_texture_count; ++i) if (pxa_frame_textures[i].offset == texture) break;
        if (i == pxa_frame_texture_count) {
            pxa_frame_textures[i].identity = identity; pxa_frame_textures[i].bytes = h.texture_bytes;
            pxa_frame_textures[i].offset = texture; ++pxa_frame_texture_count;
        }
    }
    pxa_frame_colormap = colormap; pxa_frame_colormap_source = sdd.colormap;
    return 1;
}
