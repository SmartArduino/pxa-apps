#include "core.h"
#include "state_snapshot.h"
#include "nes/nes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void nes_port_log(const char *format,...){(void)format;}
typedef struct {uint8_t *data;size_t size;int fail;} image_t;
static int read_image(void *ctx,uint64_t off,void *out,size_t n){
    image_t *i=ctx;if(i->fail || off>i->size || n>i->size-off)return -1;memcpy(out,i->data+off,n);return 0;
}
static uint32_t run(void){
    uint32_t crc=0;
    for(unsigned i=0;i<120;++i){nes_core_frame(i%3==0 || i==119,0,0);size_t n;const int16_t *p=nes_core_audio(&n);crc=rom_crc32(crc,p,n*2);}
    return rom_crc32(crc,nes_core_video()-8,NES_SCREEN_PITCH*NES_SCREEN_HEIGHT);
}
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
    uint8_t *data=malloc(n);assert(data && fread(data,1,n,f)==n);fclose(f);
    assert(!nes_core_load(data,n));uint32_t expected=run();nes_core_close();
    image_t image={data,n,0};rom_source_t source={&image,read_image,n,rom_crc32(0,data+16,n-16)};
    assert(!nes_core_load_source(&source,65536));assert(run()==expected);
    size_t state_size=nes_state_size();void *state=malloc(state_size);assert(state && !nes_state_save(state,state_size));
    uint32_t replay=run();assert(!nes_state_load(state,state_size));assert(run()==replay);
    free(state);nes_core_close();free(data);
    size_t prg=254*16384u,chr=128*8192u,size=16+prg+chr;data=calloc(1,size);assert(data);
    memcpy(data,"NES\032",4);data[4]=254;data[5]=128;data[6]=0x10;data[7]=0xf0;
    for(unsigned bank=0;bank<127;++bank)memset(data+16+bank*32768,bank,32768);
    for(unsigned bank=0;bank<1024;++bank)memset(data+16+prg+bank*1024,bank,1024);
    image=(image_t){data,size,0};source=(rom_source_t){&image,read_image,size,rom_crc32(0,data+16,size-16)};
    assert(!nes_core_load_source(&source,65536));
    for(unsigned pass=0;pass<3;++pass)for(unsigned bank=0;bank<127;++bank){
        mmc_bankprg(32,0x8000,bank,PRG_ROM);assert(mem_getpage(0x8000/MEM_PAGESIZE)[100]==bank);
        mmc_bankchr(1,0,bank*7,CHR_ROM);assert(ppu_getpage(0)[42]==(uint8_t)(bank*7));
        assert(mem_getpage(0xc000/MEM_PAGESIZE)[100]==bank);
    }
    assert(!nes_core_rom_cache()->error && nes_core_rom_cache()->evictions>1000);
    nes_core_close();free(data);puts("NES >4 MiB cartridge / 64 KiB cache: banks, video/PCM and snapshot replay passed");
}
