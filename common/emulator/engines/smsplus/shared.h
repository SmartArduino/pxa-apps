#ifndef _SHARED_H_
#define _SHARED_H_
#include <stdint.h>

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef uint32_t uint32;

typedef signed char int8;
typedef signed short int int16;
typedef int32_t int32;

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <limits.h>
#include <math.h>

#ifdef RETRO_GO
#include <rg_system.h>
#define LOG_PRINTF(level, x...) rg_system_log(RG_LOG_PRINTF, NULL, x)
#define crc32_le(a, b, c) rg_crc32(a, b, c)
#else
#define LOG_PRINTF(level, x...) pxa_sms_log(level, x)
void pxa_sms_log(int level,const char *format,...);
#define IRAM_ATTR
#define crc32_le(a, b, c) pxa_sms_crc(a,b,c)
uint32_t pxa_sms_crc(uint32_t,const void *,size_t);
#endif

#define MESSAGE_ERROR(x, ...) LOG_PRINTF(1, "!! %s: " x, __func__, ## __VA_ARGS__)
#define MESSAGE_WARN(x, ...)  LOG_PRINTF(2, "** %s: " x, __func__, ## __VA_ARGS__)
#define MESSAGE_INFO(x, ...)  LOG_PRINTF(3, " * %s: " x, __func__, ## __VA_ARGS__)
#define MESSAGE_DEBUG(x, ...) LOG_PRINTF(4, ">> %s: " x, __func__, ## __VA_ARGS__)

// #include "config.h"
#include "cpu/z80.h"
#include "memz80.h"
#include "loadrom.h"
#include "pio.h"
#include "render.h"
#include "sms.h"
#include "state.h"
#include "system.h"
#include "tms.h"
#include "vdp.h"
#include "sound/sn76489.h"
#include "sound/emu2413.h"
#include "sound/ym2413.h"
#include "sound/fmintf.h"
#include "sound/sound.h"

#endif /* _SHARED_H_ */

uint8_t *pxa_sms_rom_map(uint8_t *base,unsigned offset,unsigned owner);
uint32_t pxa_sms_cart_crc(const void *data,size_t size);
