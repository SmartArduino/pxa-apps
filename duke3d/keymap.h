/* PXA keys -> Duke's internal keyboard codes, including extended arrows.
 * KB_KeyEvent's ESP path consumes these directly, without PC E0 translation. */
#pragma once
#include "vendor/Game/src/keyboard.h"
static inline int port_duke_scancode(int key, int menu) {
  switch (key) {
  case 27: return sc_Escape;
  case 13: return sc_Enter;
  case 128: case 'w': return sc_UpArrow;
  case 129: case 's': return sc_DownArrow;
  case 130: return sc_LeftArrow;
  case 131: return sc_RightArrow;
  case 'a': return menu ? sc_LeftArrow : sc_Q;
  case 'd': return menu ? sc_RightArrow : sc_E;
  case ' ': return menu ? sc_Enter : sc_X;
  case 'e': return sc_Space;
  case 'q': return sc_P;
  case 200: return menu ? sc_Enter : sc_LeftControl;
  default: return sc_None;
  }
}
