/* MPL-2.0. Diagnostic wall-clock accounting of every outer phase call.
 * Inclusive phases may nest; do not add them together. Never enable for FPS. */
#include "gb_profile.h"
#ifdef PXA_GB_PROFILE_PHASES
#include <string.h>
#include <time.h>
gb_profile_phase_t gb_profile_phases[GB_PROFILE_PHASE_COUNT];
uint64_t gb_profile_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
gb_profile_scope_t gb_profile_begin(unsigned phase) {
    gb_profile_phase_t *p=&gb_profile_phases[phase];
    gb_profile_scope_t s={phase,0,0};
    if(!p->depth++){++p->calls;s.sampled=1;s.started=gb_profile_now();}
    return s;
}
void gb_profile_end(gb_profile_scope_t *s) {
    gb_profile_phase_t *p=&gb_profile_phases[s->phase];--p->depth;
    if(s->sampled){p->us+=gb_profile_now()-s->started;++p->samples;}
}
void gb_profile_reset(void){memset(gb_profile_phases,0,sizeof(gb_profile_phases));}
#endif
