/* SPDX-License-Identifier: MIT. PXA platform replacement: no threads, audio or disk. */
#include "common/alloc.h"
#include "common/audio.h"
#include "common/debug_console.h"
#include "common/graphics.h"
#include "common/inputs.h"
#include "common/storage.h"
#include "common/log.h"
#include "golf/ui.h"
#include "pxa_port_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static golf_graphics_t graphics;
static golf_inputs_t inputs;
static golf_ui_t ui;
static int score[20];
void *golf_alloc_tracked(size_t n,const char *category) {(void)category;return malloc(n);}
void *golf_realloc_tracked(void *p,size_t n,const char *category) {(void)category;return realloc(p,n);}
void golf_free_tracked(void *p) {free(p);}
golf_graphics_t *golf_graphics_get(void) {return &graphics;}
golf_inputs_t *golf_inputs_get(void) {return &inputs;}
golf_ui_t *golf_ui_get(void) {return &ui;}
void golf_ui_init(void) {memset(&ui,0,sizeof(ui));}
void golf_ui_update(float dt) {(void)dt;}
void golf_audio_init(void) {}
void golf_audio_start_sound(const char *n,const char *p,float v,bool r,bool f) {(void)n;(void)p;(void)v;(void)r;(void)f;}
void golf_audio_stop_sound(const char *n,float t) {(void)n;(void)t;}
void golf_audio_update(float dt) {(void)dt;}
void golf_debug_console_add_tab(const char *name,void (*fn)(void)) {(void)name;(void)fn;}
void golf_debug_console_update(float dt) {(void)dt;}
static void log_v(const char *fmt,va_list ap) {char text[256];vsnprintf(text,sizeof(text),fmt,ap);pxa_log_write(2,text);}
void golf_log_warning(const char *fmt,...) {va_list ap;va_start(ap,fmt);log_v(fmt,ap);va_end(ap);}
void golf_log_note(const char *fmt,...) {va_list ap;va_start(ap,fmt);log_v(fmt,ap);va_end(ap);}
void golf_log_error(const char *fmt,...) {va_list ap;va_start(ap,fmt);log_v(fmt,ap);va_end(ap);}
bool golf_storage_get_num(const char *key,float *value) {int n=0;if(sscanf(key,"stroke_count_level_%d",&n)==1 && n>=0 && n<20 && score[n]) {*value=score[n];return true;}return false;}
void golf_storage_set_num(const char *key,float value) {int n=0;if(sscanf(key,"stroke_count_level_%d",&n)==1 && n>=0 && n<20) score[n]=(int)value;}
void golf_storage_save(void) {/* Session-only best scores. */}
void golf_graphics_update_proj_view_mat(void) {
    graphics.proj_mat=mat4_perspective_projection(66.0f,graphics.viewport_size.x/graphics.viewport_size.y,0.1f,150.0f);
    graphics.view_mat=mat4_look_at(graphics.cam_pos,vec3_add(graphics.cam_pos,graphics.cam_dir),graphics.cam_up);
    graphics.proj_view_mat=mat4_multiply(graphics.proj_mat,graphics.view_mat);
}
/* Debug UI is not part of gameplay and is not displayed in this frontend. */
bool igCheckbox(const char *label,bool *value) {(void)label;(void)value;return false;}
bool igTreeNodeEx_Ptr(const void *id,int flags,const char *fmt,...) {(void)id;(void)flags;(void)fmt;return false;}
void igText(const char *fmt,...) {(void)fmt;}
void igTreePop(void) {}
bool igIsItemHovered(int flags) {(void)flags;return false;}
