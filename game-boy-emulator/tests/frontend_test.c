/* Exercise the actual frontend and event codecs without a display/device. */
#include "../main.c"
#include "../vendor/gnuboy/hw.h"
#include <assert.h>

static unsigned closed,calls[24][24];
static uint32_t reject_route;
static int io_reply=PXA_STATUS_WOULD_BLOCK;
static int surface_test;
static unsigned audio_test,audio_calls,audio_consumed,audio_block;
static unsigned perf_logs,stats_logs;
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t size) {
    (void)token;(void)data;(void)size;
    unsigned service=route&65535,opcode=route>>16;
    if (service==PXA_LOG_SERVICE && opcode==PXA_LOG_WRITE) {
        assert(size<=PXA_LOG_MAX_MESSAGE_BYTES+1);
        if(size>9 && !memcmp(data+1,"GB PERF ",8))++perf_logs;
        if(size>10 && !memcmp(data+1,"GB STATS ",9))++stats_logs;
    }
    if(service<24 && opcode<24)++calls[service][opcode];
    if(service==PXA_CORE_SERVICE && opcode==PXA_CORE_CLOSE_HANDLE)++closed;
    return route==reject_route?PXA_STATUS_DENIED:0;
}
int32_t pxa_io(uint64_t handle,uint32_t operation,uint8_t *data,uint32_t size) {
    if(audio_test && (uint32_t)handle==9 && operation==PXA_AUDIO_IO_WRITE) {
        ++audio_calls;if(audio_block)return PXA_STATUS_WOULD_BLOCK;
        unsigned bytes=size>64?64:size;
        for(unsigned n=0;n<bytes/2;++n)assert(((int16_t*)data)[n]==(int16_t)audio_consumed++);
        return bytes;
    }
    if(surface_test && operation==PXA_SURFACE_IO_ACQUIRE){memset(data,0,size);return (int)size;}
    if(surface_test && operation==PXA_SURFACE_IO_PRESENT)return (int)size;
    return io_reply==INT32_MAX?(int)size:io_reply;
}
static void event(unsigned service,unsigned opcode,uint64_t token,const uint8_t *payload,unsigned size) {
    uint8_t bytes[256];uint32_t length;
    assert(pxa_build_message(bytes,sizeof(bytes),service,opcode,token,payload,size,&length));
    assert(pxa_app_on_event(bytes,length)==PXA_EVENT_HANDLED);
}
static void menu_test_controller(unsigned player,uint32_t buttons,unsigned connected) {
    uint8_t data[32]={0};
    pxa_store_u32(data,1);pxa_store_u32(data+4,1);pxa_store_u32(data+8,1);
    pxa_store_u16(data+12,PXA_UI_EVENT_CONTROLLER_STATE_KIND);
    data[24]=player;data[25]=connected;pxa_store_u32(data+28,buttons);
    event(PXA_SERVICE_UI,PXA_UI_EVENT,0,data,sizeof(data));
}
static void controller(uint32_t buttons,unsigned connected) {menu_test_controller(0,buttons,connected);}
static void reset(void) {
    mode=MENU;background=0;count=3;selected=2;page=0;
    scan_pending=save_pending=writing_save=save_replace_pending=reading_save=rom_loaded=0;
    menu_focus=menu_repeat_buttons=0;menu_repeat_at=0;exit_requested=0;save_failed=0;
    audio=permission=0;audio_requested=audio_failed=0;reject_route=0;
    memset(calls,0,sizeof(calls));memset(controller_connected,0,sizeof(controller_connected));
    memset(controller_raw_previous,0,sizeof(controller_raw_previous));
    memset(controller_previous,0,sizeof(controller_previous));
    gb_controls_init(&controls,320,240);gb_audio_fifo_clear(&audio_fifo);
}
static void touch_controls(void) {
    reset();mode=PLAYING;release_input();surface=0;
    pxa_ui_pointer_data_t p={0};p.phase=PXA_POINTER_DOWN;p.x=160;p.y=90;
    pointer(&p);assert(mode==PLAYING && !touches[0]);
    gb_control_button_t *r=&controls.buttons[GB_CONTROL_RIGHT],*a=&controls.buttons[GB_CONTROL_A];
    p.x=r->x+r->w/2;p.y=r->y+r->h/2;pointer(&p);
    assert(touches[0]==GB_PAD_RIGHT);
    p.pointer_id=1;p.x=a->x+a->w/2;p.y=a->y+a->h/2;pointer(&p);
    assert(touches[0]==GB_PAD_RIGHT && touches[1]==GB_PAD_A);
    p.pointer_id=0;p.phase=PXA_POINTER_UP;pointer(&p);
    assert(!touches[0] && touches[1]==GB_PAD_A);
    p.pointer_id=1;pointer(&p);assert(!touches[1]);
    controls.controller_hidden=1;p.phase=PXA_POINTER_DOWN;p.x=160;p.y=90;
    pointer(&p);assert(mode==PLAYING && !controls.controller_hidden);
    gb_control_button_t *m=&controls.buttons[GB_CONTROL_MENU];
    p.x=m->x+m->w/2;p.y=m->y+m->h/2;pointer(&p);assert(mode==PAUSED);
    puts("PASS: empty touches keep playing; multi-touch release and dedicated MENU pause");
}
static void padded_surface(const char *path) {
    FILE *f=fopen(path,"rb");assert(f);
    fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);
    uint8_t *rom=malloc(size);assert(rom && fread(rom,1,size,f)==size);fclose(f);
    assert(!gb_core_load(rom,size));
    for(unsigned i=0;i<21;++i)gb_core_frame(0);
    uint16_t reference[GB_VIDEO_WIDTH*GB_VIDEO_HEIGHT];
    memcpy(reference,gb_core_video(),sizeof(reference));
    assert(!gb_core_load(rom,size));
    for(unsigned i=0;i<20;++i)gb_core_frame(0);
    reset();mode=PLAYING;width=296;height=240;surface_stride=304;
    frame_pixels=surface_stride*height;frame_bytes=frame_pixels*2;
    uint16_t *allocation=malloc((frame_pixels*2+64)*2);assert(allocation);
    for(unsigned i=0;i<frame_pixels*2+64;++i)allocation[i]=0x54a9;
    frames=allocation+32;surface=9;surface_test=1;pending_buffer=-1;clear_frames=0;
    controls.alpha=0;release_input();
    gb_display_rect_t area={0,0,width,height};
    gb_video_init(&video_renderer,width,height,area);
    gb_video_set_fit(&video_renderer,area,0);gb_video_set_stride(&video_renderer,surface_stride);
    gb_pacer_reset(&pacer);pacer.credit_q16=65536;display_divisor=1;
    gb_schedule_reset(&schedule);schedule.debt=UINT64_C(1000000)*GB_FRAME_CYCLES;metric_at=micros();
    n_frames=n_present=0;tick(micros());assert(n_frames==1 && n_present==1);
    unsigned x0=video_renderer.view_x,y0=video_renderer.view_y;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<surface_stride;++x) {
        uint16_t expected=x>=x0 && x<x0+160 && y>=y0 && y<y0+144 ? reference[(y-y0)*160+x-x0] : 0x54a9;
        assert(frames[y*surface_stride+x]==expected);
    }
    for(unsigned i=0;i<32;++i)assert(allocation[i]==0x54a9 && frames[frame_pixels*2+i]==0x54a9);
    core_avg_us=GB_FRAME_BUDGET_US*2;display_divisor=2;
    gb_schedule_reset(&schedule);schedule.debt=UINT64_C(1000000)*GB_FRAME_CYCLES;
    tick(micros());assert(n_frames==2 && n_present==1);
    gb_schedule_reset(&schedule);schedule.debt=UINT64_C(1000000)*GB_FRAME_CYCLES;
    tick(micros());assert(n_frames==3 && n_present==2);
    /* Expensive PPU frames must not disable pacing when skipped frames fit
     * the simulation budget. Both frames still execute the CPU and APU. */
    gb_pacer_reset(&pacer);pacer.draw_us=24000;pacer.skip_us=4000;
    core_avg_us=GB_FRAME_BUDGET_US*2;display_divisor=1;
    gb_schedule_reset(&schedule);schedule.debt=UINT64_C(1000000)*GB_FRAME_CYCLES;
    tick(micros());assert(n_frames==4 && n_present==2 && !GB.video.enabled);
    gb_schedule_reset(&schedule);schedule.debt=UINT64_C(1000000)*GB_FRAME_CYCLES;
    tick(micros());assert(n_frames==5 && n_present==3 && GB.video.enabled);
    /* Large counters must still reach the Host's 256-byte log interface. */
    metric_at=micros()-5000000;cpu_us=audio_dropped=UINT64_MAX;
    perf_logs=stats_logs=0;tick(micros());assert(perf_logs==1 && stats_logs==1);
    gb_core_close();free(rom);free(allocation);frames=NULL;surface=0;surface_test=0;
    puts("PASS: frontend PPU target uses negotiated padded Surface stride");
}

