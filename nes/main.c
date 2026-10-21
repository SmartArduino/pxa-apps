/* PXA NES frontend. GPL-2.0-or-later. */
#include "core.h"
#include "state_snapshot.h"
#include "audio_fifo.h"
#include "controls.h"
#include "video_renderer.h"
#include "scheduler.h"
#include "pacer.h"
#include "font8x8.h"
#include "ui_font.h"
#include "ui_strings.h"
#include "pxa.h"
#include "pxa_audio.h"
#include "pxa_assets.h"

#include "pxa_clock.h"
#include "pxa_fs_roots.h"
#include "pxa_game_render_spans.h"
#include "pxa_permission.h"
#include "pxa_surface.h"
#include "pxa_ui.h"
#include "pxa_window.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

enum { T_SURFACE=1,T_LAYER,T_ROOT,T_DIR,T_ENTRY,T_ROM,T_SAVE_READ,
       T_SAVE_WRITE,T_SAVE_REPLACE,T_AUDIO_PERMISSION,T_AUDIO_OPEN,T_AUDIO_GAIN,
       T_LIMITS,T_CONFIGURE,T_WINDOW,T_MKDIR,T_ASSET,T_STATS,T_RASTER,T_MEDIA_PATH };
enum { MENU, LOADING, PLAYING, PAUSED, ERROR_SCREEN, STATES };
static uint16_t frames[2][NES_DISPLAY_MAX_WIDTH*NES_DISPLAY_MAX_HEIGHT] __attribute__((aligned(64)));
static uint8_t video_history[NES_VIDEO_HISTORY_BYTES] __attribute__((aligned(8)));
static uint16_t colors[256];
static unsigned buffer_mode[2];
static nes_controls_t controls;
static nes_video_renderer_t video_renderer;
static uint64_t surface, media, directory, file, save_file, audio, audio_pending, permission;
static uint64_t raster_context, native_frames, native_fallback;
static uint64_t frame_id, metric_at, cpu_us, scan_us, n_frames, n_present, skipped, audio_dropped;
static uint64_t paint_us,paint_frames,paint_pixels,core_draw_us,core_skip_us;
static uint64_t skip_pacing,skip_buffer,tick_us,tick_count;
static uint64_t save_cookie;
static unsigned width=296,height=240, mode=MENU, source=0, page, count, selected;
static unsigned input_scale=1;
static uint32_t input_density_q16=65536;
static unsigned reading_save, writing_save, save_pending, save_replace_pending, scan_pending;
static unsigned save_failed;
static unsigned rom_loaded, background, redraw=1, pending_index=255;
static unsigned display_divisor=4;
static char rom_names[2][128][128],(*names)[128]=rom_names[0];
static unsigned source_count[2],preferred_source;
static char message[112];
static char media_display_path[256]="/PXA/media/";
static char save_path[40], save_tmp[48];
static uint8_t *rom_data, *save_data;
static size_t rom_size, rom_used, save_size, save_used;
static uint32_t saved_crc, pending_save_crc;
static uint8_t pad[2], touches[16];
static uint8_t controller_previous[2],controller_connected[2];
static uint32_t controller_raw_previous[2];
enum { PAUSE_TOP=68, PAUSE_PITCH=30, PAUSE_CHOICES=7 };
static unsigned pause_selected,pause_top,exit_requested;
static unsigned menu_focus; /* 0=ROM list, 1=refresh, 2=exit */
static uint32_t menu_repeat_buttons;
static uint64_t menu_repeat_at;
static unsigned menu_repeat_mode;
static pxa_i18n_t i18n;
static int audio_requested,audio_failed;
static nes_audio_fifo_t audio_fifo;
static nes_schedule_t schedule;
static nes_pacer_t pacer;
static uint64_t micros(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000u+t.tv_nsec/1000u;
}
void nes_port_log(const char *format, ...) {
    char line[384]; va_list ap;va_start(ap,format);vsnprintf(line,sizeof(line),format,ap);va_end(ap);
    size_t n=strlen(line);if(n && line[n-1]=='\n')line[n-1]=0;
    pxa_log_write(2,line);
}
static void fail(const char *s) { snprintf(message,sizeof(message),"%s",s);mode=ERROR_SCREEN;redraw=1;nes_port_log("NES ERROR %s",s); }
static void close_handle(uint64_t *h) { if(*h)pxa_close_handle(*h);*h=0; }
static void release_input(void) { memset(pad,0,sizeof(pad));memset(touches,0,sizeof(touches)); }
/* Surface pixels and Host Canvas share the screen. Hide the last game
 * frame while the Host menu is active; change this only at transitions. */
static unsigned surface_visible;
static int surface_visibility(unsigned visible) {
    if(!surface || surface_visible==visible)return 1;
    uint8_t packet[160];uint32_t n;
    if(!pxa_surface_build_configure_layer(packet,sizeof(packet),T_LAYER,surface,0,0,width,height,0,visible,&n) || pxa_control_packet(packet,n))return 0;
    surface_visible=visible;return 1;
}
#include "system_menu.h"
static uint32_t menu_zoom(void) {return menu_zoom_q16;}
static unsigned menu_width(void) {return ((uint64_t)menu_area.width<<16)/menu_zoom();}
static unsigned menu_height(void) {return ((uint64_t)menu_area.height<<16)/menu_zoom();}
static void rect(uint16_t *out,int x,int y,int w,int h,uint16_t color) {
    if(menu_commands){menu_box(x,y,w,h,0,color);return;}
    unsigned stride=menu_width(),rows=menu_height();
    for(int j=y;j<y+h && j<(int)rows;++j)for(int i=x;i<x+w && i<(int)stride;++i)
        if(j>=0 && i>=0)out[j*stride+i]=color;
}
static void text(uint16_t *out,int x,int y,const char *s,uint16_t color,unsigned max) {
    if(menu_commands){menu_label(x,y,max*8,20,s,color,0);return;}
    nes_ui_draw(out,menu_width(),menu_height(),x,y,s,color,max*8);
}
enum { UI_BG=0x0863, UI_CARD=0x10c5, UI_EDGE=0x2148, UI_FOCUS=0x11ea,
       UI_ACCENT=0x4ed8, UI_INK=0xef9f, UI_MUTED=0x8cb3 };
static void card(uint16_t *out,int x,int y,int w,int h,uint16_t color) {
    if(menu_commands){menu_box(x,y,w,h,4,color);return;}
    /* Fixed-radius corners, used only when a menu actually needs repainting. */
    rect(out,x+4,y,w-8,h,color);rect(out,x+2,y+1,w-4,h-2,color);
    rect(out,x+1,y+2,w-2,h-4,color);rect(out,x,y+4,w,h-8,color);
}
static void button(uint16_t *out,int x,int y,int w,const char *label,uint16_t color) {
    card(out,x,y,w,23,color);
    if(menu_commands){menu_label(x+5,y+1,w-10,22,label,color==UI_ACCENT?UI_BG:UI_INK,1);return;}
    unsigned label_width=nes_ui_width(label);
    int left=label_width<(unsigned)w-10?x+(w-(int)label_width)/2:x+5;
    nes_ui_draw(out,menu_width(),menu_height(),left,y+3,label,
                color==UI_ACCENT?UI_BG:UI_INK,w-10);
}
/* Legacy font fallback: scale a transient menu into the safe rectangle.
 * The temporary allocation is released before any game frame is simulated. */
