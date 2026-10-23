#include "md_profile.h"
#ifdef PXA_MD_PROFILE
#include "core.h"
#include <string.h>
#include <time.h>
static struct { uint64_t calls, samples, ns; } phases[MD_PHASE_COUNT];
static uint64_t clock_cost;
static unsigned frames;
static uint64_t ticks(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + t.tv_nsec;
}
static __attribute__((noinline)) void empty_scope(void) { pprof_start(frame); }
void md_profile_reset(void) {
    memset(phases, 0, sizeof(phases)); frames = 0; clock_cost = UINT64_MAX;
    for (unsigned i = 0; i < 32; ++i) {
        uint64_t start = ticks(), elapsed = ticks() - start;
        if (elapsed < clock_cost) clock_cost = elapsed;
    }
    uint64_t overhead = UINT64_MAX;
    for (unsigned i = 0; i < 32; ++i) {
        uint64_t before = phases[MD_PHASE_frame].ns;
        empty_scope();
        uint64_t elapsed = phases[MD_PHASE_frame].ns - before;
        if (elapsed < overhead) overhead = elapsed;
    }
    clock_cost += overhead;
    memset(phases, 0, sizeof(phases));
}
md_profile_scope_t md_profile_begin(unsigned phase) {
    uint64_t call = ++phases[phase].calls;
    /* CPU/draw hooks can run hundreds of times per frame. Prime sampling
     * limits clock overhead; sound and complete frames are always measured. */
    unsigned sampled = phase == MD_PHASE_frame || phase == MD_PHASE_sound || call % 17u == 1u;
    md_profile_scope_t s = {phase, sampled, 0};
    if (sampled) s.started = ticks();
    return s;
}
void md_profile_end(md_profile_scope_t *scope) {
    if (scope->sampled) {
        uint64_t ns = ticks() - scope->started;
        phases[scope->phase].ns += ns > clock_cost ? ns - clock_cost : 0;
        ++phases[scope->phase].samples;
    }
}
void md_profile_frame(void) {
    if (++frames != 120) return;
    uint64_t us[MD_PHASE_COUNT];
    for (unsigned i = 0; i < MD_PHASE_COUNT; ++i)
        us[i] = phases[i].samples ?
            phases[i].ns / phases[i].samples * phases[i].calls / 1000 : 0;
    /* These scopes are inclusive: MMIO may invoke draw/audio from a CPU.
     * Do not add phase estimates or present them as acceptance FPS. */
    emu_core_log("MD PHASE frames=%u inclusive=1 frame_us=%llu m68k_est_us=%llu "
                 "z80_est_us=%llu draw_est_us=%llu sound_us=%llu clock_ns=%llu",
                 frames, (unsigned long long)us[MD_PHASE_frame],
                 (unsigned long long)us[MD_PHASE_m68k], (unsigned long long)us[MD_PHASE_z80],
                 (unsigned long long)us[MD_PHASE_draw], (unsigned long long)us[MD_PHASE_sound],
                 (unsigned long long)clock_cost);
    memset(phases, 0, sizeof(phases)); frames = 0;
}
#endif
