/* Resident versus positional cartridge execution, with independent bank data. */
#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(EMU_SMS_GEAR)
#include "engines/smsplus/shared.h"
#elif defined(EMU_PAGING_MD)
#include "vendor/picodrive/pico/pico_int.h"
#include "vendor/picodrive/pico/memory.h"
#include "rom_pager.h"
#elif defined(EMU_PAGING_SNES)
#include "vendor/snes9x/snes9x.h"
#include "vendor/snes9x/src/dma.h"
#elif defined(EMU_PAGING_PCE)
#include "vendor/pce-go/pce.h"
#endif
typedef struct {const uint8_t *data;size_t size;unsigned reads;int fail;} image_t;
static int read_image(void *ctx,uint64_t off,void *out,size_t n){
    image_t *i=ctx;++i->reads;if(i->fail || off>i->size || n>i->size-off)return -1;
    memcpy(out,i->data+off,n);return 0;
}
static uint32_t run(void){
    uint32_t crc=0;
    for(unsigned i=0;i<120;++i){
        emu_core_frame_render(i<60?0:EMU_PAD_A|EMU_PAD_RIGHT,0,i%3==0 || i==119);
        size_t n;const int16_t *audio=emu_core_audio(&n);crc=rom_crc32(crc,audio,n*4);
    }
    const uint16_t *p=emu_core_video();
    for(unsigned y=0;y<EMU_VIDEO_HEIGHT;++y)crc=rom_crc32(crc,p+y*emu_core_info.stride,EMU_VIDEO_WIDTH*2);
    return crc;
}

#ifdef EMU_PAGING_SNES
static uint32_t hdma_trace(void){
    extern uint8_t *HDMAMemPointers[8],*HDMABasePointers[8];
    memset(HDMAMemPointers,0,8*sizeof(void*));memset(HDMABasePointers,0,8*sizeof(void*));
    S9xResetDMA();IPPU.RenderThisFrame=false;
    for(unsigned i=0;i<8;++i){
        DMA[i].HDMAIndirectAddressing=true;DMA[i].IndirectBank=i*8;DMA[i].IndirectAddress=0xc100;
        DMA[i].LineCount=127;DMA[i].Repeat=false;DMA[i].FirstLine=true;
        DMA[i].TransferMode=4;DMA[i].BAddress=0x30;
    }
    uint32_t crc=0;
    for(unsigned line=0;line<64;++line)for(unsigned i=0;i<8;++i){
        assert(S9xDoHDMA(1u<<i)==(1u<<i));crc=rom_crc32(crc,Memory.FillRAM+0x2130,4);
    }
    return crc;
}
#endif
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
    uint8_t *data=calloc(1,EMU_ROM_CAPACITY(n));assert(data && fread(data,1,n,f)==n);fclose(f);
    image_t image={data,n,0,0};rom_source_t source={&image,read_image,n,rom_crc32(0,data,n)};
    assert(!emu_core_load(data,n));emu_core_set_audio_enabled(true);uint32_t expected=run();emu_core_close();
    /* MD's resident adapter transforms its borrowed bytes. Reload pristine. */
    f=fopen(argv[1],"rb");assert(f && fread(data,1,n,f)==n);fclose(f);
    const char *error=emu_core_load_source(&source,256*1024);
    if(error){fprintf(stderr,"paging load: %s\n",error);return 2;}
    emu_core_set_audio_enabled(true);assert(run()==expected);assert(!emu_core_rom_cache()->error);
    emu_core_close();free(data);
#if defined(EMU_SMS_GEAR) || defined(EMU_PAGING_PCE)
    size_t size=2*1024*1024;data=malloc(size);assert(data);
    for(unsigned i=0;i<size/8192;++i)memset(data+i*8192,i,8192);
    f=fopen(argv[1],"rb");assert(f && fread(data,1,n,f)==n);fclose(f);
    image=(image_t){data,size,0,0};source=(rom_source_t){&image,read_image,size,rom_crc32(0,data,size)};
    assert(!emu_core_load_source(&source,128*1024));
    for(unsigned pass=0;pass<3;++pass)for(unsigned bank=8;bank<size/8192;++bank){
#if defined(EMU_SMS_GEAR)
        mapper_8k_w(0,bank);assert(cpu_readmap[0x20][100]==(uint8_t)bank);
        assert(cpu_readmap[0][0]==data[0]);
#else
        pxa_pce_rom_banks[0x60]=(int16_t)bank;pce_bank_set(2,0x60);
        assert(PageR[2][0x4000+100]==(uint8_t)bank);
#endif
    }
    assert(emu_core_rom_cache()->evictions>500 && !emu_core_rom_cache()->error);
    emu_core_close();free(data);