static void expand_menu(uint16_t *out,const uint16_t *source) {
    unsigned w=menu_width(),h=menu_height();
    for(unsigned n=0;n<width*height;++n)out[n]=UI_BG;
    for(unsigned y=0;y<menu_area.height;++y)for(unsigned x=0;x<menu_area.width;++x)
        out[(menu_area.y+y)*width+menu_area.x+x]=
            source[(y*h/menu_area.height)*w+x*w/menu_area.width];
}
static const char *rom_title(const char *name) {
    if(!strcmp(name,"metal-max-cn.nes") || !strcmp(name,"06-metal.nes"))return UI_METAL;
    if(!strcmp(name,"mario.nes") || !strcmp(name,"05-mario.nes"))return UI_MARIO;
    if(!strcmp(name,"00-bomberman.nes"))return UI_BOMBERMAN;
    if(!strcmp(name,"01-contra.nes"))return UI_CONTRA;
    if(!strcmp(name,"02-ninja.nes"))return UI_NINJA;
    if(!strcmp(name,"03-tmnt3.nes"))return UI_TMNT3;
    if(!strcmp(name,"04-toads.nes"))return UI_TOADS;
    if(!strcmp(name,"07-xunqin.nes"))return UI_XUNQIN;
    if(!strcmp(name,"08-1943.nes"))return "1943";
    if(!strcmp(name,"pxa-demo.nes"))return UI_DEMO;
    return name;
}
static void update_locale(void) {
    nes_controls_set_labels(&controls,"SELECT","START","MENU");
    nes_video_invalidate(&video_renderer,0);nes_video_invalidate(&video_renderer,1);
    redraw=1;nes_port_log("NES LOCALE %s",i18n.locale);
}
static void format_number(pxa_i18n_message_id_t id,const char *name,unsigned value,char *out,size_t bytes) {
    const pxa_i18n_argument_t arg={.name=name,.name_size=(uint8_t)strlen(name),
        .type=PXA_I18N_ARGUMENT_U32,.value.u32=value};
    pxa_i18n_format(&i18n,id,&arg,1,out,bytes);
}
static int acquire(void) {
    if(!surface)return -1;
    if(pending_index!=255) {
        int r=pxa_surface_present_buffer(surface,pending_index,frame_id);
        if(r==PXA_STATUS_WOULD_BLOCK)return -1;
        if(r!=16){fail("Surface present failed");return -1;}
        ++n_present;pending_index=255;
    }
    uint8_t i=255;int r=pxa_surface_acquire_buffer(surface,&i);
    if(r==PXA_STATUS_WOULD_BLOCK)return -1;
    if(r!=4 || i>1){fail("Surface acquire failed");return -1;}
    return i;
}
static void present(unsigned i) {
    ++frame_id;int r=pxa_surface_present_buffer(surface,i,frame_id);
    if(r==PXA_STATUS_WOULD_BLOCK)pending_index=i;
    else if(r==16)++n_present;
    else fail("Surface submission failed");
}
static int native_resolve(uint16_t *target,unsigned pitch,unsigned w,unsigned h,
                          const uint8_t *source,unsigned source_pitch) {
    if(!raster_context)return 0;
    pxa_game_render_blit_wire_t b={
        .source_offset=(uint32_t)(uintptr_t)source,
        .source_bytes=239*source_pitch+256,.source_width=256,.source_height=240,
        .source_pitch=source_pitch,.palette_offset=(uint32_t)(uintptr_t)colors,
        .palette_bytes=sizeof(colors),.target_offset=(uint32_t)(uintptr_t)target,
        .target_bytes=((h-1)*pitch+w)*2,.target_width=w,.target_height=h,
        .target_pitch=pitch*2};
    int result=pxa_game_render_blit_indexed_rect(raster_context,&b);
    if(result==PXA_GAME_RENDER_BLIT_BYTES){++native_frames;return 1;}
    ++native_fallback;
    if(result!=PXA_STATUS_WOULD_BLOCK){
        nes_port_log("NES RASTER fallback status=%d",result);
        close_handle(&raster_context);
    }
    return 0;
}
static void paint_slots(uint16_t *out);
static void slots_tick(void);
static void slots_open(void);
static void request_exit(void);
static void exit_tick(void);
static void menu_controller_repeat(uint64_t now);
static unsigned pause_rows(void) {unsigned n=(menu_height()-PAUSE_TOP-18)/PAUSE_PITCH;return n?n:1;}
static void scroll_bar(uint16_t *out,unsigned top,unsigned rows,unsigned total,unsigned y,unsigned height) {
    if(total<=rows)return;
    rect(out,menu_width()-18,y,2,height,UI_EDGE);
    unsigned thumb=height*rows/total;if(thumb<8)thumb=8;
    rect(out,menu_width()-18,y+(height-thumb)*top/(total-rows),2,thumb,UI_ACCENT);
}
static void path_lines(uint16_t *out,int y,const char *path) {
    unsigned chars=(menu_width()-64)/9;if(chars<12)chars=12;if(chars>80)chars=80;
    char line[81];
    while(*path){unsigned n=strlen(path);if(n>chars)n=chars;memcpy(line,path,n);line[n]=0;
        if(menu_commands)menu_label(32,y,menu_width()-64,16,line,UI_MUTED,0);else text(out,32,y,line,UI_MUTED,chars);
        y+=16;path+=n;}
}
/* Cold menu rendering stays out of the emulation/scanout path. No animation,
 * new framebuffer, timer, or per-game-frame text/layout work is required. */