#define menu_test_reset reset
#include "../../common/emulator/tests/menu_contract.h"
#include "../../common/emulator/tests/theme_contract.h"

#include "../../common/emulator/tests/library_contract.h"
int main(int argc,char **argv) {
    assert(argc==2);
    width=320;height=240;input_density_q16=65536;
    pxa_ui_environment_t env={0};menu_environment(&env);
    pxa_i18n_init(&i18n,&pxa_app_i18n_bundle);

    reset();controller(PXA_CONTROLLER_DOWN,1);
    assert(controls.controller_hidden && menu_focus==1);
    controller(0,1);controller(PXA_CONTROLLER_RIGHT,1);assert(menu_focus==2);
    controller(0,1);controller(PXA_CONTROLLER_A,1);assert(exit_requested==1);
    assert(!pad[0]);exit_tick();assert(exit_requested==2);
    controller(0,0);assert(!controls.controller_hidden);
    reset();count=20;selected=0;
    controller(PXA_CONTROLLER_DOWN,1);assert(selected==1 && menu_repeat_at);
    menu_controller_repeat(menu_repeat_at);assert(selected==2);
    controller(0,1);assert(!menu_repeat_buttons);
    reset();save_pending=1;request_exit();exit_tick();assert(exit_requested==1);
    save_pending=0;exit_tick();assert(exit_requested==2);
    reset();mode=PAUSED;pause_action(5);assert(display_divisor==2 && mode==PAUSED);
    gb_video_init(&video_renderer,320,240,menu_area);
    assert(video_renderer.fit);
    pause_action(6);assert(!video_renderer.fit && mode==PAUSED);
    pause_action(6);assert(video_renderer.fit);
    reset();reject_route=(PXA_PERMISSION_ACQUIRE<<16)|PXA_PERMISSION_SERVICE;
    request_audio();assert(audio_failed && !audio_requested && !audio && !permission);
    reset();audio=9;permission=8;audio_requested=1;
    request_audio();assert(!audio && !permission && !audio_requested);
    assert(has_game_extension("a.gb",4) && has_game_extension("b.GBC",5));
    assert(!has_game_extension("a.gba",5));
    uint8_t payload[128]={0};const char path[]="/PXA/media/lucinhu/pxa-game-boy/";
    pxa_store_u16(payload+4,sizeof(path)-1);memcpy(payload+6,path,sizeof(path)-1);
    event(PXA_FS_SERVICE,PXA_FS_APP_MEDIA_PATH,T_MEDIA_PATH,payload,6+sizeof(path)-1);
    assert(!strcmp(path,sd_media_path));
    reset();audio=UINT64_C(0x100000009);audio_rate=16000;audio_channels=1;audio_test=1;
    int16_t pcm[1000];for(unsigned n=0;n<1000;++n)pcm[n]=(int16_t)n;
    audio_block=1;enqueue_audio(pcm,1000);assert(audio_calls==1 && audio_fifo.count==1000);
    audio_block=0;audio_calls=0;drain_audio();assert(audio_calls==4 && audio_consumed==128);
    while(audio_fifo.count){unsigned before=audio_calls;drain_audio();assert(audio_calls-before<=4);}
    assert(audio_consumed==1000);audio_test=0;audio=0;

    /* Exact PCM must fall back only on unsupported Hosts, then preserve the
     * existing gain transaction and explicit permission gate. */
    reset();audio_requested=1;permission=UINT64_C(0x100000008);
    uint8_t unsupported[4];pxa_store_u32(unsupported,(uint32_t)PXA_STATUS_UNSUPPORTED);
    event(PXA_AUDIO_SERVICE,PXA_AUDIO_OPEN_PCM,T_AUDIO_OPEN,unsupported,4);
    assert(!audio_failed && audio_requested && calls[PXA_AUDIO_SERVICE][PXA_AUDIO_OPEN_SESSION]==1);
    uint8_t reply[56]={0},value[8];size_t at=4;
    pxa_store_u64(value,UINT64_C(0x100000009));assert(pxa_audio_append(reply,sizeof(reply),&at,3,value,8));
    pxa_store_u32(value,32768);assert(pxa_audio_append(reply,sizeof(reply),&at,4,value,4));
    value[0]=2;assert(pxa_audio_append(reply,sizeof(reply),&at,5,value,1));
    pxa_store_u16(value,20);assert(pxa_audio_append(reply,sizeof(reply),&at,6,value,2));
    pxa_store_u32(value,16000);assert(pxa_audio_append(reply,sizeof(reply),&at,7,value,4));
    value[0]=1;assert(pxa_audio_append(reply,sizeof(reply),&at,8,value,1));
    pxa_store_u32(value,3932);assert(pxa_audio_append(reply,sizeof(reply),&at,9,value,4));assert(at==56);
    reset();audio_requested=1;event(PXA_AUDIO_SERVICE,PXA_AUDIO_OPEN_PCM,T_AUDIO_OPEN,reply,sizeof(reply));
    assert(audio_pending==UINT64_C(0x100000009) && audio_rate==32768 && audio_channels==2);
    assert(calls[PXA_AUDIO_SERVICE][PXA_AUDIO_COMMIT_GRAPH]==1 && !audio_failed);
    uint8_t gain_ok[4]={0};event(PXA_AUDIO_SERVICE,PXA_AUDIO_COMMIT_GRAPH,T_AUDIO_GAIN,gain_ok,4);
    assert(audio==UINT64_C(0x100000009) && !audio_pending && audio_state.fade_remaining==32768/200);
    reset();audio=UINT64_C(0x100000009);audio_rate=32768;audio_channels=2;audio_test=1;
    audio_calls=audio_consumed=0;audio_block=0;
    enqueue_audio(pcm,64);assert(audio_calls>=1 && audio_consumed==64 && !audio_fifo.count);
    audio_test=0;audio=0;
    test_theme_contract();
 test_library_contract("gbc");
 test_menu_contract();
    touch_controls();
    padded_surface(argv[1]);
    puts("PASS: controller focus/repeat/hide/restore, exit-save gate, options, audio retry/bounded draining and SD path");
}
