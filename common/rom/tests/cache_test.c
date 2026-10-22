#include "../rom_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
typedef struct {unsigned calls;int fail;} source_t;
static uint8_t byte_at(uint64_t offset){return (uint8_t)((offset>>12)^(offset*17));}
static int read_source(void *ctx,uint64_t offset,void *out,size_t bytes){
    source_t *s=ctx;++s->calls;if(s->fail)return -1;
    for(size_t i=0;i<bytes;++i)((uint8_t*)out)[i]=byte_at(offset+i);
    return 0;
}
int main(void){
    source_t reader={0};rom_source_t source={&reader,read_source,64u*1024u*1024u+17u,0};
    rom_cache_t c={0};assert(!rom_cache_init(&c,&source,3000,65536,8));
    assert(rom_cache_init(&c,&source,4096,6*4096,8));
    uint8_t *a=rom_cache_get(&c,0,0);assert(a[123]==byte_at(123));
    assert(rom_cache_get(&c,0,1)==a && c.slots[c.owners[0]].refs==2);
    rom_cache_unpin(&c,0);assert(c.slots[c.owners[1]].refs==1);
    for(unsigned owner=2;owner<6;++owner)rom_cache_get(&c,owner*4096u,owner);
    for(unsigned page=100;page<1000;++page){
        uint8_t *p=rom_cache_get(&c,page*4096u,UINT32_MAX);assert(p[123]==byte_at(page*4096u+123));
        assert(a[123]==byte_at(123));
    }
    assert(!c.error && c.evictions>800 && c.count==6);
    /* Replacing the last free transient page with a pinned one exhausts the
     * cache without invalidating any of the previous five live windows. */
    rom_cache_get(&c,999*4096u,6);unsigned calls=reader.calls;
    assert(rom_cache_get(&c,1001*4096u,7)[20]==255 && c.error==2);
    assert(reader.calls==calls && a[123]==byte_at(123));rom_cache_close(&c);
    assert(rom_cache_init(&c,&source,4096,65536,2));
    uint64_t last=source.size&~UINT64_C(4095);uint8_t *p=rom_cache_get(&c,last,0);
    assert(p[16]==byte_at(last+16) && p[17]==255 && p[4095]==255);
    reader.fail=1;assert(rom_cache_get(&c,0,1)[100]==255 && c.error==3);rom_cache_close(&c);
    /* A very large requested budget must clamp before narrowing to u32. */
    source.size=4096;reader.fail=0;assert(rom_cache_init(&c,&source,4096,SIZE_MAX,1));assert(c.count==1);
    rom_cache_close(&c);
    assert(rom_crc32(rom_crc32(0,"1234",4),"56789",5)==UINT32_C(0xcbf43926));
    puts("64 MiB source / 24 KiB cache: aliases, pin exhaustion, eviction, partial page, I/O failure, bounds and CRC passed");
}
