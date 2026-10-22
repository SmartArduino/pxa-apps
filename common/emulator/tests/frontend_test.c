/* Exercise actual PXA event codecs: separate pads, twelve buttons and menu chord. */
#include "../main.c"
#include <assert.h>
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t size){(void)route;(void)token;(void)data;(void)size;return 0;}
int32_t pxa_io(uint64_t handle,uint32_t operation,uint8_t *data,uint32_t size){(void)handle;(void)operation;(void)data;(void)size;return PXA_STATUS_WOULD_BLOCK;}
static void controller(unsigned player,uint32_t buttons,unsigned connected){
 uint8_t data[32]={0},bytes[256];uint32_t length;
 pxa_store_u32(data,1);pxa_store_u32(data+4,1);pxa_store_u32(data+8,1);
 pxa_store_u16(data+12,PXA_UI_EVENT_CONTROLLER_STATE_KIND);
 data[24]=player;data[25]=connected;pxa_store_u32(data+28,buttons);
 assert(pxa_build_message(bytes,sizeof(bytes),PXA_SERVICE_UI,PXA_UI_EVENT,0,data,sizeof(data),&length));
 assert(pxa_app_on_event(bytes,length)==PXA_EVENT_HANDLED);
}
static void menu_test_reset(void) {
 release_input();memset(controller_previous,0,sizeof(controller_previous));
 memset(controller_raw_previous,0,sizeof(controller_raw_previous));
 memset(controller_connected,0,sizeof(controller_connected));memset(menu_chord,0,sizeof(menu_chord));
 menu_repeat_buttons=0;menu_repeat_at=0;menu_repeat_controller=0;
 pxa_ui_environment_t env={0};menu_environment(&env);
}
#define menu_test_controller controller
#include "menu_contract.h"
#include "theme_contract.h"
int main(void){
 emu_controls_init(&controls,320,240);mode=PLAYING;background=0;release_input();
 const uint32_t host[]={PXA_CONTROLLER_UP,PXA_CONTROLLER_DOWN,PXA_CONTROLLER_LEFT,PXA_CONTROLLER_RIGHT,
  PXA_CONTROLLER_A,PXA_CONTROLLER_B,PXA_CONTROLLER_X,PXA_CONTROLLER_Y,PXA_CONTROLLER_L,PXA_CONTROLLER_R,PXA_CONTROLLER_SELECT,PXA_CONTROLLER_START};
 const uint16_t game[]={EMU_PAD_UP,EMU_PAD_DOWN,EMU_PAD_LEFT,EMU_PAD_RIGHT,EMU_PAD_A,EMU_PAD_B,EMU_PAD_X,EMU_PAD_Y,EMU_PAD_L,EMU_PAD_R,EMU_PAD_SELECT,EMU_PAD_START};
 for(unsigned i=0;i<12;++i){controller(0,host[i],1);assert(pad[0]==game[i] && !pad[1] && mode==PLAYING);}
 assert(controls.controller_hidden);
 controller(1,PXA_CONTROLLER_B|PXA_CONTROLLER_R,1);assert(pad[1]==(EMU_PAD_B|EMU_PAD_R) && pad[0]==EMU_PAD_START);
 controller(0,0,0);assert(!pad[0] && pad[1] && controls.controller_hidden);
 controller(1,0,0);assert(!pad[1] && !controls.controller_hidden);
 controller(0,PXA_CONTROLLER_START|PXA_CONTROLLER_SELECT,1);assert(mode==PAUSED && !pad[0]);
 controller(0,PXA_CONTROLLER_START|PXA_CONTROLLER_SELECT,1);assert(mode==PAUSED);
 controller(0,PXA_CONTROLLER_START,1);assert(!pad[0] && mode==PAUSED);
 controller(0,0,1);controller(0,PXA_CONTROLLER_START|PXA_CONTROLLER_SELECT,1);assert(mode==PLAYING && !pad[0]);
 controller(0,0,0);
 pxa_ui_pointer_data_t p={.phase=PXA_POINTER_DOWN,.x=160,.y=50};pointer(&p);assert(mode==PLAYING && !touches[0]);
 emu_control_button_t *right=&controls.buttons[EMU_CONTROL_RIGHT],*a=&controls.buttons[EMU_CONTROL_A];
 p.x=right->x+right->w/2;p.y=right->y+right->h/2;pointer(&p);assert(touches[0]==EMU_PAD_RIGHT);
 p.pointer_id=1;p.x=a->x+a->w/2;p.y=a->y+a->h/2;pointer(&p);assert(touches[0]==EMU_PAD_RIGHT && touches[1]==EMU_PAD_A);
 p.phase=PXA_POINTER_UP;pointer(&p);assert(touches[0]==EMU_PAD_RIGHT && !touches[1]);
 p.pointer_id=0;pointer(&p);assert(!touches[0]);
 test_theme_contract();
 test_menu_contract();
 puts("PASS: 12 gamepad buttons, independent players, auto hide, menu chord and multi-touch");
}
