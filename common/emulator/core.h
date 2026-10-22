/* PXA console core contract. GPL-2.0-or-later. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "app_config.h"

enum {
    EMU_PAD_RIGHT=1u<<0, EMU_PAD_LEFT=1u<<1, EMU_PAD_UP=1u<<2, EMU_PAD_DOWN=1u<<3,
    EMU_PAD_A=1u<<4, EMU_PAD_B=1u<<5, EMU_PAD_X=1u<<6, EMU_PAD_Y=1u<<7,
    EMU_PAD_L=1u<<8, EMU_PAD_R=1u<<9, EMU_PAD_SELECT=1u<<10, EMU_PAD_START=1u<<11,
};
typedef struct {
    unsigned width,height,stride,aspect_num,aspect_den;
    uint32_t rate_num,rate_den,audio_rate,checksum;
    unsigned model;
    char title[64];
} emu_core_info_t;
extern emu_core_info_t emu_core_info;
#define EMU_VIDEO_WIDTH (emu_core_info.width)
#define EMU_VIDEO_HEIGHT (emu_core_info.height)
#define EMU_CLOCK_HZ (emu_core_info.rate_num)
#define EMU_FRAME_CYCLES (emu_core_info.rate_den)
#define EMU_FRAME_BUDGET_US (UINT64_C(1000000)*EMU_FRAME_CYCLES/EMU_CLOCK_HZ)
typedef void (*emu_log_sink)(const char *line);
void emu_core_set_log_sink(emu_log_sink sink);
void emu_core_log(const char *format,...);
void emu_core_defaults(void);
uint32_t emu_port_crc32(uint32_t seed,const void *data,size_t size);
const char *emu_core_validate(const uint8_t *rom,size_t size);
/* The image is borrowed until close; adapters must document any conversion. */
const char *emu_core_load(uint8_t *rom,size_t size);
#include "../rom/rom_cache.h"
const char *emu_core_load_source(const rom_source_t *source,size_t budget);
const rom_cache_t *emu_core_rom_cache(void);
void emu_core_close(void);
void emu_core_set_audio_enabled(bool enabled);
/* Skipping video must retain CPU, interrupts, timers and all sound state. */
void emu_core_frame_render(uint16_t player1,uint16_t player2,bool render);
void emu_core_frame_to(uint16_t player1,uint16_t player2,uint16_t *target,size_t stride);
const uint16_t *emu_core_video(void);
/* Optional borrowed indexed output. Palette always has 256 RGB565 entries.
 * RGB565 remains available lazily for hosts without native indexed blitting. */
void emu_core_set_indexed_output(bool enabled);
const uint8_t *emu_core_indexed_video(unsigned *stride,const uint16_t **palette);
const int16_t *emu_core_audio(size_t *frames);
size_t emu_core_save_size(void);
bool emu_core_save_export(uint8_t *data,size_t size);
bool emu_core_save_import(const uint8_t *data,size_t size);
static inline size_t emu_core_sram_size(void){return emu_core_save_size();}
static inline uint32_t emu_core_checksum(void){return emu_core_info.checksum;}
static inline unsigned emu_core_rate(void){return (EMU_CLOCK_HZ+EMU_FRAME_CYCLES/2)/EMU_FRAME_CYCLES;}
static inline unsigned emu_core_model(void){return emu_core_info.model;}
static inline const char *emu_core_title(void){return emu_core_info.title;}
static inline unsigned emu_core_audio_rate(void){return emu_core_info.audio_rate;}
