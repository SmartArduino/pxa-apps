/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings.h"
#include <stdio.h>
#include <string.h>
const unsigned nano_resolution_presets[][2]={{128,160},{176,208},{176,220},{240,320},{320,240},{240,400},{320,480},{360,640}};
const unsigned nano_heap_presets[]={1024,1536,2048,2560,3072};
const unsigned nano_heap_count=sizeof(nano_heap_presets)/sizeof(*nano_heap_presets);
const unsigned nano_resolution_count=sizeof(nano_resolution_presets)/sizeof(*nano_resolution_presets);
const char *const nano_pad_names[NANO_PAD_BUTTONS]={"上","下","左","右","A","B","START","SELECT","X","Y","L","R"};
static const char *const names[]={"不映射","数字 0","数字 1","数字 2","数字 3","数字 4","数字 5","数字 6","数字 7","数字 8","数字 9","*","#","方向上","方向下","方向左","方向右","确认 / FIRE","左软键","右软键","清除","游戏 A","游戏 B","游戏 C","游戏 D","音量+","音量-","电源","打开菜单"};
_Static_assert(sizeof(names)/sizeof(*names)==NJ_KEY_COUNT+1,"key names must follow nj_key");
const char *nano_action_name(unsigned action) {return action<=NANO_ACTION_MENU?names[action]:"不映射";}
void nano_settings_default(nano_settings *s) {
    static const uint8_t defaults[NANO_PAD_BUTTONS]={NJ_KEY_UP,NJ_KEY_DOWN,NJ_KEY_LEFT,NJ_KEY_RIGHT,NJ_KEY_FIRE,NJ_KEY_SOFT_RIGHT,NANO_ACTION_MENU,NJ_KEY_0,NJ_KEY_SOFT_LEFT,NJ_KEY_SOFT_RIGHT,NJ_KEY_STAR,NJ_KEY_POUND};
    *s=(nano_settings){.width=240,.height=320,.heap_kib=2048,.fps_cap=30,.alpha=3,.button_size=1};memcpy(s->map,defaults,sizeof(defaults));
}
void nano_settings_phone_keys(nano_settings *s) {
    nano_settings defaults;nano_settings_default(&defaults);memcpy(s->map,defaults.map,sizeof(s->map));
    s->map[0]=NJ_KEY_2;s->map[1]=NJ_KEY_8;s->map[2]=NJ_KEY_4;s->map[3]=NJ_KEY_6;s->map[4]=NJ_KEY_5;
}
int nano_resolution_valid(unsigned w,unsigned h) {return w>=64 && h>=64 && w<=640 && h<=640 && (uint64_t)w*h<=NANO_GAME_PIXELS;}
static uint32_t crc(const uint8_t *p,size_t n) {
    uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;
}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(i*8));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int nano_settings_encode(const nano_settings *s,uint8_t out[NANO_SETTINGS_BYTES]) {
    if(!nano_resolution_valid(s->width,s->height) || s->touch_mode>2 || s->alpha<1 || s->alpha>7 || s->button_size>2 || s->numeric>1 || (s->fps_cap!=30 && s->fps_cap!=60))return 0;
    unsigned valid_heap=0;for(unsigned i=0;i<nano_heap_count;++i)valid_heap|=s->heap_kib==nano_heap_presets[i];
    if(!valid_heap)return 0;
    for(unsigned i=0;i<NANO_PAD_BUTTONS;++i)if(s->map[i]>NANO_ACTION_MENU)return 0;
    memset(out,0,NANO_SETTINGS_BYTES);memcpy(out,"NJP2",4);out[4]=3;out[6]=NANO_SETTINGS_BYTES;
    out[8]=s->width;out[9]=s->width>>8;out[10]=s->height;out[11]=s->height>>8;
    out[12]=s->touch_mode;out[13]=s->alpha;out[14]=s->button_size;out[15]=s->numeric;
    memcpy(out+16,s->map,NANO_PAD_BUTTONS);out[28]=s->heap_kib;out[29]=s->heap_kib>>8;out[30]=(uint8_t)s->fps_cap;put32(out+32,crc(out,32));return 1;
}
int nano_settings_decode(nano_settings *s,const uint8_t *p,size_t size) {
    nano_settings next;uint8_t check[NANO_SETTINGS_BYTES];
    if(!p || size!=NANO_SETTINGS_BYTES || memcmp(p,"NJP2",4) || p[4]<1 || p[4]>3 || p[5] || p[6]!=NANO_SETTINGS_BYTES || p[7] || get32(p+32)!=crc(p,32))return 0;
    for(unsigned i=p[4]==1?28:p[4]==2?30:31;i<32;++i)if(p[i])return 0;
    next=(nano_settings){.width=(uint16_t)(p[8]|p[9]<<8),.heap_kib=p[4]==1?2048:(uint16_t)(p[28]|p[29]<<8),.fps_cap=p[4]<3?30:p[30],.height=(uint16_t)(p[10]|p[11]<<8),.touch_mode=p[12],.alpha=p[13],.button_size=p[14],.numeric=p[15]};
    memcpy(next.map,p+16,NANO_PAD_BUTTONS);if(!nano_settings_encode(&next,check))return 0;*s=next;return 1;
}
int nano_settings_load(nano_settings *s,const char *path) {
    FILE *f=fopen(path,"rb");if(!f)return 0;uint8_t bytes[NANO_SETTINGS_BYTES+1];size_t n=fread(bytes,1,sizeof(bytes),f);int ok=!ferror(f);fclose(f);return ok && nano_settings_decode(s,bytes,n);
}
int nano_settings_save(const nano_settings *s,const char *path,int (*replace)(const char*,const char*)) {
    uint8_t bytes[NANO_SETTINGS_BYTES];char temp[160];
    if(!nano_settings_encode(s,bytes) || snprintf(temp,sizeof(temp),"%s.pending",path)>=(int)sizeof(temp))return 0;
    FILE *f=fopen(temp,"wb");if(!f)return 0;int ok=fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes);if(fclose(f))ok=0;
    if(ok)ok=(replace?replace(temp,path):rename(temp,path))==0;
    if(!ok)remove(temp);return ok;
}