static __attribute__((noinline,cold)) void paint_menu(uint16_t *out) {
    const unsigned w=menu_width(),h=menu_height();
    rect(out,0,0,w,h,UI_BG);
    card(out,24,9,25,19,UI_ACCENT);
    rect(out,29,17,9,3,UI_BG);rect(out,32,14,3,9,UI_BG);
    rect(out,41,14,3,3,UI_BG);rect(out,39,20,3,3,UI_BG);
    text(out,57,11,UI_TITLE,UI_INK,24);
    if(mode==MENU) {
        unsigned visible=(h-108)/22;
        char position[24];snprintf(position,sizeof(position),"%u / %u",count?selected+1:0,count);
        unsigned tw=nes_ui_width(position);
        if(count){if(menu_commands)menu_label(w-100,9,76,20,position,UI_MUTED,2);
        else text(out,w-24-tw,11,position,UI_MUTED,12);}
        unsigned tabs=!!source_count[0]+!!source_count[1],slot=0,tab=tabs?(w-48-4*(tabs-1))/tabs:0;
        for(unsigned n=0;n<2;++n)if(source_count[n]) {
            button(out,24+slot*(tab+4),34,tab,n?UI_SD:UI_FLASH,source==n?UI_ACCENT:UI_CARD);++slot;
        }
        for(unsigned row=0;row<visible;++row) {
            unsigned n=page+row;if(n>=count)break;
            unsigned y=64+row*22;
            card(out,24,y,w-48,20,n==selected && !menu_focus?UI_FOCUS:UI_CARD);
            if(n==selected && !menu_focus)rect(out,24,y+5,2,10,UI_ACCENT);
            char number[8];snprintf(number,sizeof(number),"%02u",n+1);
            text(out,33,y+2,number,n==selected?UI_ACCENT:UI_MUTED,3);
            if(menu_commands)menu_label(60,y,w-108,20,rom_title(names[n]),UI_INK,0);
            else nes_ui_draw(out,w,h,60,y+2,rom_title(names[n]),UI_INK,w-108);
            if(n==selected && !menu_focus)text(out,w-39,y+2,">",UI_ACCENT,1);
        }
        if(!count) {
            text(out,32,40,scan_pending?UI_SCANNING:UI_EMPTY_HELP,UI_INK,36);
            text(out,32,72,UI_FLASH_PATH,UI_ACCENT,32);
            path_lines(out,96,"roms/");
            text(out,32,112,UI_SD_PATH,UI_ACCENT,32);
            path_lines(out,134,media_display_path);
        } else scroll_bar(out,page,visible,count,64,visible*22-2);
        rect(out,24,h-45,w-48,1,UI_EDGE);
        button(out,24,h-39,(w-56)/2,UI_REFRESH,menu_focus==1?UI_ACCENT:UI_CARD);
        button(out,32+(w-56)/2,h-39,(w-56)/2,UI_EXIT,menu_focus==2?UI_ACCENT:UI_CARD);
    } else if(mode==PAUSED) {
        const char *status=save_failed?UI_SAVE_ERROR:
            (save_pending||writing_save||save_replace_pending?UI_SAVING:UI_PAUSED);
        text(out,32,36,status,save_failed?0xfb8c:UI_MUTED,28);
        rect(out,32,58,w-64,1,UI_EDGE);
        char display[112],keys[112];
        format_number(PXA_MSG_UI_DISPLAY,"frames",display_divisor,display,sizeof(display));
        if(controls.controller_hidden)snprintf(keys,sizeof(keys),"%s",UI_KEYS_AUTO);
        else if(controls.alpha)format_number(PXA_MSG_UI_KEYS_ALPHA,"opacity",(controls.alpha*100+4)/8,keys,sizeof(keys));
        else snprintf(keys,sizeof(keys),"%s",UI_KEYS_OFF);
        const char *labels[]={UI_RESUME,UI_LIBRARY,UI_STATES,
            audio?UI_SOUND_ON:(audio_requested?UI_SOUND_STARTING:(audio_failed?UI_SOUND_FAILED:UI_SOUND_OFF)),keys,display,UI_EXIT};
        for(unsigned visible=0;visible<pause_rows() && pause_top+visible<PAUSE_CHOICES;++visible) {
            unsigned row=pause_top+visible,y=PAUSE_TOP+visible*PAUSE_PITCH;
            card(out,32,y,w-64,24,row==pause_selected?UI_FOCUS:UI_CARD);
            if(row==pause_selected)rect(out,32,y+6,2,12,UI_ACCENT);
            if(menu_commands)menu_label(44,y+1,w-108,23,labels[row],UI_INK,0);
            else nes_ui_draw(out,w,h,44,y+4,labels[row],UI_INK,w-108);
            text(out,w-51,y+4,">",row==pause_selected?UI_ACCENT:UI_MUTED,1);
        }
        scroll_bar(out,pause_top,pause_rows(),PAUSE_CHOICES,PAUSE_TOP,pause_rows()*PAUSE_PITCH-6);
    } else if(mode==STATES) {paint_slots(out);
    } else {
        card(out,24,66,w-48,104,UI_CARD);
        text(out,36,80,mode==LOADING?UI_LOADING:UI_LOAD_ERROR,UI_ACCENT,28);
        if(mode==ERROR_SCREEN) {
            text(out,36,111,strstr(message,"SD needs")?UI_HOST_UNSUPPORTED:UI_ERROR_DETAIL,UI_INK,28);
            text(out,36,137,UI_ERROR_RETRY,UI_MUTED,28);
            button(out,32,h-54,w-64,UI_BACK_LIBRARY,UI_FOCUS);
        }
    }
}
static __attribute__((noinline,cold)) int paint_system_menu(void) {
    if(!surface_visibility(0))return 0;
    uint8_t data[8192],packet[1024];pxa_canvas_frame_t frame;
    pxa_canvas_begin(&frame,data,sizeof(data));
    pxa_canvas_rect_rgba(&frame,0,0,menu_origin(width),menu_origin(height),menu_rgba(UI_BG),0,0,0);
    menu_commands=&frame;
    paint_menu(NULL);menu_commands=NULL;
    int ok=pxa_canvas_present_regions(2,&frame,&menu_generation,&menu_initialized,
        /* Canvas owns pointer events; subscribing the parent too duplicates
         * taps when LVGL bubbles the same event to the root. */
        PXA_UI_EVENT_MASK_CONTROLLER_STATE,NULL,0,packet,sizeof(packet),NULL,0);
    if(ok)redraw=0;
    return ok;
}
static __attribute__((noinline)) void paint(unsigned i,int game) {
    uint16_t *out=(uint16_t*)frames+i*width*height;
    if(!game || buffer_mode[i]!=PLAYING)nes_video_invalidate(&video_renderer,i);
    buffer_mode[i]=game?PLAYING:mode;
    if(game) {
        uint64_t began=micros();
        uint8_t pressed=pad[0];for(unsigned t=0;t<16;++t)pressed|=touches[t];
        const uint8_t *v=nes_core_video();
        nes_video_render(&video_renderer,out,i,v,NES_VIDEO_PITCH,&controls,pressed);
        paint_pixels+=video_renderer.painted_pixels;
        paint_us+=micros()-began;++paint_frames;
    } else {
        size_t bytes=(size_t)menu_width()*menu_height()*sizeof(uint16_t);
        uint16_t *menu=malloc(bytes);
        if(!menu){
            fail("Menu allocation failed");
            for(unsigned n=0;n<width*height;++n)out[n]=UI_BG;
            nes_ui_draw(out,width,height,menu_area.x,menu_area.y,UI_LOAD_ERROR,UI_INK,menu_area.width);
            present(i);redraw=0;return;
        }
        paint_menu(menu);expand_menu(out,menu);free(menu);
    }

    present(i);redraw=0;
}
static void library_scan_source(unsigned which) {
    source=which;names=rom_names[source];count=0;
    int r=source?pxa_fs_request_app_media(T_ROOT):pxa_fs_request_open(T_DIR,"roms",4,PXA_FS_OPEN_READ|PXA_FS_OPEN_DIRECTORY);
    if(r){source_count[source]=0;if(!source)library_scan_source(1);else{scan_pending=0;source=preferred_source;count=source_count[source];names=rom_names[source];redraw=1;}}
}
static void library_scanned(void) {
    if(directory==media)directory=0;else close_handle(&directory);
    for(unsigned i=0;i<count;++i)for(unsigned j=i+1;j<count;++j)if(strcmp(names[i],names[j])>0){char tmp[128];memcpy(tmp,names[i],128);memcpy(names[i],names[j],128);memcpy(names[j],tmp,128);}
    source_count[source]=count;
    if(!source){library_scan_source(1);return;}
    source=source_count[preferred_source]?preferred_source:(source_count[0]?0:1);
    count=source_count[source];names=rom_names[source];scan_pending=0;page=selected=0;menu_focus=count?0:1;redraw=1;
    nes_port_log("NES LIBRARY flash=%u sd=%u",source_count[0],source_count[1]);
}
static void open_library(void) {
    if(file || reading_save || scan_pending)return;
    if(directory==media)directory=0;else close_handle(&directory);close_handle(&media);
    preferred_source=source>1?0:source;source_count[0]=source_count[1]=0;count=page=selected=0;menu_focus=0;scan_pending=1;mode=MENU;redraw=1;
    library_scan_source(0);
}
static void start_save(void) {
    if(!rom_loaded || save_pending || writing_save || save_replace_pending)return;
    size_t size;uint8_t *ram=nes_core_sram(&size);if(!ram || !size)return;
    uint32_t crc=nes_port_crc32(0,ram,size);if(crc==saved_crc)return;
    uint8_t *copy=realloc(save_data,size);if(!copy){save_failed=1;redraw=1;nes_port_log("NES SRAM snapshot allocation failed");return;}
    save_data=copy;memcpy(save_data,ram,size);save_size=size;save_used=0;pending_save_crc=crc;
    save_failed=0;save_pending=1;
    int r=pxa_fs_request_open(T_SAVE_WRITE,save_tmp,strlen(save_tmp),PXA_FS_OPEN_WRITE|PXA_FS_OPEN_CREATE|PXA_FS_OPEN_TRUNCATE);
    if(r){save_pending=0;save_failed=1;redraw=1;nes_port_log("NES SRAM open rejected %d",r);}
}
static void game_ready(void) {
    nes_video_reset(&video_renderer);
    reading_save=0;rom_loaded=1;
    if(!menu_dismiss()){fail("Cannot dismiss menu");return;}
    mode=PLAYING;release_input();nes_schedule_reset(&schedule);
    metric_at=micros();n_frames=n_present=skipped=cpu_us=scan_us=paint_us=paint_frames=paint_pixels=core_draw_us=core_skip_us=0;save_failed=0;
    skip_pacing=skip_buffer=tick_us=tick_count=0;nes_pacer_reset(&pacer);
    nes_port_log("NES READY name=%s mapper=%u hz=%u bytes=%u crc=%08x",names[selected],nes_core_mapper(),nes_core_rate(),(unsigned)rom_size,nes_core_checksum());
}
static void finish_rom(void) {
    close_handle(&file);
    const char *error=nes_core_load(rom_data,rom_size);if(error){fail(error);return;}
    snprintf(save_path,sizeof(save_path),"nes-%08x.sav",nes_core_checksum());
    snprintf(save_tmp,sizeof(save_tmp),"%s.tmp",save_path);
    saved_crc=0;reading_save=1;
    int r=pxa_fs_request_open(T_SAVE_READ,save_path,strlen(save_path),PXA_FS_OPEN_READ);
    if(r)game_ready();
}
static void select_rom(unsigned n) {
    if(n>=count || save_pending || writing_save || save_replace_pending)return;
    nes_core_close();rom_loaded=0;free(rom_data);rom_data=NULL;rom_size=rom_used=0;
    selected=n;mode=LOADING;redraw=1;release_input();nes_schedule_reset(&schedule);
    char path[150];snprintf(path,sizeof(path),"roms/%s",names[n]);
    int r=source?pxa_fs_request_open_at(T_ROM,media,names[n],strlen(names[n]),PXA_FS_OPEN_READ):
        pxa_fs_request_open(T_ROM,path,strlen(path),PXA_FS_OPEN_READ);
    if(r)fail("ROM open request failed");
}
static void audio_failure(const char *stage,int status) {
    close_handle(&audio);close_handle(&audio_pending);close_handle(&permission);
    nes_audio_fifo_clear(&audio_fifo);audio_requested=0;audio_failed=1;redraw=1;
    nes_port_log("NES SOUND FAILED stage=%s status=%d",stage,status);
}
static void request_audio(void) {
    if(audio) {
        close_handle(&audio);close_handle(&permission);
        nes_audio_fifo_clear(&audio_fifo);audio_requested=audio_failed=0;redraw=1;
        nes_port_log("NES SOUND OFF");return;
    }
    if(audio_requested)return;
    audio_requested=1;audio_failed=0;redraw=1;
    uint8_t packet[160];uint32_t size;
    if(!pxa_permission_build(packet,sizeof(packet),PXA_PERMISSION_ACQUIRE,T_AUDIO_PERMISSION,"audio.playback",14,(const uint8_t*)"media",5,&size)) {
        audio_failure("permission-build",-1);return;
    }
    int status=pxa_control_packet(packet,size);
    if(status)audio_failure("permission-request",status);
    else nes_port_log("NES SOUND STARTING");
}
static void drain_audio(void) {
    if(!audio)return;
    for(unsigned attempt=0;attempt<4 && audio_fifo.count;++attempt) {
        unsigned samples;int16_t *pcm=nes_audio_fifo_peek(&audio_fifo,320,&samples);
        int r=pxa_audio_write_pcm(audio,(uint8_t*)pcm,samples*2);
        if(r==PXA_STATUS_WOULD_BLOCK || r==0)break;
        if(r<0 || (unsigned)r>samples*2 || (r&1)) {
            audio_dropped+=audio_fifo.count;nes_audio_fifo_clear(&audio_fifo);
            audio_failure("pcm-write",r);break;
        }
        nes_audio_fifo_consume(&audio_fifo,r/2);
    }
}
static __attribute__((noinline)) void io_tick(void) {
    if(mode!=PLAYING)slots_tick();
    drain_audio();
    uint64_t started=micros();
    if(file && rom_data && !reading_save && mode==LOADING) {
        for(unsigned k=0;k<8 && micros()-started<4000;++k) {
            size_t left=rom_size-rom_used;if(left>4096)left=4096;
            int r=pxa_fs_read(file,rom_data+rom_used,left);
            if(r==PXA_STATUS_WOULD_BLOCK)break;
            if(r<=0 || (unsigned)r>left){close_handle(&file);fail("ROM read failed or truncated");break;}
            rom_used+=r;if(rom_used==rom_size){finish_rom();break;}
        }
    }
    if(file && reading_save) {
        size_t size;uint8_t *ram=nes_core_sram(&size);
        if(save_used<size) {
            unsigned chunk=size-save_used;if(chunk>4096)chunk=4096;
            int r=pxa_fs_read(file,save_data+save_used,chunk);
            if(r>0 && (unsigned)r<=chunk)save_used+=r;
            else if(r!=PXA_STATUS_WOULD_BLOCK){close_handle(&file);game_ready();}
        }
        if(reading_save && save_used==size){memcpy(ram,save_data,size);saved_crc=nes_port_crc32(0,ram,size);close_handle(&file);game_ready();}
    }
    if(writing_save && save_file) {
        unsigned chunk=save_size-save_used;if(chunk>4096)chunk=4096;
        int r=pxa_fs_write(save_file,save_data+save_used,chunk);
        if(r>0 && (unsigned)r<=chunk)save_used+=r;
        else if(r!=PXA_STATUS_WOULD_BLOCK){nes_port_log("NES SRAM write failed %d",r);close_handle(&save_file);writing_save=0;save_failed=1;redraw=1;}
        if(writing_save && save_used==save_size) {
            close_handle(&save_file);writing_save=0;
            uint8_t packet[200];uint32_t size;
            if(pxa_fs_build_replace(packet,sizeof(packet),T_SAVE_REPLACE,save_tmp,strlen(save_tmp),save_path,strlen(save_path),&size) && !pxa_control_packet(packet,size))save_replace_pending=1;
        }
    }
}
static void enqueue_audio(const int16_t *pcm, unsigned samples) {
    if(!audio)return;
    /* The host can free space while a frame is emulated/rendered. Retry
     * before discarding samples, only when this push would overflow. */
    if(samples>NES_AUDIO_FIFO_SAMPLES-audio_fifo.count)drain_audio();
    if(!audio)return; /* A failed write closes the session. */
    audio_dropped+=nes_audio_fifo_push(&audio_fifo,pcm,samples);
    drain_audio();
}
static __attribute__((noinline)) void tick(uint64_t now) {
    io_tick();exit_tick();if(background)return;
    if(mode!=PLAYING){menu_controller_repeat(now);if(redraw){if(system_menu_supported)paint_system_menu();else {int i=acquire();if(i>=0)paint(i,0);}}return;}
    /* Tick events can wait behind other events. Account actual elapsed time. */
    now=micros();
    nes_schedule_tick(&schedule,now,nes_core_rate());
    nes_pacer_tick(&pacer,now,schedule.debt,nes_core_rate(),display_divisor);
    uint64_t started=micros();
    for(unsigned step=0;step<4 && nes_schedule_due(&schedule);++step) {
        uint64_t frame_started=micros();
        int draw=nes_pacer_due(&pacer,frame_started);
        if(!draw)++skip_pacing;
        int index=draw?acquire():-1;
        if(draw && index<0){++skip_buffer;pacer.buffer_limited=1;}
        draw=index>=0;
        uint8_t p=pad[0];for(unsigned i=0;i<16;++i)p|=touches[i];
        if((p&0x30)==0x30)p&=~0x30;if((p&0xc0)==0xc0)p&=~0xc0;
        uint64_t t=micros();nes_core_frame(draw,p,pad[1]);uint64_t core_elapsed=micros()-t;
        cpu_us+=core_elapsed;if(draw)core_draw_us+=core_elapsed;else core_skip_us+=core_elapsed;
        scan_us+=nes_core_scan_time();
        ++n_frames;nes_schedule_step(&schedule);
        if(draw){paint(index,1);nes_pacer_drew(&pacer,frame_started);}else ++skipped;
        if(audio){size_t samples;const int16_t *pcm=nes_core_audio(&samples);enqueue_audio(pcm,samples);}
        nes_pacer_observe(&pacer,draw,micros()-frame_started);
        /* A heavy PAL/NTSC frame can exceed 12ms. Allow a second frame in
           the same callback to amortize host dispatch and catch up. */
        if(micros()-started>=24000)break;
    }
    uint64_t wall_now=micros();
    tick_us+=wall_now-started;++tick_count;
    if(wall_now>=metric_at+5000000) {
        uint64_t elapsed=wall_now-metric_at;
        nes_port_log("NES PERF version=0.1.37 wall_us=%llu frames=%llu elapsed_us=%llu sim_x100=%llu submit_x100=%llu cpu_avg_us=%llu scan_avg_us=%llu skipped=%llu debt_frames=%llu audio_drop=%llu mapper=%u paint_avg_us=%llu keys_alpha=%u keys_auto=%u paint_px_avg=%llu",
            (unsigned long long)wall_now,(unsigned long long)n_frames,(unsigned long long)elapsed,(unsigned long long)(n_frames*100000000/elapsed),(unsigned long long)(n_present*100000000/elapsed),
            (unsigned long long)(cpu_us/(n_frames?n_frames:1)),(unsigned long long)(scan_us/(n_frames?n_frames:1)),
            (unsigned long long)skipped,(unsigned long long)(schedule.debt/1000000),
            (unsigned long long)audio_dropped,nes_core_mapper(),(unsigned long long)(paint_frames?paint_us/paint_frames:0),nes_controls_visible(&controls)?controls.alpha:0,controls.controller_hidden,(unsigned long long)(paint_frames?paint_pixels/paint_frames:0));
        nes_port_log("NES CORE version=0.1.37 wall_us=%llu drawn_frames=%llu skipped_frames=%llu draw_avg_us=%llu skip_avg_us=%llu",
            (unsigned long long)wall_now,(unsigned long long)paint_frames,
            (unsigned long long)(n_frames-paint_frames),
            (unsigned long long)(paint_frames?core_draw_us/paint_frames:0),
            (unsigned long long)(n_frames>paint_frames?core_skip_us/(n_frames-paint_frames):0));
        nes_port_log("NES SCHED version=0.1.37 wall_us=%llu pacing_skip=%llu buffer_skip=%llu requested=%u every=%u interval_x1000=%u draw_est_us=%u skip_est_us=%u feedback=%u sound=%u ticks=%llu work_us=%llu",
            (unsigned long long)wall_now,(unsigned long long)skip_pacing,
            (unsigned long long)skip_buffer,display_divisor,pacer.every,(unsigned)((uint64_t)pacer.interval_q16*1000/65536),
            pacer.draw_us,pacer.skip_us,pacer.feedback,audio!=0,
            (unsigned long long)tick_count,(unsigned long long)tick_us);
        nes_core_profile_t prof[2];nes_core_profile_take(prof);
        for(unsigned kind=0;kind<2;++kind)if(prof[kind].samples){
            nes_core_profile_t *p=&prof[kind];
            nes_port_log("NES PHASE draw=%u samples=%u cpu_bus_us=%llu ppu_us=%llu apu_us=%llu",
                kind,p->samples,(unsigned long long)(p->cpu_bus_us/p->samples),
                (unsigned long long)(p->ppu_us/p->samples),(unsigned long long)(p->apu_us/p->samples));
        }
        nes_port_log("NES RASTER native=%llu fallback=%llu",
            (unsigned long long)native_frames,(unsigned long long)native_fallback);
        native_frames=native_fallback=0;
        uint8_t packet[64];uint32_t bytes;
        if(pxa_surface_build_query_state(packet,sizeof(packet),T_STATS,surface,&bytes))pxa_control_packet(packet,bytes);
        metric_at=wall_now;n_frames=n_present=skipped=cpu_us=scan_us=audio_dropped=paint_us=paint_frames=paint_pixels=core_draw_us=core_skip_us=0;
        skip_pacing=skip_buffer=tick_us=tick_count=0;
    }
}
static void pause_game(void) {
    mode=PAUSED;pause_selected=pause_top=0;redraw=1;release_input();nes_schedule_reset(&schedule);start_save();
    nes_audio_fifo_clear(&audio_fifo);
    if(audio)pxa_audio_flush(++save_cookie+1000,audio);
}
static void pause_action(unsigned choice) {
    if(choice==0){if(!menu_dismiss())return;mode=PLAYING;nes_schedule_reset(&schedule);}
    else if(choice==1)open_library();
    else if(choice==2)slots_open();
    else if(choice==3)request_audio();
    else if(choice==4){nes_controls_cycle(&controls);release_input();redraw=1;}
    else if(choice==5){display_divisor=display_divisor%4+1;redraw=1;}
    else if(choice==6)request_exit();
}
#include "slots.h"
static void request_exit(void) {
    if(slot_phase)return;
    exit_requested=1;release_input();start_save();
    if(mode==PLAYING)pause_game();
    redraw=1;
}
static void exit_tick(void) {
    if(!exit_requested || save_pending || writing_save || save_replace_pending || slot_phase)return;
    exit_requested=0;
    if(save_failed){redraw=1;return;}
    /* Window 0.4 has a Host-delivered BACK request, then the normal stop path. */
    exit_requested=2;
    if(pxa_control_call(PXA_WINDOW_SERVICE,4,0,NULL,0)) {
        exit_requested=0;pxa_window_show_toast(3000,UI_EXIT_BACK,strlen(UI_EXIT_BACK));
    }
}
static unsigned drag_active,drag_moved,drag_mode,drag_id;
static int drag_y;static uint64_t drag_top;
static __attribute__((noinline,cold)) void list_pointer(const pxa_ui_pointer_data_t *p,int x,int y) {
    if((mode==MENU && scan_pending) || (mode==STATES && slot_phase))return;
    unsigned top=mode==PAUSED?PAUSE_TOP:64,pitch=mode==PAUSED?PAUSE_PITCH:22;
    unsigned rows=mode==PAUSED?pause_rows():(menu_height()-(mode==STATES?114:108))/22;
    if(p->phase==PXA_POINTER_DOWN) {
        if(x<24 || x>=(int)menu_width()-24 || y<(int)top || y>=(int)(top+rows*pitch))return;
        if(mode==MENU)menu_focus=0;else if(mode==STATES)slot_focus=0;
        drag_active=1;drag_moved=0;drag_y=y;drag_mode=mode;drag_id=p->pointer_id;
        drag_top=mode==MENU?page:(mode==PAUSED?pause_top:slot_top);return;
    }
    if(!drag_active || drag_id!=p->pointer_id || drag_mode!=mode)return;
    int delta=drag_y-y;if(delta>6 || delta< -6)drag_moved=1;
    if(drag_moved) {
        int64_t n=(int64_t)drag_top+delta/(int)pitch;if(n<0)n=0;
        if(mode==MENU){unsigned max=count>rows?count-rows:0;page=n>max?max:n;}
        else if(mode==PAUSED){unsigned max=PAUSE_CHOICES>rows?PAUSE_CHOICES-rows:0;pause_top=n>max?max:n;}
        else slots_scroll(n);
        redraw=1;
    }
    if(p->phase==PXA_POINTER_UP) {
        drag_active=0;
        if(drag_moved || x<24 || x>=(int)menu_width()-24 || y<(int)top || y>=(int)(top+rows*pitch))return;
        unsigned row=(y-top)/pitch;
        if(mode==MENU)select_rom(page+row);
        else if(mode==PAUSED)pause_action(pause_top+row);
        else slot_load(slot_top+row);
    }
}
static uint8_t map_buttons(uint32_t b) {
    return ((b&PXA_CONTROLLER_A)?1:0)|((b&PXA_CONTROLLER_B)?2:0)|((b&PXA_CONTROLLER_SELECT)?4:0)|((b&PXA_CONTROLLER_START)?8:0)|
        ((b&PXA_CONTROLLER_UP)?16:0)|((b&PXA_CONTROLLER_DOWN)?32:0)|((b&PXA_CONTROLLER_LEFT)?64:0)|((b&PXA_CONTROLLER_RIGHT)?128:0);
}
static void pointer(const pxa_ui_pointer_data_t *p) {
    if(p->pointer_id>=16 || background)return;
    touches[p->pointer_id]=0;
    if(p->phase==PXA_POINTER_CANCEL){drag_active=0;return;}
    /* UI pointers are dp, while the Surface and environment extents are pixels.
     * Undo UI density before converting panel pixels to our smaller canvas. */
    int x=(int)((int64_t)p->x*input_density_q16/(65536*(int64_t)input_scale));
    int y=(int)((int64_t)p->y*input_density_q16/(65536*(int64_t)input_scale));
    if(p->phase==PXA_POINTER_DOWN && mode!=PLAYING){menu_repeat_buttons=0;menu_repeat_at=0;}
    if(p->phase==PXA_POINTER_DOWN && y<(int)height-24 && nes_controls_use_touch(&controls)) {
        release_input();nes_port_log("NES CONTROLS source=touch visible=%u",nes_controls_visible(&controls));if(mode==PLAYING)return;
    }
    if(mode==PLAYING) {
        if(p->phase==PXA_POINTER_UP)return;
        int bit=nes_controls_hit(&controls,x,y);
        if(bit<0){if(p->phase==PXA_POINTER_DOWN)pause_game();return;}
        touches[p->pointer_id]=bit;return;
    }
    if(x<(int)menu_area.x || y<(int)menu_area.y ||
       x>=(int)(menu_area.x+menu_area.width) || y>=(int)(menu_area.y+menu_area.height))return;
    x-=menu_area.x;y-=menu_area.y;
    x=(int)(((int64_t)x<<16)/menu_zoom());y=(int)(((int64_t)y<<16)/menu_zoom());
    const unsigned height=menu_height();
    if(mode==MENU || mode==PAUSED || mode==STATES) {
        list_pointer(p,x,y);
        if(p->phase!=PXA_POINTER_DOWN)return;
    }
    if(p->phase!=PXA_POINTER_DOWN)return;
    if(mode==ERROR_SCREEN){open_library();return;}
    if(mode==PAUSED)return;
    if(mode==STATES) {
        if(!slot_phase && y>=(int)height-39 && y<(int)height-16) {
            if(x<(int)menu_width()/2)slot_new();else{mode=PAUSED;redraw=1;}
        }
        return;
    }
    if(mode!=MENU || scan_pending)return;
    if(y>=34 && y<57 && x>=24 && x<(int)menu_width()-24) {
        unsigned tabs=!!source_count[0]+!!source_count[1];if(!tabs)return;
        unsigned tab=(menu_width()-48-4*(tabs-1))/tabs,slot=(x-24)/(tab+4);
        unsigned chosen=source_count[0]?(slot?1:0):1;
        if(!source_count[chosen])return;menu_focus=0;source=chosen;count=source_count[source];names=rom_names[source];page=selected=0;redraw=1;return;
    }
    if(y>=(int)height-39 && y<(int)height-16 && x>=24 && x<(int)menu_width()-24) {
        if(x<(int)menu_width()/2)open_library();else request_exit();
    }
}
#include "menu_controller.h"
int32_t pxa_app_start(const uint8_t *config,uint32_t size) {
    pxa_i18n_init_from_start_config(&i18n,&pxa_app_i18n_bundle,config,size);
    pxa_ui_environment_t env={0};if(pxa_ui_parse_start_environment(config,size,&env)){width=env.width;height=env.height;input_density_q16=env.density_q16;}
    menu_environment(&env);
    input_scale=nes_display_canvas(&width,&height);
    if(!input_scale)return PXA_STATUS_INVALID_ARGUMENT;
    nes_port_log("NES DISPLAY canvas=%ux%u input_scale=%u density_q16=%u",width,height,input_scale,input_density_q16);
    pxa_window_fullscreen();pxa_window_request_snapshot(T_WINDOW);
    uint8_t packet[512];uint32_t n;pxa_ui_transaction_t tx={0};
    if(!pxa_ui_transaction_begin(&tx,1,PXA_UI_TRANSACTION_REPLACE_SURFACE,packet,sizeof(packet)) ||
       !pxa_ui_create(&tx,1,0,0,PXA_UI_NODE_ROOT) || !pxa_ui_set_rgba(&tx,1,PXA_UI_PROPERTY_BACKGROUND,0) ||
       !pxa_ui_set_event_mask(&tx,1,PXA_UI_EVENT_MASK_POINTER|PXA_UI_EVENT_MASK_CONTROLLER_STATE) || !pxa_ui_transaction_commit(&tx))return PXA_STATUS_INTERNAL;
    for(unsigned i=0;i<256;++i)colors[i]=nes_core_color(i);
    nes_controls_init(&controls,width,height,colors);
    controls.safe=menu_area;
    update_locale();
    nes_video_init_in_rect(&video_renderer,width,height,menu_area,video_history,sizeof(video_history),&controls);
    video_renderer.resolve=native_resolve;
    /* Optional acceleration: an older Host still uses the portable renderer. */
    pxa_game_render_create_raster(T_RASTER,256);
    pxa_surface_desc_t desc={.width=width,.height=height,.format=PXA_SURFACE_RGB565,.buffer_count=2,
        .flags=PXA_SURFACE_GUEST_MAPPED|PXA_SURFACE_PREFER_DIRECT_SCANOUT};
    if(!pxa_surface_build_create(packet,sizeof(packet),T_SURFACE,desc,&n))return PXA_STATUS_INTERNAL;
    int r=pxa_control_packet(packet,n);if(r)return r;
    r=pxa_clock_set_period(16);if(r)return r;pxa_clock_limits(T_LIMITS);
    pxa_fs_request_app_media_path(T_MEDIA_PATH);
    pxa_fs_request_path(PXA_FS_MAKE_DIRECTORY,T_MKDIR,"roms",4);
    open_library();return 0;
}
void pxa_app_stop(uint32_t reason) {
    (void)reason;
    /* Host has already revoked handles. Core imports are forbidden on stop. */
    nes_core_close();free(rom_data);free(save_data);free(slot_data);slot_data=NULL;slot_phase=0;slot_dir=slot_file=0;rom_data=save_data=NULL;
}
int32_t pxa_app_on_event(const uint8_t *data,uint32_t size) {
    pxa_event_t e;if(!pxa_parse_event(data,size,&e))return PXA_EVENT_UNHANDLED;
    if(e.service==PXA_GAME_RENDER_SERVICE && e.token==T_RASTER){
        pxa_game_render_raster_result_t r;
        if(pxa_game_render_parse_raster(&e,T_RASTER,&r) && !r.status)
            raster_context=r.handle;
        nes_port_log("NES RASTER context=%u",(unsigned)(raster_context!=0));
        return PXA_EVENT_HANDLED;
    }
    int locale_changed=pxa_i18n_handle_event(&i18n,&e);
    if(locale_changed){if(locale_changed==1)update_locale();return PXA_EVENT_HANDLED;}

    uint64_t now;if(pxa_clock_parse_tick(&e,&now)){tick(now);return PXA_EVENT_HANDLED;}
    if(e.token==T_STATS && e.service==PXA_SURFACE_SERVICE) {
        pxa_surface_state_t state;
        if(pxa_surface_parse_state(&e,T_STATS,&state) && !state.status)
            nes_port_log("NES DISPLAY version=0.1.37 wall_us=%llu submitted=%llu consumed=%llu replaced=%llu free=%u",
                (unsigned long long)micros(),(unsigned long long)state.submitted_frames,
                (unsigned long long)state.presented_frames,(unsigned long long)state.replaced_frames,state.free_buffers);
        return PXA_EVENT_HANDLED;
    }
    pxa_ui_environment_t env;
    if(pxa_ui_parse_environment_event(&e,&env)){
        input_density_q16=env.density_q16;menu_environment(&env);release_input();
        controls.safe=menu_area;update_locale();
        nes_video_init_in_rect(&video_renderer,width,height,menu_area,video_history,sizeof(video_history),&controls);
        video_renderer.resolve=native_resolve;redraw=1;return PXA_EVENT_HANDLED;
    }
    if(slots_event(&e))return PXA_EVENT_HANDLED;
    pxa_ui_pointer_data_t p;if(pxa_ui_parse_pointer(&e,&p)){if(p.phase!=PXA_POINTER_MOVE)nes_port_log("NES INPUT phase=%u x=%d y=%d mode=%u",p.phase,p.x,p.y,mode);pointer(&p);return PXA_EVENT_HANDLED;}
    pxa_ui_controller_data_t c;if(pxa_ui_parse_controller(&e,&c)) {
        if(c.controller<2){uint32_t buttons=c.connected?c.buttons:0;
            uint8_t b=map_buttons(buttons),previous=controller_previous[c.controller];
            uint32_t pressed=buttons&~controller_raw_previous[c.controller];controller_raw_previous[c.controller]=buttons;
            controller_previous[c.controller]=b;controller_connected[c.controller]=c.connected;
            if(background){pad[c.controller]=0;return PXA_EVENT_HANDLED;}
            if(!background && c.connected && c.buttons && nes_controls_use_controller(&controls)) {
                memset(touches,0,sizeof(touches));nes_port_log("NES CONTROLS source=controller player=%u visible=0",c.controller+1);
            }
            if(!controller_connected[0] && !controller_connected[1] && nes_controls_use_touch(&controls))
                nes_port_log("NES CONTROLS source=disconnect visible=%u",nes_controls_visible(&controls));
            if((pressed&PXA_CONTROLLER_X) && (mode==PLAYING || mode==PAUSED)) {
                if(mode==PLAYING)pause_game();else pause_action(0);return PXA_EVENT_HANDLED;
            }
            if((b&12)==12 && (previous&12)!=12 && mode==PLAYING){pause_game();return PXA_EVENT_HANDLED;}
            if(mode!=PLAYING) {
                if(c.controller==0)menu_controller_event(pressed,buttons);
                pad[c.controller]=0;return PXA_EVENT_HANDLED;
            }
            pad[c.controller]=background?0:b;
        }return PXA_EVENT_HANDLED;
    }
    uint8_t life;if(pxa_lifecycle_parse(&e,&life)) {
        background=life==PXA_SYSTEM_LIFECYCLE_BACKGROUND;menu_repeat_buttons=0;menu_repeat_at=0;release_input();nes_schedule_reset(&schedule);
        if(background){start_save();nes_audio_fifo_clear(&audio_fifo);if(audio)pxa_audio_flush(++save_cookie+1000,audio);}else redraw=1;
        return PXA_EVENT_HANDLED;
    }
    if(e.service==PXA_WINDOW_SERVICE && e.opcode==PXA_WINDOW_BACK_REQUESTED){
        if(exit_requested==2){exit_requested=0;return 0;}
        if(slot_phase)return PXA_EVENT_HANDLED;
        if(mode==PLAYING)pause_game();else if(mode==PAUSED)pause_action(0);else if(mode==STATES){mode=PAUSED;redraw=1;}
        else if(mode==MENU){if(save_pending || writing_save || save_replace_pending)request_exit();else return 0;}
        else open_library();return PXA_EVENT_HANDLED;
    }
    if(e.token==T_SURFACE && e.service==PXA_SURFACE_SERVICE) {
        pxa_surface_create_result_t r;if(!pxa_surface_parse_create(&e,T_SURFACE,&r) || r.status)return PXA_STATUS_INTERNAL;
        surface=r.handle;/* Buffers must be packed at the actual negotiated size. */
        int result=pxa_surface_register_buffers(surface,frames,width*height*2,2);if(result!=(int)width*height*4)return PXA_STATUS_INTERNAL;
        unsigned visible=mode==PLAYING || !system_menu_supported;
        surface_visible=!visible;if(!surface_visibility(visible))return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    }
    if(e.service==PXA_CLOCK_SERVICE && e.token==T_CONFIGURE) {
        pxa_clock_configure_result_t r;
        if(pxa_clock_parse_configure(&e,T_CONFIGURE,8000,&r) && !r.status)display_divisor=1;
        return PXA_EVENT_HANDLED;
    }
    if(e.service==PXA_CLOCK_SERVICE && e.token==T_LIMITS) {
        pxa_clock_limits_result_t r;if(pxa_clock_parse_limits(&e,T_LIMITS,&r) && !r.status && pxa_clock_period_supported(&r.limits,8000))pxa_clock_configure(T_CONFIGURE,8000);
        return PXA_EVENT_HANDLED;
    }
    if(e.service==PXA_FS_SERVICE) {
        if(e.token==T_MEDIA_PATH) {
            if(pxa_fs_parse_app_media_path(&e,T_MEDIA_PATH,media_display_path,sizeof(media_display_path))) {
                nes_port_log("NES SD media=%s",media_display_path);
                if(mode==MENU)redraw=1;
            }
            return PXA_EVENT_HANDLED;
        }
        if(e.token==T_ROOT) {
            pxa_fs_root_result_t r;if(pxa_fs_parse_root(&e,T_ROOT,&r) && !r.status){media=r.handle;if(source){directory=media;pxa_fs_request_read_directory(T_ENTRY,directory);}}
            else {count=0;library_scanned();}return PXA_EVENT_HANDLED;
        }
        if(e.token==T_DIR) {
            int ok=0;if(e.opcode==PXA_FS_OPEN_AT){pxa_fs_at_result_t r;if(pxa_fs_parse_open_at(&e,T_DIR,&r) && !r.status){directory=r.handle;ok=1;}}
            else {pxa_fs_open_result_t r;if(pxa_fs_parse_open(&e,T_DIR,&r) && !r.status){directory=r.handle;ok=1;}}
            if(ok)pxa_fs_request_read_directory(T_ENTRY,directory);else {count=0;library_scanned();}return PXA_EVENT_HANDLED;
        }
        if(e.token==T_ENTRY) {
            pxa_fs_directory_result_t r;
            if(!pxa_fs_parse_directory(&e,T_ENTRY,&r) || r.status || r.end || count==128) {
                library_scanned();
            } else {
                if(r.kind==PXA_FS_KIND_REGULAR && r.name_size>4 && r.name_size<128) {
                    const uint8_t *s=r.name+r.name_size-4;
                    if(s[0]=='.' && tolower(s[1])=='n' && tolower(s[2])=='e' && tolower(s[3])=='s') {memcpy(names[count],r.name,r.name_size);names[count++][r.name_size]=0;}
                }pxa_fs_request_read_directory(T_ENTRY,directory);
            }return PXA_EVENT_HANDLED;
        }
        if(e.token==T_ROM) {
            if(e.opcode==PXA_FS_OPEN_AT) {
                pxa_fs_at_result_t r;if(!pxa_fs_parse_open_at(&e,T_ROM,&r) || r.status){fail("Cannot open SD ROM");return PXA_EVENT_HANDLED;}
                file=r.handle;rom_size=r.size;
            }else {
                pxa_fs_open_result_t r;if(!pxa_fs_parse_open(&e,T_ROM,&r) || r.status){fail("Cannot open flash ROM");return PXA_EVENT_HANDLED;}
                file=r.handle;uint8_t packet[80];uint32_t n;
                if(pxa_fs_build_seek(packet,sizeof(packet),T_ROM+100,file,0,PXA_FS_SEEK_END,&n))pxa_control_packet(packet,n);
                return PXA_EVENT_HANDLED;
            }
        }
        if(e.token==T_ROM+100) {
            pxa_fs_seek_result_t r;if(!pxa_fs_parse_seek(&e,T_ROM+100,&r) || r.status){close_handle(&file);fail("Cannot determine ROM size");return PXA_EVENT_HANDLED;}
            rom_size=r.position;pxa_fs_request_seek(T_ROM+101,file,0,PXA_FS_SEEK_START);
            return PXA_EVENT_HANDLED;
        }
        if(e.token==T_ROM+101) {pxa_fs_seek_result_t r;if(!pxa_fs_parse_seek(&e,T_ROM+101,&r) || r.status){close_handle(&file);fail("Cannot seek ROM");return PXA_EVENT_HANDLED;}}
        if(e.token==T_ROM || e.token==T_ROM+101) {
            if(rom_size<16 || rom_size>NES_ROM_LIMIT){close_handle(&file);fail("ROM size invalid (max 3 MiB)");return PXA_EVENT_HANDLED;}
            rom_data=malloc(rom_size);if(!rom_data){close_handle(&file);fail("Not enough memory for ROM");}
            return PXA_EVENT_HANDLED;
        }
        if(e.token==T_SAVE_READ) {
            pxa_fs_open_result_t r;save_used=0;
            if(pxa_fs_parse_open(&e,T_SAVE_READ,&r) && !r.status) {
                size_t n;nes_core_sram(&n);
                uint8_t *p=n?realloc(save_data,n):NULL;
                if(!p){pxa_close_handle(r.handle);game_ready();}
                else {save_data=p;file=r.handle;}
            } else game_ready();
            return PXA_EVENT_HANDLED;
        }
        if(e.token==T_SAVE_WRITE) {
            pxa_fs_open_result_t r;save_pending=0;
            if(pxa_fs_parse_open(&e,T_SAVE_WRITE,&r) && !r.status){save_file=r.handle;writing_save=1;save_used=0;}
            else {save_failed=1;redraw=1;nes_port_log("NES SRAM open failed");}return PXA_EVENT_HANDLED;
        }
        if(e.token==T_SAVE_REPLACE) {
            int32_t status;save_replace_pending=0;
            if(pxa_fs_parse_status(&e,T_SAVE_REPLACE,PXA_FS_REPLACE,&status) && !status){saved_crc=pending_save_crc;redraw=1;nes_port_log("NES SRAM SAVED bytes=%u crc=%08x",(unsigned)save_size,saved_crc);}
            else {save_failed=1;redraw=1;nes_port_log("NES SRAM atomic replace failed");}return PXA_EVENT_HANDLED;
        }
    }
    pxa_permission_revoked_t revoked;
    if(pxa_permission_parse_revoked(&e,&revoked) && revoked.name_size==14 &&
       !memcmp(revoked.name,"audio.playback",14)) {
        audio_failure("permission-revoked",PXA_STATUS_DENIED);
        return PXA_EVENT_HANDLED;
    }
    if(e.token==T_AUDIO_PERMISSION && e.service==PXA_PERMISSION_SERVICE) {
        pxa_permission_acquire_result_t r;
        if(!pxa_permission_parse_acquire(&e,T_AUDIO_PERMISSION,&r) || r.status)audio_failure("permission",r.status?r.status:-1);
        else if(!audio_requested)pxa_close_handle(r.handle);
        else {permission=r.handle;int status=pxa_audio_open_media(T_AUDIO_OPEN,permission);if(status)audio_failure("open-request",status);}
    }
    if(e.token==T_AUDIO_OPEN && e.service==PXA_AUDIO_SERVICE) {
        pxa_audio_open_result_t r;
        int parsed=pxa_audio_parse_open(&e,T_AUDIO_OPEN,&r);
        if(parsed && !r.status)audio_pending=r.handle;
        if(!parsed || r.status || r.channels!=1 || r.sample_rate!=16000 || !audio_requested)
            audio_failure("open-format",r.status?r.status:-1);
        else {int status=pxa_audio_commit_gain(T_AUDIO_GAIN,audio_pending,-6*256);if(status)audio_failure("gain-request",status);}
    }
    if(e.token==T_AUDIO_GAIN && e.service==PXA_AUDIO_SERVICE) {
        int32_t status=0;
        if(pxa_audio_parse_status(&e,T_AUDIO_GAIN,PXA_AUDIO_COMMIT_GRAPH,&status) && !status && audio_pending && audio_requested) {
            audio=audio_pending;audio_pending=0;redraw=1;
            nes_port_log("NES SOUND ON rate=16000 channels=1 gain_db=-6");
        } else audio_failure("gain",status?status:-1);
    }
    return PXA_EVENT_HANDLED;
}
