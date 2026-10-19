#include "../../../duke3d/keymap.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint8_t KB_KeyDown_bit[(MAXKEYBOARDSCAN + 7) / 8];
int main(void) {
  const int pxa[] = {128, 129, 130, 131, 'w', 's'};
  const int engine[] = {sc_UpArrow, sc_DownArrow, sc_LeftArrow, sc_RightArrow,
                         sc_UpArrow, sc_DownArrow};
  for (int menu = 0; menu <= 1; ++menu) {
    for (unsigned i = 0; i < sizeof(pxa)/sizeof(pxa[0]); ++i) {
      memset(KB_KeyDown_bit, 0, sizeof(KB_KeyDown_bit));
      int code = port_duke_scancode(pxa[i], menu);
      // Consume the adapter's result with the engine's real keyboard state
      // accessor. Raw PC/keypad codes do not set these extended arrow bits.
      set_KB_KeyDown(code, 1);
      assert(KB_KeyPressed(engine[i]));
      assert(!KB_KeyPressed(sc_kpad_8) && !KB_KeyPressed(sc_kpad_2));
      set_KB_KeyDown(code, 0);
      assert(!KB_KeyPressed(engine[i]));
    }
  }
  assert(port_duke_scancode(200, 0) == sc_LeftControl);
  assert(port_duke_scancode(200, 1) == sc_Enter);
  assert(port_duke_scancode('a', 0) == sc_Q);
  assert(port_duke_scancode('a', 1) == sc_LeftArrow);
  assert(port_duke_scancode('d', 0) == sc_E);
  assert(port_duke_scancode('d', 1) == sc_RightArrow);
  puts("Duke: game/menu directions reach engine extended-arrow state");
}
