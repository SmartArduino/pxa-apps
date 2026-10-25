/* Real runtime/event-port fixture; all input and drawing go through runtime.c. */
#include "port.h"
#include "controller.h"

void port_game_main(void) {
  static uint8_t pixels[320*240];
  uint16_t colors[256] = {0x001f};
  port_palette_rgb565(colors, 0);
  for (;;) {
    port_poll_input();
    port_frame(pixels, 320, 240, 320);
    port_sleep(16);
  }
}
void port_game_key(int key, int down) {
#ifdef PORT_RTS
  if ((key == PORT_PAD_LEFT_CLICK || key == PORT_PAD_RIGHT_CLICK) &&
      (port_mouse_x() != 160 || port_mouse_y() != 120)) __builtin_trap();
#endif
  port_log("INPUT FIXTURE key=%d down=%d", key, down);
}
void port_game_motion(int dx, int dy) {
  port_log("INPUT FIXTURE motion=%d,%d", dx, dy);
}
int port_game_in_menu(void) { return 0; }
void port_game_audio(void) {}
#ifdef PORT_RTS
void port_game_scroll(int dx, int dy) { (void)dx; (void)dy; }
void port_game_move_cursor(int dx, int dy, int *x, int *y) {
  static int cx = 160, cy = 120;
  cx += dx; cy += dy;
  *x = cx; *y = cy;
}
#endif
