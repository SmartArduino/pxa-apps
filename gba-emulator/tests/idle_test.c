/* GPL-2.0-or-later. Compare accelerated and ordinary polling at event edges. */
#include "core.h"
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* Freeze the cartridge RTC epoch for byte-exact cross-process traces. */
time_t time(time_t *out) { time_t value=1700000000; if(out)*out=value; return value; }
extern bool gba_cpu_idle_enabled;
static void word(unsigned off,u32 v){memcpy(vram+off,&v,4);}
static void half(unsigned off,u16 v){memcpy(vram+off,&v,2);}
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);u8 *rom=malloc(32768);assert(rom&&fread(rom,1,32768,f)==32768);fclose(f);
    u8 *states[2]={NULL,NULL};size_t size=0;
    for(unsigned thumb=0;thumb<2;++thumb)
    for(unsigned kind=0;kind<4;++kind)
    for(unsigned threshold=1;threshold<5;++threshold) {
        u32 result[2];
        for(unsigned enabled=0;enabled<2;++enabled) {
            gba_cpu_idle_enabled=enabled;
            assert(!emu_core_load(rom,32768));size=emu_state_size();
            if(!states[enabled]){states[enabled]=malloc(size);assert(states[enabled]);}
            memset(vram,0,128);reg[REG_PC]=0x06000000;reg[REG_CPSR]=thumb?0x3f:0x1f;
            reg[1]=kind==0?0x04000006:kind==1?0x03000020:kind==2?0x04000100:0x03000021;
            reg[2]=0x02000000;reg[3]=0x08000000;
            if(thumb){
                half(0,0x8808); /* LDRH r0,[r1] */
                half(2,0x2800|threshold);half(4,0xd3fc); /* CMP / BCC */
                half(6,0x6010);half(8,0x4718); /* STR / BX r3 */
            }else{
                word(0,0xe1d100b0);word(4,0xe3500000|threshold);word(8,0x3afffffc);
                word(12,0xe5820000);word(16,0xe12fff13);
            }
            result[enabled]=execute_arm_vram(execute_cycles);
            assert(!emu_state_save(states[enabled],size));emu_core_close();
        }
        assert(result[0]==result[1]);assert(!memcmp(states[0],states[1],size));
    }
    gba_cpu_idle_enabled=true;free(states[0]);free(states[1]);free(rom);
    puts("PASS: ARM/Thumb polling preserves complete state and cycle exit at video events, RAM waits, excluded timers and odd addresses");
}
