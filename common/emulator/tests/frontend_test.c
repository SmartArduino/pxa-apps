/* Exercise actual PXA event codecs: separate pads, twelve buttons and menu chord. */
#include "../main.c"
#include <assert.h>
static unsigned control_calls;
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t size){(void)route;(void)token;(void)data;(void)size;++control_calls;return 0;}
static unsigned surface_ready,surface_presented;
int32_t pxa_io(uint64_t handle,uint32_t operation,uint8_t *data,uint32_t size){
 if(handle==123 && surface_ready){
  if(operation==PXA_SURFACE_IO_ACQUIRE){assert(size==4);data[0]=0;return 4;}
  if(operation==PXA_SURFACE_IO_PRESENT){assert(size==16);++surface_presented;return 16;}
 }
 return PXA_STATUS_WOULD_BLOCK;
}
static void test_pending_render(void){
 surface=123;pending_buffer=-1;render_pending=1;render_started=1;render_cost_us=80000;
 uint64_t old_frames=n_frames;
 for(unsigned n=0;n<3;++n){tick(1);assert(render_pending && n_frames==old_frames && !surface_presented);}
 frame_pixels=320*240;frame_bytes=frame_pixels*2;surface_stride=320;frames=calloc(2,frame_bytes);assert(frames);
 emu_core_defaults();emu_video_init(&video_renderer,320,240,(emu_display_rect_t){0,0,320,240});
 controls.controller_hidden=1;surface_ready=1;tick(1);
 assert(!render_pending && n_frames==old_frames && surface_presented==1);
 assert(flush_rendered_frame() && surface_presented==1);
 render_pending=1;pause_game();assert(!render_pending);
 free(frames);frames=NULL;emu_video_deinit(&video_renderer);surface=surface_ready=0;
 mode=PLAYING;controls.controller_hidden=0;
}
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
#include "library_contract.h"
static void test_save_baseline(const char *path) {
 FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long bytes=ftell(f);assert(bytes>0);rewind(f);
 uint8_t *rom=malloc((size_t)bytes);assert(rom && fread(rom,1,(size_t)bytes,f)==(size_t)bytes);fclose(f);
 assert(!emu_core_load(rom,(size_t)bytes));rom_loaded=1;saved_crc=0;
 strcpy(save_path,"test.sav");strcpy(save_tmp,"test.sav.tmp");
 size_t size=emu_core_save_size();
 if(size) {
  remember_save_baseline();assert(save_baseline_valid && save_data);uint32_t baseline=saved_crc;
  unsigned before=control_calls;start_save();assert(!save_pending && control_calls==before);
  save_baseline_valid=0;start_save();assert(save_pending);save_pending=0;
  remember_save_baseline();before=control_calls;
  uint8_t *changed=malloc(size);assert(changed && emu_core_save_export(changed,size));
  changed[size/2]^=0x5a;assert(emu_core_save_import(changed,size));free(changed);
  start_save();assert(save_pending && control_calls>before && saved_crc==baseline && pending_save_crc!=baseline);
  /* A failed/incomplete write must not acknowledge the modified progress. */
  save_pending=0;before=control_calls;start_save();assert(save_pending && control_calls>before && saved_crc==baseline);
 }
 save_pending=rom_loaded=0;free(save_data);save_data=NULL;emu_core_close();free(rom);
}
int main(int argc,char **argv){
 assert(argc==2);test_save_baseline(argv[1]);
 emu_controls_init(&controls,320,240);mode=PLAYING;background=0;release_input();
 test_pending_render();
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
 char extension[16];size_t length=strcspn(EMU_EXTENSIONS,"|");assert(length<sizeof(extension));
 memcpy(extension,EMU_EXTENSIONS,length);extension[length]=0;test_library_contract(extension);
 test_theme_contract();
 test_menu_contract();
 puts("PASS: 12 gamepad buttons, independent players, auto hide, menu chord and multi-touch");
}
