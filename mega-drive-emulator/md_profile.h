/* Optional diagnostics, compiled out of release builds. */
#pragma once
#ifdef PXA_MD_PROFILE
#include <stdint.h>
enum { MD_PHASE_frame, MD_PHASE_m68k, MD_PHASE_z80, MD_PHASE_draw,
       MD_PHASE_sound, MD_PHASE_s68k, MD_PHASE_msh2, MD_PHASE_ssh2, MD_PHASE_COUNT };
typedef struct { unsigned phase, sampled; uint64_t started; } md_profile_scope_t;
md_profile_scope_t md_profile_begin(unsigned phase);
void md_profile_end(md_profile_scope_t *scope);
void md_profile_reset(void);
void md_profile_frame(void);
#define pprof_start(name) \
    md_profile_scope_t md_scope_##name __attribute__((cleanup(md_profile_end))) = \
        md_profile_begin(MD_PHASE_##name)
#define pprof_end(...)
#define pprof_end_sub(...)
#define pprof_init()
#define pprof_finish()
#else
#define md_profile_reset() ((void)0)
#define md_profile_frame() ((void)0)
#endif
