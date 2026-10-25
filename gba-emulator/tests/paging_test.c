/* Cartridge larger than the cache, aliases and sticky I/O failure. */
#include "core.h"
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {uint8_t *data;size_t size;bool fail;} source_t;
static int read_at(void *ctx,uint64_t offset,void *out,size_t size) {
    source_t *s=ctx;
    if(s->fail || offset>s->size || size>s->size-offset)return -1;
    memcpy(out,s->data+(size_t)offset,size);return 0;
}
int main(int argc,char **argv) {
    assert(argc==2);source_t image={.size=8*1024*1024};image.data=malloc(image.size);assert(image.data);
    for(unsigned i=0;i<image.size/32768;++i)memset(image.data+i*32768,(uint8_t)i,32768);
    FILE *f=fopen(argv[1],"rb");assert(f);size_t n=fread(image.data,1,32768,f);assert(n==32768);fclose(f);
    rom_source_t source={&image,read_at,image.size,rom_crc32(0,image.data,image.size)};
    assert(emu_core_load_source(&source,512*1024));
    assert(!emu_core_load_source(&source,1024*1024));
    assert(emu_core_rom_cache()->count==32);
    assert(!emu_core_load_source(&source,EMU_ROM_CACHE_BUDGET));
    for(unsigned pass=0;pass<3;++pass)for(unsigned i=1;i<image.size/32768;++i) {
        unsigned address=0x08000000+i*32768+256;
        unsigned expected=(uint8_t)i*0x01010101u;
        assert(read_memory32(address)==expected);
        assert(read_memory32(address+0x02000000)==expected);
    }
    assert(emu_core_rom_cache()->count*emu_core_rom_cache()->page_size==EMU_ROM_CACHE_BUDGET);
    assert(emu_core_rom_cache()->bytes>image.size && !emu_core_rom_cache()->error);
    image.fail=true;(void)read_memory32(0x08000200);assert(emu_core_rom_cache()->error);
    emu_core_close();image.fail=false;
    /* One-MiB cartridges use the same bounded cache for four-MiB mirrors. */
    source.size=image.size=1024*1024;
    assert(!emu_core_load_source(&source,EMU_ROM_CACHE_BUDGET));
    assert(emu_core_rom_cache()->count==32);
    for(unsigned i=0;i<128;++i) {
        unsigned offset=(i%32)*32768+256,word;
        memcpy(&word,image.data+offset,4);
        assert(read_memory32(0x08000000+i*32768+256)==word);
    }
    emu_core_close();free(image.data);
    puts("PASS: 8 MiB ROM with bounded 1/2 MiB caches, mirror windows, eviction, Mini ROM and read failure");
}