#if defined(EMU_SMS_GEAR)
    /* Full SEGA register range: 256 sixteen-KiB banks must not wrap to zero. */
    size=4u*1024*1024;data=malloc(size);assert(data);
    for(unsigned bank=0;bank<256;++bank)memset(data+bank*16384,bank,16384);
    f=fopen(argv[1],"rb");assert(f && fread(data,1,n,f)==n);fclose(f);
    image=(image_t){data,size,0,0};source=(rom_source_t){&image,read_image,size,rom_crc32(0,data,size)};
    assert(!emu_core_load_source(&source,128*1024));assert(slot.pages==256);
    for(unsigned pass=0;pass<2;++pass)for(unsigned bank=2;bank<256;++bank){
        mapper_16k_w(3,bank);assert(cpu_readmap[0x20][100]==(uint8_t)bank);
        assert(cpu_readmap[0][0]==data[0]);
    }
    assert(!emu_core_rom_cache()->error && emu_core_rom_cache()->evictions>100);
    emu_core_close();free(data);
#endif
#endif

#ifdef EMU_PAGING_SNES
    size_t big_size=8u*1024*1024;data=calloc(1,EMU_ROM_CAPACITY(big_size));assert(data);
    for(unsigned i=0;i<big_size/65536;++i)memset(data+i*65536,i,65536);
    f=fopen(argv[1],"rb");assert(f && fread(data,1,n,f)==n);fclose(f);
    memcpy(data+0x400000,data,n);data[0x7fd7]=data[0x407fd7]=13;
    uint8_t expected_bytes[4096];unsigned addresses[4096],count=0;
    assert(!emu_core_load(data,big_size));
    for(unsigned block=0;block<4096;++block)if(Memory.MapInfo[block].Type==MAP_TYPE_ROM && Memory.Map[block]>=(uint8_t*)MAP_LAST){
        addresses[count]=(block<<12)+0x345;expected_bytes[count]=S9xGetByte(addresses[count]);++count;
    }
    uint32_t expected_hdma=hdma_trace();
    emu_core_close();image=(image_t){data,big_size,0,0};source=(rom_source_t){&image,read_image,big_size,rom_crc32(0,data,big_size)};
    assert(!emu_core_load_source(&source,256*1024));
    for(unsigned pass=0;pass<3;++pass)for(unsigned i=0;i<count;++i)assert(S9xGetByte(addresses[i])==expected_bytes[i]);
    assert(hdma_trace()==expected_hdma);
    assert(emu_core_rom_cache()->evictions>64 && !emu_core_rom_cache()->error);emu_core_close();free(data);
#endif

#ifdef EMU_PAGING_MD
    /* Independently encode SMD; the paged reader must match plain ROM output. */
    data=calloc(1,n+512);uint8_t *plain=malloc(n);assert(data && plain);
    f=fopen(argv[1],"rb");assert(f && fread(plain,1,n,f)==n);fclose(f);
    for(size_t at=0;at<n;at+=16384)for(unsigned i=0;i<8192;++i){data[512+at+i]=plain[at+2*i+1];data[512+at+8192+i]=plain[at+2*i];}
    image=(image_t){data,n+512,0,0};source=(rom_source_t){&image,read_image,n+512,rom_crc32(0,data,n+512)};
    assert(!emu_core_load_source(&source,512*1024));emu_core_set_audio_enabled(true);assert(run()==expected);
    emu_core_close();free(data);
    size_t large_size=8u*1024*1024;data=malloc(large_size);assert(data);
    for(unsigned bank=0;bank<16;++bank)memset(data+bank*0x80000,bank,0x80000);
    memcpy(data,plain,n);free(plain);
    image=(image_t){data,large_size,0,0};source=(rom_source_t){&image,read_image,large_size,rom_crc32(0,data,large_size)};
    assert(!emu_core_load_source(&source,512*1024));
    extern void carthw_ssf2_write8(unsigned,unsigned);
    for(unsigned pass=0;pass<4;++pass)for(unsigned bank=1;bank<16;++bank){
        carthw_ssf2_write8(0xa130f3,bank);
        assert(m68k_read8(0x080123)==bank);assert(m68k_read16(0x080124)==(bank*257));
        uint16_t *dma=md_rom_dma_base(0x080200);assert(dma && dma[123]==bank*257);
        uintptr_t base=md_rom_fetch_base(0x080200,0);assert(*(uint16_t*)(base+0x080200)==bank*257);
    }
    assert(!emu_core_rom_cache()->error && emu_core_rom_cache()->evictions>40);
    emu_core_close();free(data);
#endif
    printf("%s resident/paged video and PCM parity; bounded cartridge banks passed\n",EMU_SYSTEM_TAG);
}
