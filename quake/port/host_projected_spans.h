/* SPDX-License-Identifier: GPL-2.0-or-later
 * One synchronous projected-surface submission. The upstream functions remain
 * the complete fallback; never submit a prefix and then redraw the surface. */
#include "pxa_game_render_spans.h"
#include "port.h"
extern uint64_t port_game_render_context;
static uint8_t pxa_projected_bytes[PXA_GAME_RENDER_PROJECTED_PREFIX_BYTES + 256 * PXA_GAME_RENDER_ROW_BYTES];
static unsigned pxa_projected_fallbacks;

static int Pxa_TryProjectedSurface(espan_t *first, int write_depth) {
    if (!port_game_render_context || !first) return 0;
    pxa_game_render_spans_header_wire_t h;
    pxa_game_render_projection_wire_t p;
    Pxa_DescribeProjectedSurface(&h, &p, write_depth);
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
