/* GPL-2.0-or-later. Differential idle-loop cycle/interrupt/bus regression. */
#include "core.h"
#include "nes/nes.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

/* Exercise the guarded path directly, including in the reference build. */
#define nes6502_execute nes6502_execute_idle

void nes_port_log(const char *format, ...) { (void)format; }
static unsigned reads;
static uint8 handler(uint32 address) {
    ++reads;
    return nes_getptr()->mem->pages[address >> MEM_PAGESHIFT][address];
}
static void byte(unsigned address, uint8 value) {
    nes_getptr()->mem->pages[address >> MEM_PAGESHIFT][address] = value;
}
static void jump(unsigned pc, unsigned destination) {
    byte(pc, 0x4c); byte(pc+1, destination); byte(pc+2, destination >> 8);
}
static uint32 signature;
static unsigned cases;
static void record(void) {
    nes6502_t *c=nes_getptr()->cpu;
    const uint32 state[]={c->pc_reg,c->a_reg,c->x_reg,c->y_reg,c->s_reg,
        c->p_reg,c->total_cycles,c->burn_cycles,c->int_pending,reads};
    signature=nes_port_crc32(signature,state,sizeof(state));
    signature=nes_port_crc32(signature,c->stack,256);
    ++cases;
}
static void reset(unsigned pc, unsigned flags, unsigned burn) {
    nes6502_t *c=nes_getptr()->cpu;
    c->pc_reg=pc;c->a_reg=0x91;c->x_reg=0xab;c->y_reg=0x37;c->s_reg=0xfb;
    c->p_reg=flags;c->total_cycles=123;c->burn_cycles=burn;
    c->int_pending=false;c->jammed=false;reads=0;
    memset(c->stack,0xa5,256);
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);
    uint8_t *rom=malloc(size);assert(rom && fread(rom,1,size,f)==size);fclose(f);
    assert(!nes_core_load(rom,size));
    mem_t *m=nes_getptr()->mem;nes6502_t *c=nes_getptr()->cpu;
    /* Independent backing avoids mirrored PRG banks in the synthetic cases. */
    uint8_t backing[32768];memset(backing,0xea,sizeof(backing));
    for(unsigned a=0x8000;a<0x10000;a+=MEM_PAGESIZE)mem_setpage(a>>MEM_PAGESHIFT,backing+a-0x8000);
    const unsigned pcs[]={0x20,0x7fd,0x7fe,0x7ff,0x1ffd,0x8000,0x87fd,0x87fe,0x87ff,0xfffd};
    const unsigned burns[]={0,1,7,513};
    for(unsigned p=0;p<sizeof(pcs)/sizeof(*pcs);++p) {
        jump(pcs[p],pcs[p]);
        for(unsigned flags=0;flags<256;flags+=17)
        for(unsigned b=0;b<sizeof(burns)/sizeof(*burns);++b)
        for(int budget=-2;budget<=256;++budget) {
            reset(pcs[p],flags,burns[b]);
            int burn=budget>0 ? MIN((unsigned)budget,burns[b]) : 0;
            int left=budget-burn,loops=left>0?(left+2)/3:0;
            int actual=nes6502_execute(budget);
            assert(actual==burn+loops*3 && c->pc_reg==pcs[p]);
            assert(c->total_cycles==123u+actual && c->burn_cycles==burns[b]-burn);
            assert(c->a_reg==0x91 && c->x_reg==0xab && c->y_reg==0x37 && c->s_reg==0xfb);
            assert(c->p_reg==(flags|R_FLAG));record();
        }
    }
    /* Handler-covered operands, including a jump split across map pages,
       must perform both reads for every instruction, even for a self jump. */
    for(unsigned pc=0x87fd;pc<=0x87ff;++pc) {
        jump(pc,pc);
        for(unsigned which=0;which<2;++which) {
            unsigned page=(pc+which+1)>>MEM_PAGESHIFT;
            m->flags[page]|=MEM_PAGE_HAS_READ_HANDLER;
            m->read_handlers[0]=(mem_read_handler_t){page<<MEM_PAGESHIFT,((page+1)<<MEM_PAGESHIFT)-1,handler};
            m->read_handlers[1].handler=NULL;
            for(unsigned budget=1;budget<=256;++budget) {
                reset(pc,R_FLAG,0);unsigned loops=(budget+2)/3;
                assert(nes6502_execute(budget)==(int)loops*3);
                unsigned covered=((pc+1)>>MEM_PAGESHIFT==page)+((pc+2)>>MEM_PAGESHIFT==page);
                assert(reads==loops*covered && c->pc_reg==pc);record();
            }
            m->flags[page]&=~MEM_PAGE_HAS_READ_HANDLER;
        }
    }
    m->read_handlers[0].handler=NULL;
    /* Edits and bank switches between execute calls invalidate naturally. */
    jump(0x8000,0x8000);jump(0x9000,0x9000);reset(0x8000,R_FLAG,0);
    assert(nes6502_execute(74)==75);record();
    jump(0x8000,0x9000);assert(nes6502_execute(40)==42 && c->pc_reg==0x9000);record();
    jump(0x8000,0x8000);reset(0x8000,R_FLAG,0);
    assert(nes6502_execute(74)==75);
    uint8_t bank[MEM_PAGESIZE];memset(bank,0xea,sizeof(bank));bank[0]=0x4c;bank[1]=0;bank[2]=0x90;
    mem_setpage(0x8000>>MEM_PAGESHIFT,bank);
    assert(nes6502_execute(40)==42 && c->pc_reg==0x9000);record();
    mem_setpage(0x8000>>MEM_PAGESHIFT,backing);
    /* Interrupt after an idle chunk must push the original PC/flags and
       execute the handler at the next chunk, preserving interrupt burn. */
    byte(NMI_VECTOR,0);byte(NMI_VECTOR+1,0x90);byte(IRQ_VECTOR,0);byte(IRQ_VECTOR+1,0x90);
    for(unsigned irq=0;irq<3;++irq)for(unsigned budget=1;budget<=113;++budget) {
        reset(0x8000,R_FLAG,0);nes6502_execute(budget);record();
        if(irq==0)nes6502_nmi();else if(irq==1)nes6502_irq();else c->int_pending=true;
        nes6502_execute(74);assert(c->pc_reg==0x9000);record();
    }
    /* Unmapped current/historical hints must not even peek at the backing
       pointer. Poison it to expose reads performed before checking flags. */
    nes6502_reset();reset(0x8000,R_FLAG,0);
    m->flags[0x8000>>MEM_PAGESHIFT]&=~MEM_PAGE_HAS_MEMORY;
    m->pages[0x8000>>MEM_PAGESHIFT]=NULL;
    assert(!nes6502_idle_hint());record();
    mem_setpage(0x8000>>MEM_PAGESHIFT,backing);
    jump(0x8000,0x8000);reset(0x8000,R_FLAG,0);nes6502_nmi();
    reset(0x9000,R_FLAG,0);byte(0x9000,0xea);
    m->flags[0x8000>>MEM_PAGESHIFT]&=~MEM_PAGE_HAS_MEMORY;
    m->pages[0x8000>>MEM_PAGESHIFT]=NULL;
    assert(!nes6502_idle_hint());record();
    mem_setpage(0x8000>>MEM_PAGESHIFT,backing);
    /* Differential zero-page polling: both instruction entry points, taken
       and untaken branches, register/flag values, DMA and page crossings. */
    const unsigned poll_pcs[]={0x40,0x8000,0x80fd,0x87fd,0x87fe,0x87ff,0xfff0};
    const unsigned loads[]={0xa5,0xa6,0xa4},values[]={0,1,0x80,0xff};
    const unsigned flags[]={0,2,0x80,0x82,0xff};
    for(unsigned p=0;p<sizeof(poll_pcs)/sizeof(*poll_pcs);++p)
    for(unsigned load=0;load<3;++load)for(unsigned equal=0;equal<2;++equal){
        unsigned pc=poll_pcs[p];byte(pc,loads[load]);byte(pc+1,0x20);
        byte(pc+2,equal?0xf0:0xd0);byte(pc+3,0xfc);jump(pc+4,pc+4);
        for(unsigned v=0;v<4;++v)for(unsigned f=0;f<5;++f)
        for(unsigned entry=0;entry<2;++entry)for(unsigned b=0;b<4;++b)
        for(int budget=-1;budget<=128;++budget){
            byte(0x20,values[v]);reset(pc+entry*2,flags[f],burns[b]);
            int consumed=nes6502_execute(budget);
            assert(c->total_cycles==123+consumed);record();
        }
    }
    byte(0x8000,0xa5);byte(0x8001,0x20);byte(0x8002,0xd0);byte(0x8003,0xfc);jump(0x8004,0x8004);
    jump(0x9000,0x9000);byte(NMI_VECTOR,0);byte(NMI_VECTOR+1,0x90);byte(IRQ_VECTOR,0);byte(IRQ_VECTOR+1,0x90);
    for(unsigned irq=0;irq<3;++irq)for(unsigned budget=1;budget<=113;++budget){
        byte(0x20,1);reset(0x8000,R_FLAG,0);nes6502_execute(budget);record();
        if(irq==0)nes6502_nmi();else if(irq==1)nes6502_irq();else c->int_pending=true;
        nes6502_execute(74);assert(c->pc_reg==0x9000);record();
    }
    /* A changed RAM flag between slices must end the wait. */
    byte(0x20,1);reset(0x8000,R_FLAG,0);nes6502_execute(113);record();
    byte(0x20,0);nes6502_execute(113);assert(c->pc_reg==0x8004);record();
    /* A read handler on the code or RAM page forces ordinary execution. */
    for(unsigned page_index=0;page_index<2;++page_index){
        unsigned page=page_index?0x8000>>MEM_PAGESHIFT:0;
        m->flags[page]|=MEM_PAGE_HAS_READ_HANDLER;
        m->read_handlers[0]=(mem_read_handler_t){page<<MEM_PAGESHIFT,((page+1)<<MEM_PAGESHIFT)-1,handler};
        for(unsigned budget=1;budget<=128;++budget){
            byte(0x20,1);reset(0x8000,R_FLAG,0);nes6502_execute(budget);record();
        }
        m->flags[page]&=~MEM_PAGE_HAS_READ_HANDLER;
    }
    m->read_handlers[0].handler=NULL;
    /* The interpreter uses its dedicated zero-page pointer, not pages[0]. */
    byte(0x8000,0xa5);byte(0x8001,0x20);byte(0x8002,0xd0);byte(0x8003,0xfc);
    c->zp[0x20]=0x80;
    uint8_t other_zp[MEM_PAGESIZE]={0};uint8_t *old_page0=m->pages[0];
    mem_setpage(0,other_zp);reset(0x8000,R_FLAG,0);
    assert(nes6502_execute(61)==63 && c->a_reg==0x80 && c->pc_reg==0x8002);record();
    mem_setpage(0,old_page0);
    printf("PASS cases=%u signature=%08x: self-JMP and zero-page load/branch waits, partial iterations, flags, DMA, page splits, read handlers, edits, bank switches, IRQ/NMI, unmapped guards, dedicated zero-page mapping\n",cases,signature);
    nes_core_close();free(rom);
}
