#pragma once
#include <stdint.h>
typedef struct {
  uint32_t offset, size;
  uint8_t *bytes;
} duke_transient_region_t;
void duke_transient_init(uint32_t space, const duke_transient_region_t *, unsigned);
void duke_transient_read(uint32_t, void *, uint32_t);
void duke_transient_write(uint32_t, const void *, uint32_t, int);
