/* Every merged fixed point is compared to the original instruction runner. */
#include "core.h"
#include "vendor/gnuboy/hw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef GNUBOY_WAIT_BATCH
extern int gb_cpu_test_wait_disabled;
int gb_cpu_test_wait(int available);
#endif
#ifdef GNUBOY_HALT_BATCH
extern int gb_cpu_test_halt_disabled;
int gb_cpu_test_halt(int available);
#endif
int main(int argc,char **argv){
    assert(argc==2);FILE *fp=fopen(argv[1],"rb");assert(fp);uint8_t rom[32768];assert(fread(rom,1,sizeof(rom),fp)==sizeof(rom));fclose(fp);
    unsigned cases=0,merged=0;
#ifdef GNUBOY_WAIT_BATCH
    for(unsigned nops=0;nops<3;++nops)for(unsigned high=0;high<2;++high)
    for(unsigned nonzero=0;nonzero<2;++nonzero)for(unsigned logic=0;logic<2;++logic)
    for(unsigned speed=0;speed<2;++speed)for(unsigned boundary=0;boundary<3;++boundary){
        unsigned addr=high?0xffb0:0xc123,at=0;uint8_t code[8]={0};
        at=nops;
        if(high){code[at++]=0xf0;code[at++]=addr&255;}
        else {code[at++]=0xfa;code[at++]=addr&255;code[at++]=addr>>8;}
        code[at++]=logic?0xb7:0xa7;code[at++]=nonzero?0x20:0x28;code[at]=(uint8_t)-(at+1);++at;
        memcpy(rom+0x200,code,at);assert(!gb_core_load(rom,sizeof(rom)));
        GB.cpu->pc.w=0x200;GB.cpu->af.b[1]=nonzero?0x81:0;GB.cpu->af.b[0]=(nonzero?0:FZ)|(logic?0:FH);
        GB.cpu->ime=GB.cpu->ima=1;GB.cpu->double_speed=speed;R_IF=0;R_IE=0x1f;
        writeb(addr,nonzero?0x81:0);R_TAC=5;R_TIMA=0xfc;GB.cpu->timer=111;GB.cycles=boundary==1?37:1000;GB.serial=boundary==2?73:0;
        if(boundary!=0)R_TAC=0;
        gb_cpu_t cpu_before=*GB.cpu;gb_t before=GB;gb_snd_t snd_before=*GB.snd;
        for(unsigned budget=1;budget<1200;++budget){
            GB=before;*GB.cpu=cpu_before;*GB.snd=snd_before;
            int elapsed=gb_cpu_test_wait(budget);assert(elapsed<(int)budget);
            gb_cpu_t cpu_fast=*GB.cpu;gb_t fast=GB;gb_snd_t snd_fast=*GB.snd;
            GB=before;*GB.cpu=cpu_before;*GB.snd=snd_before;
            gb_cpu_test_wait_disabled=1;
            if(elapsed)assert(gb_cpu_emulate(elapsed)==elapsed);
            gb_cpu_test_wait_disabled=0;
            assert(!memcmp(GB.cpu,&cpu_fast,sizeof(cpu_fast)));
            assert(!memcmp(&GB,&fast,sizeof(GB)));
            assert(!memcmp(GB.snd,&snd_fast,sizeof(snd_fast)));
            ++cases;if(elapsed)++merged;
        }
        GB=before;*GB.cpu=cpu_before;*GB.snd=snd_before;R_IF=1;assert(!gb_cpu_test_wait(1000));
        R_IF=0;GB.cpu->ime=0;assert(!gb_cpu_test_wait(1000));
        gb_core_close();
    }
#endif
#ifdef GNUBOY_HALT_BATCH
    for(unsigned speed=0;speed<2;++speed)for(unsigned irq=0;irq<2;++irq)
    for(unsigned boundary=0;boundary<3;++boundary){
        assert(!gb_core_load(rom,sizeof(rom)));
        GB.cpu->halted=1;GB.cpu->ime=GB.cpu->ima=irq;GB.cpu->double_speed=speed;
        R_IF=0;R_IE=0x1f;R_TAC=boundary?0:5;R_TIMA=0xfc;GB.cpu->timer=111;
        GB.cycles=boundary==1?37:1000;GB.serial=boundary==2?73:0;
        gb_cpu_t cpu_before=*GB.cpu;gb_t before=GB;gb_snd_t snd_before=*GB.snd;
        for(unsigned budget=1;budget<1200;++budget){
            GB=before;*GB.cpu=cpu_before;*GB.snd=snd_before;
            int elapsed=gb_cpu_test_halt(budget);assert(elapsed<(int)budget);
            gb_cpu_t cpu_fast=*GB.cpu;gb_t fast=GB;gb_snd_t snd_fast=*GB.snd;
            GB=before;*GB.cpu=cpu_before;*GB.snd=snd_before;
            gb_cpu_test_halt_disabled=1;
            if(elapsed)assert(gb_cpu_emulate(elapsed)==elapsed);
            gb_cpu_test_halt_disabled=0;
            assert(!memcmp(GB.cpu,&cpu_fast,sizeof(cpu_fast)));
            assert(!memcmp(&GB,&fast,sizeof(GB)));
            assert(!memcmp(GB.snd,&snd_fast,sizeof(snd_fast)));
            ++cases;if(elapsed)++merged;
        }
        R_IF=1;assert(!gb_cpu_test_halt(1000));gb_core_close();
    }
#endif
    assert(merged>10000);printf("PASS: %u idle fixed points, %u merged, enabled idle paths, both speeds, timer/serial/LCD/frame boundaries\n",cases,merged);
}
