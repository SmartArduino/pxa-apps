#include "core.h"
#include "state_snapshot.h"
#include "nes/nes.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void nes_port_log(const char *format,...) {(void)format;}
static uint32_t trace(unsigned count) {
    uint32_t crc=0;
    for(unsigned i=0;i<count;++i) {
        nes_core_frame(i%3==0,0x80|((i%53)<11?1:0),0);
        nes_t *m=nes_getptr();nes6502_t *c=m->cpu;
        uint32_t regs[]={c->pc_reg,c->a_reg,c->x_reg,c->y_reg,c->s_reg,c->p_reg,c->total_cycles,c->burn_cycles,c->int_pending};
        crc=nes_port_crc32(crc,regs,sizeof(regs));crc=nes_port_crc32(crc,m->mem->ram,2048);
        crc=nes_port_crc32(crc,m->ppu->nametab,4096);crc=nes_port_crc32(crc,m->cart->prg_ram,m->cart->prg_ram_banks*8192u);
        crc=nes_port_crc32(crc,nes_core_video()-8,NES_SCREEN_PITCH*240);
        size_t n;const int16_t *p=nes_core_audio(&n);crc=nes_port_crc32(crc,p,n*2);
    }
    return crc;
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
    uint8_t *rom=malloc(n);assert(rom && fread(rom,1,n,f)==n);fclose(f);assert(!nes_core_load(rom,n));
    for(unsigned i=0;i<1900;++i)nes_core_frame(i%3==0,(i==600 || i==1200)?8:(i>900?0x81:0),0);
    size_t size=nes_state_size();uint8_t *state=malloc(size),*check=malloc(size);assert(state && check);
    const char *err=nes_state_save(state,size);if(err){fprintf(stderr,"SAVE: %s\n",err);return 2;}
    uint32_t expected=trace(180);err=nes_state_load(state,size);if(err){fprintf(stderr,"LOAD: %s\n",err);return 3;}
    uint32_t actual=trace(180);if(actual!=expected){fprintf(stderr,"REPLAY %08x != %08x\n",expected,actual);return 4;}
    /* Truncated/corrupt files cannot mutate the current state. */
    assert(!nes_state_save(check,size));state[size-1]^=1;assert(nes_state_load(state,size));
    uint8_t *after=malloc(size);assert(!nes_state_save(after,size));assert(!memcmp(after,check,size));state[size-1]^=1;
    assert(nes_state_load(state,size-1));assert(!nes_state_save(after,size));assert(!memcmp(after,check,size));
    /* Reopen/reallocation: no serialized pointers may be reused. */
    nes_core_close();f=fopen(argv[1],"rb");assert(f && fread(rom,1,n,f)==n);fclose(f);assert(!nes_core_load(rom,n));err=nes_state_load(state,size);if(err){fprintf(stderr,"REOPEN LOAD: %s\n",err);return 6;}
    actual=trace(180);if(actual!=expected){fprintf(stderr,"REOPEN %08x != %08x\n",expected,actual);return 5;}
    printf("PASS mapper=%u bytes=%zu replay=%08x, atomic invalid-file rejection and reopen\n",nes_core_mapper(),size,expected);
    free(after);free(check);free(state);nes_core_close();free(rom);
}
