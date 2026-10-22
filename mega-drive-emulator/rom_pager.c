#include "rom_pager.h"
#include "vendor/picodrive/pico/pico_int.h"
#include <string.h>
static rom_cache_t cache;
static rom_source_t raw;
static size_t image_size;
static bool smd;
static int32_t banks[256];
static uint8_t dma_window[128*1024];
bool md_rom_active;
const rom_cache_t *emu_core_rom_cache(void){return &cache;}
static int read_rom(void *ctx,uint64_t at,void *out,size_t n){
    (void)ctx;uint8_t *p=out;size_t left=n;
    while(left){
        size_t k=left;if(k>16384-(at&16383))k=16384-(at&16383);
        size_t take=at<image_size?image_size-at:0;if(take>k)take=k;
        if(smd && take){
            uint8_t halves[4096];size_t bank=(size_t)at&~(size_t)16383,column=((size_t)at&16383)/2;
            if(take>4096 || (take&1))return -1;
            if(!rom_source_read(&raw,512+bank+column,halves,take/2) ||
                !rom_source_read(&raw,512+bank+8192+column,halves+take/2,take/2))return -1;
            for(size_t i=0;i<take/2;++i){p[2*i]=halves[i];p[2*i+1]=halves[take/2+i];}
        }else if(take){
            if(!rom_source_read(&raw,at,p,take))return -1;
            for(size_t i=0;i+1<take;i+=2){uint8_t v=p[i];p[i]=p[i+1];p[i+1]=v;}
        }
        if(take<k)memset(p+take,0,k-take);
        at+=k;p+=k;left-=k;
    }return 0;
}
bool md_rom_open(const rom_source_t *source,size_t budget){
    raw=*source;smd=source->size%16384==512;image_size=source->size-(smd?512:0);
    size_t capacity=0x80000;while(capacity<image_size)capacity<<=1;
    rom_source_t logical={NULL,read_rom,capacity+0x80000,source->checksum};
    memset(banks,255,sizeof(banks));
    md_rom_active=rom_cache_init(&cache,&logical,4096,budget,2);return md_rom_active;
}
void md_rom_close(void){md_rom_active=false;rom_cache_close(&cache);}
uint8_t *md_rom_pointer(size_t offset,unsigned owner){
    if(!md_rom_active)return Pico.rom+offset;
    unsigned index=offset<cache.source.size?cache.lookup[offset>>12]:UINT32_MAX;
    if(owner==UINT32_MAX && index!=UINT32_MAX && !cache.error)return cache.data+(size_t)index*4096+(offset&4095);
    return rom_cache_get(&cache,offset,owner);
}
void md_rom_map(unsigned start,unsigned end,size_t offset){
    for(unsigned i=start>>16;i<=end>>16 && i<256;++i)banks[i]=(int32_t)(offset+(i-(start>>16))*65536);
}
void md_rom_unmap(unsigned start,unsigned end){for(unsigned i=start>>16;i<=end>>16 && i<256;++i)banks[i]=-1;}
static size_t physical(unsigned a){return (size_t)banks[(a>>16)&255]+(a&65535);}
unsigned md_rom_read8(unsigned a){return *md_rom_pointer(physical(a)^1,UINT32_MAX);}
unsigned md_rom_read16(unsigned a){const uint8_t *p=md_rom_pointer(physical(a&~1u),UINT32_MAX);return p[0]|((unsigned)p[1]<<8);}
uintptr_t md_rom_fetch_base(unsigned a,uintptr_t fallback){
    if(banks[(a>>16)&255]<0)return fallback;
    return (uintptr_t)md_rom_pointer(physical(a),0)-(a&0xffffff);
}
uint16_t md_rom_fetch_word(unsigned a,uintptr_t fallback){
    if(banks[(a>>16)&255]<0)return *(uint16_t*)(fallback+a);
    return (uint16_t)md_rom_read16(a);
}
uint16_t *md_rom_dma_prepare(unsigned a,unsigned words){
    size_t bytes=(size_t)words*2;
    if(!bytes || bytes>131072 || banks[(a>>16)&255]<0)return NULL;
    if(bytes<=4096-(physical(a)&4095)){
        uint8_t *p=md_rom_pointer(physical(a),1);
        return (uint16_t*)((uintptr_t)p-(a&131071));
    }
    unsigned bank_base=a&0xfe0000,position=a&131071;
    while(bytes){
        unsigned address=bank_base|position;
        if(banks[(address>>16)&255]<0)return NULL;
        size_t off=physical(address),n=4096-(off&4095);if(n>bytes)n=bytes;
        memcpy(dma_window+position,md_rom_pointer(off,1),n);
        bytes-=n;position=(position+(unsigned)n)&131071;
    }
    return (uint16_t*)dma_window;
}
uint16_t *md_rom_dma_base(unsigned a){return md_rom_dma_prepare(a&0xfe0000,65536);}

uint32_t md_rom_crc(unsigned size){
    if(!size || size>=image_size)return raw.checksum;
    uint8_t buf[4096];uint32_t crc=0;
    for(size_t at=0;at<size;){size_t n=size-at;if(n>sizeof(buf))n=sizeof(buf);
        memcpy(buf,md_rom_pointer(at,UINT32_MAX),n);
        for(size_t i=0;i+1<n;i+=2){uint8_t v=buf[i];buf[i]=buf[i+1];buf[i+1]=v;}
        crc=rom_crc32(crc,buf,n);at+=n;
    }return crc;
}
