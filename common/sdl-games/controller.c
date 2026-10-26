/* Digital PXA buttons retain SDL controller semantics in menus and gameplay.
 * GPL-3.0-or-later. */
#include "controller.h"
#include "pxa_ui.h"
#include <string.h>
int port_controller_menu(port_controller_t *p, int menu, int rts, uint8_t keys[256]) {
  (void)p; (void)menu; (void)rts; (void)keys; return 0;
}
int port_controller_state(port_controller_t *p, unsigned index, int connected,
    uint32_t buttons, int menu, int rts, uint8_t keys[256]) {
  (void)menu; (void)rts;
  if (index) return 0;
  buttons = connected ? buttons & PXA_CONTROLLER_BUTTON_MASK : 0;
  if (p->initialized && p->buttons == buttons) return 0;
  p->initialized = 1; p->buttons = buttons;
  memset(keys, 0, 256);
  for (int i = 0; i < 13; ++i) keys[224+i] = !!(buttons & (1u << i));
  return 1;
}
port_controller_motion_t port_controller_step(port_controller_t *p, uint64_t t, int rts) {
  (void)p; (void)t; (void)rts; return (port_controller_motion_t){0};
}
port_controller_motion_t port_controller_advance(port_controller_t *p, uint64_t t, int rts) {
  return port_controller_step(p,t,rts);
}
