/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "frontend.h"
#include "controls.h"
#include "input.h"
#include "system_font.h"
#include "port.h"
#include "rpc.h"
#include "pxa_fs_roots.h"
#include "pxa_ui.h"
#include "pxa_canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LIBRARY_CAP 128
#define JAR_LIMIT (2u*1024u*1024u)
enum { GAME,LIBRARY,PAUSED,SETTINGS,RESOLUTION,MAPPING,BINDING,MEMORY,FPS_LIMIT };
enum { ACTION_NONE,ACTION_SCAN,ACTION_IMPORT };
typedef struct {char name[256];uint32_t size;} library_entry;
typedef struct {
    int active,role,item,button,mode,start_y,last_y,dragged,x,y;
    unsigned id,action;
    uint64_t keys;
} touch;
static struct {
    port_video_environment_t video;
    nano_rect safe,view;
    nano_settings defaults,settings;
    nano_controls controls;
    nano_input input;
    library_entry library[LIBRARY_CAP];
    touch touches[8];
    uint32_t pad_buttons[2],blocked[2];
    uint8_t pad_connected[2],physical[256];
    unsigned control_style,count,selected,generation,buffer_generation[2],frames,game_version,buffer_game_version[2];
    uint16_t source_x[1024],source_y[1024];
    unsigned mode,focus,top,action,binding,repeat,repeat_pad,settings_revision,pending_mode;
    int dirty,layout_dirty,settings_dirty,running,exiting,launch,restart;
    int memory_warning,capture,controller_hidden,pointer_owner,game_w,game_h,game_heap,game_fps,rendering,building,busy;
    uint64_t media,hash,repeat_at,stats_at;
    uint16_t *game_pixels;
    char current_name[256],status[160];
} f={.mode=LIBRARY,.pointer_owner=-1};

/* Retained system commands exist only on library/settings screens. Gameplay
 * writes cached controls directly into its mapped RGB565 buffers. This follows
 * the emulator menu approach used by game-boy-emulator (GPL-2.0-or-later). */
