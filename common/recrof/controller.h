/* Digital PXA controller policy shared by the five engine ports. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { PORT_PAD_LEFT_CLICK = 201, PORT_PAD_RIGHT_CLICK = 203 };
typedef struct {
  uint32_t buttons, blocked;
  uint64_t tick_us;
  int32_t remainder_x, remainder_y;
  uint8_t menu, initialized;
} port_controller_t;
typedef struct { int x, y, scroll; } port_controller_motion_t;

/* Controller zero owns this single-player seat. Other controllers are ignored.
 * Menu transitions release actions and require held buttons to be released. */
int port_controller_state(port_controller_t *pad, unsigned controller,
    int connected, uint32_t buttons, int menu, int rts, uint8_t keys[256]);
int port_controller_menu(port_controller_t *pad, int menu, int rts,
    uint8_t keys[256]);
port_controller_motion_t port_controller_step(port_controller_t *pad,
    uint64_t now_us, int rts);
/* Sample once per simulation update. Fixed-step engines pass the duration of
 * that update even when several updates run between two presented frames. */
port_controller_motion_t port_controller_advance(port_controller_t *pad,
    uint64_t elapsed_us, int rts);
#ifdef __cplusplus
}
#endif
