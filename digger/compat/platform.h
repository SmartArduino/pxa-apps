/* SPDX-License-Identifier: MIT. Host-only declarations for the local PXA port. */
#ifndef DIGGER_PXA_PLATFORM_H
#define DIGGER_PXA_PLATFORM_H
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
void strupr(char *);
extern int16_t sound_device;
void doscreenupdate(bool);
void digger_port_draw(void);
uint64_t digger_port_time_us(void);
void digger_port_delay_us(uint32_t);
#endif

