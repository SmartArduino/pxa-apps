/* Bounded, lossless 8-bit PCX decoding. GPL-3.0-or-later. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  void *context;
  uint32_t bytes;
  int (*read)(void *,uint32_t,void *,uint32_t); /* exact, 0 on failure */
} port_pcx_source_t;
typedef struct {uint16_t width,height,stride;uint32_t data_end;} port_pcx_info_t;
/* source and info must remain unchanged between these calls. No allocation. */
int port_pcx_header(const port_pcx_source_t *,port_pcx_info_t *);
int port_pcx_pixels(const port_pcx_source_t *,const port_pcx_info_t *,uint8_t *,size_t);
int port_pcx_palette(const port_pcx_source_t *,uint8_t palette[768]);
#ifdef __cplusplus
}
#endif