static pxa_canvas_frame_t *menu_commands;
static uint32_t menu_generation=1;
static uint8_t menu_initialized;
static void draw(void);
static void select_item(unsigned item,int x);
static void change_mode(unsigned mode);
static void menu_pad(uint32_t pressed);
static void refresh_layout(void);
static void release_touches(void);
static unsigned list_count(void) {
    switch(f.mode){case LIBRARY:return f.count;case PAUSED:return 5;case SETTINGS:return 8;
        case RESOLUTION:return nano_resolution_count+2;case MAPPING:return NANO_PAD_BUTTONS;
        case FPS_LIMIT:return 2;case MEMORY:return nano_heap_count;case BINDING:return NJ_KEY_COUNT+1;default:return 0;}
}
static unsigned footer_count(void) {
    return (f.mode==MEMORY || f.mode==FPS_LIMIT)?2:f.mode==LIBRARY?3:f.mode==RESOLUTION?3:f.mode==MAPPING?3:f.mode==SETTINGS?1:0;
}
static int row_height(void) {return nano_clamp(system_text_height()+14,24,40);}
static int header_height(void) {return system_text_height()+18;}
static int notice_height(void) {return (f.status[0] || (f.mode!=LIBRARY && f.mode!=GAME))?system_text_height()+8:0;}
static int footer_height(void) {return footer_count()?system_text_height()+20:6;}
static int rows(void) {return nano_clamp((f.safe.h-header_height()-notice_height()-footer_height())/row_height(),1,32);}
static void wake(void) {f.dirty=1;port_wake();}
static void invalidate(void) {++f.generation;f.layout_dirty=1;wake();}
static void focus_visible(void) {
    unsigned n=list_count(),visible=(unsigned)rows();
    if(f.focus<n){if(f.focus<f.top)f.top=f.focus;else if(f.focus>=f.top+visible)f.top=f.focus-visible+1;}
    if(f.top>n)f.top=n? n-1:0;
}
static int controls_visible(void) {
    return f.settings.touch_mode==1 || (f.settings.touch_mode==0 && !f.controller_hidden);
}
static void profile_path(char out[128]) {snprintf(out,128,"/data/profiles/%016llx.njp",(unsigned long long)f.hash);}
static void save_settings(void) {
    if(!f.settings_dirty)return;
    char path[128];unsigned revision=f.settings_revision;nano_settings snapshot=f.settings;
    if(f.running){mkdir("/data/profiles",0700);profile_path(path);}
    else strcpy(path,"/data/ui-settings.njp");
    if(nano_settings_save(&snapshot,path,port_replace)) {
        if(!f.running)f.defaults=snapshot;
        if(revision==f.settings_revision)f.settings_dirty=0;
    } else snprintf(f.status,sizeof(f.status),"设置未能保存，请检查存储空间");
}
static void edited(void) {f.settings_dirty=1;++f.settings_revision;invalidate();}
static void mask_label(uint16_t *p,int w,int h,const char *text) {
    int x=(w-system_text_width(text))/2;if(x<1)x=1;
    system_text(p,w,h,x,(h-system_text_height())/2,w-x,text);
}
static void refresh_layout(void) {
    if(!f.layout_dirty)return;
    unsigned generation=f.generation;f.building=1;release_touches();
    f.safe=nano_safe_rect(f.video.width,f.video.height,f.video.shape,f.video.insets,f.video.radii);
    f.view=nano_fit(f.safe,f.game_w?f.game_w:f.settings.width,f.game_h?f.game_h:f.settings.height);
    if(f.game_w && f.game_h) {
        for(int x=0;x<f.view.w;++x)f.source_x[x]=(unsigned)x*f.game_w/f.view.w;
        for(int y=0;y<f.view.h;++y)f.source_y[y]=(unsigned)y*f.game_h/f.view.h;
    }
    unsigned style=f.settings.alpha|(f.settings.button_size<<4)|(f.settings.numeric<<6)|(controls_visible()<<7);
    if(!f.controls.count || style!=f.control_style || memcmp(&f.controls.bounds,&f.safe,sizeof(f.safe))) {
        nano_controls_build(&f.controls,f.safe,&f.settings,controls_visible(),mask_label);f.control_style=style;
    }
    f.layout_dirty=f.generation!=generation;f.building=0;
    port_log("J2ME UI canvas=%ux%u safe=%d,%d,%d,%d view=%d,%d,%d,%d controls=%u mask=%u",
        f.video.width,f.video.height,f.safe.x,f.safe.y,f.safe.w,f.safe.h,
        f.view.x,f.view.y,f.view.w,f.view.h,f.controls.count,f.controls.used);
}
int port_game_canvas_size(int dw,int dh,int *w,int *h) {return nano_canvas_size(dw,dh,w,h);}
void port_game_video_environment(const port_video_environment_t *video) {
    f.video=*video;invalidate();
}
static int jar_name(const char *name) {
    size_t n=strlen(name);return n>4 && name[n-4]=='.' && (name[n-3]=='j'||name[n-3]=='J') &&
        (name[n-2]=='a'||name[n-2]=='A') && (name[n-1]=='r'||name[n-1]=='R');
}
static int compare_entries(const void *a,const void *b) {return strcmp(((const library_entry*)a)->name,((const library_entry*)b)->name);}
static void scan(void) {
    f.busy=1;
    if(f.media)pxa_close_handle(f.media);f.media=0;f.count=0;
    uint8_t p[28]={PXA_FS_ROOT_APP_MEDIA};pxa_fs_root_result_t root;
    const pxa_event_t *event=port_rpc(PXA_FS_SERVICE,PXA_FS_OPEN_ROOT,p,sizeof(p));
    if(!event || !pxa_fs_parse_root(event,event->token,&root) || root.status) {
        snprintf(f.status,sizeof(f.status),"媒体目录暂不可用");f.busy=0;wake();return;
    }
    f.media=root.handle;pxa_store_u64(p,f.media);
    for(unsigned i=0;i<4096 && !f.exiting;++i) {
        event=port_rpc(PXA_FS_SERVICE,PXA_FS_READ_DIRECTORY,p,8);pxa_fs_directory_result_t entry;
        if(!event || !pxa_fs_parse_directory(event,event->token,&entry) || entry.status) {
            snprintf(f.status,sizeof(f.status),"读取游戏库失败");break;
        }
        if(entry.end)break;
        if(entry.kind!=1 || entry.name_size>255 || entry.size>JAR_LIMIT || f.count==LIBRARY_CAP)continue;
        char name[256];memcpy(name,entry.name,entry.name_size);name[entry.name_size]=0;
        if(jar_name(name)) {
            library_entry *item=&f.library[f.count++];strcpy(item->name,name);item->size=(uint32_t)entry.size;
        }
    }
    qsort(f.library,f.count,sizeof(*f.library),compare_entries);
    if(f.selected>=f.count)f.selected=f.count?f.count-1:0;
    f.focus=f.count?f.selected:0;f.top=0;focus_visible();
    port_log("J2ME library jars=%u",f.count);f.busy=0;wake();
}
static void import_jar(void) {
    f.busy=1;
    char old[LIBRARY_CAP][256];unsigned previous=f.count;
    for(unsigned i=0;i<previous;++i)strcpy(old[i],f.library[i].name);
    const char purpose[]="选择要打开的 J2ME 游戏（JAR）";
    uint8_t packet[8+3+sizeof(purpose)];memset(packet,0,8);
    pxa_store_u16(packet+4,3);pxa_store_u16(packet+6,sizeof(purpose)-1);
    memcpy(packet+8,"jar",3);memcpy(packet+11,purpose,sizeof(purpose)-1);
    port_video_visible(0);
    const pxa_event_t *event=port_rpc(PXA_FS_SERVICE,PXA_FS_IMPORT_FILES,packet,11+sizeof(purpose)-1);
    pxa_fs_import_result_t result={0};
    if(!event || !pxa_fs_parse_import(event,event->token,&result))result.status=PXA_STATUS_PROTOCOL_ERROR;
    port_video_visible(1);
    if(!result.status)snprintf(f.status,sizeof(f.status),"已导入 %u 个 JAR",result.imported);
    else snprintf(f.status,sizeof(f.status),"%s",result.status==PXA_STATUS_CANCELLED?"已取消选择":
        result.status==PXA_STATUS_UNSUPPORTED?"系统暂不支持文件选择":"打开 JAR 失败");
    scan();f.busy=0;
    if(!result.status && result.imported==1) {
        unsigned added=0,index=0;
        for(unsigned i=0;i<f.count;++i) {
            int found=0;for(unsigned j=0;j<previous;++j)if(!strcmp(old[j],f.library[i].name)){found=1;break;}
            if(!found){++added;index=i;}
        }
        if(added==1 || f.count==1){f.selected=added==1?index:0;f.launch=1;}
    }
    invalidate();
}
void *nano_frontend_read_jar(const char *name,nj_u4 *size) {
    f.busy=1;
    uint8_t p[270];size_t length=strlen(name);pxa_store_u64(p,f.media);
    pxa_store_u32(p+8,PXA_FS_OPEN_READ);pxa_store_u16(p+12,(uint16_t)length);memcpy(p+14,name,length);
    const pxa_event_t *event=port_rpc(PXA_FS_SERVICE,PXA_FS_OPEN_AT,p,(uint32_t)length+14);
    pxa_fs_at_result_t file;
    if(!event || !pxa_fs_parse_open_at(event,event->token,&file) || file.status){f.busy=0;return NULL;}
    if(file.kind!=1 || !file.size || file.size>JAR_LIMIT){pxa_close_handle(file.handle);f.busy=0;return NULL;}
    void *bytes=malloc(file.size);unsigned offset=0;
    while(bytes && offset<file.size && !f.exiting) {
        unsigned chunk=(unsigned)(file.size-offset);if(chunk>4096)chunk=4096;
        int got=pxa_io(file.handle,PXA_FS_IO_READ,(uint8_t*)bytes+offset,chunk);
        if(got==PXA_STATUS_WOULD_BLOCK){port_sleep(1);continue;}
        if(got<=0 || (uint64_t)got>file.size-offset){free(bytes);bytes=NULL;break;}
        offset+=(unsigned)got;port_poll_input();
    }
    pxa_close_handle(file.handle);if(f.exiting){free(bytes);bytes=NULL;}
    f.busy=0;if(bytes)*size=(nj_u4)file.size;return bytes;
}
static void release_touches(void) {
    if(f.pointer_owner>=0) {
        touch *t=&f.touches[f.pointer_owner];
        nano_input_push(&f.input,(nj_event){.type=NJ_EV_POINTER_UP,.x=t->x,.y=t->y});
    }
    for(unsigned i=0;i<8;++i)for(unsigned k=1;k<NJ_KEY_COUNT;++k)
        if(f.touches[i].keys&(UINT64_C(1)<<k))nano_input_key(&f.input,k,0);
    f.pointer_owner=-1;memset(f.touches,0,sizeof(f.touches));
}
void nano_frontend_cancel_input(void) {
    release_touches();nano_input_release(&f.input);
    for(unsigned i=0;i<2;++i)f.blocked[i]=f.pad_buttons[i];
    memset(f.physical,0,sizeof(f.physical));wake();
}
static void change_mode(unsigned mode) {
    if(f.rendering){f.pending_mode=mode+1;wake();return;}
    if(f.running && f.mode==GAME && mode!=GAME)nano_frontend_cancel_input();
    f.mode=mode;f.focus=f.top=0;f.capture=0;f.repeat=f.repeat_at=0;
    if(mode==LIBRARY)f.focus=f.selected;
    if(mode==MAPPING)f.focus=f.binding;
    if(mode==BINDING)f.focus=f.settings.map[f.binding];
    if(mode==GAME)for(unsigned i=0;i<2;++i)f.blocked[i]=f.pad_buttons[i];
    focus_visible();invalidate();
}
static void back(void) {
    if(f.capture){f.capture=0;wake();return;}
    switch(f.mode) {
        case GAME:change_mode(PAUSED);break;
        case PAUSED:change_mode(GAME);break;
        case SETTINGS:change_mode(f.running?PAUSED:LIBRARY);break;
        case FPS_LIMIT:case MEMORY:case RESOLUTION:case MAPPING:change_mode(SETTINGS);break;
        case BINDING:change_mode(MAPPING);break;
        default:f.exiting=1;port_wake();break;
    }
}
static void change_value(unsigned row,int direction) {
    if(f.mode==SETTINGS) {
        if(row==1)f.settings.touch_mode=(f.settings.touch_mode+3+direction)%3;
        else if(row==2)f.settings.alpha=(uint8_t)nano_clamp(f.settings.alpha+direction,1,7);
        else if(row==3)f.settings.button_size=(f.settings.button_size+3+direction)%3;
        else return;
    } else if(f.mode==RESOLUTION && row>=nano_resolution_count) {
        if(row==nano_resolution_count)f.settings.width=(uint16_t)nano_clamp(f.settings.width+8*direction,64,
            nano_clamp((int)(NANO_GAME_PIXELS/f.settings.height),64,640));
        else f.settings.height=(uint16_t)nano_clamp(f.settings.height+8*direction,64,
            nano_clamp((int)(NANO_GAME_PIXELS/f.settings.width),64,640));
    } else if(f.mode==MAPPING && row<NANO_PAD_BUTTONS)
        f.settings.map[row]=(f.settings.map[row]+NANO_ACTION_MENU+1+direction)%(NANO_ACTION_MENU+1);
    else return;
    edited();
}
static void select_item(unsigned item,int x) {
    if(f.busy)return;
    unsigned n=list_count();
    if(f.mode==LIBRARY) {
        if(item<n){f.selected=item;f.launch=1;}
        else if(item==n)f.action=ACTION_IMPORT;
        else if(item==n+1)change_mode(SETTINGS);
        else f.exiting=1;
    } else if(f.mode==PAUSED) {
        if(item==0)change_mode(GAME);
        else if(item==1)change_mode(SETTINGS);
        else {f.restart=item==3;f.action=item==2?ACTION_IMPORT:ACTION_NONE;f.input.quit=1;change_mode(GAME);}
    } else if(f.mode==SETTINGS) {
        if(item==0)change_mode(RESOLUTION);
        else if(item>=1 && item<=3)change_value(item,x>=0 && x<f.safe.x+f.safe.w/2?-1:1);
        else if(item==4)change_mode(MAPPING);
        else if(item==5){nano_settings defaults;nano_settings_default(&defaults);memcpy(f.settings.map,defaults.map,sizeof(defaults.map));edited();}
        else if(item==6)change_mode(MEMORY);
        else if(item==7)change_mode(FPS_LIMIT);
        else back();
    } else if(f.mode==FPS_LIMIT) {
        if(item<2){f.settings.fps_cap=item?60:30;edited();}
        else if(item==2)back();
        else if(f.running && f.settings.fps_cap!=f.game_fps){f.restart=1;f.input.quit=1;change_mode(GAME);}
        else back();
    } else if(f.mode==MEMORY) {
        if(item<nano_heap_count){f.settings.heap_kib=nano_heap_presets[item];edited();}
        else if(item==nano_heap_count)back();
        else if(f.running && f.settings.heap_kib!=f.game_heap){f.restart=1;f.input.quit=1;change_mode(GAME);}
        else back();
    } else if(f.mode==RESOLUTION) {
        if(item<nano_resolution_count) {
            f.settings.width=nano_resolution_presets[item][0];f.settings.height=nano_resolution_presets[item][1];edited();
            if(!f.running)change_mode(SETTINGS);
        } else if(item<n)change_value(item,x>=0 && x<f.safe.x+f.safe.w/2?-1:1);
        else if(item==n)back();
        else if(item==n+1){f.settings.width=240;f.settings.height=320;edited();}
        else if(f.running && (f.settings.width!=f.game_w || f.settings.height!=f.game_h)) {
            f.restart=1;f.input.quit=1;change_mode(GAME);
        } else back();
    } else if(f.mode==MAPPING) {
        if(item<n){f.binding=item;change_mode(BINDING);f.focus=f.settings.map[f.binding];focus_visible();}
        else if(item==n){f.capture=1;wake();}
        else if(item==n+2){nano_settings_phone_keys(&f.settings);edited();}
        else {nano_settings defaults;nano_settings_default(&defaults);memcpy(f.settings.map,defaults.map,sizeof(defaults.map));edited();}
    } else if(f.mode==BINDING && item<=NANO_ACTION_MENU) {
        f.settings.map[f.binding]=(uint8_t)item;edited();unsigned row=f.binding;change_mode(MAPPING);f.focus=row;focus_visible();
    }
    wake();
}
static void menu_pad(uint32_t pressed) {
    unsigned total=list_count()+footer_count();if(!total)return;
    if(pressed&PXA_CONTROLLER_B){back();return;}
    if(f.busy)return;
    if(f.mode==LIBRARY && (pressed&PXA_CONTROLLER_X)){f.action=ACTION_IMPORT;wake();return;}
    if(f.mode==LIBRARY && (pressed&PXA_CONTROLLER_Y)){f.action=ACTION_SCAN;wake();return;}
    if(pressed&PXA_CONTROLLER_UP)f.focus=f.focus?f.focus-1:total-1;
    else if(pressed&PXA_CONTROLLER_DOWN)f.focus=(f.focus+1)%total;
    else if(pressed&(PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT)) {
        int direction=pressed&PXA_CONTROLLER_RIGHT?1:-1;
        if((f.mode==SETTINGS && f.focus>=1 && f.focus<=3) ||
           (f.mode==RESOLUTION && f.focus>=nano_resolution_count && f.focus<list_count()) ||
           (f.mode==MAPPING && f.focus<NANO_PAD_BUTTONS))change_value(f.focus,direction);
        else if(f.focus>=list_count())f.focus=list_count()+(f.focus-list_count()+footer_count()+direction)%footer_count();
    } else if(pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_START))select_item(f.focus,-1);
    focus_visible();wake();
}
void port_game_controller(unsigned index,int connected,uint32_t buttons) {
    if(index>=2)return;uint32_t old=f.pad_buttons[index];buttons=connected?buttons:0;
    if(old==buttons && f.pad_connected[index]==!!connected)return;
    f.pad_buttons[index]=buttons;f.pad_connected[index]=!!connected;
    uint32_t pressed=buttons&~old;f.blocked[index]&=buttons;
    if(!connected)nano_input_pad(&f.input,index,0,f.settings.map);
    if(f.mode==GAME && f.running) {
        if(pressed && f.settings.touch_mode==0 && !f.controller_hidden) {
            f.controller_hidden=1;nano_frontend_cancel_input();invalidate();
            f.blocked[index]&=~pressed;
        }
        if(!f.pad_connected[0] && !f.pad_connected[1] && f.controller_hidden){f.controller_hidden=0;invalidate();}
        int menu=0;
        for(unsigned i=0;i<NANO_PAD_BUTTONS;++i)if((pressed&(1u<<i)) && f.settings.map[i]==NANO_ACTION_MENU)menu=1;
        const uint32_t chord=PXA_CONTROLLER_START|PXA_CONTROLLER_SELECT;
        if((buttons&chord)==chord && (old&chord)!=chord)menu=1;
        if(menu){change_mode(PAUSED);return;}
        nano_input_pad(&f.input,index,buttons&~f.blocked[index],f.settings.map);
        if(controls_visible())wake();else port_wake();
    } else if(pressed && f.capture) {
        for(unsigned i=0;i<NANO_PAD_BUTTONS;++i)if(pressed&(1u<<i)) {
            f.binding=i;change_mode(BINDING);f.focus=f.settings.map[i];focus_visible();break;
        }
    } else {
        if(pressed)menu_pad(pressed);
        uint32_t direction=buttons&(PXA_CONTROLLER_UP|PXA_CONTROLLER_DOWN);
        if(direction && !(direction&(direction-1))) {
            if(f.repeat!=direction || f.repeat_pad!=index){f.repeat=direction;f.repeat_pad=index;f.repeat_at=port_time_us()+350000;}
        } else if(f.repeat_pad==index)f.repeat=f.repeat_at=0;
    }
}
static unsigned keyboard_key(int code) {
    if(code>='0' && code<='9')return NJ_KEY_0+code-'0';
    switch(code){case 128:case 224:return NJ_KEY_UP;case 129:case 225:return NJ_KEY_DOWN;
        case 130:case 226:return NJ_KEY_LEFT;case 131:case 227:return NJ_KEY_RIGHT;
        case 13:case 228:case ' ':return NJ_KEY_FIRE;case 232:return NJ_KEY_SOFT_LEFT;
        case 229:case 233:return NJ_KEY_SOFT_RIGHT;case 234:case '*':return NJ_KEY_STAR;
        case 235:case '#':return NJ_KEY_POUND;default:return NJ_KEY_NONE;}
}
void nano_frontend_key(int code,int down) {
    if(code<0 || code>=256 || f.physical[code]==!!down)return;f.physical[code]=!!down;
    if(code==27 || code==230){if(down)back();return;}
    if(f.mode==GAME && f.running)nano_input_key(&f.input,keyboard_key(code),down);
    else if(down) {
        uint32_t b=code==128 || code==224?PXA_CONTROLLER_UP:code==129 || code==225?PXA_CONTROLLER_DOWN:
            code==130 || code==226?PXA_CONTROLLER_LEFT:code==131 || code==227?PXA_CONTROLLER_RIGHT:
            code==13 || code==228?PXA_CONTROLLER_A:code==229?PXA_CONTROLLER_B:code==232?PXA_CONTROLLER_X:0;
        if(b)menu_pad(b);
    }
    wake();
}
static int menu_hit(int x,int y) {
    if(!nano_inside(f.safe,x,y))return -1;
    int header=header_height(),footer=footer_height();unsigned n=list_count(),feet=footer_count();
    if(y<f.safe.y+header) {
        if(f.mode!=LIBRARY && x<f.safe.x+34)return -2;
        if(f.mode==LIBRARY && x>=f.safe.x+f.safe.w-52)return -3;
        return -1;
    }
    header+=notice_height();
    if(y<f.safe.y+header)return -1;
    if(feet && y>=f.safe.y+f.safe.h-footer) {
        int cell=(f.safe.w-8)/(int)feet;
        return (int)n+nano_clamp((x-f.safe.x-4)/cell,0,(int)feet-1);
    }
    unsigned i=f.top+(unsigned)(y-f.safe.y-header)/row_height();return i<n && i<f.top+(unsigned)rows()?(int)i:-1;
}
static void set_touch_keys(touch *t,uint64_t next) {
    if(t->keys==next)return;
    for(unsigned k=1;k<NJ_KEY_COUNT;++k)if((t->keys^next)&(UINT64_C(1)<<k))nano_input_key(&f.input,k,(next&(UINT64_C(1)<<k))!=0);
    t->keys=next;wake();
}
void nano_frontend_pointer(int x,int y,unsigned phase,unsigned buttons,unsigned id) {
    if(f.building)return;
    (void)buttons;int slot=-1;
    for(unsigned i=0;i<8;++i)if(f.touches[i].active && f.touches[i].id==id){slot=(int)i;break;}
    if(phase==PXA_POINTER_DOWN) {
        if(slot<0)for(unsigned i=0;i<8;++i)if(!f.touches[i].active){slot=(int)i;break;}
        if(slot<0)return;
        touch *t=&f.touches[slot];*t=(touch){.active=1,.id=id,.start_y=y,.last_y=y,.mode=f.mode,.item=-1,.button=-1};
        if(f.mode!=GAME || !f.running) {t->role=3;t->item=menu_hit(x,y);if(t->item>=0)f.focus=t->item;wake();return;}
        if(f.settings.touch_mode==0 && f.controller_hidden) {f.controller_hidden=0;invalidate();t->role=5;return;}
        unsigned action;int button;uint64_t keys=nano_controls_hit(&f.controls,x,y,&action,&button);
        if(action){t->role=4;t->action=action;t->button=button;wake();return;}
        if(keys){t->role=1;set_touch_keys(t,keys);wake();return;}
        if(nano_inside(f.view,x,y) && f.pointer_owner<0){t->role=2;f.pointer_owner=slot;}
        else {t->role=5;return;}
    }
    if(slot<0)return;touch *t=&f.touches[slot];
    if(t->role==1) {
        unsigned action;int button;uint64_t keys=phase>=PXA_POINTER_UP?0:nano_controls_hit(&f.controls,x,y,&action,&button);
        set_touch_keys(t,keys);
    } else if(t->role==2) {
        int px=nano_clamp((x-f.view.x)*f.game_w/f.view.w,0,f.game_w-1);
        int py=nano_clamp((y-f.view.y)*f.game_h/f.view.h,0,f.game_h-1);t->x=px;t->y=py;
        nano_input_push(&f.input,(nj_event){.type=phase==PXA_POINTER_DOWN?NJ_EV_POINTER_DOWN:phase==PXA_POINTER_MOVE?NJ_EV_POINTER_MOVE:NJ_EV_POINTER_UP,.x=px,.y=py});
        if(phase>=PXA_POINTER_UP)f.pointer_owner=-1;
    } else if(t->role==3 && f.mode==(unsigned)t->mode) {
        if(phase==PXA_POINTER_MOVE && abs(y-t->start_y)>6)t->dragged=1;
        if(t->dragged && phase==PXA_POINTER_MOVE) {
            int step=(t->last_y-y)/row_height();
            if(step){int maximum=(int)list_count()-rows();if(maximum<0)maximum=0;
                f.top=(unsigned)nano_clamp((int)f.top+step,0,maximum);t->last_y=y;wake();}
        }
        if(phase==PXA_POINTER_UP && !t->dragged && t->item==menu_hit(x,y)) {
            int item=t->item;memset(t,0,sizeof(*t));
            if(item==-2)back();else if(item==-3){f.action=ACTION_SCAN;wake();}else if(item>=0)select_item((unsigned)item,x);
            return;
        }
    } else if(t->role==4 && phase==PXA_POINTER_UP) {
        unsigned action;int button;nano_controls_hit(&f.controls,x,y,&action,&button);
        unsigned expected=t->action;memset(t,0,sizeof(*t));
        if(action==expected){if(action==NANO_ACTION_MENU)change_mode(PAUSED);
            else if(action==NANO_ACTION_KEYBOARD){f.settings.numeric=!f.settings.numeric;edited();}}
        return;
    }
    int role=t->role;
    if(phase>=PXA_POINTER_UP)memset(t,0,sizeof(*t));
    if(role==3 || role==4)wake();else port_wake();
}

