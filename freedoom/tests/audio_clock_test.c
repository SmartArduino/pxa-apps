// SPDX-License-Identifier: GPL-3.0-or-later
#include "../audio_clock.h"
#include <assert.h>
#include <stdio.h>
static unsigned jitter(unsigned legacy) {
    fd_audio_clock_t clock={0};unsigned queued=0,missing=0,fraction=0;
    for(unsigned us=0;us<=2000000;us+=1000) {
        if(us) {
            fraction+=11025;unsigned consumed=fraction/1000;fraction%=1000;
            if(consumed>queued) {missing+=consumed-queued;queued=0;} else queued-=consumed;
        }
        unsigned phase=us%70000;
        if(legacy ? phase==0 : (phase==0 || phase==18000 || phase==40000 || phase==55000)) {
            unsigned frames=legacy?315:fd_audio_clock_take(&clock,us,1);
            if(!legacy)assert(frames<=FD_AUDIO_LEAD && !(frames%FD_AUDIO_PACKET));
            queued+=frames;
            assert(queued<880);
        }
    }
    return missing;
}
int main(void) {
    unsigned old=jitter(1),current=jitter(0);
    assert(old>10000 && current==0);
    fd_audio_clock_t c={0};
    assert(fd_audio_clock_take(&c,0,1)==660);
    assert(!fd_audio_clock_take(&c,1000,1));
    assert(fd_audio_clock_take(&c,20000,1)==220);
    assert(fd_audio_clock_take(&c,5000000,1)==660 && c.resets==1);
    assert(fd_audio_clock_take(&c,5001000,2)==660 && c.resets==2);
    assert(fd_audio_clock_take(&c,1,2)==660 && c.resets==3);
    printf("PASS: 14 FPS renderer with cooperative phases: missing source samples %u -> %u; bounded packets, stalls and lifecycle reset\n",old,current);
}
