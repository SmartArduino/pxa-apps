/* Deterministic cartridge lifecycle, video and sound skipping regression. GPL-2.0-or-later. */
#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef EMU_TEST_BLUE
#define EMU_TEST_BLUE 0x001f
#endif
static uint8_t *read_rom(const char *path,size_t *size){FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);*size=ftell(f);rewind(f);uint8_t *p=calloc(1,EMU_ROM_CAPACITY(*size));assert(p && fread(p,1,*size,f)==*size);fclose(f);return p;}
int main(int argc,char **argv){
    assert(emu_port_crc32(0,"123456789",9)==0xcbf43926u);
    assert(emu_port_crc32(emu_port_crc32(0,"1234",4),"56789",5)==0xcbf43926u);
    assert(argc==2);size_t size;uint8_t *rom=read_rom(argv[1],&size);
    assert(emu_core_validate(NULL,0));assert(!emu_core_validate(rom,size));
    uint32_t video=0,pcm=0;uint64_t samples=0;
    for(unsigned mode=0;mode<3;++mode){
        if(mode){free(rom);rom=read_rom(argv[1],&size);}
        emu_core_set_indexed_output(mode==2);
        assert(!emu_core_load(rom,size));emu_core_set_audio_enabled(true);
        uint32_t sound=0;uint64_t count=0;
        for(unsigned frame=0;frame<180;++frame){
            emu_core_frame_render(frame>60?EMU_PAD_RIGHT|EMU_PAD_A:0,EMU_PAD_B,mode==0 || frame%3==0 || frame==179);
            size_t n;const int16_t *p=emu_core_audio(&n);assert(n<2048);count+=n;sound=emu_port_crc32(sound,p,n*4);
            assert(EMU_VIDEO_WIDTH && EMU_VIDEO_HEIGHT && EMU_VIDEO_WIDTH<=EMU_MAX_SOURCE_WIDTH && EMU_VIDEO_HEIGHT<=EMU_MAX_SOURCE_HEIGHT);
        }
        uint32_t screen=0;const uint16_t *pixels=emu_core_video();assert(pixels);
        for(unsigned y=0;y<EMU_VIDEO_HEIGHT;++y)screen=emu_port_crc32(screen,pixels+y*emu_core_info.stride,EMU_VIDEO_WIDTH*2u);
        if(!mode){video=screen;pcm=sound;samples=count;assert(samples>60000);assert(pixels[(EMU_VIDEO_HEIGHT/2)*emu_core_info.stride+EMU_VIDEO_WIDTH/2]==EMU_TEST_BLUE);}
        else {assert(screen==video);assert(sound==pcm);assert(count==samples);}
        unsigned index_stride;const uint16_t *colors;
        const uint8_t *indexed=emu_core_indexed_video(&index_stride,&colors);
        if(indexed)for(unsigned y=0;y<EMU_VIDEO_HEIGHT;++y)for(unsigned x=0;x<EMU_VIDEO_WIDTH;++x)
            assert(colors[indexed[y*index_stride+x]]==pixels[y*emu_core_info.stride+x]);
        emu_core_set_indexed_output(false);
        size_t save_size=emu_core_save_size();if(save_size){uint8_t *save=malloc(save_size);assert(save);assert(emu_core_save_export(save,save_size));assert(emu_core_save_import(save,save_size));assert(!emu_core_save_import(save,save_size-1));free(save);}
        emu_core_set_audio_enabled(false);emu_core_frame_render(0,0,false);size_t n;emu_core_audio(&n);assert(n==0);
        emu_core_close();emu_core_close();
    }
    free(rom);printf("PASS: %s reload, RGB565 blue, controller, full/skip PCM and SRAM; %llu sample pairs\n",EMU_SYSTEM_TAG,(unsigned long long)samples);
}
