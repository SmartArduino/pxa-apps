/* GPL-2.0-or-later: exercise the frontend through real PXA event codecs. */
#include "../main.c"
#include <assert.h>
static unsigned calls[20][8],closed,io_calls;
static uint32_t rejected_route;
static int io_result=PXA_STATUS_WOULD_BLOCK;
static const uint64_t permit=UINT64_C(0x100000001),session=UINT64_C(0x200000001);
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t length) {
    (void)token;(void)data;(void)length;
    unsigned service=route&65535,op=route>>16;
    if(service<20 && op<8)++calls[service][op];
    if(service==PXA_CORE_SERVICE && op==PXA_CORE_CLOSE_HANDLE)++closed;
    return route==rejected_route?PXA_STATUS_UNSUPPORTED:0;
}
int32_t pxa_io(uint64_t h,uint32_t op,uint8_t *data,uint32_t n) {
    (void)h;(void)op;(void)data;++io_calls;return io_result==INT32_MAX?(int32_t)n:io_result;
}
static void reset(void) {
    audio=audio_pending=permission=0;audio_requested=audio_failed=0;
    menu_focus=slot_focus=0;menu_repeat_buttons=0;menu_repeat_at=0;menu_repeat_mode=MENU;
    memset(controller_raw_previous,0,sizeof(controller_raw_previous));
    memset(controller_previous,0,sizeof(controller_previous));
    memset(controller_connected,0,sizeof(controller_connected));
    mode=PAUSED;background=0;redraw=0;rom_loaded=0;pause_top=pause_selected=0;drag_active=0;slot_phase=0;exit_requested=0;
    memset(calls,0,sizeof(calls));closed=io_calls=rejected_route=0;
    io_result=PXA_STATUS_WOULD_BLOCK;audio_dropped=0;nes_audio_fifo_clear(&audio_fifo);
}
static void send_event(unsigned service,unsigned op,uint64_t token,const uint8_t *p,unsigned n) {
    uint8_t packet[160];uint32_t bytes;
    assert(pxa_build_message(packet,sizeof(packet),service,op,token,p,n,&bytes));
    assert(pxa_app_on_event(packet,bytes)==PXA_EVENT_HANDLED);
}
static void status(unsigned service,unsigned op,uint64_t token,int code) {
    uint8_t p[4];pxa_store_u32(p,code);send_event(service,op,token,p,sizeof(p));
}
static void granted(void) {
    uint8_t p[12]={0};pxa_store_u64(p+4,permit);
    send_event(PXA_PERMISSION_SERVICE,PXA_PERMISSION_ACQUIRE,T_AUDIO_PERMISSION,p,sizeof(p));
}
static void opened(unsigned rate,unsigned channels) {
    uint8_t p[35]={0};unsigned offset=4;size_t n;
    uint8_t value[8];pxa_store_u64(value,session);
    assert(pxa_wire_record_encode(p+offset,sizeof(p)-offset,3,value,8,&n));offset+=n;
    pxa_store_u32(value,rate);
    assert(pxa_wire_record_encode(p+offset,sizeof(p)-offset,4,value,4,&n));offset+=n;
    value[0]=channels;
    assert(pxa_wire_record_encode(p+offset,sizeof(p)-offset,5,value,1,&n));offset+=n;
    pxa_store_u16(value,20);
    assert(pxa_wire_record_encode(p+offset,sizeof(p)-offset,6,value,2,&n));offset+=n;
    assert(offset==sizeof(p));send_event(PXA_AUDIO_SERVICE,PXA_AUDIO_OPEN_SESSION,T_AUDIO_OPEN,p,sizeof(p));
}
static void tap_sound(void) {
    pxa_ui_pointer_data_t p={.phase=PXA_POINTER_DOWN,.x=150,.y=PAUSE_TOP+3*PAUSE_PITCH+11};pointer(&p);p.phase=PXA_POINTER_UP;pointer(&p);
}
static void enable(void) {
    tap_sound();granted();opened(16000,1);
    status(PXA_AUDIO_SERVICE,PXA_AUDIO_COMMIT_GRAPH,T_AUDIO_GAIN,0);
    assert(audio==session && permission==permit && audio_requested && !audio_failed);
}
static void failed(void) {
    assert(audio_failed && !audio_requested && !audio && !audio_pending && !permission && redraw);
}
static void locale_event(const char *locale) {
    uint8_t p[80];size_t n;
    assert(pxa_wire_record_encode(p,sizeof(p),PXA_SYSTEM_CONFIGURATION_LOCALE,
        (const uint8_t*)locale,strlen(locale),&n));
    send_event(PXA_SERVICE_SYSTEM,PXA_SYSTEM_CONFIGURATION_EVENT,0,p,n);
}
static void test_i18n(void) {
    uint8_t config[100],records[80];size_t n,m;
    assert(pxa_wire_record_encode(records,sizeof(records),PXA_SYSTEM_CONFIGURATION_LOCALE,
        (const uint8_t*)"en-US",5,&n));
    assert(pxa_wire_record_encode(config,sizeof(config),PXA_SYSTEM_CONFIG_ENVIRONMENT,records,n,&m));
    assert(!pxa_app_start(config,m));
    assert(!strcmp(UI_TITLE,"NES Emulator") && !strcmp(controls.buttons[6].label,"MENU"));
    char formatted[112];format_number(PXA_MSG_UI_DISPLAY,"frames",2,formatted,sizeof(formatted));
    assert(!strcmp(formatted,"Frame interval: 2"));
    mode=PLAYING;controls.alpha=5;controls.controller_hidden=1;pad[0]=64;
    locale_event("zh-CN");
    assert(!strcmp(UI_TITLE,"NES模拟器") && !strcmp(controls.buttons[6].label,"MENU"));
    assert(mode==PLAYING && controls.alpha==5 && controls.controller_hidden && pad[0]==64);
    format_number(PXA_MSG_UI_KEYS_ALPHA,"opacity",63,formatted,sizeof(formatted));
    assert(!strcmp(formatted,"按键：不透明度 63%"));
    video_renderer.valid[0]=video_renderer.valid[1]=1;locale_event("zh-CN");
    assert(video_renderer.valid[0] && video_renderer.valid[1]);
    locale_event("zh-Hans-CN");assert(!strcmp(UI_MARIO,"超级马里奥"));
    locale_event("fr-FR");assert(!strcmp(UI_MARIO,"Super Mario Bros."));
    locale_event("en-US");assert(!strcmp(controls.buttons[5].label,"START"));
    assert(!video_renderer.valid[0] && !video_renderer.valid[1]);
    const char *labels[]={UI_TITLE,UI_PAUSED,UI_SAVING,UI_SAVE_ERROR,UI_RESUME,UI_LIBRARY,
        UI_SOUND_ON,UI_SOUND_OFF,UI_SOUND_STARTING,UI_SOUND_FAILED,UI_KEYS_AUTO,UI_KEYS_OFF};
    for(unsigned i=0;i<sizeof(labels)/sizeof(labels[0]);++i)assert(nes_ui_width(labels[i])<=216);
    puts("PASS: system start locale, live locale events, Chinese parent fallback, English fallback, named arguments and preserved game/input settings");
}
static void controller(uint32_t buttons,unsigned player) {
    uint8_t event[32]={0};
    pxa_store_u32(event,1);pxa_store_u32(event+4,1);pxa_store_u32(event+8,1);
    pxa_store_u16(event+12,PXA_UI_EVENT_CONTROLLER_STATE_KIND);
    event[24]=player;event[25]=1;pxa_store_u32(event+28,buttons);
    send_event(PXA_UI_SERVICE,PXA_UI_EVENT,0,event,sizeof(event));
}
static void press(uint32_t buttons) {controller(buttons,0);controller(0,0);}
static void test_controller_menus(void) {
    reset();mode=MENU;count=2;selected=1;page=0;scan_pending=0;
    press(PXA_CONTROLLER_DOWN);assert(menu_focus==1 && selected==1);
    press(PXA_CONTROLLER_RIGHT);assert(menu_focus==2);
    press(PXA_CONTROLLER_LEFT);assert(menu_focus==1);
    press(PXA_CONTROLLER_UP);assert(!menu_focus && selected==1);
    press(PXA_CONTROLLER_DOWN);press(PXA_CONTROLLER_A);assert(scan_pending && mode==MENU);
    reset();mode=MENU;count=0;scan_pending=0;
    press(PXA_CONTROLLER_RIGHT);assert(menu_focus==2);
    press(PXA_CONTROLLER_A);assert(exit_requested==1);
    reset();mode=MENU;count=0;scan_pending=0;
    press(PXA_CONTROLLER_A);assert(scan_pending); /* Empty library can refresh. */
    reset();mode=MENU;count=9;selected=0;scan_pending=0;
    controller(PXA_CONTROLLER_DOWN,1);assert(!selected); /* One UI cursor. */
    controller(PXA_CONTROLLER_DOWN,0);assert(selected==1 && menu_repeat_at);
    uint64_t due=menu_repeat_at;
    menu_controller_repeat(due-1);assert(selected==1);
    menu_controller_repeat(due);assert(selected==2);
    menu_controller_repeat(due+90000);assert(selected==3);
    controller(0,0);menu_controller_repeat(due+1000000);assert(selected==3);
    press(PXA_CONTROLLER_B);assert(exit_requested==1);
    reset();mode=STATES;slot_total=0;slot_phase=0;
    press(PXA_CONTROLLER_RIGHT);assert(slot_focus==2);
    press(PXA_CONTROLLER_A);assert(mode==PAUSED);
    reset();mode=STATES;slot_total=40;slot_selected=39;slot_top=34;slot_base=32;slot_cached=8;slot_phase=0;
    press(PXA_CONTROLLER_DOWN);assert(slot_focus==1 && slot_selected==39);
    press(PXA_CONTROLLER_RIGHT);assert(slot_focus==2);
    press(PXA_CONTROLLER_UP);assert(!slot_focus && slot_selected==39);
    press(PXA_CONTROLLER_B);assert(mode==PAUSED);
    puts("PASS: controller library/footer/empty/snapshot focus, confirm/back, primary cursor and menu-only repeat");
}
static void test_scaled_library(void) {
    reset();width=800;height=480;input_scale=1;input_density_q16=65536;
    pxa_ui_environment_t full={0};menu_environment(&full);
    mode=MENU;scan_pending=0;count=10;selected=5;page=0;
    memset(controller_raw_previous,0,sizeof(controller_raw_previous));
    uint8_t event[32]={0};
    pxa_store_u32(event,1);pxa_store_u32(event+4,1);pxa_store_u32(event+8,1);
    pxa_store_u16(event+12,PXA_UI_EVENT_CONTROLLER_STATE_KIND);
    event[25]=1;pxa_store_u32(event+28,PXA_CONTROLLER_DOWN);
    send_event(PXA_UI_SERVICE,PXA_UI_EVENT,0,event,sizeof(event));
    /* Six rows fit: selection scrolls by one row, with no pagination. */
    assert(selected==6 && page==1);
    pxa_ui_pointer_data_t tap={.phase=PXA_POINTER_DOWN,.x=790,.y=410};
    pointer(&tap);assert(page==1 && mode==MENU); /* Outside footer. */
    tap.x=110;pointer(&tap);assert(page==0 && redraw && scan_pending); /* Refresh. */
    scan_pending=0;
    mode=PAUSED;audio_requested=0;
    tap.x=710;tap.y=(PAUSE_TOP+3*PAUSE_PITCH+11)*2;
    pointer(&tap);tap.phase=PXA_POINTER_UP;pointer(&tap);assert(audio_requested); /* Right edge of expanded sound row. */
    width=296;height=240;input_scale=1;input_density_q16=65536;
    memset(controller_raw_previous,0,sizeof(controller_raw_previous));
    puts("PASS: large-screen controller scrolling, footer bounds and expanded sound hit area");
}
static void test_scroll_and_exit(void) {
    reset();width=320;height=240;input_density_q16=65536;
    pxa_ui_environment_t env={0};menu_environment(&env);mode=MENU;count=100;page=0;scan_pending=0;
    pxa_ui_pointer_data_t p={.pointer_id=0,.phase=PXA_POINTER_DOWN,.x=120,.y=170};pointer(&p);
    p.phase=PXA_POINTER_MOVE;p.y=80;pointer(&p);p.phase=PXA_POINTER_UP;pointer(&p);
    assert(page==4 && mode==MENU); /* A drag never starts a game. */
    request_exit();assert(exit_requested==1);exit_tick();assert(exit_requested==2 && calls[PXA_WINDOW_SERVICE][4]==1);
    uint8_t packet[32];uint32_t n;assert(pxa_build_message(packet,sizeof(packet),PXA_WINDOW_SERVICE,PXA_WINDOW_BACK_REQUESTED,0,NULL,0,&n));
    assert(pxa_app_on_event(packet,n)==0 && !exit_requested);
    /* Pending SRAM must finish before an exit request can reach the Host. */
    reset();mode=MENU;save_pending=1;request_exit();exit_tick();assert(exit_requested==1 && !calls[PXA_WINDOW_SERVICE][4]);
    save_pending=0;exit_tick();assert(exit_requested==2);exit_requested=0;
    puts("PASS: drag without accidental launch, one-row scrolling and exit waits for SRAM");
}
static void test_system_menu(void) {
    reset();width=640;height=480;input_density_q16=131072;
    pxa_ui_environment_t env={.features=NES_UI_SIZED_TEXT,.font_scale_q16=65536};
    menu_environment(&env);assert(system_menu_supported && menu_width()==320 && menu_height()==240);
    uint8_t data[256];pxa_canvas_frame_t frame;pxa_canvas_begin(&frame,data,sizeof(data));
    menu_commands=&frame;menu_label(10,4,100,22,"上一页",0xffff,1);menu_commands=NULL;
    assert(!frame.failed && frame.records==1 && data[0]==12);
    assert(pxa_read_u32(data+4)==10 && pxa_read_u32(data+8)==4);
    assert(pxa_read_u32(data+12)==100 && pxa_read_u32(data+16)==22);
    assert(pxa_read_u16(data+24)==28 && data[26]==1); /* Native font size, dp box. */
    surface=7;surface_visible=1;unsigned layer_calls=calls[PXA_SURFACE_SERVICE][PXA_SURFACE_CONFIGURE_LAYER];
    assert(paint_system_menu() && !surface_visible);
    assert(calls[PXA_SURFACE_SERVICE][PXA_SURFACE_CONFIGURE_LAYER]==layer_calls+1);
    assert(paint_system_menu()); /* Repainting menu must not reconfigure the layer. */
    assert(calls[PXA_SURFACE_SERVICE][PXA_SURFACE_CONFIGURE_LAYER]==layer_calls+1);
    menu_initialized=1;assert(menu_dismiss() && !menu_initialized && surface_visible);
    assert(calls[PXA_SURFACE_SERVICE][PXA_SURFACE_CONFIGURE_LAYER]==layer_calls+2);
    uint32_t generation=menu_generation;
    assert(menu_dismiss() && menu_generation==generation); /* No UI calls during play. */
    assert(calls[PXA_SURFACE_SERVICE][PXA_SURFACE_CONFIGURE_LAYER]==layer_calls+2);
    surface=0;surface_visible=0;
    env.safe_insets[0]=10;env.safe_insets[2]=12;
    env.display_shape=PXA_UI_DISPLAY_SHAPE_ROUNDED_RECTANGLE;
    for(unsigned i=0;i<4;++i)env.corner_radii[i]=20;
    menu_environment(&env);
    assert(menu_area.x==12 && menu_area.y==20 && menu_area.width==616 && menu_area.height==436);
    width=height=320;input_density_q16=65536;memset(env.safe_insets,0,sizeof(env.safe_insets));
    env.display_shape=PXA_UI_DISPLAY_SHAPE_CIRCLE;menu_environment(&env);
    assert(menu_area.x==47 && menu_area.y==47 && menu_area.width==226 && menu_area.height==226);
    unsigned inset[4]={0},radius[4]={0};
    for(unsigned r=1;r<=120;++r) {
        radius[0]=r;nes_display_rect_t a=nes_display_safe_rect(320,240,1,inset,radius);
        int dx=(int)r-a.x,dy=(int)r-a.y;assert(dx*dx+dy*dy<=(int)(r*r));
    }
    inset[0]=UINT32_MAX;assert(!nes_display_safe_rect(320,240,0,inset,radius).width);
    width=296;height=240;input_density_q16=65536;memset(&env,0,sizeof(env));menu_environment(&env);
    puts("PASS: system text wire format, density applied once, native font size and one-time menu teardown");
}
int main(void) {
    /* Native canvas keeps existing menu/button physical tap targets. */
    unsigned cw=800,ch=480;assert(nes_display_canvas(&cw,&ch)==1 && cw==800 && ch==480);
    cw=296;ch=240;assert(nes_display_canvas(&cw,&ch)==1 && cw==296 && ch==240);
    cw=320;ch=240;assert(nes_display_canvas(&cw,&ch)==1 && cw==320 && ch==240);
    cw=801;ch=480;assert(!nes_display_canvas(&cw,&ch));
    reset();width=800;height=480;input_scale=1;
    pxa_ui_environment_t full={0};menu_environment(&full);
    uint16_t *small=malloc(400*240*2);assert(small);
    uint16_t *menu=(uint16_t *)frames;
    for(unsigned y=0;y<240;++y)for(unsigned x=0;x<400;++x)
        small[y*400+x]=(uint16_t)(y*400+x);
    expand_menu(menu,small);free(small);
    for(unsigned y=0;y<480;++y)for(unsigned x=0;x<800;++x)
        assert(menu[y*800+x]==(uint16_t)((y/2)*400+x/2));
    pxa_ui_pointer_data_t physical={.phase=PXA_POINTER_DOWN,.x=300,.y=(PAUSE_TOP+3*PAUSE_PITCH+11)*2};
    pointer(&physical);physical.phase=PXA_POINTER_UP;pointer(&physical);assert(audio_requested);physical.phase=PXA_POINTER_DOWN;
    reset();input_density_q16=88474; /* Korvo's 1.35x UI density. */
    pxa_ui_pointer_data_t dp=physical;
    dp.x=(int64_t)physical.x*65536/input_density_q16;
    dp.y=(int64_t)physical.y*65536/input_density_q16;
    pointer(&dp);dp.phase=PXA_POINTER_UP;pointer(&dp);assert(audio_requested);dp.phase=PXA_POINTER_DOWN;
    mode=PLAYING;nes_controls_init(&controls,width,height,colors);
    dp.pointer_id=0;dp.x=256*65536/88474;dp.y=358*65536/88474;
    pointer(&dp);assert(touches[0]==128);
    dp.pointer_id=1;dp.x=742*65536/88474;dp.y=342*65536/88474;
    pointer(&dp);assert(touches[1]==1 && touches[0]==128);
    dp.phase=PXA_POINTER_CANCEL;pointer(&dp);assert(!touches[1] && touches[0]==128);
    dp.pointer_id=0;pointer(&dp);assert(!touches[0]);
    width=296;height=240;input_scale=1;input_density_q16=65536;
    reset();test_i18n();test_scaled_library();test_system_menu();test_scroll_and_exit();test_controller_menus();
    reset();tap_sound();tap_sound();assert(audio_requested && !audio && redraw);
    assert(calls[PXA_PERMISSION_SERVICE][PXA_PERMISSION_ACQUIRE]==1);
    granted();opened(16000,1);status(PXA_AUDIO_SERVICE,PXA_AUDIO_COMMIT_GRAPH,T_AUDIO_GAIN,0);
    assert(audio==session && !audio_failed);tap_sound();
    assert(!audio && !permission && !audio_requested && !audio_failed && closed==2);
    enable();assert(audio==session); /* off -> on again */
    reset();tap_sound();status(PXA_PERMISSION_SERVICE,PXA_PERMISSION_ACQUIRE,T_AUDIO_PERMISSION,PXA_STATUS_DENIED);
    failed();enable(); /* denied -> successful retry */
    reset();rejected_route=PXA_PERMISSION_SERVICE|(PXA_PERMISSION_ACQUIRE<<16);tap_sound();failed();
    reset();tap_sound();rejected_route=PXA_AUDIO_SERVICE|(PXA_AUDIO_OPEN_SESSION<<16);granted();failed();assert(closed==1);
    reset();tap_sound();granted();status(PXA_AUDIO_SERVICE,PXA_AUDIO_OPEN_SESSION,T_AUDIO_OPEN,PXA_STATUS_BUSY);failed();assert(closed==1);
    reset();tap_sound();granted();opened(16000,2);failed();assert(closed==2);
    reset();tap_sound();granted();rejected_route=PXA_AUDIO_SERVICE|(PXA_AUDIO_COMMIT_GRAPH<<16);opened(16000,1);failed();assert(closed==2);
    reset();tap_sound();granted();opened(16000,1);status(PXA_AUDIO_SERVICE,PXA_AUDIO_COMMIT_GRAPH,T_AUDIO_GAIN,PXA_STATUS_DENIED);failed();assert(closed==2);
    reset();tap_sound();uint8_t malformed[3]={0};send_event(PXA_PERMISSION_SERVICE,PXA_PERMISSION_ACQUIRE,T_AUDIO_PERMISSION,malformed,3);failed();
    reset();enable();int16_t samples[16]={1};nes_audio_fifo_push(&audio_fifo,samples,16);drain_audio();assert(audio && audio_fifo.count==16 && io_calls==1);
    io_result=PXA_STATUS_IO_ERROR;drain_audio();failed();assert(closed==2 && !audio_fifo.count);
    reset();enable();nes_audio_fifo_push(&audio_fifo,samples,16);io_result=3;drain_audio();failed();
    reset();enable();uint8_t revoke[27];size_t n,m;
    assert(pxa_wire_record_encode(revoke,sizeof(revoke),1,(const uint8_t*)"audio.playback",14,&n));
    assert(pxa_wire_record_encode(revoke+n,sizeof(revoke)-n,2,(const uint8_t*)"media",5,&m));
    send_event(PXA_PERMISSION_SERVICE,PXA_PERMISSION_REVOKED,0,revoke,n+m);failed();assert(closed==2);
    reset();tap_sound();audio_failure("test-cancel",PXA_STATUS_CANCELLED);granted();assert(closed==1 && !permission && !audio);
    /* A consumer becoming ready during frame computation must get queued
     * samples before overflow handling discards them. */
    int16_t backlog[NES_AUDIO_FIFO_SAMPLES]={0};
    reset();enable();nes_audio_fifo_push(&audio_fifo,backlog,NES_AUDIO_FIFO_SAMPLES-6);
    io_result=INT32_MAX;enqueue_audio(samples,16);
    assert(audio && !audio_dropped && audio_fifo.count<NES_AUDIO_FIFO_SAMPLES-6);
    reset();enable();nes_audio_fifo_push(&audio_fifo,backlog,NES_AUDIO_FIFO_SAMPLES-6);
    enqueue_audio(samples,16);assert(audio && audio_dropped==10 && audio_fifo.count==NES_AUDIO_FIFO_SAMPLES);
    reset();enable();nes_audio_fifo_push(&audio_fifo,backlog,NES_AUDIO_FIFO_SAMPLES-6);
    io_result=PXA_STATUS_IO_ERROR;enqueue_audio(samples,16);failed();assert(!audio_fifo.count);
    reset();enable();io_result=INT32_MAX;enqueue_audio(samples,16);
    assert(io_calls==1 && !audio_fifo.count && !audio_dropped);
    puts("PASS: sound touch, duplicate click, enable/disable/re-enable, denied retry, synchronous/asynchronous errors, format, backpressure, pre-overflow retry, PCM failure, revocation and late permission cleanup");
}
