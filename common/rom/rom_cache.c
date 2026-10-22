#include "rom_cache.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int rom_slice_read(void *context,uint64_t offset,void *out,size_t n){
    rom_slice_t *s=context;
    return offset<=UINT64_MAX-s->offset && rom_source_read(&s->source,s->offset+offset,out,n)?0:-1;
}
bool rom_source_crc(const rom_source_t *s,uint64_t start,size_t n,uint32_t *crc){
    uint8_t buf[4096];uint32_t v=0;
    while(n){size_t k=n>sizeof(buf)?sizeof(buf):n;
        if(!rom_source_read(s,start,buf,k))return false;
        v=rom_crc32(v,buf,k);start+=k;n-=k;
    }*crc=v;return true;
}

bool rom_source_read(const rom_source_t *s,uint64_t at,void *out,size_t n){
    return s && s->read && out && at<=s->size && n<=s->size-at &&
        (!n || s->read(s->context,at,out,n)==0);
}
void rom_cache_close(rom_cache_t *c){
    if(!c)return;
    free(c->data);free(c->fault_page);free(c->slots);free(c->lookup);free(c->owners);
    memset(c,0,sizeof(*c));
}
bool rom_cache_init(rom_cache_t *c,const rom_source_t *s,unsigned page,size_t budget,unsigned owners){
    if(!c || !s || !s->read || !s->size || page<1024 || (page&(page-1)) ||
        s->size>UINT32_MAX || budget/page<2 || owners>1024)return false;
    memset(c,0,sizeof(*c));c->source=*s;c->page_size=page;
    c->pages=(uint32_t)((s->size+page-1)/page);
    size_t count=budget/page;if(count>c->pages)count=c->pages;c->count=(uint32_t)count;
    c->owner_count=owners;
    c->data=malloc((size_t)c->count*page);c->fault_page=malloc(page+16u);
    c->slots=calloc(c->count,sizeof(*c->slots));c->lookup=malloc((size_t)c->pages*sizeof(*c->lookup));
    c->owners=owners?malloc(owners*sizeof(*c->owners)):NULL;
    if(!c->data || !c->fault_page || !c->slots || !c->lookup || (owners && !c->owners)){
        rom_cache_close(c);return false;
    }
    memset(c->fault_page,255,page+16u);memset(c->lookup,255,c->pages*sizeof(*c->lookup));
    if(owners)memset(c->owners,255,owners*sizeof(*c->owners));
    for(unsigned i=0;i<c->count;++i)c->slots[i].page=UINT32_MAX;
    return true;
}
void rom_cache_unpin(rom_cache_t *c,unsigned owner){
    if(owner>=c->owner_count)return;
    uint32_t i=c->owners[owner];
    if(i!=UINT32_MAX){--c->slots[i].refs;c->owners[owner]=UINT32_MAX;}
}
uint8_t *rom_cache_get(rom_cache_t *c,uint64_t at,unsigned owner){
    if(!c || !c->page_size)return NULL;
    unsigned in=(unsigned)(at&(c->page_size-1));
    if(c->error || at>=c->source.size){c->error=1;return c->fault_page+in;}
    uint32_t page=(uint32_t)(at/c->page_size),i=c->lookup[page];
    if(owner<c->owner_count && c->owners[owner]!=i)rom_cache_unpin(c,owner);
    if(i==UINT32_MAX){
        uint32_t age=UINT32_MAX;
        for(unsigned k=0;k<c->count;++k)if(!c->slots[k].refs &&
            (c->slots[k].page==UINT32_MAX || c->slots[k].age<=age)){
            i=k;age=c->slots[k].age;if(c->slots[k].page==UINT32_MAX)break;
        }
        if(i==UINT32_MAX){c->error=2;return c->fault_page+in;}
        rom_cache_slot_t *slot=c->slots+i;
        if(slot->page!=UINT32_MAX){c->lookup[slot->page]=UINT32_MAX;++c->evictions;}
        slot->page=UINT32_MAX;
        uint64_t start=(uint64_t)page*c->page_size;
        size_t n=c->source.size-start;if(n>c->page_size)n=c->page_size;
        uint8_t *data=c->data+(size_t)i*c->page_size;
        if(!rom_source_read(&c->source,start,data,n)){
            c->error=3;return c->fault_page+in;
        }
        if(n<c->page_size)memset(data+n,255,c->page_size-n);
        slot->page=page;c->lookup[page]=i;++c->misses;c->bytes+=n;
    }else ++c->hits;
    /* Age is updated only on bank accesses, never per emulated byte. */
    if(++c->clock==0){for(unsigned k=0;k<c->count;++k)c->slots[k].age>>=1;c->clock=UINT32_C(0x80000000);}
    c->slots[i].age=c->clock;
    if(owner<c->owner_count && c->owners[owner]!=i){c->owners[owner]=i;++c->slots[i].refs;}
    return c->data+(size_t)i*c->page_size+in;
}
uint32_t rom_crc32(uint32_t crc,const void *data,size_t n){
    static uint32_t table[256];
    if(!table[1])for(unsigned i=0;i<256;++i){uint32_t v=i;for(unsigned j=0;j<8;++j)v=(v>>1)^((0u-(v&1))&UINT32_C(0xedb88320));table[i]=v;}
    const uint8_t *p=data;crc=~crc;
    while(n--)crc=(crc>>8)^table[(crc^*p++)&255];return ~crc;
}
