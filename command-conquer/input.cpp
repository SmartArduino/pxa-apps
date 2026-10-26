#include "cnc_platform.h"
#include "port.h"
#include "controller.h"
#include "wwkeyboard.h"
#include "sdl_keymap.h"
#include <string.h>
struct Event {
  int key, down, x, y;
};
static Event queue[128];
static unsigned head, tail;
static int cursor_x = 160, cursor_y = 120, primary_key = 200;
static int scroll_x, scroll_y;
static unsigned mouse_sources[2];
static bool right_button(int x, int y) { return port_control_key_at(x,y,1)==202; }
static bool control_button(int x, int y) { return port_control_key_at(x,y,1)!=0; }
static void push(int key, int down, bool controller = false) {
  unsigned next = (head + 1) & 127;
  if (next != tail) {
    int x = port_mouse_x(), y = port_mouse_y();
    if (controller || control_button(x, y)) { x = cursor_x; y = cursor_y; }
    queue[head] = {key, down, x, y};
    head = next;
  }
}
extern "C" void port_game_key(int k, int down) {
  int original = k;
  bool controller = k == PORT_PAD_LEFT_CLICK || k == PORT_PAD_RIGHT_CLICK;
  if (controller) k = k == PORT_PAD_LEFT_CLICK ? 200 : 202;
  else if (k == 200) {
    if (down) primary_key = right_button(port_mouse_x(), port_mouse_y()) ? 202 : 200;
    k = primary_key;
  }
  if (k == ' ') k = 202;
  if (k == 200 || k == 202) {
    unsigned bit = 1u << (original == 200 ? 0 : original == PORT_PAD_LEFT_CLICK ? 1 :
        original == 202 ? 2 : original == PORT_PAD_RIGHT_CLICK ? 3 : 4);
    unsigned &sources = mouse_sources[k == 202];
    unsigned previous = sources;
    if (down) sources |= bit; else sources &= ~bit;
    if (!!sources != !!previous) push(k, !!sources, controller);
    return;
  }
  push(k, down);
}
extern "C" int port_game_in_menu(void) { return 0; }
extern "C" void port_game_motion(int x, int y) {
  int unused_x, unused_y;
  port_game_move_cursor(x, y, &unused_x, &unused_y);
}
extern "C" void port_game_move_cursor(int x, int y, int *out_x, int *out_y) {
  cursor_x += x; cursor_y += y;
  if (cursor_x < 0) cursor_x = 0;
  if (cursor_x > 319) cursor_x = 319;
  if (cursor_y < 0) cursor_y = 0;
  if (cursor_y > 239) cursor_y = 239;
  *out_x = cursor_x; *out_y = cursor_y;
  videoSetMouse(cursor_x, cursor_y);
}
extern "C" void port_game_scroll(int x, int y) {
  if (!x && !y) { scroll_x = scroll_y = 0; return; }
  /* Negate the drag delta: pressing right reveals the map to the right. */
  scroll_x -= x; scroll_y -= y;
  if (scroll_x < -64) scroll_x = -64;
  if (scroll_x > 64) scroll_x = 64;
  if (scroll_y < -64) scroll_y = -64;
  if (scroll_y > 64) scroll_y = 64;
}
class Keyboard : public WWKeyboardClass {
public:
  void Fill_Buffer_From_System() override {
    // Take fresh pointer/controller state before the engine tests its queue.
    // Throttle empty nonblocking polls in the legacy busy-wait menus.
    static uint64_t last_poll;
    uint64_t now=port_time_us();
    if(now-last_poll>=8000u) {port_poll_input();last_poll=port_time_us();}
    // Legacy Get() waits by polling. Give PXA a chance to deliver the next key.
    static unsigned idle_polls;
    if(head==tail){
      if(++idle_polls==64){idle_polls=0;port_sleep(0);}
    }else idle_polls=0;
    if (!control_button(port_mouse_x(), port_mouse_y())) {
      cursor_x = port_mouse_x(); cursor_y = port_mouse_y();
    }
    videoSetMouse(cursor_x, cursor_y);
    while (head != tail && !Is_Buffer_Full()) {
      Event e = queue[tail];
      tail = (tail + 1) & 127;
      if (e.key == 200 || e.key == 202) {
        videoSetMouse(e.x, e.y);
        int x, y;
        videoGetMouse(&x, &y);
        Put_Mouse_Message(e.key == 200 ? KN_LMOUSE : KN_RMOUSE, x, y, !e.down);
      } else {
        int sc = 0;
        switch (e.key) {
        case 27:
          sc = 41;
          break;
        case 13:
        case ' ':
          sc = 40;
          break;
        case 128:
          sc = 82;
          break;
        case 129:
          sc = 81;
          break;
        case 130:
          sc = 80;
          break;
        case 131:
          sc = 79;
          break;
        default:
          if (e.key >= 'a' && e.key <= 'z')
            sc = e.key - 'a' + 4;
          break;
        }
        if (sc)
          Put_Key_Message(sc, !e.down);
      }
    }
  }
  bool Is_Gamepad_Active() override { return false; }
  bool Is_Analog_Scroll_Active() override { return false; }
  unsigned char Get_Scroll_Direction() override { return 0; }
  KeyASCIIType To_ASCII(unsigned short key) override {
    if (key & WWKEY_RLS_BIT)
      return KA_NONE;
    bool shift = key & WWKEY_SHIFT_BIT;
    key &= 255;
    return key < 128 ? sdl_keymap[key + (shift ? 128 : 0)] : KA_NONE;
  }
};
WWKeyboardClass *CreateWWKeyboardClass() { return new Keyboard; }
bool Input_Take_Drag_Scroll(int &x, int &y) {
  x = scroll_x; y = scroll_y;
  scroll_x = scroll_y = 0;
  return x || y;
}
