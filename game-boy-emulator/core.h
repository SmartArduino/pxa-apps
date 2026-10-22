/* PXA Game Boy / Game Boy Color core wrapper. GPL-2.0-or-later. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Largest cartridge we accept. GB ROMs are 32 KiB..8 MiB. */
#define GB_ROM_LIMIT (8u * 1024u * 1024u)
#define GB_VIDEO_WIDTH 160u
#define GB_VIDEO_HEIGHT 144u
#define GB_VIDEO_PITCH 160u
/* Exactly 64 double-speed cycles per sample; no fractional APU clock. */
#define GB_AUDIO_RATE 32768u
/* Cycles in one Game Boy video frame, used by the pacing helper. */
#define GB_FRAME_CYCLES 70224u
#define GB_CLOCK_HZ 4194304u
/* Wall time per hardware frame (59.72750057 Hz). */
#define GB_FRAME_BUDGET_US (UINT64_C(1000000)*GB_FRAME_CYCLES/GB_CLOCK_HZ)

enum gb_board {
    GB_BOARD_DMG = 0,
    GB_BOARD_CGB = 1,
    GB_BOARD_SGB = 2,
};

/* Button bit layout, matching the Game Boy hardware register order. */
enum {
    GB_PAD_RIGHT  = 1u << 0,
    GB_PAD_LEFT   = 1u << 1,
    GB_PAD_UP     = 1u << 2,
    GB_PAD_DOWN   = 1u << 3,
    GB_PAD_A      = 1u << 4,
    GB_PAD_B      = 1u << 5,
    GB_PAD_SELECT = 1u << 6,
    GB_PAD_START  = 1u << 7,
};

/* Receives one formatted line per core diagnostic. */
typedef void (*gb_log_sink)(const char *line);
void gb_core_set_log_sink(gb_log_sink sink);
/* Muting skips output writes while retaining emulated APU state. */
void gb_core_set_audio_enabled(bool enabled);

/* Returns NULL when the image looks like a loadable cartridge. */
const char *gb_core_validate(const uint8_t *bytes, size_t size);
/* Loads a cartridge. The core keeps a reference to `bytes` until close. */
const char *gb_core_load(const uint8_t *bytes, size_t size);
void gb_core_close(void);

/* Advances exactly one video frame with the given button state. */
void gb_core_frame(uint16_t pad);
/* Skip presentation work; CPU, timers, DMA and the APU still advance. */
void gb_core_frame_render(uint16_t pad, bool render);
/* Native-size render into a leased Surface subrectangle. The target is
 * borrowed only during this call; gb_core_video keeps its previous contents. */
void gb_core_frame_to(uint16_t pad, uint16_t *target, size_t stride);
/* RGB565LE framebuffer, GB_VIDEO_WIDTH * GB_VIDEO_HEIGHT pixels. */
const uint16_t *gb_core_video(void);
/* Interleaved stereo samples; returns the frame count (pairs). */
const int16_t *gb_core_audio(size_t *frames);

size_t gb_core_sram_size(void);
const uint8_t *gb_core_sram(void);
/* Restores a previously saved cartridge RAM image. */
void gb_core_sram_import(const uint8_t *data, size_t size);
size_t gb_core_save_size(void);
bool gb_core_save_export(uint8_t *data, size_t capacity);
bool gb_core_save_import(const uint8_t *data, size_t size);
uint32_t gb_core_checksum(void);
unsigned gb_core_rate(void);
unsigned gb_core_model(void);
const char *gb_core_title(void);
/* CRC32 over a buffer, used to detect cartridge RAM changes. */
uint32_t gb_port_crc32(uint32_t seed, const void *data, size_t size);

#include "../common/rom/rom_cache.h"
const char *gb_core_load_source(const rom_source_t *source,size_t budget);
const rom_cache_t *gb_core_rom_cache(void);
