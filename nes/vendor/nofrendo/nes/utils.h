#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
void nes_port_log(const char *format, ...);
uint64_t nes_port_time(void);
void nes_port_scan_time(uint64_t us);
void nes_port_phase_time(unsigned draw,uint64_t cpu,uint64_t ppu,uint64_t apu);
uint32_t nes_port_crc32(uint32_t seed, const void *data, size_t size);
#define LOG_PRINTF(level, x...) nes_port_log(x)
#define IRAM_ATTR
#define CRC32(a, b, c) nes_port_crc32(a, b, c)

#define MESSAGE_ERROR(x...) LOG_PRINTF(1, "!! " x)
#define MESSAGE_WARN(x...)  LOG_PRINTF(2, " ! " x)
#define MESSAGE_INFO(x...)  LOG_PRINTF(3, x)
#ifdef NOFRENDO_DEBUG
#define MESSAGE_DEBUG(x, ...) LOG_PRINTF(4, "> %s: " x, __func__, ## __VA_ARGS__)
#else
#define MESSAGE_DEBUG(x...)
#endif
#define MESSAGE_TRACE(x, ...) LOG_PRINTF(4, "~ %s: " x, __func__, ## __VA_ARGS__)

#undef MIN
#define MIN(a, b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b);_a < _b ? _a : _b; })
#undef MAX
#define MAX(a, b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b);_a > _b ? _a : _b; })

#define ASSERT(expr) while (!(expr)) { LOG_PRINTF(1, "ASSERTION FAILED IN %s: " #expr "\n", __func__); abort(); }
#define UNUSED(x) (void)x
