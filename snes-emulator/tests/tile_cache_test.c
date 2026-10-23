/* Transparent tiles must remain cached, but a different bit depth and VRAM
 * writes must invalidate that result. Cache contents act as a decode sentinel. */
#include "core.h"
#include "vendor/snes9x/snes9x.h"
#include "vendor/snes9x/src/tile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void DrawTile16(uint32_t tile,int32_t offset,uint32_t start_line,uint32_t line_count);

static void mode(unsigned depth) {
    BG.Depth=depth; BG.BitShift=2u<<depth; BG.TileShift=4u+depth;
    BG.TileAddress=0; BG.NameSelect=0; BG.DirectColourMode=0;
    BG.Buffer=IPPU.TileCache; BG.Buffered=IPPU.TileCached;
    BG.StartPalette=0; BG.PaletteMask=0; BG.PaletteShift=0;
    GFX.S=GFX.Screen; GFX.DB=GFX.ZBuffer; GFX.PPL=256;
    GFX.Z1=1; GFX.Z2=1;
    memset(GFX.ZBuffer,0,256*8);
    for(unsigned i=0;i<256*8;++i) ((uint16_t*)GFX.Screen)[i]=0x1234;
}
static void draw(void) { DrawTile16(0,0,0,8); }
int main(int argc,char **argv) {
    assert(argc==2); FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));size_t n=ftell(f);rewind(f);
    uint8_t *rom=calloc(1,EMU_ROM_CAPACITY(n));assert(rom);
    assert(fread(rom,1,n,f)==n);fclose(f);assert(!emu_core_load(rom,n));
    for(unsigned depth=0;depth<3;++depth) {
        memset(Memory.VRAM,0,64);memset(IPPU.TileCached,0,MAX_2BIT_TILES);
        mode(depth);draw();assert(IPPU.TileCached[0]&TILE_CACHE_BLANK);
        IPPU.TileCache[0]=0xa5;draw();
        assert(IPPU.TileCache[0]==0xa5); // No redundant decode of the cached blank.
        assert(((uint16_t*)GFX.Screen)[0]==0x1234);
        PPU.VMA.FullGraphicCount=0;PPU.VMA.Address=0;PPU.VMA.High=0;PPU.VMA.Increment=1;
        REGISTER_2118(0x80);assert(IPPU.TileCached[0]==0);IPPU.ScreenColors[1]=0xf800;
        draw();assert(!(IPPU.TileCached[0]&TILE_CACHE_BLANK));
        assert(((uint16_t*)GFX.Screen)[0]==0xf800);
        assert(((uint16_t*)GFX.Screen)[1]==0x1234);
    }
    for(unsigned low=0;low<2;++low) {
        memset(Memory.VRAM,0,64);memset(IPPU.TileCached,0,MAX_2BIT_TILES);
        mode(low);draw();assert(IPPU.TileCached[0]&TILE_CACHE_BLANK);
        Memory.VRAM[low?32:16]=0x80;
        unsigned color=low?16:4;IPPU.ScreenColors[color]=0x07e0;
        mode(low+1);draw();assert(((uint16_t*)GFX.Screen)[0]==0x07e0);
        mode(low);draw();assert(IPPU.TileCached[0]&TILE_CACHE_BLANK);
        assert(((uint16_t*)GFX.Screen)[0]==0x1234);
    }
    emu_core_close();free(rom);puts("transparent tile cache/depth invalidation passed");
}
