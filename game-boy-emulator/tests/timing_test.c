#include "scheduler.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    gb_schedule_t s={0};uint64_t frames=0;
    const uint64_t duration=UINT64_C(3600000000);
    gb_schedule_tick(&s,100,60);
    for(uint64_t t=8000;t<=duration;t+=8000) {
        gb_schedule_tick(&s,t+100,60);
        while(gb_schedule_due(&s)){gb_schedule_step(&s);++frames;}
    }
    assert(frames==duration*GB_CLOCK_HZ/(UINT64_C(1000000)*GB_FRAME_CYCLES));
    assert(frames==215019);
    uint64_t before=s.debt;
    gb_schedule_tick(&s,duration+100+1000000,60);
    assert(s.debt==before+UINT64_C(1000000)*GB_CLOCK_HZ);
    gb_schedule_reset(&s);assert(!gb_schedule_due(&s));
    puts("PASS: exact 59.7275 Hz clock over one hour, fractional debt and reset");
}
