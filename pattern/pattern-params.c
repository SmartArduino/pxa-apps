/* SPDX-License-Identifier: MIT
 * Pattern-only platform configuration. The official backend stays unchanged.
 * 5x5 is an original validated custom size, not an official preset.
 */
#include "puzzles.h"
#include "pxa_port_log.h"

midend *__real_midend_new(frontend *, const game *, const drawing_api *, void *);
void __real_midend_serialise(midend *, void (*)(void *, const void *, int), void *);

midend *__wrap_midend_new(frontend *fe, const game *original,
                         const drawing_api *api, void *handle)
{
    midend *me = __real_midend_new(fe, original, api, handle);
    game_params *params = original->default_params();
    original->decode_params(params, "5x5");
    const char *error = original->validate_params(params, true);
    if (error) fatal("Pattern original custom parameters rejected: %s", error);
    midend_set_params(me, params); /* Original midend duplicates the parameters. */
    original->free_params(params);
    return me;
}

/* Preserve the original save stream and expose its public rule status for QA.
 * No solver, rule change or write to persistent storage is introduced.
 */
void __wrap_midend_serialise(midend *me,
                             void (*write)(void *, const void *, int), void *ctx)
{
    __real_midend_serialise(me, write, ctx);
    pxlog_t log;
    pxlog_init(&log);
    pxlog_text(&log, "PATTERN ORIGINAL");
    pxlog_field(&log, "status", midend_status(me));
    pxlog_send(&log);
}