static int menu_dp(int coordinate,int axis) {
    int origin=((axis?f.video.display_height:f.video.display_width)-
        (axis?f.video.height:f.video.width)*f.video.scale)/2;
    return (int)(((int64_t)(origin+coordinate*(int)f.video.scale)*65536+
        f.video.density_q16/2)/f.video.density_q16);
}
static uint32_t rgba(uint16_t color) {
    return ((uint32_t)(color>>11)*255/31<<24)|
        ((uint32_t)((color>>5)&63)*255/63<<16)|((uint32_t)(color&31)*255/31<<8)|255;
}
static void box(uint16_t *p,nano_rect r,uint16_t color) {
    if(menu_commands) {
        int x=menu_dp(r.x,0),y=menu_dp(r.y,1);
        pxa_canvas_rect_rgba(menu_commands,x,y,menu_dp(r.x+r.w,0)-x,
            menu_dp(r.y+r.h,1)-y,rgba(color),0,0,0);return;
    }
    int x0=nano_clamp(r.x,0,f.video.width),y0=nano_clamp(r.y,0,f.video.height);
    int x1=nano_clamp(r.x+r.w,0,f.video.width),y1=nano_clamp(r.y+r.h,0,f.video.height);
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)p[y*f.video.width+x]=color;
}
static void text(uint16_t *p,nano_rect r,const char *s,uint16_t color,int center) {
    if(r.w<6 || r.h<1)return;
    if(menu_commands) {
        size_t size=strlen(s);if(size>UINT16_MAX-24){menu_commands->failed=1;return;}
        unsigned pixels=system_text_pixels()*f.video.scale;
        static const uint8_t sizes[]={8,10,12,14,16,18,20,24,28,32,40,48,64,80,96,128};
        unsigned chosen=8;
        for(unsigned i=0;i<sizeof(sizes);++i)if(sizes[i]<=pixels)chosen=sizes[i];
        int x=menu_dp(r.x+(center?0:5),0),y=menu_dp(r.y,1);
        int ok=pxa_canvas_primitive(menu_commands,12,(uint16_t)(24+size)) &&
            pxa_put_u32(menu_commands,x) && pxa_put_u32(menu_commands,y) &&
            pxa_put_u32(menu_commands,menu_dp(r.x+r.w-3,0)-x) &&
            pxa_put_u32(menu_commands,menu_dp(r.y+r.h,1)-y) &&
            pxa_put_u32(menu_commands,rgba(color)) && pxa_put_u16(menu_commands,chosen) &&
            pxa_put_u8(menu_commands,center?1:0) && pxa_put_u8(menu_commands,0) &&
            pxa_put_bytes(menu_commands,(const uint8_t*)s,size);
        if(ok)++menu_commands->records;
        return;
    }
    int x=r.x+5;if(center){x=r.x+(r.w-system_text_width(s))/2;if(x<r.x+3)x=r.x+3;}
    system_text_tint(p,f.video.width,f.video.height,x,r.y+(r.h-system_text_height())/2,r.x+r.w-x-3,s,color);
}
static const char *title(void) {
    return f.mode==FPS_LIMIT?"帧率上限":f.mode==MEMORY?"Java 内存":f.mode==LIBRARY?"nano J2ME":f.mode==PAUSED?"游戏菜单":f.mode==SETTINGS?"运行设置":
        f.mode==RESOLUTION?"JAR 分辨率":f.mode==MAPPING?"手柄映射":"映射到";
}
static void row_label(unsigned row,char left[256],char right[128]) {
    left[0]=right[0]=0;
    if(f.mode==LIBRARY){snprintf(left,256,"%s",f.library[row].name);return;}
    if(f.mode==PAUSED) {
        static const char *const names[]={"继续游戏","运行设置","打开其他 JAR","重新启动","返回游戏库"};strcpy(left,names[row]);
    } else if(f.mode==SETTINGS) {
        static const char *const names[]={"运行分辨率","悬浮按键","按键透明度","按键大小","手柄按键映射","恢复默认映射","Java 内存","帧率上限"};strcpy(left,names[row]);
        if(row==0)snprintf(right,128,"%ux%u%s",f.settings.width,f.settings.height,
            f.running && (f.settings.width!=f.game_w || f.settings.height!=f.game_h)?" *":"");
        if(row==7)snprintf(right,128,"%u FPS%s",f.settings.fps_cap,f.running && f.settings.fps_cap!=f.game_fps?" *":"");
        if(row==6)snprintf(right,128,"%u KiB%s",f.settings.heap_kib,f.running && f.settings.heap_kib!=f.game_heap?" *":"");
        if(row==1)strcpy(right,(const char*[]){"自动","显示","隐藏"}[f.settings.touch_mode]);
        if(row==2)snprintf(right,128,"%u%%",f.settings.alpha*100/8);
        if(row==3)strcpy(right,(const char*[]){"小","标准","大"}[f.settings.button_size]);
    } else if(f.mode==FPS_LIMIT) {
        snprintf(left,256,"%u FPS",row?60:30);if(f.settings.fps_cap==(row?60:30))strcpy(right,"已选");
    } else if(f.mode==MEMORY) {
        snprintf(left,256,"%u KiB",nano_heap_presets[row]);if(f.settings.heap_kib==nano_heap_presets[row])strcpy(right,"已选");
    } else if(f.mode==RESOLUTION) {
        if(row<nano_resolution_count){snprintf(left,256,"%u x %u",nano_resolution_presets[row][0],nano_resolution_presets[row][1]);
            if(f.settings.width==nano_resolution_presets[row][0] && f.settings.height==nano_resolution_presets[row][1])strcpy(right,"已选");}
        else {strcpy(left,row==nano_resolution_count?"自定义宽度":"自定义高度");snprintf(right,128,"- %u +",row==nano_resolution_count?f.settings.width:f.settings.height);}
    } else if(f.mode==MAPPING){strcpy(left,nano_pad_names[row]);snprintf(right,128,"%s",nano_action_name(f.settings.map[row]));}
    else if(f.mode==BINDING){snprintf(left,256,"%s",nano_action_name(row));if(f.settings.map[f.binding]==row)strcpy(right,"已选");}
}
static const char *footer(unsigned i) {
    if(f.mode==MEMORY || f.mode==FPS_LIMIT)return i?(f.running?"应用重启":"应用"):"返回";
    if(f.mode==LIBRARY)return (const char*[]){"打开JAR","设置","退出"}[i];
    if(f.mode==RESOLUTION)return (const char*[]){"返回","240x320",f.running?"应用重启":"应用"}[i];
    if(f.mode==MAPPING)return i==2?"手机键位":i?"重置映射":f.capture?"等待按键":"识别按键";
    return "保存并返回";
}
static void draw_menu(uint16_t *p) {
    box(p,(nano_rect){0,0,f.video.width,f.video.height},0x0842);
    nano_rect s=f.safe;int header=header_height(),row_h=row_height(),foot=footer_height();
    box(p,(nano_rect){s.x,s.y,s.w,header},0x10e5);
    if(f.mode!=LIBRARY)text(p,(nano_rect){s.x+2,s.y+2,28,header-4},"<",0xffff,1);
    text(p,(nano_rect){s.x+(f.mode==LIBRARY?5:32),s.y,s.w-(f.mode==LIBRARY?58:36),header},title(),0xffff,0);
    if(f.mode==LIBRARY)text(p,(nano_rect){s.x+s.w-52,s.y,50,header},"刷新",0x8e5f,1);
    int notice=notice_height();
    if(notice) {
        char detail[320];
        if(f.mode==PAUSED){if(f.memory_warning)snprintf(detail,sizeof(detail),"内存不足：可调整 Java 内存或运行分辨率");
            else snprintf(detail,sizeof(detail),"%s  %dx%d",f.current_name,f.game_w,f.game_h);}
        else if(f.mode==FPS_LIMIT)snprintf(detail,sizeof(detail),"上限不保证实际帧率；运行中应用会重新启动游戏");
        else if(f.mode==MEMORY)snprintf(detail,sizeof(detail),"提高可用 Java 内存；大 JAR 或高分辨率需留出余量");
        else if(f.mode==RESOLUTION)snprintf(detail,sizeof(detail),"%s",f.running?"应用后重新启动当前 JAR":"选择预设，或用 +/- 调整尺寸");
        else if(f.mode==MAPPING)snprintf(detail,sizeof(detail),"%s",f.capture?"请按要映射的手柄键":"START + SELECT 可随时打开菜单");
        else if(f.mode==BINDING)snprintf(detail,sizeof(detail),"%s -> 选择游戏按键",nano_pad_names[f.binding]);
        else if(f.mode==SETTINGS)snprintf(detail,sizeof(detail),"%s",f.running?"设置保存在当前 JAR 的配置中":"默认设置，手柄可自动收起悬浮按键");
        else snprintf(detail,sizeof(detail),"%s",f.status);
        text(p,(nano_rect){s.x+4,s.y+header,s.w-8,notice},detail,0x9cf3,0);
    }
    header+=notice;
    unsigned count=list_count();
    if(f.mode==LIBRARY && !count) {
        int middle=s.y+header+(s.h-header-foot)/2;
        text(p,(nano_rect){s.x,middle-24,s.w,24},"还没有 JAR 游戏",0xffff,1);
        text(p,(nano_rect){s.x,middle,s.w,24},"点击下方“打开JAR”选择文件",0x9cf3,1);
    }
    for(unsigned line=0;line<(unsigned)rows() && f.top+line<count;++line) {
        unsigned i=f.top+line;nano_rect r={s.x+4,s.y+header+(int)line*row_h+2,s.w-8,row_h-4};
        box(p,r,i==f.focus?0x226e:0x10a3);
        if(i==f.focus)box(p,(nano_rect){r.x,r.y,3,r.h},0x55ff);
        char left[256],right[128];row_label(i,left,right);
        int right_w=*right?system_text_width(right)+10:0;
        if(right_w>r.w*3/5)right_w=r.w*3/5;
        text(p,(nano_rect){r.x+3,r.y,r.w-right_w-4,r.h},left,0xffff,0);
        if(right_w)text(p,(nano_rect){r.x+r.w-right_w,r.y,right_w,r.h},right,0xaebf,1);
    }
    unsigned feet=footer_count();
    if(feet)for(unsigned i=0;i<feet;++i) {
        int cell=(s.w-8)/(int)feet;
        nano_rect r={s.x+4+(int)i*cell,s.y+s.h-foot+3,cell-3,foot-6};
        box(p,r,f.focus==count+i?0x2b54:i==0?0x226e:0x18e5);text(p,r,footer(i),0xffff,1);
    }
}
static void draw_game(uint16_t *p,unsigned index) {
    if(f.buffer_generation[index]!=f.generation){memset(p,0,(size_t)f.video.width*f.video.height*2);f.buffer_generation[index]=f.generation;f.buffer_game_version[index]=0;}
    nano_rect v=f.view;
    if(f.game_pixels && v.w>0 && v.h>0 && f.buffer_game_version[index]!=f.game_version) {
        for(int y=0;y<v.h;++y) {
            const uint16_t *src=f.game_pixels+(size_t)f.source_y[y]*f.game_w;
            uint16_t *dest=p+(size_t)(v.y+y)*f.video.width+v.x;
            if(v.w==f.game_w)memcpy(dest,src,(size_t)v.w*2);
            else if(y && f.source_y[y]==f.source_y[y-1])memcpy(dest,dest-f.video.width,(size_t)v.w*2);
            else for(int x=0;x<v.w;++x)dest[x]=src[f.source_x[x]];
        }
    } else if(f.game_pixels) {
        /* An idle MIDlet needs only the pixels underneath changing keys
         * restored. Each mapped buffer keeps its own game generation. */
        for(unsigned i=0;i<f.controls.count;++i) {
            nano_rect r=f.controls.buttons[i].rect;
            int x0=r.x>v.x?r.x:v.x,y0=r.y>v.y?r.y:v.y;
            int x1=r.x+r.w<v.x+v.w?r.x+r.w:v.x+v.w,y1=r.y+r.h<v.y+v.h?r.y+r.h:v.y+v.h;
            for(int y=y0;y<y1;++y) {
                const uint16_t *src=f.game_pixels+(size_t)f.source_y[y-v.y]*f.game_w;
                uint16_t *dest=p+(size_t)y*f.video.width;
                for(int x=x0;x<x1;++x)dest[x]=src[f.source_x[x-v.x]];
            }
        }
    }
    f.buffer_game_version[index]=f.game_version;
    /* Restore only the bars underneath cached controls. Re-blending a static
     * black margin would otherwise make translucent keys darken each frame. */
    for(unsigned i=0;i<f.controls.count;++i) {
        nano_rect r=f.controls.buttons[i].rect;
        for(int y=r.y;y<r.y+r.h;++y) {
            uint16_t *row=p+(size_t)y*f.video.width;
            if(y<v.y || y>=v.y+v.h)memset(row+r.x,0,(size_t)r.w*2);
            else {
                int end=r.x+r.w<v.x?r.x+r.w:v.x;if(end>r.x)memset(row+r.x,0,(size_t)(end-r.x)*2);
                int start=r.x>v.x+v.w?r.x:v.x+v.w;if(r.x+r.w>start)memset(row+start,0,(size_t)(r.x+r.w-start)*2);
            }
        }
    }
    uint64_t keys=0;uint32_t buttons=0;
    for(unsigned k=1;k<NJ_KEY_COUNT;++k)if(f.input.refs[k])keys|=UINT64_C(1)<<k;
    for(unsigned i=0;i<8;++i)if(f.touches[i].active && f.touches[i].role==4 && f.touches[i].button>=0)buttons|=1u<<f.touches[i].button;
    nano_controls_draw(&f.controls,p,f.video.width,keys,buttons);
}
static void draw(void) {
    if(!f.dirty)return;f.dirty=0;f.rendering=1;refresh_layout();
    int ok=0;
    if(f.mode!=GAME) {
        /* 32 visible rows with 255-byte UTF-8 filenames fit without truncation. */
        static uint8_t commands[16384];uint8_t packet[1024];pxa_canvas_frame_t frame;
        port_video_visible(0);pxa_canvas_begin(&frame,commands,sizeof(commands));
        menu_commands=&frame;draw_menu(NULL);menu_commands=NULL;
        ok=pxa_canvas_present_regions(2,&frame,&menu_generation,&menu_initialized,
            PXA_UI_EVENT_MASK_POINTER|PXA_UI_EVENT_MASK_CONTROLLER_STATE,NULL,0,
            packet,sizeof(packet),NULL,0);
        f.buffer_generation[0]=f.buffer_generation[1]=0;
    } else {
        if(menu_initialized) {
            uint8_t packet[256];pxa_ui_transaction_t tx={0};
            if(pxa_ui_transaction_begin(&tx,++menu_generation,PXA_UI_TRANSACTION_REPLACE_SURFACE,packet,sizeof(packet)) &&
                pxa_ui_create(&tx,1,0,0,PXA_UI_NODE_ROOT) &&
                pxa_ui_set_rgba(&tx,1,PXA_UI_PROPERTY_BACKGROUND,0) &&
                pxa_ui_set_event_mask(&tx,1,PXA_UI_EVENT_MASK_POINTER|PXA_UI_EVENT_MASK_CONTROLLER_STATE) &&
                pxa_ui_transaction_commit(&tx))menu_initialized=0;
            else if(tx.active)pxa_ui_transaction_cancel(&tx);
        }
        port_video_visible(1);
        unsigned index;uint16_t *pixels=port_video_begin(&index);
        if(pixels){draw_game(pixels,index);port_video_end(index);ok=1;}
    }
    /* Host replies can dispatch input while this frame is being submitted. */
    f.rendering=0;f.dirty|=f.layout_dirty || !ok;++f.frames;
    if(f.pending_mode){unsigned mode=f.pending_mode-1;f.pending_mode=0;change_mode(mode);}
    uint64_t now=port_time_us();
    if(now-f.stats_at>=5000000) {
        port_log("J2ME UI frames=%u overlay_pixels=%llu mask_rebuilds=%u mode=%u",f.frames,
            (unsigned long long)f.controls.blended_pixels,f.controls.cache_rebuilds,f.mode);
        f.stats_at=now;f.frames=0;f.controls.blended_pixels=0;
    }
}
static void menu_tick(void) {
    if(f.repeat && f.repeat_at && port_time_us()>=f.repeat_at) {
        f.repeat_at=port_time_us()+90000;menu_pad(f.repeat);
    }
    if(f.action && !f.running) {
        unsigned action=f.action;f.action=0;if(action==ACTION_IMPORT)import_jar();else scan();
    }
    save_settings();draw();
}
static unsigned menu_wait_ms(void) {
    if(f.dirty)return 16;
    if(f.repeat && f.repeat_at) {
        uint64_t now=port_time_us();if(now>=f.repeat_at)return 1;
        unsigned wait=(unsigned)((f.repeat_at-now+999)/1000);return wait<200?wait:200;
    }
    return 200;
}
int nano_frontend_init(void) {
    if(!f.video.width)f.video=(port_video_environment_t){.width=320,.height=240,.scale=1,.density_q16=65536};
    nano_settings_default(&f.defaults);nano_settings_load(&f.defaults,"/data/ui-settings.njp");f.settings=f.defaults;
    f.layout_dirty=1;refresh_layout();scan();return 1;
}
int nano_frontend_next_game(char name[256]) {
    while(!f.exiting) {
        menu_tick();
        if(f.launch && f.count) {f.launch=0;strcpy(name,f.library[f.selected].name);save_settings();return 1;}
        port_sleep(menu_wait_ms());
    }
    if(f.media){pxa_close_handle(f.media);f.media=0;}return 0;
}
void nano_frontend_profile(uint64_t hash,const char *name) {
    f.hash=hash;snprintf(f.current_name,sizeof(f.current_name),"%s",name);f.settings=f.defaults;
    char path[128];profile_path(path);nano_settings_load(&f.settings,path);
}
const nano_settings *nano_frontend_settings(void) {return &f.settings;}
int nano_frontend_begin_game(void) {
    f.memory_warning=0;f.game_w=f.settings.width;f.game_h=f.settings.height;f.game_heap=f.settings.heap_kib;f.game_fps=f.settings.fps_cap;
    f.game_pixels=calloc((size_t)f.game_w*f.game_h,sizeof(uint16_t));
    if(!f.game_pixels){snprintf(f.status,sizeof(f.status),"内存不足，请降低运行分辨率");return 0;}
    memset(&f.input,0,sizeof(f.input));memset(f.touches,0,sizeof(f.touches));f.pointer_owner=-1;
    f.restart=0;f.running=1;f.controller_hidden=f.pad_connected[0]||f.pad_connected[1];
    f.game_version=1;
    f.status[0]=0;
    change_mode(GAME);refresh_layout();return 1;
}
int nano_frontend_finish_game(int result,unsigned frames) {
    save_settings();int restart=f.restart;f.restart=0;f.running=0;
    free(f.game_pixels);f.game_pixels=NULL;f.game_w=f.game_h=0;
    if(!restart){f.settings=f.defaults;change_mode(LIBRARY);if(f.memory_warning)snprintf(f.status,sizeof(f.status),"游戏内存不足：可调整 Java 内存或降低分辨率重试");
        else if(result)snprintf(f.status,sizeof(f.status),"启动失败：可调整 Java 内存、分辨率，或检查游戏兼容性");
        else snprintf(f.status,sizeof(f.status),"游戏已结束，%u 帧",frames);}
    return restart;
}
int nano_frontend_display(int *w,int *h,int *stride,void **pixels) {
    if(!f.game_pixels)return NJ_ERR;*w=f.game_w;*h=f.game_h;*stride=f.game_w*2;*pixels=f.game_pixels;return NJ_PIXFMT_RGB565;
}
void nano_frontend_cooperate(void) {
    port_poll_input();
    while(f.running && f.mode!=GAME && !f.input.quit && !f.exiting) {menu_tick();port_sleep(menu_wait_ms());}
    if(f.settings_dirty)save_settings();
    if(f.running && !f.input.quit)draw();
}
void nano_frontend_present(void) {++f.game_version;f.dirty=1;nano_frontend_cooperate();}
int nano_frontend_poll(nj_event *event) {nano_frontend_cooperate();return nano_input_poll(&f.input,event);}
int nano_frontend_in_menu(void) {return !f.running || f.mode!=GAME;}
int nano_frontend_finished(void) {return f.exiting;}
void nano_frontend_error(const char *message) {
    f.running=0;free(f.game_pixels);f.game_pixels=NULL;f.game_w=f.game_h=0;
    f.settings=f.defaults;change_mode(LIBRARY);snprintf(f.status,sizeof(f.status),"%s",message);
}

void nano_frontend_memory_warning(void) {f.memory_warning=1;}
