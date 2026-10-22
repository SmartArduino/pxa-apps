/* Independent timed workload: rendering, external work and clock debt. */
#include "pacer.h"
#include "scheduler.h"
#include <assert.h>
#include <stdio.h>
static void simulate(unsigned skip,unsigned draw,unsigned outside,unsigned minimum_fps,unsigned stall){
    gb_schedule_t clock={0};gb_pacer_t p;gb_pacer_reset(&p);
    uint64_t now=1000000,start=now,frames=0,drawn=0;
    gb_schedule_tick(&clock,now,60);
    while(now-start<60000000){
        if(stall && now-start>=2000000){now+=stall;stall=0;}
        gb_schedule_tick(&clock,now,60);
        unsigned count=0;
        while(gb_schedule_due(&clock) && count++<4){
            gb_pacer_tick(&p,now,clock.debt/GB_FRAME_CYCLES,60,1);
            int render=gb_pacer_due(&p,now);unsigned work=render?draw:skip;
            now+=work;gb_pacer_observe(&p,render,work);gb_schedule_step(&clock);++frames;
            if(render){++drawn;gb_pacer_drew(&p,now);}
        }
        now+=outside;
    }
    if(skip<16000){
        assert(frames>=59*59 && frames<=60*61);
        assert(clock.debt/GB_FRAME_CYCLES<3000000);
        assert(drawn>=minimum_fps*59);
    }else{
        assert(frames>2000 && frames<59*59);
        assert(clock.debt/GB_FRAME_CYCLES>100000000); /* Retain genuine lag. */
        assert(drawn>100); /* Still offer visible progress under overload. */
    }
    printf("pacer skip=%u draw=%u outside=%u frames=%llu rendered=%llu\n",skip,draw,outside,
        (unsigned long long)frames,(unsigned long long)drawn);
}
int main(void){
    simulate(14000,24000,500,9,0);
    simulate(11060,19300,1000,27,0);
    simulate(11060,19300,1000,24,250000);
    simulate(21000,35000,500,0,0);
}
