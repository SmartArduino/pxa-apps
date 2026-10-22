/* PXA READ_AT transport. A file stays open for the cartridge's lifetime. */
#pragma once
#include "rom_cache.h"
#include "pxa_fs_roots.h"
#include <string.h>
#include <time.h>
static inline uint64_t pxa_rom_micros(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000u+t.tv_nsec/1000u;}
typedef struct {uint64_t handle,cookie,read_calls,read_bytes,read_us,max_read_us;uint8_t packet[24+4096];} pxa_rom_file_t;
static inline int pxa_rom_read(void *context,uint64_t offset,void *out,size_t bytes){
    pxa_rom_file_t *f=context;uint8_t *p=out;
    while(bytes){unsigned n=bytes>4096?4096:(unsigned)bytes;
        uint64_t start=pxa_rom_micros();
        int r=pxa_fs_read_at(f->handle,offset,++f->cookie,f->packet,24+n);
        uint64_t elapsed=pxa_rom_micros()-start;f->read_us+=elapsed;++f->read_calls;
        if(elapsed>f->max_read_us)f->max_read_us=elapsed;
        if(r>0)f->read_bytes+=(unsigned)r;
        if(r<=0 || (unsigned)r>n)return -1;
        memcpy(p,f->packet+24,(unsigned)r);offset+=(unsigned)r;p+=r;bytes-=(unsigned)r;
    }return 0;
}
