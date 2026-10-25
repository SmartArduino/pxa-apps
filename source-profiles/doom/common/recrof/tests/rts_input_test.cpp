#include "port.h"
#include "controller.h"
#include "wwkeyboard.h"
#include <cassert>
#include <cstdio>
std::vector<RecordedInput> recorded_input;
static int pointer_x = 160, pointer_y = 120, video_x, video_y;
extern "C" int port_mouse_x() { return pointer_x; }
extern "C" int port_mouse_y() { return pointer_y; }
extern "C" void port_sleep(unsigned) {}
extern "C" int port_control_key_at(int x, int y, int) {
  return x > 280 && y < 60 ? 202 : 0;
}
extern "C" void videoSetMouse(float x, float y) { video_x = x; video_y = y; }
extern "C" void videoGetMouse(int *x, int *y) { *x = video_x; *y = video_y; }
extern WWKeyboardClass *CreateWWKeyboardClass();
extern bool Input_Take_Drag_Scroll(int &, int &);
int main() {
  auto *keyboard = CreateWWKeyboardClass();
  keyboard->Fill_Buffer_From_System();
  pointer_x = 300; pointer_y = 35; // Touch-only overlay must not hijack pad clicks.
  int x, y;
  port_game_move_cursor(20, 30, &x, &y);
  assert(x == 180 && y == 150);
  port_game_key(PORT_PAD_LEFT_CLICK, 1);
  port_game_key(202, 1); // Concurrent touch RMB.
  keyboard->Fill_Buffer_From_System();
  assert(recorded_input.size() == 2);
  assert(recorded_input[0].key == KN_LMOUSE && recorded_input[0].x == 180);
  assert(recorded_input[1].key == KN_RMOUSE && recorded_input[1].y == 150);
  port_game_key(202, 0);
  port_game_key(PORT_PAD_LEFT_CLICK, 0);
  keyboard->Fill_Buffer_From_System();
  assert(recorded_input.size() == 4 && recorded_input[3].released);
  recorded_input.clear();
  pointer_x = 180; pointer_y = 150;
  port_game_key(PORT_PAD_LEFT_CLICK, 1);
  port_game_key(200, 1); // Touch and pad hold the same mouse button.
  port_game_key(PORT_PAD_LEFT_CLICK, 0);
  keyboard->Fill_Buffer_From_System();
  assert(recorded_input.size() == 1 && !recorded_input[0].released);
  port_game_move_cursor(30, -10, &x, &y);
  pointer_x = x; pointer_y = y;
  port_game_key(200, 0);
  keyboard->Fill_Buffer_From_System();
  assert(recorded_input.size() == 2 && recorded_input[1].released);
  assert(recorded_input[1].x == 210 && recorded_input[1].y == 140);
  port_game_move_cursor(999, -999, &x, &y);
  assert(x == 319 && y == 0);
  port_game_scroll(5, -3);
  assert(Input_Take_Drag_Scroll(x, y) && x == -5 && y == 3);
  assert(!Input_Take_Drag_Scroll(x, y));
  port_game_scroll(9, 9); port_game_scroll(0, 0);
  assert(!Input_Take_Drag_Scroll(x, y)); // Lifecycle reset cancels queued scroll.
  delete keyboard;
  puts("RTS input: cursor, overlay, drag, mixed sources and scroll passed");
}
