/* GPL-2.0-or-later. Exercise wall-time cadence, not just interval formulas. */
#include "../pacer.h"
#include <assert.h>
#include <stdio.h>

typedef struct { unsigned draws, frames; } cadence_t;

static cadence_t simulate(unsigned draw_us,unsigned skip_us,unsigned host_us,
                         unsigned minimum,int slow) {
    emu_pacer_t p;emu_pacer_reset(&p);
    uint64_t now=1,last=0,end=20000001;
    unsigned frames=0;
    cadence_t result={0};
    while(now<end) {
        uint64_t debt=slow?now*60:0;
        emu_pacer_tick(&p,now,debt,60,minimum);
        int drawn=emu_pacer_due(&p,now);
        uint64_t started=now;
        if(drawn) {
            assert(!last || frames-last>=minimum);
            last=frames;
            if(now>=5000001)++result.draws;
            emu_pacer_drew(&p,started);
        }
        unsigned cost=drawn?draw_us:skip_us;
        if(now>=5000001)++result.frames;
        emu_pacer_observe(&p,drawn,cost);
        now+=cost+host_us;
        if(!slow && now<1+(uint64_t)(frames+1)*1000000/60)
            now=1+(uint64_t)(frames+1)*1000000/60;
        ++frames;
    }
    printf("draw=%u skip=%u host=%u divisor=%u fps=%.2f sim_hz=%.2f\n",
           draw_us,skip_us,host_us,minimum,result.draws/15.0,result.frames/15.0);
    return result;
}

int main(void) {
    /* A fast core stays at 60 Hz. The fractional 1.5-frame case stays near 40. */
    assert(simulate(7000,4000,1000,1,0).draws>=895);
    unsigned medium=simulate(20000,5000,1000,1,0).draws;
    assert(medium>=590 && medium<=610);
    /* Real S31 cost ranges with growing CPU debt and dispatch overhead. */
    assert(simulate(65000,25000,2500,1,1).draws>150);
    assert(simulate(85000,50000,3500,1,1).draws>150);
    assert(simulate(45000,38000,2500,1,1).draws>150);
    /* Exceeding the 90 ms budget alone does not justify halving display
     * cadence when continuous drawing can still sustain more than 10 FPS. */
    assert(simulate(92000,30000,2500,1,1).draws>150);
    /* User divisors remain explicit, even when the wall deadline expires. */
    assert(simulate(85000,50000,3500,3,1).draws<150);
    /* A 165 ms render cannot achieve 10 display FPS. It must not also force
     * every simulated frame to pay that cost; retain useful visible updates
     * while advancing more than 10 complete CPU/APU frames per second. */
    cadence_t heavy=simulate(165000,29000,2500,1,1);
    assert(heavy.frames>=150 && heavy.draws>=70 && heavy.draws<100);
    emu_pacer_t p;emu_pacer_reset(&p);
    /* A costly initial full repaint must allow a skipped-cost measurement,
     * even if no skipped frame has been observed and the deadline expired. */
    emu_pacer_observe(&p,1,165000);
    emu_pacer_drew(&p,1);
    emu_pacer_tick(&p,165001,9900000,60,1);
    assert(!emu_pacer_due(&p,165001));
    emu_pacer_observe(&p,0,29000);
    emu_pacer_tick(&p,194001,11640000,60,1);
    assert(emu_pacer_due(&p,194001));
    emu_pacer_reset(&p);
    p.draw_us=25000;p.skip_us=5000;
    emu_pacer_tick(&p,1,0,60,1);
    for(unsigned n=0;n<100;++n)emu_pacer_due(&p,n+1);
    assert(p.credit_q16<p.interval_q16+65536);
    emu_pacer_drew(&p,101);
    assert(!emu_pacer_due(&p,102));
    /* Recover after a slow scene; accumulated debt must not lock video at 4Hz. */
    p.feedback=15;p.draw_us=7000;p.skip_us=4000;
    emu_pacer_tick(&p,1000001,60000000,60,1);
    assert(p.interval_q16<16u*65536);
    return 0;
}
