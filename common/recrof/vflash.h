#pragma once
#include <stdint.h>
void vflash_init(const char *path, uint32_t virtual_size, uint32_t nvm_size);
void *vflash_read(uint32_t address, void *dest, uint32_t length);
void vflash_write(uint32_t address, const void *data, uint32_t length,
                  int erase);
uint32_t vflash_data_size(void);
typedef struct {
  uint64_t hits, misses, read_us, nvm_reads, nvm_read_us;
  uint32_t cache_bytes;
} vflash_stats_t;
void vflash_stats(vflash_stats_t *out, int reset);
