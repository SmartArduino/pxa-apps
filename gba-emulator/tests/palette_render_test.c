/* GPL-2.0-or-later. Scanline output against a scalar 4bpp tile-map oracle. */
#include "core.h"
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned pixel(unsigned layer,unsigned x,unsigned y) {
    unsigned ctrl=read_ioreg(REG_BGxCNT(layer));
    x=(x+read_ioreg(REG_BGxHOFS(layer)))&255;
    y=(y+read_ioreg(REG_BGxVOFS(layer)))&255;
    unsigned offset=((ctrl>>8)&31)*2048+((y/8)*32+x/8)*2;
    unsigned tile=eswap16(address16(vram,offset));
    unsigned tx=x&7,ty=y&7;
    if(tile&0x400)tx=7-tx;
    if(tile&0x800)ty=7-ty;
    unsigned packed=vram[((ctrl>>2)&3)*16384+(tile&1023)*32+ty*4+tx/2];
    unsigned index=(packed>>((tx&1)*4))&15;
    return index?((tile>>12)*16+index):0;
}

int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    u8 *rom=malloc(32768);assert(rom&&fread(rom,1,32768,f)==32768);fclose(f);
    assert(!emu_core_load(rom,32768));skip_next_frame=0;
    memset(io_registers,0,sizeof(io_registers));memset(vram,0,sizeof(vram));
    /* Every palette, both flips, zero nibbles and all byte values. */
    for(unsigned layer=0;layer<2;++layer) {
        for(unsigned i=0;i<32*16;++i)vram[layer*16384+i]=(u8)(i*53+layer*17);
        for(unsigned i=0;i<1024;++i)
            address16(vram,0x8000+layer*2048+i*2)=eswap16((i%16)|((i%4)<<10)|((i%16)<<12));
        write_ioreg(REG_BGxCNT(layer),((16+layer)<<8)|(layer<<2)|layer);
    }
    for(unsigned i=0;i<256;++i)write_memory16(0x05000000+i*2,(i*137)&0x7fff);
    uint16_t *saved=gba_screen_pixels;
    uint16_t *screen=malloc(GBA_SCREEN_BUFFER_SIZE+4);assert(screen);
    gba_screen_pixels=screen;
    for(unsigned pass=0;pass<4;++pass)
    for(unsigned shift=0;shift<8;++shift)
    for(unsigned layers=1;layers<=3;++layers) {
        write_ioreg(REG_DISPCNT,layers<<8);
        write_ioreg(REG_BG0HOFS,shift);write_ioreg(REG_BG1HOFS,(shift+3)&7);
        /* Writes after the cache is populated must invalidate it, including
         * byte replication, paired writes, and the global backdrop color. */
        if(pass==1)write_memory8(0x05000000+(shift*32+6),shift*17);
        if(pass==2)write_memory32(0x05000000+shift*32,0x31a24716u+shift);
        if(pass==3)write_memory16(0x05000000,(shift*191)&0x7fff);
        for(unsigned y=0;y<16;++y) {
            write_ioreg(REG_VCOUNT,y);update_scanline();
            for(unsigned x=0;x<240;++x) {
                unsigned color=layers&1?pixel(0,x,y):0;
                if(!color&&(layers&2))color=pixel(1,x,y);
                uint16_t expected=convert_palette(eswap16(palette_ram[color]));
                if(screen[y*240+x]!=expected)
                    fprintf(stderr,"pass=%u shift=%u layers=%u x=%u y=%u got=%04x expected=%04x\n",pass,shift,layers,x,y,screen[y*240+x],expected);
                assert(screen[y*240+x]==expected);
            }
        }
    }
    gba_screen_pixels=saved;free(screen);emu_core_close();free(rom);
    puts("PASS: palette writes, backdrop, transparency, flips and odd-pixel scrolling");
}
