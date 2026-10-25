/* Distribute batched relative touch input across engine updates. */
#pragma once
#include <stdint.h>

typedef struct { float x, y; } port_pointer_delta_t;
typedef struct { float x, y; } port_pointer_motion_t;

static inline void port_pointer_reset(port_pointer_motion_t *p) {
  p->x = p->y = 0;
}
static inline void port_pointer_push(port_pointer_motion_t *p, int x, int y) {
  p->x += x;
  p->y += y;
}
static inline port_pointer_delta_t port_pointer_sample(port_pointer_motion_t *p,
    uint32_t elapsed_us) {
  port_pointer_delta_t out = {0};
  if (!elapsed_us) return out;
  if (elapsed_us > 250000) elapsed_us = 250000;
  const float gain = (float)elapsed_us / ((float)elapsed_us + 18000.0f);
  out.x = p->x * gain;
  out.y = p->y * gain;
  p->x -= out.x;
  p->y -= out.y;
  /* Finish subpixel tails exactly rather than retaining tiny denormals. */
  if (p->x > -0.00390625f && p->x < 0.00390625f) {
    out.x += p->x;
    p->x = 0;
  }
  if (p->y > -0.00390625f && p->y < 0.00390625f) {
    out.y += p->y;
    p->y = 0;
  }
  return out;
}
