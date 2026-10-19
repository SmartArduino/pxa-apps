/* SPDX-License-Identifier: GPL-2.0-or-later
 * GameRender adapter. Included after the upstream drawLitSpan definition;
 * original renderer remains the differential-test and failure reference. */
#include "pxa_game_render_spans.h"
#include "port.h"
extern uint64_t port_game_render_context;
static uint8_t pxa_span_bytes[PXA_GAME_RENDER_SPANS_HEADER_BYTES + 256 * PXA_GAME_RENDER_SPAN_BYTES];
static pxa_game_render_span_batch_t pxa_span_batch;
static int pxa_span_active;
static unsigned pxa_span_fallbacks;

static void Pxa_FlushHostSpans(void) {
    if (!pxa_span_batch.count) return;
    int32_t result = pxa_game_render_execute_spans(port_game_render_context, &pxa_span_batch);
    if (result < 0) {
        /* Validation/WOULD_BLOCK writes no pixels. Replay using unchanged
         * surface state before the engine can reuse its light/texture data. */
        for (unsigned i = 0; i < pxa_span_batch.count; ++i) {
            pxa_game_render_span_wire_t s;
            pxa_game_render_span_decode(pxa_span_bytes + PXA_GAME_RENDER_SPANS_HEADER_BYTES +
                i * PXA_GAME_RENDER_SPAN_BYTES, PXA_GAME_RENDER_SPAN_BYTES, &s);
            drawLitSpan((uint8_t *)_g->d_viewbuffer + s.destination,
                s.count, s.s, s.t, s.ds, s.dt);
        }
        if (pxa_span_fallbacks++ < 4)
            port_log("PXA GameRender span fallback=%d spans=%u", result, pxa_span_batch.count);
    }
    pxa_span_batch.count = 0;
    pxa_span_batch.status = PXA_STATUS_OK;
}

static void Pxa_BeginHostSpans(void) {
    pxa_span_active = port_game_render_context != 0;
    pxa_span_batch.count = 0;
    if (!pxa_span_active) return;
    unsigned shift = 4 - (r_drawsurf.surfmip & 3);
    unsigned light_height = (r_drawsurf.surf->extents[1] >> 4) + 1;
    pxa_game_render_spans_header_wire_t h = {0};
    h.target_offset = (uint32_t)(uintptr_t)_g->d_viewbuffer;
    h.target_bytes = screenwidth * VID_HEIGHT;
    h.texture_offset = (uint32_t)(uintptr_t)sdd.r_source;
    h.texture_width = sdd.r_texture_width;
    h.texture_height = sdd.r_texture_height;
    h.texture_bytes = h.texture_width * h.texture_height;
    h.light_offset = (uint32_t)(uintptr_t)_g->blocklights;
    h.light_width = sdd.r_lightwidth;
    h.light_height = light_height;
    h.light_bytes = h.light_width * h.light_height * 4;
    h.light_origin_s = sdd.r_surf_soffs >> shift;
    h.light_origin_t = sdd.r_surf_toffs >> shift;
    h.colormap_offset = (uint32_t)(uintptr_t)sdd.colormap;
    h.colormap_levels = VID_GRADES;
    h.colormap_bytes = VID_GRADES * 256;
    h.light_shift = shift;
    h.flags = (!sdd.textureType ? PXA_GAME_RENDER_SPANS_POW2 : 0) |
        (r_fullbright ? PXA_GAME_RENDER_SPANS_FULLBRIGHT : 0);
    h.wrap_s = sdd.qcs; h.wrap_t = sdd.qct;
    pxa_game_render_span_batch_begin(&pxa_span_batch, pxa_span_bytes,
        sizeof(pxa_span_bytes), &h);
}

static int Pxa_QueueHostSpan(uint8_t *dest, int count, int s, int t, int ds, int dt) {
    if (!pxa_span_active) return 0;
    if (pxa_span_batch.count == 256) Pxa_FlushHostSpans();
    pxa_game_render_span_wire_t span = {
        (uint32_t)(dest - (uint8_t *)_g->d_viewbuffer), (uint32_t)count, s, t, ds, dt};
    return pxa_game_render_span_append(&pxa_span_batch, &span);
}
