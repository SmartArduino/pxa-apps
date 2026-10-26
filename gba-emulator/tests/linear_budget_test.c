/* GPL-2.0-or-later. Run under a fixed seven-MiB WASI memory as well as native.
 * Include the frontend so its static data is part of the measured budget. */
#include "../../common/emulator/main.c"
#include <assert.h>

int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t size) {
    (void)route;(void)token;(void)data;(void)size;return 0;
}
int32_t pxa_io(uint64_t handle,uint32_t op,uint8_t *data,uint32_t size) {
    (void)handle;(void)op;(void)data;(void)size;return PXA_STATUS_WOULD_BLOCK;
}
static int read_cartridge(void *context,uint64_t offset,void *out,size_t size) {
    uint8_t *rom=context,*dest=out;
    while(size) {
        size_t at=offset%32768,n=32768-at;if(n>size)n=size;
        memcpy(dest,rom+at,n);offset+=n;dest+=n;size-=n;
    }
    return 0;
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    uint8_t *rom=malloc(32768);assert(rom && fread(rom,1,32768,f)==32768);fclose(f);
    /* Two 800x480 surfaces, a scaled menu, and room for library names. */
    void *display=calloc(1,800u*480u*2u*2u+64);
    void *menu=calloc(1,400u*240u*2u);
    void *names_reserve=calloc(1,128u*1024u);
    assert(display && menu && names_reserve);
    rom_source_t source={rom,read_cartridge,32u*1024u*1024u,0x12345678};
    assert(!emu_core_load_source(&source,EMU_ROM_CACHE_BUDGET));
    const rom_cache_t *cache=emu_core_rom_cache();
    assert((size_t)cache->count*cache->page_size==2u*1024u*1024u);
    size_t battery_bytes=emu_core_save_size(),state_bytes=emu_state_size();
    uint8_t *battery=malloc(battery_bytes),*state=malloc(state_bytes);
    assert(battery && state && emu_core_save_export(battery,battery_bytes));
    for(unsigned i=0;i<8;++i) {
        emu_core_frame_render(0,0,true);
        assert(!emu_state_save(state,state_bytes));
        /* Loading allocates another temporary rollback image at the peak. */
        assert(!emu_state_load(state,state_bytes));
    }
    assert(!cache->error);
    free(battery);free(state);emu_core_close();free(names_reserve);free(menu);free(display);free(rom);
    puts("PASS: 32-MiB paged ROM, 2-MiB cache, 800x480 buffers, battery and snapshot rollback fit");
}
