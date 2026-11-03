/* SPDX-License-Identifier: GPL-3.0-or-later
 * PXA application adapter. nano_j2me itself remains MIT and unmodified. */
#include "nj2me/nj.h"
#include "port.h"
#include "rpc.h"
#include "pxa_audio.h"
#include "pxa_window.h"
#include "system_font.h"
#include "frontend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
/* The pinned runner calls libc free even for opts.jar_data marked borrowed.
 * Keep this one caller-owned allocation alive until our run/restart loop ends;
 * every other engine allocation retains its normal ownership. */
static void *borrowed_jar;
void nano_runner_free(void *p) {if(p!=borrowed_jar)free(p);}
static unsigned frames;
static nj_audio_tone tones[64];
static int tone_count,tone_index;
static unsigned tone_frame,tone_phase;
static uint64_t tone_at;
/* This frontend owns the floating controls and its input-method policy. */
int __wrap_port_control_key_at(int x,int y,int rts) {(void)x;(void)y;(void)rts;return 0;}
void __wrap_port_draw_controls(uint16_t *p,int w,int h,int rts) {(void)p;(void)w;(void)h;(void)rts;}
int __wrap_port_controls_set_visible(int v) {(void)v;return 0;}
int __wrap_port_controls_visible(void) {return 1;}
void port_game_key(int code,int down) {nano_frontend_key(code,down);}
void port_game_pointer(int x,int y,unsigned phase,unsigned buttons,unsigned id) {nano_frontend_pointer(x,y,phase,buttons,id);}
void port_game_motion(int x,int y) {(void)x;(void)y;}
void port_game_audio(void) {}
void port_game_reset_motion(void) {nano_frontend_cancel_input();}
int port_game_in_menu(void) {return nano_frontend_in_menu();}
int port_game_finished(void) {return nano_frontend_finished();}
static nj_u4 millis(void *ud) {(void)ud;return (nj_u4)(port_time_us()/1000);}
static int display_init(void *ud,int *w,int *h,int *stride,void **pixels) {
    (void)ud;return nano_frontend_display(w,h,stride,pixels);
}
static void display_present(void *ud,int x,int y,int w,int h) {
    (void)ud;(void)x;(void)y;(void)w;(void)h;frames++;nano_frontend_present();
}
static int input_poll(void *ud,nj_event *event) {(void)ud;return nano_frontend_poll(event);}
static void pump_tone(void) {
    if(tone_index>=tone_count || port_time_us()<tone_at)return;
    int16_t samples[320];
    for(unsigned i=0;i<320;i++) {
        while(tone_index<tone_count && tone_frame>=tones[tone_index].duration_ms*16u)
            {tone_index++;tone_frame=tone_phase=0;}
        if(tone_index>=tone_count){samples[i]=0;continue;}
        tone_phase=(tone_phase+tones[tone_index].freq_hz)%16000u;tone_frame++;
        samples[i]=tones[tone_index].freq_hz?(tone_phase<8000?1800:-1800):0;
    }
    tone_at=port_time_us()+20000;port_pcm(samples,320,16000,1);
}
static void idle(void *ud) {(void)ud;nano_frontend_cooperate();pump_tone();if(port_cooperate_due(port_time_us(),8000))port_sleep(0);}
static void sleep_ms(void *ud,nj_u4 ms) {(void)ud;nano_frontend_cooperate();port_sleep(ms);nano_frontend_cooperate();}
static void log_line(void *ud,int level,const char *text) {(void)ud;if(strstr(text,"OutOfMemoryError") || strstr(text,"out of memory") || strstr(text,"out of memory?") || strstr(text,"cannot create the VM"))nano_frontend_memory_warning();
    port_log("J2ME [%d] %s",level,text);}
