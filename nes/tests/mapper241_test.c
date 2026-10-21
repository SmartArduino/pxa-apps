/* Synthetic banks exercise the mapper independently of a commercial ROM. */
#include "core.h"
#include "nes/nes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1);}} while(0)
void nes_port_log(const char *format, ...) {(void)format;}
int main(void) {
    size_t size=16+16*32768+8192;uint8_t *rom=calloc(1,size);CHECK(rom);
    memcpy(rom,"NES\032",4);rom[4]=32;rom[5]=1;rom[6]=0x12;rom[7]=0xf0;
    for(unsigned bank=0;bank<16;++bank)memset(rom+16+bank*32768,bank,32768);
    memset(rom+16+16*32768,0x5a,8192);
    CHECK(!nes_core_load(rom,size));CHECK(nes_core_mapper()==241);
    CHECK(mem_getbyte(0x8000)==0 && mem_getbyte(0xffff)==0);
    mem_putbyte(0x6123,0xa7);
    unsigned cases=0;
    for(unsigned a=0x8000;a<=0xffff;a+=0x1111) for(unsigned b=0;b<256;++b){
        mem_putbyte(a,b);
        for(unsigned page=0;page<8;++page)CHECK(mem_getbyte(0x8000+page*4096)==(b%16));
        CHECK(mem_getbyte(0xffff)==(b%16));CHECK(mem_getbyte(0x6123)==0xa7);
        for(unsigned page=0;page<8;++page)CHECK(ppu_getpage(page)[0]==0x5a);
        ++cases;
    }
    nes_reset(false);CHECK(mem_getbyte(0x8000)==0);
    nes_core_close();free(rom);printf("PASS mapper 241: %u bank writes, wrap, fixed CHR, SRAM and reset\n",cases);
}
