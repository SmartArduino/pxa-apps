/* GPL-2.0-or-later. Machine replay, lifecycle rebinding and invalid-file atomicity. */
#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *image;
static size_t image_size;
static int read_rom(void *context,uint64_t offset,void *out,size_t bytes) {
    (void)context;if(offset>image_size || bytes>image_size-offset)return -1;
    memcpy(out,image+(size_t)offset,bytes);return 0;
}
static void advance(unsigned first,unsigned frames,uint32_t *video,uint32_t *sound) {
    *video=*sound=0;
    for(unsigned n=first;n<first+frames;++n) {
        emu_core_frame_render((n%7<3?EMU_PAD_RIGHT:EMU_PAD_LEFT)|(n%5==0?EMU_PAD_A:0),0,true);
        for(unsigned y=0;y<EMU_VIDEO_HEIGHT;++y)
            *video=emu_port_crc32(*video,emu_core_video()+y*emu_core_info.stride,EMU_VIDEO_WIDTH*2);
        size_t count;const int16_t *pcm=emu_core_audio(&count);
        *sound=emu_port_crc32(*sound,pcm,count*4);
    }
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);fseek(file,0,SEEK_END);
    image_size=ftell(file);rewind(file);image=malloc(image_size);assert(image&&fread(image,1,image_size,file)==image_size);fclose(file);
    uint8_t *rom=calloc(1,EMU_ROM_CAPACITY(image_size));assert(rom);memcpy(rom,image,image_size);
    assert(!emu_core_load(rom,image_size));uint32_t a,b,c,d;advance(0,37,&a,&b);
    size_t bytes=emu_state_size();assert(bytes>1000);void *state=malloc(bytes),*before=malloc(bytes),*future=malloc(bytes);assert(state&&before&&future);
    assert(!emu_state_save(state,bytes));advance(37,29,&a,&b);assert(!emu_state_save(future,bytes));
    assert(!emu_state_load(state,bytes));advance(37,29,&c,&d);if(a!=c || b!=d)fprintf(stderr,"replay mismatch video=%08x/%08x PCM=%08x/%08x\n",a,c,b,d);assert(a==c&&b==d);
    assert(!emu_state_save(before,bytes));
    if(memcmp(before,future,bytes)) {
        unsigned shown=0;
        for(size_t i=32;i<bytes && shown<24;++i)if(((uint8_t*)before)[i]!=((uint8_t*)future)[i]) {
            fprintf(stderr,"snapshot diff offset=%zu restored=%02x expected=%02x\n",i,((uint8_t*)before)[i],((uint8_t*)future)[i]);++shown;
        }
        const char *directory=getenv("PXA_STATE_DIFF_DIR");
        if(directory)for(unsigned n=0;n<2;++n) {
            char path[1024];snprintf(path,sizeof(path),"%s/%s.bin",directory,n?"expected":"restored");
            FILE *f=fopen(path,"wb");assert(f);assert(fwrite(n?future:before,1,bytes,f)==bytes);fclose(f);
        }
    }
    assert(!memcmp(before,future,bytes));
    emu_core_close();memcpy(rom,image,image_size);assert(!emu_core_load(rom,image_size));assert(!emu_state_load(state,bytes));
    advance(37,29,&c,&d);if(a!=c || b!=d)fprintf(stderr,"replay mismatch video=%08x/%08x PCM=%08x/%08x\n",a,c,b,d);assert(a==c&&b==d);
    emu_core_close();rom_source_t source={NULL,read_rom,image_size,emu_port_crc32(0,image,image_size)};
    assert(!emu_core_load_source(&source,EMU_ROM_CACHE_BUDGET));assert(!emu_state_load(state,bytes));
    advance(37,29,&c,&d);assert(a==c&&b==d && !emu_core_rom_cache()->error);
    assert(!emu_state_save(before,bytes));((uint8_t*)state)[bytes-1]^=0x80;
    assert(emu_state_load(state,bytes));assert(!emu_state_save(state,bytes));assert(!memcmp(state,before,bytes));
    assert(emu_state_load(state,bytes-1));
    printf("PASS: %s %zu-byte complete state, exact machine/RGB/PCM replay, fresh/paged ROM, atomic corrupt/truncated rejection\n",EMU_SYSTEM_TAG,bytes);
    emu_core_close();free(rom);free(image);free(state);free(before);free(future);
}