static void *aligned_alloc_port(void *ud,nj_size size) {(void)ud;void *p=NULL;return posix_memalign(&p,NJ_OBJ_ALIGN,size)?NULL:p;}
static void free_port(void *ud,void *p) {(void)ud;free(p);}
static int now(void *ud,nj_datetime *out) {
    (void)ud;time_t t=time(NULL);struct tm *v=gmtime(&t);if(!v)return 0;
    *out=(nj_datetime){v->tm_year+1900,v->tm_mon,v->tm_mday,v->tm_hour,v->tm_min,v->tm_sec,0,v->tm_wday,0};return 1;
}
static int file_open(void *ud,const char *name,const char *mode,nj_file *out) {
    (void)ud;FILE *fp=fopen(name,mode);if(!fp)return NJ_ERR;
    if(fseek(fp,0,SEEK_END)){fclose(fp);return NJ_ERR;}
    long n=ftell(fp);if(n<0 || fseek(fp,0,SEEK_SET)){fclose(fp);return NJ_ERR;}
    *out=(nj_file){fp,(nj_u4)n,0};return NJ_OK;
}
static nj_s4 file_read(void *ud,nj_file *f,void *buf,nj_u4 n) {
    (void)ud;size_t got=fread(buf,1,n,f->handle);f->pos+=got;return ferror(f->handle)?-1:(nj_s4)got;
}
static nj_s4 file_write(void *ud,nj_file *f,const void *buf,nj_u4 n) {
    (void)ud;size_t wrote=fwrite(buf,1,n,f->handle);f->pos+=wrote;if(f->pos>f->size)f->size=f->pos;
    return wrote==n?(nj_s4)wrote:-1;
}
static int file_seek(void *ud,nj_file *f,nj_u4 pos) {(void)ud;if(fseek(f->handle,pos,SEEK_SET))return NJ_ERR;f->pos=pos;return NJ_OK;}
static void file_close(void *ud,nj_file *f) {(void)ud;fclose(f->handle);f->handle=NULL;}
static int file_remove(void *ud,const char *name) {(void)ud;return remove(name)==0?NJ_OK:NJ_ERR;}
static int file_exists(void *ud,const char *name) {(void)ud;struct stat st;return stat(name,&st)==0;}
static int audio_rate,audio_channels;
static int pcm_open(void *ud,int rate,int channels) {(void)ud;if(rate<8000 || rate>48000 || (channels!=1 && channels!=2))return NJ_ERR;
    audio_rate=rate;audio_channels=channels;return NJ_OK;}
static int pcm_write(void *ud,const nj_s2 *data,int n) {(void)ud;port_pcm(data,n,audio_rate,audio_channels);return n;}
static void pcm_close(void *ud) {(void)ud;audio_rate=audio_channels=0;}
static int tone_play(void *ud,const nj_audio_tone *sequence,int n) {
    (void)ud;if(n<0 || n>64)return NJ_ERR;
    memcpy(tones,sequence,(size_t)n*sizeof(*tones));tone_count=n;tone_index=0;
    tone_at=0;tone_frame=tone_phase=0;return NJ_OK;
}
static void tone_stop(void *ud) {(void)ud;tone_count=tone_index=0;}


void port_game_main(void) {
    port_log("J2ME upstream=" NJ_VERSION_STRING "; adaptive PXA frontend");
    if(!system_fonts_init())port_error("PXA system font service unavailable.");
    nano_frontend_init();
    char name[256];
    while(nano_frontend_next_game(name)) {
        nj_u4 size=0;void *data=nano_frontend_read_jar(name,&size);
        if(!data){nano_frontend_error("无法读取 JAR，文件上限为 2 MiB");continue;}
        if(nano_frontend_finished()){free(data);break;}
        uint64_t hash=UINT64_C(14695981039346656037);
        for(nj_u4 i=0;i<size;i++){hash^=((unsigned char*)data)[i];hash*=UINT64_C(1099511628211);}
        char rms[64];snprintf(rms,sizeof(rms),"/data/rms/%016llx",(unsigned long long)hash);
        mkdir("/data/rms",0700);mkdir(rms,0700);
        nano_frontend_profile(hash,name);
        int restart;
        do {
            if(!nano_frontend_begin_game()){nano_frontend_error("内存不足，请降低运行分辨率");break;}
            const nano_settings *config=nano_frontend_settings();
            nj_port port={.name="PXA",.millis=millis,.display_init=display_init,.display_present=display_present,
                .input_poll=input_poll,.alloc=aligned_alloc_port,.free=free_port,.idle=idle,.sleep_ms=sleep_ms,
                .log=log_line,.now=now,.file_open=file_open,.file_read=file_read,.file_write=file_write,
                .file_seek=file_seek,.file_close=file_close,.file_remove=file_remove,.file_exists=file_exists,
                .pcm_open=pcm_open,.pcm_write=pcm_write,.pcm_close=pcm_close,.tone_play=tone_play,.tone_stop=tone_stop,
                .width=config->width,.height=config->height,.fps_cap=30,.rms_dir=rms};
            nj_run_opts opts={.jar_data=data,.jar_size=size,.heap_bytes=(nj_size)config->heap_kib*1024,.stats=1,
                .device=nj_device_find("nokia6230"),.primary_fonts=system_fonts(),.primary_count=6};
            frames=0;
            port_log("J2ME launch %s bytes=%u resolution=%dx%d heap=%uKiB rms=%s",name,size,port.width,port.height,config->heap_kib,rms);
            borrowed_jar=data;
            int result=nj_run(&port,&opts);borrowed_jar=NULL;tone_stop(NULL);
            if(port_audio_session()) {uint8_t handle[8];pxa_store_u64(handle,port_audio_session());port_rpc(PXA_AUDIO_SERVICE,PXA_AUDIO_FLUSH,handle,8);}
            port_log("J2ME returned result=%d frames=%u",result,frames);
            restart=nano_frontend_finish_game(result,frames);
        } while(restart && !nano_frontend_finished());
        free(data);
    }
    pxa_window_request_back();
}
