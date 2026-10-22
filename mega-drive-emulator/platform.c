/* Embedded cartridge-only PicoDrive platform. Core license in vendor/picodrive/COPYING. */
#include "core.h"
#include "vendor/picodrive/pico/pico_int.h"
#include <stdarg.h>
#include <stdio.h>
void lprintf(const char *format,...){char line[240];va_list a;va_start(a,format);vsnprintf(line,sizeof(line),format,a);va_end(a);emu_core_log("%s",line);}
void *plat_mmap(unsigned long addr,size_t size,int need_exec,int fixed){(void)addr;(void)fixed;if(need_exec)return NULL;return calloc(1,size);}
void *plat_mremap(void *p,size_t oldsize,size_t size){(void)oldsize;return realloc(p,size);}
void plat_munmap(void *p,size_t size){(void)size;free(p);}
void *plat_mem_get_for_drc(size_t size){(void)size;return NULL;}
int plat_mem_set_exec(void *p,size_t size){(void)p;(void)size;return -1;}
void cache_flush_d_inval_i(void *a,void *b){(void)a;(void)b;}
carthw_state_chunk *carthw_chunks;
void (*PicoLoadStateHook)(void);
void *p32x_bios_g,*p32x_bios_m,*p32x_bios_s;
/* The frontend accepts MD cartridges only. Media codecs and 32X are not exposed. */
int mp3_get_bitrate(void *f,int n){(void)f;(void)n;return -1;}
void mp3_start_play(void *f,int at){(void)f;(void)at;}
void mp3_update(s32 *b,int n,int s){(void)b;(void)n;(void)s;}
int ogg_get_length(void *f){(void)f;return -1;}
void ogg_start_play(void *f,int at){(void)f;(void)at;}
void ogg_stop_play(void){}
void ogg_update(s32 *b,int n,int s){(void)b;(void)n;(void)s;}
void vgm_frame(void){}
void vgm_reset(void){}
void vgm_finish(void){}
