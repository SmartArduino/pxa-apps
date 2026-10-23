#include "snes_profile.h"
#ifdef PXA_SNES_PROFILE
#include "core.h"
#include <string.h>
#include <time.h>
static struct {
    uint64_t calls, samples, ns;
} phases[SNES_PHASE_COUNT];
static uint64_t clock_cost;
static unsigned frames;
static __attribute__((noinline)) void empty_scope(void) {
    SNES_PROFILE_SCOPE(SNES_PHASE_MAIN);
}
static uint64_t ticks(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + t.tv_nsec;
}
void snes_profile_reset(void) {
    memset(phases, 0, sizeof(phases));
    frames = 0;
    clock_cost = UINT64_MAX;
    for (unsigned i = 0; i < 32; ++i) {
        uint64_t start = ticks(), elapsed = ticks() - start;
        if (elapsed < clock_cost)
            clock_cost = elapsed;
    }
    /* AOT also pays for returning the scope and entering its cleanup. An
     * empty pair of clocks alone would overestimate tiny SPC instructions. */
    uint64_t overhead = UINT64_MAX;
    for (unsigned i = 0; i < 32; ++i) {
        uint64_t before = phases[SNES_PHASE_MAIN].ns;
        empty_scope();
        uint64_t elapsed = phases[SNES_PHASE_MAIN].ns - before;
        if (elapsed < overhead)
            overhead = elapsed;
    }
    clock_cost += overhead;
    memset(phases, 0, sizeof(phases));
}
snes_profile_scope_t snes_profile_begin(unsigned phase) {
    uint64_t call = ++phases[phase].calls;
    /* Prime stride avoids repeatedly sampling the same instruction in short
     * SPC loops. SPC values are estimates; other phases measure every call. */
    snes_profile_scope_t scope = {phase, phase != SNES_PHASE_SPC || call % 251u == 1u, 0};
    if (scope.sampled)
        scope.started = ticks();
    return scope;
}
void snes_profile_end(snes_profile_scope_t *scope) {
    if (scope->sampled) {
        uint64_t ns = ticks() - scope->started;
        phases[scope->phase].ns += ns > clock_cost ? ns - clock_cost : 0;
        ++phases[scope->phase].samples;
    }
}
void snes_profile_frame(void) {
    if (++frames != 120)
        return;
    uint64_t spc = phases[SNES_PHASE_SPC].samples
                       ? phases[SNES_PHASE_SPC].ns / phases[SNES_PHASE_SPC].samples *
                             phases[SNES_PHASE_SPC].calls
                       : 0;
    emu_core_log("SNES PHASE frames=%u main_us=%llu ppu_us=%llu mix_us=%llu spc_est_us=%llu "
                 "spc_calls=%llu spc_samples=%llu clock_ns=%llu",
                 frames, (unsigned long long)(phases[SNES_PHASE_MAIN].ns / 1000),
                 (unsigned long long)(phases[SNES_PHASE_PPU].ns / 1000),
                 (unsigned long long)(phases[SNES_PHASE_MIX].ns / 1000),
                 (unsigned long long)(spc / 1000), (unsigned long long)phases[SNES_PHASE_SPC].calls,
                 (unsigned long long)phases[SNES_PHASE_SPC].samples,
                 (unsigned long long)clock_cost);
    memset(phases, 0, sizeof(phases));
    frames = 0;
}
#endif
