#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common/rom/rom_cache.h"
extern bool md_rom_active;
bool md_rom_open(const rom_source_t *,size_t);
void md_rom_close(void);
uint8_t *md_rom_pointer(size_t offset,unsigned owner);
void md_rom_map(unsigned start,unsigned end,size_t offset);
void md_rom_unmap(unsigned start,unsigned end);
unsigned md_rom_read8(unsigned address);
unsigned md_rom_read16(unsigned address);
uintptr_t md_rom_fetch_base(unsigned address,uintptr_t fallback);
uint16_t *md_rom_dma_base(unsigned address);
uint32_t md_rom_crc(unsigned size);
uint16_t md_rom_fetch_word(unsigned address,uintptr_t fallback);

uint16_t *md_rom_dma_prepare(unsigned address,unsigned words);
