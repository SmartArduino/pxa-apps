#include "rom_pager.h"
#include "vendor/snes9x/snes9x.h"
#include <string.h>
static rom_cache_t cache;
static rom_slice_t slice;
static uint32_t offsets[4096];
static struct {uint32_t offset;uint8_t value;} patches[128];
static unsigned patch_count;
static bool reorder;
bool snes_rom_fetch_guard;
uint32_t snes_rom_fetch_block;
static uint8_t fetch_guard[8],c4_guard[3][2048];
const rom_cache_t *emu_core_rom_cache(void){return &cache;}
bool snes_rom_active;
bool snes_rom_reordered(void){return reorder;}
static int read_rom(void *ctx,uint64_t off,void *out,size_t n){
    (void)ctx;uint8_t *p=out;uint64_t at=off;size_t left=n;
    while(left){
        size_t k=left;if(k>32768-(at&32767))k=32768-(at&32767);
        uint64_t raw=at;
        if(reorder)raw=at<0x80000?(at/32768)*65536+32768+(at&32767):((at-0x80000)/32768)*65536+(at&32767);
        size_t available=slice.source.size-slice.offset;
        size_t take=raw<available?available-raw:0;if(take>k)take=k;
        if(take && rom_slice_read(&slice,raw,p,take))return -1;
        if(take<k)memset(p+take,0,k-take);
        at+=k;p+=k;left-=k;
    }
    for(unsigned i=0;i<patch_count;++i)if(patches[i].offset>=off && patches[i].offset-off<n)
        ((uint8_t*)out)[patches[i].offset-off]=patches[i].value;
    return 0;
}
bool snes_rom_open(const rom_source_t *source,size_t budget){
    slice=(rom_slice_t){*source,source->size%8192==512?512:0};patch_count=0;reorder=false;
    if(source->size-slice.offset==0x100000){
        char name[22];if(rom_slice_read(&slice,0xffc0,name,sizeof(name)))return false;
        reorder=!memcmp(name,"WWF SUPER WRESTLEMANIA",22);
    }
    size_t size=(source->size-slice.offset+65535)&~(size_t)65535;
    rom_source_t logical={NULL,read_rom,size,source->checksum};
    snes_rom_active=rom_cache_init(&cache,&logical,4096,budget,5);return snes_rom_active;
}
void snes_rom_close(void){snes_rom_fetch_guard=false;snes_rom_active=false;rom_cache_close(&cache);patch_count=0;reorder=false;}
uint8_t *snes_rom_pointer(size_t offset,unsigned owner){
    if(!snes_rom_paged())return Memory.ROM+offset;
    return rom_cache_get(&cache,offset,owner);
}
uint8_t *snes_rom_map_pointer(uint8_t *base,unsigned off){
    if(!snes_rom_paged())return base+off;
    return snes_rom_pointer((uintptr_t)base+off-(uintptr_t)Memory.ROM,UINT32_MAX);
}
void snes_rom_patch(size_t off,uint8_t value){
    if(snes_rom_paged()){
        if(patch_count==128){cache.error=4;return;}
        patches[patch_count].offset=off;patches[patch_count++].value=value;
    }
    *snes_rom_pointer(off,UINT32_MAX)=value;
}
void snes_rom_bind_maps(void){
    if(!snes_rom_paged())return;
    for(unsigned i=0;i<4096;++i)if(Memory.MapInfo[i].Type==MAP_TYPE_ROM &&
        Memory.Map[i]>=(uint8_t*)MAP_LAST){
        offsets[i]=(uint32_t)((uintptr_t)Memory.Map[i]+((i<<12)&65535)-(uintptr_t)Memory.ROM);
        Memory.Map[i]=(uint8_t*)MAP_PAGED_ROM;
    }
    rom_cache_unpin(&cache,2);
}
uint8_t *snes_rom_mapped(uint32_t address,unsigned owner){
    unsigned block=(address>>12)&4095;
    size_t offset=offsets[block]+(address&4095);
    /* No LRU bookkeeping or imports for a hit in ordinary data reads. */
    uint32_t index=offset<cache.source.size?cache.lookup[offset>>12]:UINT32_MAX;
    if(owner==UINT32_MAX && index!=UINT32_MAX && !cache.error)
        return cache.data+(size_t)index*4096+(offset&4095);
    return snes_rom_pointer(offset,owner);
}
uint8_t snes_rom_dma_boundary(uint8_t **base,uint8_t bank,uint16_t off){
    if(snes_rom_paged() && ((off&4095)==0 || (off&4095)==4095)){
        *base=GetBasePointer(((uint32_t)bank<<16)|off);
        if(!*base)*base=snes_rom_pointer(0,1);
    }
    return (*base)[off];
}

uint8_t *snes_rom_pinned_pointer(uint32_t address,unsigned owner){
    if(Memory.Map[(address>>12)&4095]==(uint8_t*)MAP_PAGED_ROM){
        uint8_t *p=snes_rom_mapped(address,owner);
        if(owner>=2 && owner<5){
            size_t off=offsets[(address>>12)&4095]+(address&4095),need=owner==2?2048:6;
            if(4096-(off&4095)<need){snes_rom_copy_mapped(c4_guard[owner-2],address,need);return c4_guard[owner-2];}
        }return p;
    }
    return S9xGetMemPointer(address);
}
void snes_rom_release_c4(void){if(snes_rom_active)for(unsigned i=2;i<5;++i)rom_cache_unpin(&cache,i);}
bool snes_rom_hdma_read(uint32_t address,uint8_t *out,unsigned count){
    for(unsigned i=0;i<count;++i){
        uint32_t a=(address&0xff0000)|((address+i)&65535);
        uint8_t *p=snes_rom_pinned_pointer(a,UINT32_MAX);if(!p)return false;out[i]=*p;
    }return true;
}

void snes_rom_copy_mapped(void *output,uint32_t address,size_t bytes){
    uint8_t *out=output;
    while(bytes){size_t n=4096-(address&4095);if(n>bytes)n=bytes;
        uint8_t *p=snes_rom_pinned_pointer(address,UINT32_MAX);
        if(p)memmove(out,p,n);else memset(out,0,n);
        out+=n;address=(address+(uint32_t)n)&0xffffff;bytes-=n;
    }
}
void snes_rom_fetch_rebase(void){
    unsigned pc=(unsigned)(CPU.PC-CPU.PCBase)&65535;
    S9xSetPCBase(ICPU.ShiftedPB|pc);snes_rom_fetch_guard=false;
    if((pc&4095)>4091){
        for(unsigned i=0;i<8;++i){uint32_t a=ICPU.ShiftedPB|((pc+i)&65535);
            uint8_t *p=snes_rom_pinned_pointer(a,UINT32_MAX);fetch_guard[i]=p?*p:OpenBus;}
        CPU.PCBase=(uint8_t*)((uintptr_t)fetch_guard-pc);CPU.PC=fetch_guard;snes_rom_fetch_guard=true;
    }
}

void snes_rom_fetch_bind(uint32_t address){
    snes_rom_fetch_block=address>>12;snes_rom_fetch_guard=false;
    CPU.WaitAddress=NULL;CPU.WaitCounter=0;
    if(Memory.Map[(address>>12)&4095]!=(uint8_t*)MAP_PAGED_ROM)rom_cache_unpin(&cache,0);
}
