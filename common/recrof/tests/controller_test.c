#include "controller.h"
#include "pxa_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void timing(unsigned tick) {
  port_controller_t pad = {0}; uint8_t keys[256];
  port_controller_state(&pad, 0, 1, PXA_CONTROLLER_RIGHT, 0, 0, keys);
  int total = 0;
  port_controller_step(&pad, 1000000, 0);
  for (unsigned us = tick; us <= 960000; us += tick) {
    /* GameKit refreshes complete states every 50ms: these are not edges. */
    assert(!port_controller_state(&pad, 0, 1, PXA_CONTROLLER_RIGHT, 0, 0, keys));
    total += port_controller_step(&pad, 1000000 + us, 0).x;
  }
  assert(total == 288);
}
int main(void) {
  port_controller_t pad = {0}; uint8_t keys[256] = {0};
  uint32_t held = PXA_CONTROLLER_UP | PXA_CONTROLLER_LEFT | PXA_CONTROLLER_A |
      PXA_CONTROLLER_B | PXA_CONTROLLER_X | PXA_CONTROLLER_Y | PXA_CONTROLLER_L;
  assert(port_controller_state(&pad, 0, 1, held, 0, 0, keys));
  assert(keys[128] && keys[200] && keys[' '] && keys['e'] && keys['q'] && keys['a']);
  assert(!keys[130]); /* Left turns; L strafes. */
  port_controller_step(&pad, 1000000, 0);
  port_controller_motion_t m = port_controller_step(&pad, 1016000, 0);
  assert(m.x == -4 && m.y == 0);
  assert(!port_controller_state(&pad, 1, 0, 0, 0, 0, keys));
  assert(keys[200]); /* A second pad cannot release the primary pad. */
  assert(port_controller_menu(&pad, 1, 0, keys));
  for (int k = 0; k < 256; ++k) assert(!keys[k]);
  assert(!port_controller_step(&pad, 1032000, 0).x);
  port_controller_state(&pad, 0, 1, 0, 1, 0, keys);
  port_controller_state(&pad, 0, 1, PXA_CONTROLLER_A | PXA_CONTROLLER_LEFT,
      1, 0, keys);
  assert(keys[13] && keys[130] && !keys[200]);
  port_controller_state(&pad, 0, 1, PXA_CONTROLLER_B, 1, 0, keys);
  assert(keys[27] && !keys[13]);
  port_controller_state(&pad, 0, 0, held, 1, 0, keys);
  for (int k = 0; k < 256; ++k) assert(!keys[k]);
  timing(4000); timing(8000); timing(16000); timing(32000); timing(64000);

  memset(&pad, 0, sizeof(pad));
  held = PXA_CONTROLLER_RIGHT | PXA_CONTROLLER_DOWN | PXA_CONTROLLER_A;
  port_controller_state(&pad, 0, 1, held, 0, 1, keys);
  assert(keys[PORT_PAD_LEFT_CLICK] && !keys[131] && !keys[129]);
  port_controller_step(&pad, 1000000, 1);
  m = port_controller_step(&pad, 1050000, 1);
  assert(m.x == 6 && m.y == 6 && !m.scroll);
  port_controller_state(&pad, 0, 1, held | PXA_CONTROLLER_L | PXA_CONTROLLER_R,
      0, 1, keys);
  port_controller_step(&pad, 1060000, 1);
  m = port_controller_step(&pad, 1110000, 1);
  assert(m.x == 12 && m.y == 12 && m.scroll);
  m = port_controller_step(&pad, 9000000, 1);
  assert(m.x <= 16 && m.y <= 16); /* A stall cannot fling the cursor. */
  port_controller_state(&pad, 0, 1, PXA_CONTROLLER_RIGHT | PXA_CONTROLLER_LEFT,
      0, 1, keys);
  port_controller_step(&pad, 9100000, 1);
  m = port_controller_step(&pad, 9120000, 1);
  assert(!m.x && !m.y);
  port_controller_state(&pad, 0, 1, PXA_CONTROLLER_B | PXA_CONTROLLER_X,
      0, 1, keys);
  assert(keys[PORT_PAD_RIGHT_CLICK] && keys[13]);
  puts("controller: menu, movement, refresh, disconnect, RTS and timing passed");
}
