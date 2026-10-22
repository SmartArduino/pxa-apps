/* Bounded cartridge storage. GPL-2.0-or-later. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Exact positional reads: no cursor changes, no retained output pointer. */
typedef int (*rom_read_fn)(void *context,uint64_t offset,void *out,size_t bytes);
typedef struct {
    void *context;
    rom_read_fn read;
    uint64_t size;
    uint32_t checksum;
} rom_source_t;
typedef struct {rom_source_t source;uint64_t offset;} rom_slice_t;
int rom_slice_read(void *context,uint64_t offset,void *out,size_t bytes);
bool rom_source_crc(const rom_source_t *source,uint64_t start,size_t bytes,uint32_t *crc);
typedef struct {
    uint32_t page,age,refs;
} rom_cache_slot_t;
typedef struct {
    rom_source_t source;
    uint8_t *data,*fault_page;
    rom_cache_slot_t *slots;
    uint32_t *lookup,*owners;
    uint32_t page_size,count,pages,owner_count,clock;
    uint64_t hits,misses,bytes,evictions;
    int error;
} rom_cache_t;

bool rom_cache_init(rom_cache_t *cache,const rom_source_t *source,
    unsigned page_size,size_t budget,unsigned owners);
void rom_cache_close(rom_cache_t *cache);
void rom_cache_unpin(rom_cache_t *cache,unsigned owner);
/* owner identifies a live mapping, not a bank. Replacing it releases the old
 * reference. UINT32_MAX gives a transient pointer valid until the next miss.
 * Returns offset within the page; callers must not cross the page boundary.
 * A read failure is sticky and returns an open-bus page; frontends stop the
 * emulation at the next frame boundary and retain the existing save. */
uint8_t *rom_cache_get(rom_cache_t *cache,uint64_t offset,unsigned owner);
bool rom_source_read(const rom_source_t *source,uint64_t offset,void *out,size_t bytes);
/* Standard incremental IEEE CRC, shared by streaming loaders. */
uint32_t rom_crc32(uint32_t crc,const void *data,size_t bytes);
