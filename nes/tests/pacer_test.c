#include "pacer.h"
#include "scheduler.h"
#include <assert.h>
#include <stdio.h>

static void simulate(unsigned hz,unsigned skip,unsigned draw,unsigned wanted,unsigned stall) {
    nes_schedule_t clock={0};nes_pacer_t p;nes_pacer_reset(&p);
    uint64_t now=1000000,start=now,frames=0,drawn=0;
    nes_schedule_tick(&clock,now,hz);
    while(now-start<60000000) {
        if(stall && now-start>=2000000){now+=stall;stall=0;}
        nes_schedule_tick(&clock,now,hz);
        nes_pacer_tick(&p,now,clock.debt,hz,1);
        unsigned count=0;
        while(nes_schedule_due(&clock) && count++<4) {
            uint64_t began=now;
            int render=nes_pacer_due(&p,now);
            unsigned work=render?draw:skip;
            now+=work;
            nes_pacer_observe(&p,render,work);
            nes_schedule_step(&clock);++frames;
            if(render){++drawn;nes_pacer_drew(&p,began);}
        }
        /* Dispatch/idle work is deliberately outside the pacer's cost sample. */
        now+=1000;
    }
    assert(frames>=hz*59 && frames<=hz*61);
    assert(clock.debt<3000000);
    assert(drawn>=wanted*59);
    printf("PASS: %u Hz budget, core/paint %u/%u us, simulated=%llu drawn=%llu\n",
        hz,skip,draw,(unsigned long long)frames,(unsigned long long)drawn);
}
int main(void) {
    simulate(60,500,1000,59,0);
    simulate(60,12000,21000,14,0);
    simulate(50,15000,26000,8,0);
    /* Measured Metal Max costs straddle the old integer 2/3-frame cliff.
     * Both workloads should sustain simulation and a near-30 fps display. */
    simulate(60,11060,19300,27,0);
    simulate(60,10550,18800,31,0);
    simulate(60,11060,19300,26,250000); /* Repay 250 ms by executing frames. */
    /* A previous stall must not permanently select the 250 ms fallback.
     * Historical emulation debt is retained; only drawing is scheduled. */
    nes_pacer_t p;nes_pacer_reset(&p);
    p.draw_us=1000;p.skip_us=500;
    nes_pacer_tick(&p,1000000,60000000,60,1);
    assert(p.every==1);
    for(unsigned i=0;i<60;++i){assert(nes_pacer_due(&p,1000000));nes_pacer_drew(&p,1000000);}
    nes_pacer_tick(&p,2000000,62000001,60,1);assert(p.feedback==1);
    nes_pacer_tick(&p,3000000,58000000,60,1);assert(p.feedback==1);
    nes_pacer_tick(&p,4000000,0,60,4);assert(p.every==4);
    nes_pacer_drew(&p,4000000);
    for(unsigned i=0;i<3;++i)assert(!nes_pacer_due(&p,4000000));
    assert(nes_pacer_due(&p,4000000));
    /* A blocked Surface retains the opportunity to draw on the next frame. */
    assert(nes_pacer_due(&p,4000000));nes_pacer_drew(&p,4000000);assert(!nes_pacer_due(&p,4000000));
    p.every=16;p.interval_q16=16u*65536;p.max_gap_us=200000;
    assert(nes_pacer_due(&p,4200000)); /* Overload still permits visual feedback. */
    /* Real Surface backpressure stabilizes near-two cadence only. */
    nes_pacer_reset(&p);p.skip_us=10000;p.draw_us=19750;
    nes_pacer_tick(&p,1000000,0,60,1);assert(p.interval_q16<2u*65536);
    p.buffer_limited=1;nes_pacer_tick(&p,1001000,0,60,1);
    assert(p.interval_q16==2u*65536);
    p.draw_us=20050;nes_pacer_tick(&p,1002000,0,60,1);
    assert(p.interval_q16>2u*65536);
    p.draw_us=18000;nes_pacer_tick(&p,1003000,0,60,1);
    assert(!p.buffer_limited && p.interval_q16<2u*65536-65536/8);
    /* Fixed costs requiring 2.01 frames alternate 2/3, avoiding a permanent
     * 20-fps step. Forced UI divisor and backpressure remain independent. */
    nes_pacer_reset(&p);p.skip_us=10000;p.draw_us=20050;
    nes_pacer_tick(&p,1000000,0,60,1);
    unsigned drawn=0,last=0;
    for(unsigned frame=1;frame<=2010;++frame) {
        if(nes_pacer_due(&p,1000000+frame*16667ull)) {
            assert(frame-last==2 || frame-last==3);last=frame;
            ++drawn;nes_pacer_drew(&p,1000000+frame*16667ull);
        }
    }
    assert(drawn>=999 && drawn<=1000);
    nes_pacer_tick(&p,35000000,0,60,4);last=0;
    for(unsigned frame=1;frame<=400;++frame) {
        if(nes_pacer_due(&p,35000000+frame*16667ull)) {
            /* The first draw can be forced by the elapsed maximum gap. */
            if(last)assert(frame-last>=4);last=frame;
            nes_pacer_drew(&p,35000000+frame*16667ull);
        }
    }
    /* More than a hundred blocked frames cannot accumulate a render burst. */
    nes_pacer_reset(&p);p.skip_us=10000;p.draw_us=20050;
    nes_pacer_tick(&p,1000000,0,60,1);
    for(unsigned i=0;i<100;++i)nes_pacer_due(&p,1000000);
    nes_pacer_drew(&p,1000000);
    assert(!nes_pacer_due(&p,1000000));
    /* Entry debt can jump by three frames across normal callback batches,
       while the queue reaches one frame or less in the same second. */
    nes_pacer_reset(&p);p.draw_us=19300;p.skip_us=11060;
    nes_pacer_tick(&p,1000000,1000000,60,1);
    nes_pacer_tick(&p,1500000,1000000,60,1);
    nes_pacer_tick(&p,2000000,4000001,60,1);
    assert(p.feedback==0);
    /* The same peak held throughout the next window is real backlog. */
    nes_pacer_tick(&p,2500000,4000001,60,1);
    nes_pacer_tick(&p,3000000,4000001,60,1);
    assert(p.feedback==1);
    /* A backlog which stops growing must still be repaid by emulation. */
    nes_pacer_reset(&p);p.draw_us=19300;p.skip_us=11060;
    nes_pacer_tick(&p,1000000,12000000,60,1);
    nes_pacer_tick(&p,2000000,12000000,60,1);assert(p.feedback==1);
    nes_pacer_tick(&p,3000000,8000000,60,1);assert(p.feedback==1);
    nes_pacer_tick(&p,4000000,1000000,60,1);assert(p.feedback==0);
    puts("PASS: independent simulation clock, debt feedback, explicit divisor and backpressure");
}
