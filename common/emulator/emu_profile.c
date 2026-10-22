/* MPL-2.0. Diagnostic wall-clock accounting of every outer phase call.
 * Inclusive phases may nest; do not add them together. Never enable for FPS. */
#include "emu_profile.h"
#ifdef PXA_EMU_PROFILE_PHASES
#include <string.h>
#include <time.h>
emu_profile_phase_t emu_profile_phases[EMU_PROFILE_PHASE_COUNT];
uint64_t emu_profile_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
emu_profile_scope_t emu_profile_begin(unsigned phase) {
    emu_profile_phase_t *p=&emu_profile_phases[phase];
    emu_profile_scope_t s={phase,0,0};
    if(!p->depth++){++p->calls;s.sampled=1;s.started=emu_profile_now();}
    return s;
}
void emu_profile_end(emu_profile_scope_t *s) {
    emu_profile_phase_t *p=&emu_profile_phases[s->phase];--p->depth;
    if(s->sampled){p->us+=emu_profile_now()-s->started;++p->samples;}
}
void emu_profile_reset(void){memset(emu_profile_phases,0,sizeof(emu_profile_phases));}
#endif
