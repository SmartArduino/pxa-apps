/* Include the implementation to compare its private pixel kernels with the
 * retained scalar path and verify cache invalidation via real PPU writes. */
#include "../vendor/nofrendo/nes/ppu.c"
#include "core.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); exit(1); } } while (0)
void nes_port_log(const char *format, ...) { (void)format; }
static uint8 *replacement_nametable;
static unsigned latch_calls;
static void replace_nametable(uint32 address,uint8 value)
{
    (void)address;(void)value;++latch_calls;
    ppu_setpage(8,replacement_nametable);
}
static unsigned check_background_spans(void)
{
    uint8 chr[8192],tables[4096];
    for(unsigned i=0;i<sizeof(chr);++i)chr[i]=(i*29+(i>>5)*71)&255;
    for(unsigned i=0;i<sizeof(tables);++i)tables[i]=(i*13+(i>>7)*37)&255;
    for(unsigned i=0;i<8;++i)ppu_setpage(i,chr+i*1024);
    for(unsigned i=0;i<4;++i)ppu_setpage(8+i,tables+i*1024);
    ppu.bg_on=ppu.left_bg_on=true;ppu.latchfunc=NULL;ppu.bg_base=0x1000;
    unsigned cases=0;
    for(unsigned nt=0;nt<4;++nt)for(unsigned y=0;y<30;++y)
    for(unsigned x=0;x<32;++x)for(unsigned fine=0;fine<8;++fine){
        uint8 got[288],expected[288];memset(got,0xa5,sizeof(got));memset(expected,0xa5,sizeof(expected));
        ppu.vaddr=(nt<<10)|(y<<5)|x|((y&7)<<12);ppu.tile_xofs=fine;
        ppu_renderbg(got+8);
        for(unsigned t=0;t<33;++t){
            unsigned col=(x+t)&31,tab=nt^((x+t)>=32?1:0);
            const uint8 *page=tables+tab*1024;
            unsigned attr=page[960+(y/4)*8+col/4];
            unsigned pal=(attr>>((col&2)+((y&2)<<1)))&3;
            unsigned address=0x1000+(page[y*32+col]<<4)+(y&7);
            draw_bgtile(expected+8-fine+t*8,get_patpix(address),ppu.palette+pal*4);
        }
        CHECK(!memcmp(got,expected,sizeof(got)));++cases;
    }
    /* A latch callback can change the nametable used by subsequent tiles. */
    memset(tables,1,1024);memset(tables+1024,2,1024);
    memset(tables+960,0,64);memset(tables+1024+960,0,64);tables[0]=0xfd;
    replacement_nametable=tables+1024;latch_calls=0;ppu.latchfunc=replace_nametable;
    ppu.vaddr=0;ppu.tile_xofs=0;
    uint8 got[288]={0},expected[288]={0};ppu_renderbg(got+8);
    for(unsigned t=0;t<33;++t)
        draw_bgtile(expected+8+t*8,get_patpix(0x1000+((t?2:0xfd)<<4)),ppu.palette);
    CHECK(latch_calls==1);CHECK(!memcmp(got,expected,sizeof(got)));
    ppu.latchfunc=NULL;
    return cases+1;
}
static void check_rows(void)
{
    if (sprite_rows_dirty) prepare_sprite_rows();
    for (unsigned y = 0; y < 240; ++y)
        for (unsigned s = 0; s < 64; ++s)
        {
            unsigned first = (unsigned)ppu.oam[s * 4] + 1;
            bool expected = y >= first && y < first + (unsigned)ppu.obj_height;
            CHECK(!!(sprite_rows[y][s >> 5] & (UINT32_C(1) << (s & 31))) == expected);
        }
}
int main(void)
{
    uint8 chr[8192] = {0};
    memset(&ppu, 0, sizeof(ppu));
    for (unsigned page = 0; page < 8; ++page) ppu_setpage(page, chr + page * 1024);
    uint8 got[32] __attribute__((aligned(8))), expected[32];
    /* Four nametables, all horizontal crossings, coarse/fine scroll and
       attribute quadrants; expected addressing is independent of the loop. */
    for(unsigned i=0;i<32;++i)ppu.palette[i]=i*7;
    unsigned spans=check_background_spans();
    for (unsigned page = 0; page < 8; ++page) ppu_setpage(page, chr + page * 1024);
    unsigned cases = 0;
    for (unsigned revision = 0; revision < 3; ++revision)
    {
        for (unsigned i = 0; i < 32; ++i)
            ppu.palette[i] = (i * 17 + revision * 29) & 255;
        prepare_bg_quads();
        for (unsigned pal = 0; pal < 4; ++pal)
            for (unsigned pattern = 0; pattern < 65536; ++pattern)
            {
                chr[0] = pattern; chr[8] = pattern >> 8;
                for (unsigned align = 0; align < 8; ++align)
                {
                    memset(got, 0xa5, sizeof(got)); memset(expected, 0xa5, sizeof(expected));
                    draw_bgrow(got + 8 + align, 0, pal);
                    draw_bgtile(expected + 8 + align, get_patpix(0), ppu.palette + pal * 4);
                    CHECK(!memcmp(got, expected, sizeof(got))); ++cases;
                }
            }
    }
    /* Same tile number immediately observes CHR writes and bank replacement. */
    uint8 other[1024] = {0}; other[0] = 0xff;
    ppu_setpage(0, other); draw_bgrow(got, 0, 0);
    draw_bgtile(expected, get_patpix(0), ppu.palette);
    CHECK(!memcmp(got, expected, 8));
    for (unsigned s = 0; s < 64; ++s)
    {
        ppu_write(PPU_OAMADDR, s * 4);
        ppu_write(PPU_OAMDATA, s * 4);
    }
    ppu_write(PPU_CTRL0, 0); check_rows();
    ppu_write(PPU_CTRL0, PPU_CTRL0F_OBJ16); CHECK(sprite_rows_dirty); check_rows();
    ppu_write(PPU_OAMADDR, 0); ppu_write(PPU_OAMDATA, 239);
    CHECK(sprite_rows_dirty); check_rows();
    ppu_write(PPU_OAMADDR, 252); ppu_write(PPU_OAMDATA, 255);
    CHECK(sprite_rows_dirty); check_rows();
    ppu.obj_on = true; ppu.strikeflag = true;
    ppu_write(PPU_OAMADDR, 0); ppu_write(PPU_OAMDATA, 240);
    ppu_renderoam(got + 8, 103, true);
    CHECK(sprite_rows_dirty); /* Mid-frame writes use bounded scalar work. */
    ppu_renderoam(got + 8, 0, true);
    CHECK(!sprite_rows_dirty); check_rows();
    printf("PASS %u pixel rows, %u background spans; CHR bank changes, palette changes and OAM/height invalidation\n", cases,spans);
    return 0;
}
