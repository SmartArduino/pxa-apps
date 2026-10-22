/* GPL-2.0-or-later. Replay after restore, including a newly allocated machine. */
#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int image_read(void *ctx,uint64_t offset,void *out,size_t bytes);
static const uint8_t *image;static size_t image_size;
static int image_read(void *ctx,uint64_t offset,void *out,size_t bytes){(void)ctx;if(offset>image_size || bytes>image_size-offset)return -1;memcpy(out,image+offset,bytes);return 0;}
static void advance(unsigned first,unsigned frames,uint32_t *video,uint32_t *sound) {
    *video=*sound=0;
    for(unsigned n=first;n<first+frames;++n) {
        gb_core_frame((n%7<3?GB_PAD_RIGHT:GB_PAD_LEFT)|(n%5==0?GB_PAD_A:0));
        *video=gb_port_crc32(*video,gb_core_video(),160*144*2);
        size_t count;const int16_t *pcm=gb_core_audio(&count);
        *sound=gb_port_crc32(*sound,pcm,count*4);
    }
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);fseek(file,0,SEEK_END);
    size_t size=ftell(file);rewind(file);uint8_t *rom=malloc(size);assert(rom&&fread(rom,1,size,file)==size);fclose(file);
    assert(!gb_core_load(rom,size));uint32_t a,b,c,d;advance(0,37,&a,&b);
    size_t bytes=gb_state_size();assert(bytes>65536);void *state=malloc(bytes),*before=malloc(bytes);assert(state&&before);
    assert(!gb_state_save(state,bytes));advance(37,29,&a,&b);
    assert(!gb_state_load(state,bytes));advance(37,29,&c,&d);assert(a==c&&b==d);
    gb_core_close();assert(!gb_core_load(rom,size));assert(!gb_state_load(state,bytes));
    advance(37,29,&c,&d);assert(a==c&&b==d);
    gb_core_close();image=rom;image_size=size;rom_source_t source={NULL,image_read,size,rom_crc32(0,rom,size)};
    assert(!gb_core_load_source(&source,4*16384));assert(!gb_state_load(state,bytes));advance(37,29,&c,&d);assert(a==c&&b==d);
    assert(!gb_state_save(before,bytes));((uint8_t*)state)[bytes-1]^=0x80;
    assert(gb_state_load(state,bytes));assert(!gb_state_save(state,bytes));assert(!memcmp(state,before,bytes));
    assert(gb_state_load(state,bytes-1));
    printf("PASS: %zu-byte GB state, RGB/PCM replay after fresh load, corrupt/truncated input leaves state intact\n",bytes);
    gb_core_close();free(rom);free(state);free(before);
}
