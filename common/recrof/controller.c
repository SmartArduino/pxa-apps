#include "controller.h"
#include "pxa_ui.h"
#include <string.h>

static void map_keys(const port_controller_t *pad, int rts, uint8_t keys[256]) {
  const uint32_t b = pad->buttons & ~pad->blocked;
  memset(keys, 0, 256);
  if (rts) {
    if (b & PXA_CONTROLLER_A) keys[PORT_PAD_LEFT_CLICK] = 1;
    if (b & PXA_CONTROLLER_B) keys[PORT_PAD_RIGHT_CLICK] = 1;
    if (b & (PXA_CONTROLLER_X | PXA_CONTROLLER_SELECT)) keys[13] = 1;
    if (b & PXA_CONTROLLER_Y) keys['h'] = 1;
  } else {
    if (b & PXA_CONTROLLER_UP) keys[128] = 1;
    if (b & PXA_CONTROLLER_DOWN) keys[129] = 1;
    if (pad->menu) {
      if (b & PXA_CONTROLLER_LEFT) keys[130] = 1;
      if (b & PXA_CONTROLLER_RIGHT) keys[131] = 1;
      if (b & PXA_CONTROLLER_A) keys[13] = 1;
      if (b & PXA_CONTROLLER_B) keys[27] = 1;
    } else {
      if (b & PXA_CONTROLLER_A) keys[200] = 1;
      if (b & PXA_CONTROLLER_B) keys[' '] = 1;
      if (b & PXA_CONTROLLER_X) keys['e'] = 1;
      if (b & PXA_CONTROLLER_Y) keys['q'] = 1;
      if (b & PXA_CONTROLLER_L) keys['a'] = 1;
      if (b & PXA_CONTROLLER_R) keys['d'] = 1;
    }
    if (b & PXA_CONTROLLER_SELECT) keys[13] = 1;
  }
  if (b & PXA_CONTROLLER_START) keys[27] = 1;
}

int port_controller_menu(port_controller_t *pad, int menu, int rts,
    uint8_t keys[256]) {
  menu = !!menu;
  if (!pad->initialized || pad->menu == menu) return 0;
  pad->menu = menu;
  pad->blocked |= pad->buttons;
  pad->remainder_x = pad->remainder_y = 0;
  map_keys(pad, rts, keys);
  return 1;
}

int port_controller_state(port_controller_t *pad, unsigned controller,
    int connected, uint32_t buttons, int menu, int rts, uint8_t keys[256]) {
  if (controller) return 0;
  int changed = port_controller_menu(pad, menu, rts, keys);
  buttons = connected ? buttons & PXA_CONTROLLER_BUTTON_MASK : 0;
  if (pad->initialized && buttons == pad->buttons) return changed;
  pad->menu = !!menu;
  pad->initialized = 1;
  pad->buttons = buttons;
  pad->blocked &= buttons;
  /* Never integrate time from before a newly pressed direction. */
  pad->tick_us = 0;
  pad->remainder_x = pad->remainder_y = 0;
  map_keys(pad, rts, keys);
  return 1;
}

port_controller_motion_t port_controller_step(port_controller_t *pad,
    uint64_t now_us, int rts) {
  port_controller_motion_t motion = {0};
  const uint64_t previous = pad->tick_us;
  pad->tick_us = now_us;
  if (!previous || now_us < previous) return motion;
  uint64_t elapsed = now_us - previous;
  if (elapsed > 64000) elapsed = 64000;
  const uint32_t b = pad->buttons & ~pad->blocked;
  int x = !!(b & PXA_CONTROLLER_RIGHT) - !!(b & PXA_CONTROLLER_LEFT);
  int y = rts ? !!(b & PXA_CONTROLLER_DOWN) - !!(b & PXA_CONTROLLER_UP) : 0;
  if (!rts && pad->menu) x = 0;
  const int rate = rts ? ((b & PXA_CONTROLLER_R) ? 240 : 120) : 300;
  int32_t dx = (int32_t)elapsed * rate * x + (x ? pad->remainder_x : 0);
  int32_t dy = (int32_t)elapsed * rate * y + (y ? pad->remainder_y : 0);
  motion.x = dx / 1000000;
  motion.y = dy / 1000000;
  pad->remainder_x = dx % 1000000;
  pad->remainder_y = dy % 1000000;
  motion.scroll = rts && (b & PXA_CONTROLLER_L);
  return motion;
}
