/* SPDX-License-Identifier: MIT */
#ifndef DIGGER_PXA_HOST_H
#define DIGGER_PXA_HOST_H
#include <stdint.h>
#include <stdbool.h>
void digger_host_key(int scancode,int down);
void digger_host_release(void);
void digger_port_draw(void);
uint64_t digger_port_time_us(void);
void digger_port_delay_us(uint32_t);
const uint8_t *digger_pixels640(void);
const uint16_t *digger_palette565(void);
void digger_raster_reset(void);
void digger_sessionfs_reset(void);
#endif

