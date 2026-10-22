#include "core.h"
#include "vendor/gnuboy/hw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {uint8_t *data;size_t size;unsigned reads;int fail;} image_t;
static int read_image(void *ctx,uint64_t off,void *out,size_t n){
    image_t *i=ctx;++i->reads;if(i->fail || off>i->size || n>i->size-off)return -1;
    memcpy(out,i->data+off,n);return 0;
}
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    image_t image={.size=8*1024*1024};image.data=malloc(image.size);assert(image.data);
    for(unsigned i=0;i<512;++i)memset(image.data+i*16384,i,16384);
    assert(fread(image.data,1,32768,f)==32768);fclose(f);
    image.data[0x147]=0x1b;image.data[0x148]=8;image.data[0x149]=3;
    rom_source_t source={&image,read_image,image.size,rom_crc32(0,image.data,image.size)};
    assert(!gb_core_load(image.data,image.size));
    for(unsigned i=0;i<60;++i)gb_core_frame_render(0,true);
    uint32_t expected=rom_crc32(0,gb_core_video(),160*144*2);
    gb_core_close();assert(!gb_core_load_source(&source,4*16384));
    for(unsigned i=0;i<60;++i)gb_core_frame_render(0,true);
    assert(expected==rom_crc32(0,gb_core_video(),160*144*2));
    for(unsigned pass=0;pass<3;++pass)for(unsigned bank=2;bank<512;++bank){
        gb_hw_write(0x2000,bank&255);gb_hw_write(0x3000,bank>>8);
        assert(gb_hw_read(0x4000+123)==(uint8_t)bank);
        assert(GB.rmap[4][123]==(uint8_t)bank);
        assert(GB.rmap[0][0x147]==0x1b);
    }
    const rom_cache_t *c=gb_core_rom_cache();assert(c->count==16 && c->evictions>1000 && !c->error);
    gb_hw_write(0x2000,1);gb_hw_write(0x3000,0);
    image.fail=1;gb_hw_write(0x2000,42);gb_hw_read(0x4000);assert(c->error);
    assert(GB.rmap[4][100]==255);gb_core_close();free(image.data);
    puts("GB 8 MiB ROM / 64 KiB cache: frame parity, 1530 bank switches, pinning and I/O failure passed");
}
