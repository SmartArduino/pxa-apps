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
extern uint64_t gba_cpu_fusion_hits[6];
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
    for(unsigned counted=0;counted<2;++counted)
    for(unsigned destination=0;destination<5;++destination)
    for(unsigned budget=1;budget<=64;++budget) {
        u32 result[2];
        for(unsigned enabled=0;enabled<2;++enabled) {
            gba_cpu_fusion_enabled=enabled;assert(!emu_core_load(rom,32768));
            u32 code[]={0xe153000c,0x95932000,0x859f201c,0xe7812003,
                0xe2833004,0xe1530000,0x1afffff8,0xe12fff1d};
            if(counted) {
                u32 alternate[]={0xe152000c,0x95920000,0x859f0060,0xe2533001,
                    0xe7810002,0xe2822004,0x1afffff8,0xe12fff1d};
                memcpy(code,alternate,sizeof(code));
            }
            memcpy(vram,code,sizeof(code));
            for(unsigned i=0;i<64;++i)ewram[0x100 + i]=(u8)(i*37);
            u32 dst=destination==0?0x03000100:destination==1?0x06000200:destination==2?0x02000104:destination==3?0x050003f0:0x070003f0;
            reg[0]=0x02000120;reg[1]=dst-0x02000100;reg[3]=0x02000100;
            if(counted){reg[2]=reg[3];reg[3]=1+budget%4;}
            reg[12]=0x0effffff;reg[13]=0x08000000;
            reg[REG_PC]=0x06000000;reg[REG_CPSR]=0x1f;
            result[enabled]=execute_arm_vram(budget);
            memcpy(converted[enabled],palette_ram_converted,sizeof(converted[enabled]));
            assert(!emu_state_save(state[enabled],size));emu_core_close();
        }
        assert(result[0]==result[1]);assert(!memcmp(state[0],state[1],size));assert(!memcmp(converted[0],converted[1],sizeof(converted[0])));
    }
    /* Halfword CpuSet: page boundaries, overlapping data, unaligned fallback,
     * hooks, changed instructions, palette/OAM and mirrored writable code. */
    const unsigned half_budgets[]={1,2,3,4,5,7,11,12,13,14,15,16,17,18,19,20,21,22,31,32,39,40,41,47,63,64,95,127};
    for(unsigned kind=0;kind<6;++kind)
    for(unsigned destination=0;destination<6;++destination)
    for(unsigned b=0;b<sizeof(half_budgets)/sizeof(*half_budgets);++b) {
        u32 result[2];unsigned budget=half_budgets[b];
        for(unsigned enabled=0;enabled<2;++enabled) {
            gba_cpu_fusion_enabled=enabled;assert(!emu_core_load(rom,32768));
            u32 code[]={0xe1520004,0x91d2c0b0,0x859fc0a0,0xe181c0b2,
                0xe2822002,0xe1520003,0x1afffff8,0xe12fff1d};
            if(kind==3)code[4]=0xe2822004;
            memcpy(vram,code,sizeof(code));
            u32 src=0x02007ffe + (kind==1);
            for(unsigned i=0;i<16;++i)ewram[0x7ffe + i]=(u8)(i*37+0x80);
            u32 dst=destination==0?src+2:destination==1?0x03000100:
                destination==2?0x06018020:destination==3?0x050003fe:
                destination==4?0x070003fe:0x040000b0;
            if(kind==5){
                dst=0x06020000;u32 changed[2]={code[0]^1,code[1]};
                memcpy(ewram+(src&0x3ffff),changed,sizeof(changed));
            }
            cheat_master_hook=kind==2?0x06000014:~0u;
            reg[1]=dst-src;reg[2]=src;reg[3]=src+8;reg[4]=kind==4?src-1:0x0effffff;
            reg[5]=0x0effffff;reg[13]=0x08000000;
            reg[REG_PC]=0x06000000;reg[REG_CPSR]=0x1f|((budget&15)<<28);
            result[enabled]=execute_arm_vram(budget);
            memcpy(converted[enabled],palette_ram_converted,sizeof(converted[enabled]));
            assert(!emu_state_save(state[enabled],size));emu_core_close();
        }
        if(result[0]!=result[1]||memcmp(state[0],state[1],size))
            fprintf(stderr,"half copy kind=%u dst=%u budget=%u cycles=%u/%u\n",kind,destination,budget,result[0],result[1]);
        assert(result[0]==result[1]);assert(!memcmp(state[0],state[1],size));assert(!memcmp(converted[0],converted[1],sizeof(converted[0])));
    }
    cheat_master_hook=~0u;
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
            for(unsigned i=0;i<1024;++i)ewram[0x100 + i]=(u8)(i*37);
            reg[0]=0xffffffa5;reg[1]=0x33;reg[2]=0x1000;reg[3]=0x02000100;
            reg[4]=0x00400000 + (budget%9)*0x01000000;
            reg[5]=0x03001000 + ((budget%4)<<30);reg[6]=0xf8a3017c;reg[7]=0x7fff1294;
            reg[10]=0x00230000;reg[11]=0x000e0000;reg[phase]=0x54321;
            reg[13]=0x08000000;reg[REG_PC]=0x06000000;
            reg[REG_CPSR]=0x1f|((budget&15)<<28);
            result[enabled]=execute_arm_vram(budget);
            memcpy(converted[enabled],palette_ram_converted,sizeof(converted[enabled]));
            assert(!emu_state_save(state[enabled],size));emu_core_close();
        }
        assert(result[0]==result[1]);assert(!memcmp(state[0],state[1],size));assert(!memcmp(converted[0],converted[1],sizeof(converted[0])));
    }
    /* Signed samples, page crossings, all carry phases and exact event
     * boundaries. Changed bytes and hooks must use ordinary instructions. */
    for(unsigned modified=0;modified<3;++modified)
    for(unsigned budget=1;budget<=64;++budget) {
        u32 result[2];
        for(unsigned enabled=0;enabled<2;++enabled) {
            gba_cpu_fusion_enabled=enabled;assert(!emu_core_load(rom,32768));
            u32 code[]={0xe0d300d1,0xe001009a,0xe3c118ff,0xe0816466,
                0xe001009b,0xe3c118ff,0xe0817467,0xe2955101,0x3afffff6,0xe12fff1d};
            if(modified==1)code[3]^=1;
            memcpy(vram,code,sizeof(code));
            cheat_master_hook=modified==2?0x06000010:~0u;
            for(unsigned i=0;i<8;++i)ewram[0x7ffe + i]=(u8)(i*53+budget);
            reg[3]=0x02007ffe;reg[5]=((budget%4)<<30)|(budget&1?0:0x03001000);
            reg[6]=0xf8a3017c;reg[7]=0x7fff1294;
            reg[10]=0x00230000;reg[11]=0x000e0000;
            reg[13]=0x08000000;reg[REG_PC]=0x06000000;
            reg[REG_CPSR]=0x1f|((budget&15)<<28);
            result[enabled]=execute_arm_vram(budget);
            assert(!emu_state_save(state[enabled],size));emu_core_close();
        }
        assert(result[0]==result[1]);assert(!memcmp(state[0],state[1],size));
    }
    cheat_master_hook=~0u;
    /* Change a validated IWRAM block through both single and block stores,
     * including its mirror. The second pass must observe the changed opcode. */
    for(unsigned direct=0;direct<2;++direct)
    for(unsigned block_store=0;block_store<2;++block_store)
    for(unsigned mirror=0;mirror<2;++mirror) {
        u32 result[2];
        for(unsigned enabled=0;enabled<2;++enabled) {
            gba_cpu_fusion_enabled=enabled;assert(!emu_core_load(rom,32768));
            u32 code[]={0xe009019e,0xe0809bc9,0xe00c099a,0xe3ccc8ff,
                0xe08c6466,0xe00c099b,0xe3ccc8ff,0xe08c7467,0xe08ee004,0xe1b09bae,
                block_store?0xe88d0100:0xe58d8000,0xe2522001,0x1afffff2,
                0xe58a6000,0xe59f0008,0xe3a01000,0xe5c01000,0xeafffffe,0x04000301};
            if(direct) {
                u32 alternate[]={0xe0d300d1,0xe001009a,0xe3c118ff,0xe0816466,
                    0xe001009b,0xe3c118ff,0xe0817467,0xe2955101,0x3afffff6,
                    block_store?0xe88d0100:0xe58d8000,0xe2522001,0x1afffff3,
                    0xe58a6000,0xe59f0008,0xe3a01000,0xe5c01000,0xeafffffe,0x04000301};
                memcpy(code,alternate,sizeof(alternate));
            }
            memcpy(iwram+0xf000,code,sizeof(code));
            for(unsigned i=0;i<15;++i)reg[i]=0x1267 + i*37;
            reg[2]=2;reg[8]=0xe08c7466;reg[10]=0x0203f000;
            if(direct) {
                reg[3]=0x02000100;reg[5]=0x03001000;reg[8]=0xe0816467;
                for(unsigned i=0;i<16;++i)ewram[0x100 + i]=(u8)(i*37);
            }
            reg[13]=(mirror?0x0300f000:0x03007000)+(direct?12:28);
            reg[REG_PC]=0x03007000;reg[REG_CPSR]=0x1f;
            execute_arm(32);result[enabled]=execute_cycles;
            memcpy(converted[enabled],palette_ram_converted,sizeof(converted[enabled]));
            assert(!emu_state_save(state[enabled],size));emu_core_close();
        }
        assert(result[0]==result[1]);assert(!memcmp(state[0],state[1],size));assert(!memcmp(converted[0],converted[1],sizeof(converted[0])));
    }
    for(unsigned i=0;i<6;++i)assert(gba_cpu_fusion_hits[i]);
    gba_cpu_fusion_enabled=true;free(state[0]);free(state[1]);free(rom);
    puts("PASS: fused ARM blocks preserve full state, NZCV, cycle edges and modified code");
}
