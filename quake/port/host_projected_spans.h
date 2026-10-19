/* SPDX-License-Identifier: GPL-2.0-or-later
 * One synchronous projected-surface submission. The upstream functions remain
 * the complete fallback; never submit a prefix and then redraw the surface. */
#include "pxa_game_render_spans.h"
#include "port.h"
extern uint64_t port_game_render_context;
static uint8_t pxa_projected_bytes[PXA_GAME_RENDER_PROJECTED_PREFIX_BYTES + 256 * PXA_GAME_RENDER_ROW_BYTES];
static unsigned pxa_projected_fallbacks;

static uint32_t Pxa_FloatBits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int Pxa_TryProjectedSurface(espan_t *first, int write_depth) {
    if (!port_game_render_context || !first) return 0;
    unsigned shift = 4 - (r_drawsurf.surfmip & 3);
    pxa_game_render_spans_header_wire_t h = {0};
    h.target_offset = (uint32_t)(uintptr_t)_g->d_viewbuffer;
    h.target_bytes = screenwidth * VID_HEIGHT;
    h.texture_offset = (uint32_t)(uintptr_t)sdd.r_source;
    h.texture_width = sdd.r_texture_width; h.texture_height = sdd.r_texture_height;
    h.texture_bytes = h.texture_width * h.texture_height;
    h.light_offset = (uint32_t)(uintptr_t)_g->blocklights;
    h.light_width = sdd.r_lightwidth;
    h.light_height = (r_drawsurf.surf->extents[1] >> 4) + 1;
    h.light_bytes = h.light_width * h.light_height * 4;
    h.light_origin_s = sdd.r_surf_soffs >> shift;
    h.light_origin_t = sdd.r_surf_toffs >> shift;
    h.colormap_offset = (uint32_t)(uintptr_t)sdd.colormap;
    h.colormap_levels = VID_GRADES; h.colormap_bytes = VID_GRADES * 256;
    h.light_shift = shift;
    h.flags = (!sdd.textureType ? PXA_GAME_RENDER_SPANS_POW2 : 0) |
        (r_fullbright ? PXA_GAME_RENDER_SPANS_FULLBRIGHT : 0);
    h.wrap_s = sdd.qcs; h.wrap_t = sdd.qct;
    pxa_game_render_projection_wire_t p = {0};
    p.s_origin_bits = Pxa_FloatBits(_g->d_sdivzorigin);
    p.s_du_bits = Pxa_FloatBits(_g->d_sdivzstepu);
    p.s_dv_bits = Pxa_FloatBits(_g->d_sdivzstepv);
    p.t_origin_bits = Pxa_FloatBits(_g->d_tdivzorigin);
    p.t_du_bits = Pxa_FloatBits(_g->d_tdivzstepu);
    p.t_dv_bits = Pxa_FloatBits(_g->d_tdivzstepv);
    p.z_origin_bits = Pxa_FloatBits(_g->d_ziorigin);
    p.z_du_bits = Pxa_FloatBits(_g->d_zistepu);
    p.z_dv_bits = Pxa_FloatBits(_g->d_zistepv);
    p.s_adjust = _g->sadjust; p.t_adjust = _g->tadjust;
    p.s_max = _g->bbextents; p.t_max = _g->bbextentt;
    p.s_offset = sdd.r_surf_soffs; p.t_offset = sdd.r_surf_toffs;
    p.pitch = screenwidth;
    if (write_depth) {
        p.flags = PXA_GAME_RENDER_PROJECTED_WRITE_DEPTH;
        p.depth_offset = (uint32_t)(uintptr_t)_g->d_pzbuffer;
        p.depth_bytes = d_zwidth * VID_HEIGHT * 2; p.depth_pitch = d_zwidth * 2;
    }
    pxa_game_render_projected_batch_t batch;
    pxa_game_render_projected_batch_begin(&batch, pxa_projected_bytes,
        sizeof(pxa_projected_bytes), &h, &p);
    espan_t *span = first;
    do {
        if (batch.batch.count == 256) return 0;
        pxa_game_render_row_wire_t row = {
            span->u, _g->basespanV_p[span - _g->basespan_p], span->count, 0};
        if (!pxa_game_render_projected_append(&batch, &row)) return 0;
    } while ((span = getNextSpan(span, 1)) != NULL);
    int32_t result = pxa_game_render_execute_projected_spans(port_game_render_context, &batch);
    if (result >= 0) return 1;
    if (pxa_projected_fallbacks++ < 4)
        port_log("PXA GameRender projected fallback=%d rows=%u", result, batch.batch.count);
    return 0;
}
