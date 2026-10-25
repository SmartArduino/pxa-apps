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
      if (b & PXA_CONTROLLER_R) keys[200] = 1;
      if (b & PXA_CONTROLLER_L) keys[' '] = 1;
      if (b & PXA_CONTROLLER_SELECT) keys['e'] = 1;
#ifdef PORT_DOOM_CONTROLLER
      /* Classic DOOM has neither jump nor vertical look. L uses doors;
       * reserve the remaining face buttons for weapons and automap. */
      if (b & PXA_CONTROLLER_X) keys['q'] = 1;
      if (b & PXA_CONTROLLER_B) keys[9] = 1;
#endif
      if (b & PXA_CONTROLLER_LEFT) keys['a'] = 1;
      if (b & PXA_CONTROLLER_RIGHT) keys['d'] = 1;
    }
    if (pad->menu && (b & PXA_CONTROLLER_SELECT)) keys[13] = 1;
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
  const uint32_t motion_buttons = rts ? PXA_CONTROLLER_LEFT | PXA_CONTROLLER_RIGHT |
      PXA_CONTROLLER_UP | PXA_CONTROLLER_DOWN | PXA_CONTROLLER_R :
      PXA_CONTROLLER_A | PXA_CONTROLLER_B | PXA_CONTROLLER_X | PXA_CONTROLLER_Y;
  const uint32_t motion_changed = (buttons ^ pad->buttons) & motion_buttons;
  pad->menu = !!menu;
  pad->initialized = 1;
  pad->buttons = buttons;
  pad->blocked &= buttons;
  /* Shooting and jumping must not interrupt a held turn. */
  if (motion_changed) {
    pad->tick_us = 0;
    pad->remainder_x = pad->remainder_y = 0;
  }
  map_keys(pad, rts, keys);
  return 1;
}

port_controller_motion_t port_controller_step(port_controller_t *pad,
    uint64_t now_us, int rts) {
  port_controller_motion_t motion = {0};
  const uint64_t previous = pad->tick_us;
  pad->tick_us = now_us;
  if (!previous || now_us < previous) return motion;
  return port_controller_advance(pad, now_us - previous, rts);
}

port_controller_motion_t port_controller_advance(port_controller_t *pad,
    uint64_t elapsed, int rts) {
  port_controller_motion_t motion = {0};
  /* Bound stalls without discarding ordinary slow-frame input. */
  const uint64_t maximum_elapsed = rts ? 64000 : 250000;
  if (elapsed > maximum_elapsed) elapsed = maximum_elapsed;
  const uint32_t b = pad->buttons & ~pad->blocked;
  int x = rts ? !!(b & PXA_CONTROLLER_RIGHT) - !!(b & PXA_CONTROLLER_LEFT) :
      !!(b & PXA_CONTROLLER_A) - !!(b & PXA_CONTROLLER_Y);
  int y = rts ? !!(b & PXA_CONTROLLER_DOWN) - !!(b & PXA_CONTROLLER_UP) :
      !!(b & PXA_CONTROLLER_B) - !!(b & PXA_CONTROLLER_X);
  if (!rts && pad->menu) x = y = 0;
  const int rate = rts ? ((b & PXA_CONTROLLER_R) ? 240 : 120) : 600;
  const int pitch_rate = rts ? rate : 300;
  int32_t dx = (int32_t)elapsed * rate * x + (x ? pad->remainder_x : 0);
  int32_t dy = (int32_t)elapsed * pitch_rate * y + (y ? pad->remainder_y : 0);
  motion.x = dx / 1000000;
  motion.y = dy / 1000000;
  pad->remainder_x = dx % 1000000;
  pad->remainder_y = dy % 1000000;
  motion.scroll = rts && (b & PXA_CONTROLLER_L);
  return motion;
}
