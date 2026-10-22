/* GPL-2.0-or-later. Actual async slot FSM over a fault-injected POSIX FS. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "pxa_fs.h"
enum { PAUSED=1,STATES };
static unsigned mode=PAUSED,redraw,save_pending,writing_save,save_replace_pending,rom_loaded=1,pause_selected,pause_top;
#define UI_STATE_ERROR "Error"
#define UI_SCANNING "Scanning"
#define UI_STATES "States"
#define UI_NO_STATES "Empty"
#define UI_SAVING "Saving"
#define UI_LOADING "Loading"
#define UI_STATE_LOADED "Loaded"
#define UI_NEW_STATE "New"
#define UI_BACK "Back"
#define UI_DELETE_STATE "Delete"
#define UI_DELETE_CONFIRM "Confirm delete"
static unsigned UI_DANGER,UI_MUTED,UI_FOCUS,UI_CARD,UI_INK,UI_ACCENT,menu_commands;
static unsigned menu_width(void){return 320;}
static unsigned menu_height(void){return 240;}
static void text(uint16_t *p,unsigned x,unsigned y,const char *t,unsigned c,unsigned n){(void)p;(void)x;(void)y;(void)t;(void)c;(void)n;}
static void card(uint16_t *p,unsigned x,unsigned y,unsigned w,unsigned h,unsigned c){(void)p;(void)x;(void)y;(void)w;(void)h;(void)c;}
static void menu_label(unsigned x,unsigned y,unsigned w,unsigned h,const char *s,unsigned c,unsigned a){(void)x;(void)y;(void)w;(void)h;(void)s;(void)c;(void)a;}
static void scroll_bar(uint16_t *p,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){(void)p;(void)a;(void)b;(void)c;(void)d;(void)e;}
static void button(uint16_t *p,unsigned x,unsigned y,unsigned w,const char *s,unsigned c){(void)p;(void)x;(void)y;(void)w;(void)s;(void)c;}
static void close_handle(uint64_t *h){if(*h)pxa_close_handle(*h);*h=0;}
static uint64_t micros(void){static uint64_t n=100;return ++n;}
static void release_input(void){}
static unsigned resets,machine;
static void app_state_reset_output(void){++resets;}
static size_t app_state_size(void){return 20003;}
static const char *app_state_save(void *out,size_t n){if(n!=app_state_size())return "size";memset(out,0x55,n);memcpy(out,&machine,4);return NULL;}
static const char *app_state_load(const void *data,size_t n){if(n!=app_state_size() || ((const uint8_t*)data)[n-1]!=0x55)return "bad";memcpy(&machine,data,4);return NULL;}
static uint32_t app_state_checksum(void){return 0x12345678;}
static void app_state_log(const char *s,...){(void)s;}
#define APP_STATE_TAG "TEST"
#define APP_STATE_PREFIX "st"
#define APP_STATE_EXTENSION ".pst"
#include "../state_slots.h"
static DIR *directory;
static unsigned queued,io_calls,fail_write,fail_rename;
static pxa_event_t response;
static uint8_t reply[256];
#define FILE_HANDLE(fd) (UINT64_C(0x100000000)|(unsigned)(fd))
#define DIRECTORY_HANDLE UINT64_C(0x200000000)
static size_t record(uint8_t *out,uint16_t tag,const void *value,size_t n){size_t bytes;assert(pxa_wire_record_encode(out,200,tag,value,n,&bytes));return bytes;}
static void path_record(const uint8_t *data,uint32_t size,unsigned which,char *out){
 size_t used=0,at=0;pxa_wire_record_view_t r;
 while(at<size){assert(pxa_wire_record_decode(data+at,size-at,&r,&used));if(r.raw_tag==which){memcpy(out,r.payload,r.payload_size);out[r.payload_size]=0;return;}at+=used;}assert(0);
}
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t size){
 unsigned service=route&65535,op=route>>16;
 if(service==PXA_CORE_SERVICE && op==PXA_CORE_CLOSE_HANDLE){uint64_t h=pxa_load_u64(data);if(h==DIRECTORY_HANDLE){closedir(directory);directory=NULL;}else close((int)(uint32_t)h);return 0;}
 if(service!=PXA_FS_SERVICE)return 0;
 assert(!queued);queued=1;memset(reply,0,sizeof(reply));response=(pxa_event_t){PXA_FS_SERVICE,op,token,reply,4};
 char a[256],b[256];
 if(op==PXA_FS_MAKE_DIRECTORY || op==PXA_FS_REMOVE){path_record(data,size,1,a);int r=op==PXA_FS_REMOVE?unlink(a):mkdir(a,0700);if(r && errno!=EEXIST)pxa_store_u32(reply,PXA_STATUS_IO_ERROR);}
 else if(op==PXA_FS_OPEN){
  path_record(data,size,1,a);
  if(token==S_DIR){directory=opendir(a);if(directory){pxa_store_u64(reply+4,DIRECTORY_HANDLE);response.payload_size=12;}else pxa_store_u32(reply,PXA_STATUS_IO_ERROR);}
  else{int fd=open(a,token==S_WRITE?O_WRONLY|O_CREAT|O_EXCL:O_RDONLY,0600);if(fd<0)pxa_store_u32(reply,PXA_STATUS_IO_ERROR);else{pxa_store_u64(reply+4,FILE_HANDLE(fd));response.payload_size=12;}}
 }else if(op==PXA_FS_SEEK){off_t p=lseek((int)(uint32_t)pxa_load_u64(data),(int64_t)pxa_load_u64(data+8),data[16]==PXA_FS_SEEK_END?SEEK_END:SEEK_SET);if(p<0)pxa_store_u32(reply,PXA_STATUS_IO_ERROR);else{pxa_store_u64(reply+4,p);response.payload_size=12;}}
 else if(op==PXA_FS_READ_DIRECTORY){
  struct dirent *e;do{e=readdir(directory);}while(e && e->d_name[0]=='.');
  size_t at=4;
  if(!e)at+=record(reply+at,7,NULL,0);
  else{uint8_t kind=PXA_FS_KIND_REGULAR,bytes[8]={0};at+=record(reply+at,4,e->d_name,strlen(e->d_name));at+=record(reply+at,5,&kind,1);at+=record(reply+at,6,bytes,8);}
  response.payload_size=at;
 }else if(op==PXA_FS_RENAME){
  path_record(data,size,1,a);path_record(data,size,2,b);
  if(fail_rename || link(a,b)){fail_rename=0;pxa_store_u32(reply,PXA_STATUS_IO_ERROR);}else assert(!unlink(a));
 }else assert(0);
 return 0;
}
int32_t pxa_io(uint64_t handle,uint32_t op,uint8_t *data,uint32_t bytes){
 if(op==PXA_FS_IO_WRITE && fail_write){fail_write=0;return PXA_STATUS_IO_ERROR;}
 if(++io_calls%5==0)return PXA_STATUS_WOULD_BLOCK;
 if(bytes>997)bytes=997;
 return op==PXA_FS_IO_WRITE?write((int)(uint32_t)handle,data,bytes):read((int)(uint32_t)handle,data,bytes);
}
static void drain(void){unsigned guard=100000;while((queued || slot_phase) && --guard){if(queued){queued=0;pxa_event_t e=response;uint8_t copy[256];memcpy(copy,reply,response.payload_size);e.payload=copy;assert(slots_event(&e));}slots_tick();}assert(guard);}
static unsigned files(unsigned temporary){DIR *d=opendir("states");assert(d);unsigned count=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.' && (!!strstr(e->d_name,".tmp")==temporary))++count;closedir(d);return count;}
int main(void){
 char temp[]="/tmp/pxa-state-test-XXXXXX";assert(mkdtemp(temp));assert(!chdir(temp));
 save_pending=1;slots_open();assert(slot_open_pending && mode==PAUSED);save_pending=0;slots_tick();drain();assert(mode==STATES && !slot_total);
 machine=42;slot_new();assert(!files(0));drain();assert(slot_total==1 && files(0)==1 && !files(1));
 machine=81;slot_new();drain();assert(slot_total==2 && files(0)==2);
 slot_selected=1;slot_delete();assert(slot_delete_pending && files(0)==2);slot_delete();drain();assert(slot_total==1 && files(0)==1);
 machine=81;slot_new();drain();assert(slot_total==2);
 char first[128];snprintf(first,sizeof(first),"states/%s",slot_names[0]);
 slot_load(0);drain();assert(mode==PAUSED && resets==1);unsigned expected=machine;
 slots_open();drain();machine=199;slot_load(0);drain();assert(machine==expected && resets==2);
 slots_open();drain();fail_write=1;slot_new();drain();assert(slot_failed && files(0)==2 && !files(1));
 fail_rename=1;slot_new();drain();assert(slot_failed && files(0)==2 && !files(1));
 for(unsigned n=0;n<1103;++n){char p[128];snprintf(p,sizeof(p),"states/st-12345678-%016llx-%08x.pst",(unsigned long long)n,n);assert(!link(first,p));}
 slots_scan();drain();assert(slot_total==1105 && slot_cached==SLOT_CACHE);
 slots_scroll(1040);drain();assert(slot_top==1040 && slot_base==1040 && slot_cached==32);
 machine=201;slot_load(1040);drain();assert(machine==expected && resets==3);
 slots_open();drain();char bad[128];snprintf(bad,sizeof(bad),"states/%s",slot_names[0]);int fd=open(bad,O_WRONLY);assert(fd>=0 && lseek(fd,-1,SEEK_END)>0 && write(fd,"!",1)==1);close(fd);
 machine=233;slot_load(0);drain();assert(slot_failed && machine==233 && resets==3);
 DIR *d=opendir("states");struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.'){char p[300];snprintf(p,sizeof(p),"states/%s",e->d_name);assert(!unlink(p));}closedir(d);assert(!rmdir("states"));assert(!chdir("/"));assert(!rmdir(temp));
 puts("PASS: 1105 slots with bounded cache, async short/WouldBlock IO, atomic commits, save failures, corrupt input and deferred battery flush");
}
