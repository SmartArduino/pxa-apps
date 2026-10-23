/* Optional diagnostic instrumentation. Never enable for FPS acceptance. */
#pragma once
#ifdef PXA_SNES_PROFILE
#include <stdint.h>
enum { SNES_PHASE_MAIN, SNES_PHASE_PPU, SNES_PHASE_SPC, SNES_PHASE_MIX, SNES_PHASE_COUNT };
typedef struct {
    unsigned phase, sampled;
    uint64_t started;
} snes_profile_scope_t;
snes_profile_scope_t snes_profile_begin(unsigned phase);
void snes_profile_end(snes_profile_scope_t *scope);
void snes_profile_reset(void);
void snes_profile_frame(void);
#define SNES_PROFILE_SCOPE(phase)                                                                  \
    snes_profile_scope_t snes_scope __attribute__((cleanup(snes_profile_end))) =                   \
        snes_profile_begin(phase)
#else
#define SNES_PROFILE_SCOPE(phase) ((void)0)
#define snes_profile_reset() ((void)0)
#define snes_profile_frame() ((void)0)
#endif
