#include "pointer_motion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void conserved(unsigned step) {
  port_pointer_motion_t p = {0};
  float x = 0, y = 0;
  const int input[] = {7, 2, -3, 19, -11, 5};
  for (unsigned i = 0; i < sizeof(input)/sizeof(*input); ++i) {
    port_pointer_push(&p, input[i], -input[i]);
    port_pointer_delta_t d = port_pointer_sample(&p, step);
    x += d.x; y += d.y;
  }
  for (unsigned i = 0; i < 200; ++i) {
    port_pointer_delta_t d = port_pointer_sample(&p, step);
    x += d.x; y += d.y;
  }
  assert(fabsf(x - 19) < 0.0001f && fabsf(y + 19) < 0.0001f);
  assert(p.x == 0 && p.y == 0);
}
int main(void) {
  port_pointer_motion_t p = {0};
  port_pointer_push(&p, 24, -12);
  port_pointer_delta_t d = port_pointer_sample(&p, 0);
  assert(!d.x && !d.y && p.x == 24);
  d = port_pointer_sample(&p, 18000);
  assert(d.x == 12 && d.y == -6);
  port_pointer_reset(&p);
  d = port_pointer_sample(&p, 33333);
  assert(!d.x && !d.y); /* A menu/controller transition cannot retain a turn. */
  const int burst[] = {0, 0, 20, 0, 0, 20, 0, 0, 20, 0, 0};
  float previous = 0, largest = 0;
  for (unsigned i = 0; i < sizeof(burst)/sizeof(*burst); ++i) {
    port_pointer_push(&p, burst[i], 0);
    d = port_pointer_sample(&p, 16000);
    const float jump = fabsf(d.x - previous);
    if (jump > largest) largest = jump;
    previous = d.x;
  }
  assert(largest < 12); /* Unfiltered burst-to-idle jumps are 20 pixels. */
  conserved(4000); conserved(8000); conserved(16000);
  conserved(33333); conserved(40000); conserved(80000); conserved(250000);
  puts("Pointer: batched motion smoothed, distance conserved, subpixels and reset passed");
}
