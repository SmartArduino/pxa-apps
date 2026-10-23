/* Bus clients use the live instruction PC for polling detection and mappers. */
#include "vendor/picodrive/cpu/fame/fame.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static M68K_CONTEXT cpu;
static uint16_t mem[32768];
static unsigned reads,writes,exceptions,resets;
static void reset_cb(void) { assert(fm68k_get_pc(&cpu)==0x102);++resets; }
static unsigned rb(unsigned a) { return (mem[(a&0xffff)>>1]>>(a&1?0:8))&255; }
static unsigned rw(unsigned a) {
    if(a==0xc00004) { assert(fm68k_get_pc(&cpu)==0x110);++reads;return 0x9876; }
    return mem[(a&0xffff)>>1];
}
static unsigned rl(unsigned a) {
    if(a==32*4) { assert(fm68k_get_pc(&cpu)==0x112);++exceptions; }
    return (rw(a)<<16)|rw(a+2);
}
static void wb(unsigned a,unsigned char d) {
    unsigned i=(a&0xffff)>>1;mem[i]=(a&1)?(mem[i]&0xff00)|d:(mem[i]&255)|(d<<8);
}
static void ww(unsigned a,unsigned short d) { mem[(a&0xffff)>>1]=d; }
static void wl(unsigned a,unsigned d) {
    if(a==0xc00000) { assert(fm68k_get_pc(&cpu)==0x10a);assert(d==0x12345678);++writes; }
    else { ww(a,d>>16);ww(a+2,d); }
}
int main(void) {
    const uint16_t program[]={0x23fc,0x1234,0x5678,0x00c0,0x0000,
                              0x3039,0x00c0,0x0004,0x4e40};
    memcpy(mem+0x100/2,program,sizeof(program));
    mem[0]=0;mem[1]=0x800;mem[2]=0;mem[3]=0x100;
    mem[32*2]=0;mem[32*2+1]=0x200;
    mem[0x200/2]=0x4e72;mem[0x202/2]=0x2700;
    cpu.read_byte=rb;cpu.read_word=rw;cpu.read_long=rl;
    cpu.write_byte=wb;cpu.write_word=ww;cpu.write_long=wl;
    for(unsigned i=0;i<M68K_FETCHBANK1;++i)cpu.Fetch[i]=(uintptr_t)mem;
    fm68k_init();assert(fm68k_reset(&cpu)==M68K_OK);
    fm68k_emulate(&cpu,200,fm68k_reason_emulate);
    assert(reads==1 && writes==1 && exceptions==1);
    assert((cpu.dreg[0].D&0xffff)==0x9876);
    assert(cpu.execinfo&FM68K_HALTED);
    memset(mem,0,sizeof(mem));
    mem[0]=0;mem[1]=0x800;mem[2]=0;mem[3]=0x100;
    mem[0x100/2]=0x4e70;mem[0x102/2]=0x4e72;mem[0x104/2]=0x2700;
    assert(fm68k_reset(&cpu)==M68K_OK);cpu.reset_handler=reset_cb;
    fm68k_emulate(&cpu,200,fm68k_reason_emulate);
    assert(resets==1 && (cpu.execinfo&FM68K_HALTED));
    puts("live PC preserved across read/write/exception/reset callbacks");
}
