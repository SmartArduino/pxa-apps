/* GPL-2.0-or-later. Pause-only envelope; payloads contain no live pointers.
 * Layout IDs identify core format/ABI revisions rather than app versions.
 * Validate before touching the live machine. CRC also protects identity fields. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define PXA_SNAPSHOT_HEADER 32u
typedef uint32_t (*pxa_snapshot_crc_fn)(uint32_t,const void*,size_t);
static inline uint32_t pxa_snapshot_u32(const uint8_t *p) {
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static inline void pxa_snapshot_put32(uint8_t *p,uint32_t v) {
    for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));
}
static inline void pxa_snapshot_begin(void *buffer,size_t size,uint32_t core,uint32_t rom,uint32_t layout) {
    uint8_t *p=buffer;memset(p,0,size);memcpy(p,"PXAST01",7);
    pxa_snapshot_put32(p+8,(uint32_t)size);pxa_snapshot_put32(p+16,rom);
    pxa_snapshot_put32(p+20,layout);pxa_snapshot_put32(p+24,core);
}
static inline void pxa_snapshot_finish(void *buffer,size_t size,pxa_snapshot_crc_fn crc) {
    uint8_t *p=buffer;uint32_t sum=crc(0,p,12);sum=crc(sum,p+16,size-16);pxa_snapshot_put32(p+12,sum);
}
static inline const char *pxa_snapshot_check(const void *buffer,size_t size,size_t expected,
    uint32_t core,uint32_t rom,uint32_t layout,pxa_snapshot_crc_fn crc) {
    if(!buffer || size<PXA_SNAPSHOT_HEADER || size!=expected)return "Wrong snapshot length";
    const uint8_t *p=buffer;
    if(memcmp(p,"PXAST01",8) || pxa_snapshot_u32(p+8)!=size || pxa_snapshot_u32(p+28))return "Wrong snapshot format";
    if(pxa_snapshot_u32(p+24)!=core || pxa_snapshot_u32(p+16)!=rom)return "Snapshot is for another game";
    if(pxa_snapshot_u32(p+20)!=layout)return "Snapshot core format is incompatible";
    uint32_t sum=crc(0,p,12);sum=crc(sum,p+16,size-16);
    if(sum!=pxa_snapshot_u32(p+12))return "Snapshot checksum mismatch";
    return NULL;
}
