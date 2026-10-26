/* GPL-2.0-or-later. Interpreter fast paths versus the canonical bus handlers. */
#include "core.h"
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void word(unsigned off,u32 value) {memcpy(vram+off,&value,4);}
static void reset(u32 address,u32 value,u32 opcode) {
    init_cpu();memset(ewram,0xa5,sizeof(ewram));memset(iwram,0xa5,sizeof(iwram));
    memset(vram,0xa5,sizeof(vram));memset(palette_ram,0xa5,sizeof(palette_ram));
    memset(palette_ram_converted,0xa5,sizeof(palette_ram_converted));
    memset(oam_ram,0xa5,sizeof(oam_ram));memset(io_registers,0,sizeof(io_registers));
    word(0,opcode);word(4,0xe12fff1c); /* BX r12 */
    reg[0]=value;reg[1]=address;reg[12]=0x08000000;
    reg[REG_PC]=0x06000000;reg[REG_CPSR]=0x1f;reg[OAM_UPDATED]=0;
}
static u32 digest(void) {
    u32 crc=0;
#define HASH(a) crc=emu_port_crc32(crc,a,sizeof(a))
    HASH(ewram);HASH(iwram);HASH(vram);HASH(palette_ram);
    HASH(palette_ram_converted);HASH(oam_ram);HASH(io_registers);
#undef HASH
    return crc^reg[OAM_UPDATED];
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    u8 *rom=malloc(32768);assert(rom&&fread(rom,1,32768,f)==32768);fclose(f);
    assert(!emu_core_load(rom,32768));
    for(unsigned i=0;i<8192;++i)assert(!((uintptr_t)memory_map_read[i]&3));
    const u32 addresses[]={0x02000000,0x0203fffc,0x02fffffc,0x03000000,0x03fffffc,
        0x04000000,0x050003fc,0x06010020,0x06018020,0x06fffffc,0x070003fc,0x10000000};
    const u32 values[]={0,0x12345678,0xffffffff};
    const u32 stores[]={0xe5c10000,0xe1c100b0,0xe5810000}; /* STRB, STRH, STR */
    for(unsigned a=0;a<sizeof(addresses)/sizeof(*addresses);++a)
    for(unsigned lane=0;lane<4;++lane)
    for(unsigned v=0;v<sizeof(values)/sizeof(*values);++v)
    for(unsigned size=0;size<3;++size) {
        u32 address=addresses[a]+lane,value=values[v],op=stores[size];
        reset(address,value,op);
        if(size==0)write_memory8(address,value);
        if(size==1)write_memory16(address&~1u,value);
        if(size==2)write_memory32(address&~3u,value);
        u32 expected=digest();reset(address,value,op);execute_arm_vram(10000);
        assert(digest()==expected);
    }
    /* Shared scalar read helpers must retain signed extension, word rotation,
     * bus mirrors and the current-instruction PC convention for I/O. */
    const u32 reads[]={0xe5d10000,0xe1d100b0,0xe5910000,0xe1d100d0,0xe1d100f0};
    const u32 read_addresses[]={0,0x02000000,0x02fffffc,0x03000000,0x03fffffc,
        0x04000000,0x04000100,0x040003fc,0x050003fc,0x06018020,0x070003fc,0x10000000};
    for(unsigned a=0;a<sizeof(read_addresses)/sizeof(*read_addresses);++a)
    for(unsigned lane=0;lane<4;++lane)
    for(unsigned size=0;size<5;++size) {
        reset(read_addresses[a]+lane,0,reads[size]);
        u32 data=0x89abcdef;memcpy(ewram,&data,4);
        bool io=(reg[1]>>24)==4 && io_read_requires_handler(reg[1]);
        if(!io)reg[REG_PC]+=4;
        u32 expected=size==0?read_memory8(reg[1]):size==1?(u16)read_memory16(reg[1]):
            size==2?read_memory32(reg[1]):size==3?(u32)(s32)(s8)read_memory8(reg[1]):
            (u32)(s32)(s16)read_memory16_signed(reg[1]);
        if(!io)reg[REG_PC]-=4;
        execute_arm_vram(10000);
        if(reg[0]!=expected)fprintf(stderr,"read=%u address=%08x got=%08x expected=%08x\n",size,read_addresses[a]+lane,reg[0],expected);
        assert(reg[0]==expected);
    }
    /* A block load outside the 28-bit bus must not index the page table. */
    reset(0x10000000,0,0xe8910001); /* LDMIA r1,{r0} */
    reg[REG_PC]+=4;u32 expected=read_memory32(reg[1]);reg[REG_PC]-=4;
    execute_arm_vram(10000);assert(reg[0]==expected);
    emu_core_close();free(rom);
    puts("PASS: scalar stores, mirror windows, palette/OAM/VRAM lanes, unaligned reads and out-of-bus block loads");
}
