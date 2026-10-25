/* GPL-2.0-or-later. Flag publication at mode, PSR, memory and IRQ boundaries. */
#include "core.h"
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void word(unsigned offset,u32 opcode) {memcpy(vram+offset,&opcode,4);}
static void half(unsigned offset,u16 opcode) {memcpy(vram+offset,&opcode,2);}
static u32 add_flags(u32 a,u32 b) {
    u32 result=a+b;
    return (result&0x80000000u) | (!result?0x40000000u:0) |
        ((u64)a+b>UINT32_MAX?0x20000000u:0) |
        ((~(a^b)&(a^result)&0x80000000u)>>3);
}
static void prepare(u32 a,u32 b,bool thumb) {
    init_cpu();memset(vram,0,64);
    reg[0]=a;reg[1]=b;reg[3]=0x1f;reg[4]=0x03000000;reg[12]=0x08000000;
    reg[REG_PC]=0x06000000;reg[REG_CPSR]=thumb?0x3f:0x1f;
    write_ioreg(REG_IE,0);write_ioreg(REG_IF,0);write_ioreg(REG_IME,0);
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    u8 *rom=malloc(32768);assert(rom&&fread(rom,1,32768,f)==32768);fclose(f);
    assert(!emu_core_load(rom,32768));
    const u32 values[]={0,1,0xffffffff,0x7fffffff,0x80000000,0x40000000,0xdeadbeef};
    for(unsigned i=0;i<sizeof(values)/sizeof(*values);++i)
    for(unsigned j=0;j<sizeof(values)/sizeof(*values);++j) {
        u32 a=values[i],b=values[j],flags=add_flags(a,b);
        prepare(a,b,false);
        word(0,0xe0902001); /* ADDS r2,r0,r1 */
        word(4,0xe121f003); /* MSR CPSR_c,r3: preserve freshly calculated NZCV */
        word(8,0xe10f5000); /* MRS r5,CPSR */
        word(12,0xe5842000); /* STR r2,[r4] */
        word(16,0xe8a40004); /* STMIA r4!,{r2} */
        word(20,0xe12fff1c); /* BX r12: return from interpreted VRAM */
        execute_arm_vram(10000);
        assert(reg[2]==a+b&&reg[5]==(flags|0x1f));
        assert(reg[REG_CPSR]==(flags|0x1f));
        assert(read_memory32(0x03000000)==a+b);

        prepare(a,b,true);reg[3]=0x06000020;
        half(0,0x1842); /* ADDS r2,r0,r1 */
        half(2,0x6022); /* STR r2,[r4] */
        half(4,0xc404); /* STMIA r4!,{r2} */
        half(6,0x4718); /* BX r3: Thumb -> ARM */
        word(32,0xe10f5000);word(36,0xe12fff1c);
        execute_arm_vram(10000);
        assert(reg[2]==a+b&&reg[5]==(flags|0x1f));
        assert(reg[REG_CPSR]==(flags|0x1f));

        for(unsigned thumb=0;thumb<2;++thumb) {
            prepare(a,b,thumb);
            if(thumb){half(0,0x1842);half(2,0xdf06);}
            else {word(0,0xe0902001);word(4,0xef060000);}
            execute_arm_vram(10000);
            assert(reg[REG_PC]==8);
            assert(REG_SPSR(MODE_SUPERVISOR)==(flags|(thumb?0x3f:0x1f)));
        }

        /* Enabling IRQ via MSR must publish the flags before saving SPSR. */
        prepare(a,b,false);reg[REG_CPSR]=0x9f;
        write_ioreg(REG_IE,1);write_ioreg(REG_IF,1);write_ioreg(REG_IME,1);
        word(0,0xe0902001);word(4,0xe121f003);
        execute_arm_vram(10000);
        assert(reg[REG_PC]==0x18&&REG_SPSR(MODE_IRQ)==(flags|0x1f));
    }
    emu_core_close();free(rom);
    puts("PASS: NZCV, PSR control writes, ARM/Thumb changes, scalar/block stores, SWI and IRQ");
}
