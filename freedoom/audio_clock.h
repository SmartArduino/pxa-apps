// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FD_AUDIO_CLOCK_H
#define FD_AUDIO_CLOCK_H
#include <stdint.h>
#define FD_AUDIO_PACKET 220u
#define FD_AUDIO_LEAD (FD_AUDIO_PACKET*3u)
typedef struct {uint64_t base_us,last_us,frames;uint32_t epoch;unsigned ready,resets;} fd_audio_clock_t;
/* Feed 20 ms packets from elapsed time, independent of rendered frames. Keep
 * 40-60 ms queued; rebase after a pause/stall instead of replaying old backlog. */
static inline unsigned fd_audio_clock_take(fd_audio_clock_t *c,uint64_t now,uint32_t epoch) {
    if(!c->ready || c->epoch!=epoch || now<c->last_us || now-c->last_us>120000) {
        if(c->ready)++c->resets;
        c->base_us=now;c->frames=0;c->epoch=epoch;c->ready=1;
    }
    c->last_us=now;
    uint64_t elapsed=now-c->base_us;
    uint64_t target=elapsed/1000000*11025+(elapsed%1000000)*11025/1000000+FD_AUDIO_LEAD;
    if(target<=c->frames)return 0;
    uint64_t due=(target-c->frames)/FD_AUDIO_PACKET*FD_AUDIO_PACKET;
    if(due>FD_AUDIO_LEAD)due=FD_AUDIO_LEAD;
    c->frames+=due;
    return (unsigned)due;
}
#endif
