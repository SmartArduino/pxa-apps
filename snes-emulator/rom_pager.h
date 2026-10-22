#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common/rom/rom_cache.h"
bool snes_rom_open(const rom_source_t *,size_t);
void snes_rom_close(void);
extern bool snes_rom_active;
static inline bool snes_rom_paged(void){return snes_rom_active;}
uint8_t *snes_rom_pointer(size_t offset,unsigned owner);
uint8_t *snes_rom_map_pointer(uint8_t *base,unsigned offset);
void snes_rom_patch(size_t offset,uint8_t value);
void snes_rom_bind_maps(void);
uint8_t *snes_rom_mapped(uint32_t address,unsigned owner);
uint8_t snes_rom_dma_boundary(uint8_t **base,uint8_t bank,uint16_t offset);
static inline uint8_t snes_rom_dma_byte(uint8_t **base,uint8_t bank,uint16_t off){
    if(snes_rom_active && ((off&4095)==0 || (off&4095)==4095))return snes_rom_dma_boundary(base,bank,off);
    return (*base)[off];
}
bool snes_rom_reordered(void);
bool snes_rom_hdma_read(uint32_t address,uint8_t *out,unsigned count);
uint8_t *snes_rom_pinned_pointer(uint32_t address,unsigned owner);
void snes_rom_release_c4(void);

void snes_rom_copy_mapped(void *output,uint32_t address,size_t bytes);
extern bool snes_rom_fetch_guard;
void snes_rom_fetch_rebase(void);

extern uint32_t snes_rom_fetch_block;
void snes_rom_fetch_bind(uint32_t address);
