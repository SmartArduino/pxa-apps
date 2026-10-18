#include "cnc_platform.h"
#include "port.h"
#include "wwkeyboard.h"
#include "sdl_keymap.h"
#include <string.h>
struct Event {
  int key, down, x, y;
};
static Event queue[128];
static unsigned head, tail;
static int cursor_x = 160, cursor_y = 120, primary_key = 200;
static bool right_button(int x, int y) { return x >= 280 && y < 24; }
static void push(int key, int down) {
  unsigned next = (head + 1) & 127;
  if (next != tail) {
    int x = port_mouse_x(), y = port_mouse_y();
    if (right_button(x, y)) { x = cursor_x; y = cursor_y; }
    queue[head] = {key, down, x, y};
    head = next;
  }
}
extern "C" void port_game_key(int k, int down) {
  if (k == 200) {
    if (down) primary_key = right_button(port_mouse_x(), port_mouse_y()) ? 202 : 200;
    k = primary_key;
  }
  if (k == ' ') k = 202;
  push(k, down);
}
extern "C" void port_game_motion(int x, int y) { videoMoveMouse(x, y); }
class Keyboard : public WWKeyboardClass {
public:
  void Fill_Buffer_From_System() override {
    // Legacy Get() waits by polling. Give PXA a chance to deliver the next key.
    static unsigned idle_polls;
    if(head==tail){
      if(++idle_polls==64){idle_polls=0;port_sleep(0);}
    }else idle_polls=0;
    if (!right_button(port_mouse_x(), port_mouse_y())) {
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
  x = y = 0;
  return false;
}
