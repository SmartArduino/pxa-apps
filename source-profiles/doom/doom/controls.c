// SPDX-License-Identifier: GPL-3.0-or-later
// Keep shared safe-area placement and pixels; adapt button roles to DOOM.
#include "port.h"
#define port_control_key_at shared_control_key_at
#define port_draw_controls shared_draw_controls
#include "../common/recrof/framebuffer.c"
#undef port_control_key_at
#undef port_draw_controls
static void doom_controls(void) {
    int menu=port_game_in_menu();
    controls[1].label=menu?"UP":"WPN";
    controls[1].key=menu?128:'q';
    controls[2].label=menu?"BACK":"USE";
    controls[2].key=menu?27:' ';
    controls[3].label=menu?"DOWN":"MAP";
    controls[3].key=menu?129:9;
    controls[4].label=menu?"OK":"FIRE";
    controls[4].key=menu?13:200;
}
int port_control_key_at(int x,int y,int rts) {doom_controls();return shared_control_key_at(x,y,rts);}
void port_draw_controls(uint16_t *dest,int w,int h,int rts) {doom_controls();shared_draw_controls(dest,w,h,rts);}
