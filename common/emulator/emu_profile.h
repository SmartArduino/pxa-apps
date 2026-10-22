/* MPL-2.0. Diagnostic sampling only; absent from production hot paths. */
#pragma once
#ifdef PXA_EMU_PROFILE_PHASES
#include <stdint.h>
enum {EMU_PROFILE_EVENTS,EMU_PROFILE_RASTER,EMU_PROFILE_APU,EMU_PROFILE_HOST_APU,EMU_PROFILE_OUTPUT,EMU_PROFILE_PHASE_COUNT};
typedef struct {unsigned phase,sampled;uint64_t started;} emu_profile_scope_t;
typedef struct {uint64_t calls,samples,us;unsigned depth;} emu_profile_phase_t;
extern emu_profile_phase_t emu_profile_phases[EMU_PROFILE_PHASE_COUNT];
emu_profile_scope_t emu_profile_begin(unsigned phase);
void emu_profile_end(emu_profile_scope_t *scope);
void emu_profile_reset(void);
uint64_t emu_profile_now(void);
#define EMU_PROFILE_SCOPE(phase) emu_profile_scope_t emu_scope __attribute__((cleanup(emu_profile_end)))=emu_profile_begin(phase)
#else
#define EMU_PROFILE_SCOPE(phase) ((void)0)
#endif
