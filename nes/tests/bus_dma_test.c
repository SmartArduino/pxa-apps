/* GPL-2.0-or-later: live mapper flags, DMA source side effects and OAM wrap. */
#include "core.h"
#include "nes/nes.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static unsigned reads;
static uint8 fake_read(uint32 at){++reads;return (at*37+19)&255;}
void nes_port_log(const char *fmt,...){(void)fmt;}
static void check_dma(unsigned page,unsigned destination,int handler) {
    nes_t *n=nes_getptr();uint8 expected[256];
    for(unsigned i=0;i<256;++i)expected[i]=handler?((page*256+i)*37+19)&255:mem_getbyte(page*256+i);
    reads=0;memset(n->ppu->oam,0xcc,256);ppu_write(PPU_OAMADDR,destination);
    nes6502_t before,after;nes6502_getcontext(&before);
    ppu_write(PPU_OAMDMA,page);nes6502_getcontext(&after);
    assert(after.burn_cycles-before.burn_cycles==513);
    assert(n->ppu->oam_addr==destination);
    for(unsigned i=0;i<256;++i)assert(n->ppu->oam[(destination+i)&255]==expected[i]);
    if(handler)assert(reads==256);
}
static void check_read(unsigned address,uint8 expected) {
    mem_t *m=nes_getptr()->mem;
    m->ram[0x20]=0xad;m->ram[0x21]=address;m->ram[0x22]=address>>8;
    m->ram[0x23]=0x85;m->ram[0x24]=0x30;m->ram[0x30]=expected^255;
    nes6502_t state;nes6502_getcontext(&state);state.pc_reg=0x20;state.burn_cycles=0;state.int_pending=false;state.jammed=false;
    /* Nofrendo's public setter is a stub; the initialized console owns the
     * actual context returned by nes6502_init. */
    *nes_getptr()->cpu=state;reads=0;assert(nes6502_execute(7)==7);
    if(m->ram[0x30]!=expected){nes6502_getcontext(&state);fprintf(stderr,"read address=%04x expected=%02x actual=%02x pc=%04x A=%02x reads=%u\n",address,expected,m->ram[0x30],state.pc_reg,state.a_reg,reads);abort();}
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);
    uint8_t *rom=malloc(size);assert(rom && fread(rom,1,size,f)==size);fclose(f);assert(!nes_core_load(rom,size));
    nes_t *n=nes_getptr();mem_t *m=n->mem;
    for(unsigned i=0;i<2048;++i)m->ram[i]=(i*17+(i>>8)*9)&255;
    const unsigned words[]={0x20,0x21,0x7fe,0x7ff,0x8000,0x8001,0x87fe,0x87ff,0xfffe};
    for(unsigned i=0;i<sizeof(words)/sizeof(*words);++i) {
        unsigned a=words[i];uint32 expected=mem_getbyte(a)|(mem_getbyte(a+1)<<8);
        assert(mem_getword(a)==expected);
    }
    const unsigned destinations[]={0,1,17,255};
    for(unsigned page=0;page<32;++page)for(unsigned d=0;d<4;++d)check_dma(page,destinations[d],0);
    for(unsigned d=0;d<4;++d)check_dma(0x80,destinations[d],0);
    mem_read_handler_t handlers[MEM_HANDLERS_MAX];memcpy(handlers,m->read_handlers,sizeof(handlers));
    uint32 flags[MEM_PAGECOUNT];memcpy(flags,m->flags,sizeof(flags));
    m->read_handlers[0]=(mem_read_handler_t){0x4000,0xffff,fake_read};m->read_handlers[1].handler=NULL;
    for(unsigned page=0x4000>>MEM_PAGESHIFT;page<MEM_PAGECOUNT;++page)m->flags[page]|=MEM_PAGE_HAS_READ_HANDLER;
    for(unsigned d=0;d<4;++d)check_dma(0x40,destinations[d],1);
    m->read_handlers[0].min_range=0;
    m->flags[0]|=MEM_PAGE_HAS_READ_HANDLER;
    reads=0;uint32 expected=((0x700*37+19)&255)|(((0x701*37+19)&255)<<8);
    assert(mem_getword(0x700)==expected && reads==2);
    reads=0;expected=((0x7ff*37+19)&255)|(m->ram[0]<<8);
    assert(mem_getword(0x7ff)==expected && reads==1);
    reads=0;expected=((0x87ff*37+19)&255)|(((0x8800*37+19)&255)<<8);
    assert(mem_getword(0x87ff)==expected && reads==2);
    for(unsigned d=0;d<4;++d)check_dma(0,destinations[d],1);
    check_read(0x8000,fake_read(0x8000));assert(reads==1);
    check_read(0x6000,fake_read(0x6000));assert(reads==1);
    memcpy(m->read_handlers,handlers,sizeof(handlers));memcpy(m->flags,flags,sizeof(flags));
    uint8 alternate[MEM_PAGESIZE];
    for(unsigned i=0;i<sizeof(alternate);++i)alternate[i]=(i*41+29)&255;
    mem_setpage(0,alternate);
    for(unsigned d=0;d<4;++d)check_dma(0,destinations[d],0);
    mem_setpage(0,m->ram);
    check_read(0x8000,mem_getbyte(0x8000));
    m->ram[0x700]=0x71;check_read(0x0f00,0x71);
    m->flags[0x5000>>MEM_PAGESHIFT]=0;check_read(0x5000,255);
    memcpy(m->flags,flags,sizeof(flags));nes_core_close();free(rom);
    puts("PASS: 144 DMA cases, RAM mirrors/remapping, OAM wrapping, 513-cycle stalls, RAM/I/O read counts, word alignment/page splits and live ROM/SRAM handlers");
}
