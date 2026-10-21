/* MPL-2.0. Diagnostic sampling only; absent from production hot paths. */
#pragma once
#ifdef PXA_GB_PROFILE_PHASES
#include <stdint.h>
enum {GB_PROFILE_EVENTS,GB_PROFILE_RASTER,GB_PROFILE_APU,GB_PROFILE_HOST_APU,GB_PROFILE_OUTPUT,GB_PROFILE_PHASE_COUNT};
typedef struct {unsigned phase,sampled;uint64_t started;} gb_profile_scope_t;
typedef struct {uint64_t calls,samples,us;unsigned depth;} gb_profile_phase_t;
extern gb_profile_phase_t gb_profile_phases[GB_PROFILE_PHASE_COUNT];
gb_profile_scope_t gb_profile_begin(unsigned phase);
void gb_profile_end(gb_profile_scope_t *scope);
void gb_profile_reset(void);
uint64_t gb_profile_now(void);
#define GB_PROFILE_SCOPE(phase) gb_profile_scope_t gb_scope __attribute__((cleanup(gb_profile_end)))=gb_profile_begin(phase)
#else
#define GB_PROFILE_SCOPE(phase) ((void)0)
#endif
