/* GPL-2.0-or-later. Instruction fusion must preserve all state at event edges. */
#include "core.h"
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
time_t time(time_t *out) {time_t value=1700000000;if(out)*out=value;return value;}
extern bool gba_cpu_fusion_enabled;
extern uint64_t gba_cpu_fusion_hits[3];
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    u8 *rom=malloc(32768);assert(rom&&fread(rom,1,32768,f)==32768);fclose(f);
    u8 *state[2]={NULL,NULL};u16 converted[2][512];size_t size=0;
    for(unsigned variant=0;variant<2;++variant)
    for(unsigned modified=0;modified<2;++modified)
    for(unsigned budget=1;budget<=48;++budget) {
        u32 result[2];
        for(unsigned enabled=0;enabled<2;++enabled) {
            gba_cpu_fusion_enabled=enabled;assert(!emu_core_load(rom,32768));size=emu_state_size();
            if(!state[enabled]){state[enabled]=malloc(size);assert(state[enabled]);}
            u32 tmp=variant?14:9,phase=variant?9:14;
            u32 code[]={0xe0000190|tmp<<16|phase,0xe0800bc0|tmp<<12|tmp,
                0xe00c009a|tmp<<8,0xe3ccc8ff,0xe08c6466,0xe00c009b|tmp<<8,
                0xe3ccc8ff,0xe08c7467,0xe0800004|phase<<16|phase<<12,
                0xe1b00ba0|tmp<<12|phase,0xe12fff1d};
            if(modified)code[7]^=1; /* A changed byte must disable the fast path. */
            memcpy(vram,code,sizeof(code));
            for(unsigned i=0;i<15;++i)reg[i]=(budget+1)*(0x7134569u+i*0xabc9347u);
            reg[13]=0x08000000;reg[REG_PC]=0x06000000;
            reg[REG_CPSR]=0x1f|((budget&15)<<28);
            result[enabled]=execute_arm_vram(budget);
            memcpy(converted[enabled],palette_ram_converted,sizeof(converted[enabled]));
            assert(!emu_state_save(state[enabled],size));emu_core_close();
        }
        assert(result[0]==result[1]);assert(!memcmp(state[0],state[1],size));assert(!memcmp(converted[0],converted[1],sizeof(converted[0])));
    }
    for(unsigned destination=0;destination<5;++destination)
    for(unsigned budget=1;budget<=32;++budget) {
        u32 result[2];
        for(unsigned enabled=0;enabled<2;++enabled) {
            gba_cpu_fusion_enabled=enabled;assert(!emu_core_load(rom,32768));
            u32 code[]={0xe153000c,0x95932000,0x859f201c,0xe7812003,
                0xe2833004,0xe1530000,0x1afffff8,0xe12fff1d};
            memcpy(vram,code,sizeof(code));
            for(unsigned i=0;i<64;++i)ewram[0x100+i]=(u8)(i*37);
            u32 dst=destination==0?0x03000100:destination==1?0x06000200:destination==2?0x02000104:destination==3?0x050003f0:0x070003f0;
            reg[0]=0x02000120;reg[1]=dst-0x02000100;reg[3]=0x02000100;
            reg[12]=0x0effffff;reg[13]=0x08000000;
            reg[REG_PC]=0x06000000;reg[REG_CPSR]=0x1f;
            result[enabled]=execute_arm_vram(budget);
            memcpy(converted[enabled],palette_ram_converted,sizeof(converted[enabled]));
            assert(!emu_state_save(state[enabled],size));emu_core_close();
        }
        assert(result[0]==result[1]);assert(!memcmp(state[0],state[1],size));assert(!memcmp(converted[0],converted[1],sizeof(converted[0])));
    }
    for(unsigned variant=0;variant<2;++variant)
    for(unsigned budget=1;budget<=48;++budget) {
        u32 result[2];
        for(unsigned enabled=0;enabled<2;++enabled) {
            gba_cpu_fusion_enabled=enabled;assert(!emu_core_load(rom,32768));
            u32 tmp=variant?14:9,phase=variant?9:14;
            u32 code[]={0xe0000190|tmp<<16|phase,0xe0800bc0|tmp<<12|tmp,
                0xe00c009a|tmp<<8,0xe3ccc8ff,0xe08c6466,0xe00c009b|tmp<<8,
                0xe3ccc8ff,0xe08c7467,0xe0800004|phase<<16|phase<<12,
                0xe1b00ba0|tmp<<12|phase,0x0a000007,
                0xe3c005fe|phase<<16|phase<<12,0xe0522000|tmp,0xda000006,
                0xe2500001|tmp<<16|tmp<<12,0x00800001,0x11b300d0|tmp,
                0xe1f310d1,0xe0411000,0xe2955101,0x3affffea,0xe12fff1d};
            memcpy(vram,code,sizeof(code));
            for(unsigned i=0;i<1024;++i)ewram[0x100+i]=(u8)(i*37);
            reg[0]=0xffffffa5;reg[1]=0x33;reg[2]=0x1000;reg[3]=0x02000100;
            reg[4]=0x00400000+(budget%9)*0x01000000;
            reg[5]=0x03001000+((budget%4)<<30);reg[6]=0xf8a3017c;reg[7]=0x7fff1294;
            reg[10]=0x00230000;reg[11]=0x000e0000;reg[phase]=0x54321;
            reg[13]=0x08000000;reg[REG_PC]=0x06000000;
            reg[REG_CPSR]=0x1f|((budget&15)<<28);
            result[enabled]=execute_arm_vram(budget);
            memcpy(converted[enabled],palette_ram_converted,sizeof(converted[enabled]));
            assert(!emu_state_save(state[enabled],size));emu_core_close();
        }
        assert(result[0]==result[1]);assert(!memcmp(state[0],state[1],size));assert(!memcmp(converted[0],converted[1],sizeof(converted[0])));
    }
    /* Change a validated IWRAM block through both single and block stores,
     * including its mirror. The second pass must observe the changed opcode. */
    for(unsigned block_store=0;block_store<2;++block_store)
    for(unsigned mirror=0;mirror<2;++mirror) {
        u32 result[2];
        for(unsigned enabled=0;enabled<2;++enabled) {
            gba_cpu_fusion_enabled=enabled;assert(!emu_core_load(rom,32768));
            u32 code[]={0xe009019e,0xe0809bc9,0xe00c099a,0xe3ccc8ff,
                0xe08c6466,0xe00c099b,0xe3ccc8ff,0xe08c7467,0xe08ee004,0xe1b09bae,
                block_store?0xe88d0100:0xe58d8000,0xe2522001,0x1afffff2,
                0xe58a6000,0xe59f0008,0xe3a01000,0xe5c01000,0xeafffffe,0x04000301};
            memcpy(iwram+0xf000,code,sizeof(code));
            for(unsigned i=0;i<15;++i)reg[i]=0x1267+i*37;
            reg[2]=2;reg[8]=0xe08c7466;reg[10]=0x0203f000;
            reg[13]=(mirror?0x0300f000:0x03007000)+28;
            reg[REG_PC]=0x03007000;reg[REG_CPSR]=0x1f;
            execute_arm(32);result[enabled]=execute_cycles;
            memcpy(converted[enabled],palette_ram_converted,sizeof(converted[enabled]));
            assert(!emu_state_save(state[enabled],size));emu_core_close();
        }
        assert(result[0]==result[1]);assert(!memcmp(state[0],state[1],size));assert(!memcmp(converted[0],converted[1],sizeof(converted[0])));
    }
    assert(gba_cpu_fusion_hits[0] && gba_cpu_fusion_hits[1] && gba_cpu_fusion_hits[2]);
    gba_cpu_fusion_enabled=true;free(state[0]);free(state[1]);free(rom);
    puts("PASS: fused ARM blocks preserve full state, NZCV, cycle edges and modified code");
}
