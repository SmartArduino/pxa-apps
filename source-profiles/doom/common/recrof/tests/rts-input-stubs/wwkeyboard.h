#pragma once
#include <vector>
using KeyASCIIType = int;
enum { KN_LMOUSE = 1, KN_RMOUSE = 2, KA_NONE = 0,
       WWKEY_RLS_BIT = 0x100, WWKEY_SHIFT_BIT = 0x200 };
struct RecordedInput { int key, x, y, released; };
extern std::vector<RecordedInput> recorded_input;
class WWKeyboardClass {
public:
  virtual ~WWKeyboardClass() = default;
  virtual void Fill_Buffer_From_System() = 0;
  virtual bool Is_Gamepad_Active() = 0;
  virtual bool Is_Analog_Scroll_Active() = 0;
  virtual unsigned char Get_Scroll_Direction() = 0;
  virtual KeyASCIIType To_ASCII(unsigned short) = 0;
  bool Is_Buffer_Full() const { return false; }
  void Put_Mouse_Message(int key, int x, int y, int released) {
    recorded_input.push_back({key, x, y, released});
  }
  void Put_Key_Message(int key, int released) {
    recorded_input.push_back({key, 0, 0, released});
  }
};
