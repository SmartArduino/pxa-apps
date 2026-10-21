#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define NES_ROM_LIMIT (3u * 1024u * 1024u)
#define NES_VIDEO_PITCH 272u
const char *nes_core_validate(const uint8_t *bytes, size_t size);
const char *nes_core_load(uint8_t *bytes, size_t size);
void nes_core_close(void);
void nes_core_frame(bool draw, uint8_t pad1, uint8_t pad2);
const uint8_t *nes_core_video(void);
const int16_t *nes_core_audio(size_t *samples);
uint8_t *nes_core_sram(size_t *bytes);
uint32_t nes_core_checksum(void);
unsigned nes_core_rate(void);
unsigned nes_core_mapper(void);
uint16_t nes_core_color(unsigned index);
uint32_t nes_port_crc32(uint32_t seed, const void *data, size_t size);

uint64_t nes_core_scan_time(void);
typedef struct {
    uint64_t cpu_bus_us,ppu_us,apu_us;
    unsigned samples;
} nes_core_profile_t;
void nes_core_profile_take(nes_core_profile_t out[2]);
