/* PXA memory-backed gnuboy adapter. GPL-2.0-or-later. */
#include "core.h"
#include "gb_profile.h"
#include "vendor/gnuboy/hw.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AUDIO_CAPACITY 1024u
static uint16_t video[GB_VIDEO_WIDTH * GB_VIDEO_HEIGHT];
static int16_t audio[AUDIO_CAPACITY * 2];
static bool loaded, audio_enabled = true;
static uint32_t checksum;
static size_t ram_size;
static char title[17];
static gb_log_sink log_sink;
static uint64_t rtc_timestamp;
#ifndef PXA_GUEST
static uint64_t fixed_time;
void gb_core_test_clock(uint64_t value) { fixed_time = value; }
#endif
static uint64_t wall_time(void) {
#ifndef PXA_GUEST
    if (fixed_time) return fixed_time;
#endif
    time_t value = time(NULL);
    return value > 0 ? (uint64_t)value : 0;
}
void gb_core_set_log_sink(gb_log_sink sink) { log_sink = sink; }
void pxa_gnuboy_log(int level, const char *format, ...) {
    if (!log_sink || level > 1) return;
    char line[240]; va_list args; va_start(args,format);
    vsnprintf(line,sizeof(line),format,args); va_end(args); log_sink(line);
}
void gb_core_set_audio_enabled(bool enabled) {
    audio_enabled = enabled;
    GB.audio.enabled = enabled;
    GB.audio.pos = 0;
}

/* Existing frontend CRC semantics: standard IEEE CRC XOR the supplied seed. */
uint32_t gb_port_crc32(uint32_t seed, const void *data, size_t size) {
    static uint32_t table[256];
    if (!data || !size) return seed;
    if (!table[1]) for (unsigned i=0;i<256;++i) {
        uint32_t value=i;
        for (unsigned j=0;j<8;++j) value=(value>>1)^((0u-(value&1))&0xedb88320u);
        table[i]=value;
    }
    const uint8_t *bytes=data;uint32_t crc=UINT32_MAX;
    for (size_t i=0;i<size;++i) crc=(crc>>8)^table[(crc^bytes[i])&255];
    return ~crc ^ seed;
}

const char *gb_core_validate(const uint8_t *bytes, size_t size) {
    if (!bytes || size<0x8000 || size%0x4000) return "ROM is truncated or not a whole 16 KiB bank";
    if (size>GB_ROM_LIMIT) return "ROM is larger than the 8 MiB cartridge limit";
    if (bytes[0x148]>8 && (bytes[0x148]<0x52 || bytes[0x148]>0x54)) return "Invalid cartridge ROM-size code";
    if (bytes[0x149]>5) return "Invalid cartridge RAM-size code";
    switch(bytes[0x147]) {
    case 0: case 1: case 2: case 3: case 5: case 6: case 8: case 9:
    case 0x0f: case 0x10: case 0x11: case 0x12: case 0x13:
    case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0xff:
        return NULL;
    default: return "This cartridge mapper is not supported by gnuboy";
    }
}
void gb_core_close(void) {
    gnuboy_free_rom();free(GB.rambanks);free(GB.vbanks);free(GB.bios);
    memset(&GB,0,sizeof(GB));loaded=false;ram_size=0;checksum=0;
    memset(video,0,sizeof(video));memset(title,0,sizeof(title));
}
const char *gb_core_load(const uint8_t *bytes, size_t size) {
    const char *error=gb_core_validate(bytes,size);if(error)return error;
    gb_core_close();
    if (gnuboy_init(GB_AUDIO_RATE,GB_AUDIO_STEREO_S16,GB_PIXEL_565_LE,NULL,NULL)) {
        gb_core_close();return "Failed to allocate the Game Boy board";
    }
    if (gnuboy_load_rom(bytes,size)) {gb_core_close();return "Failed to map the cartridge image";}
    /* SGB-labelled cartridges use the native DMG viewport, not SGB commands. */
    if(GB.hwtype==GB_HW_SGB)GB.hwtype=GB_HW_DMG;
    GB.video.colorize=GB_PALETTE_MGB0;
    gnuboy_set_framebuffer(video);gnuboy_set_soundbuffer(audio,AUDIO_CAPACITY*2);
    gnuboy_reset(true);GB.audio.enabled=audio_enabled;
    static const size_t sizes[]={0,2048,8192,32768,131072,65536};
    ram_size=cart.mbc==MBC_MBC2?512:sizes[bytes[0x149]];
    if(bytes[0x147]==0 || bytes[0x147]==1 || bytes[0x147]==0x0f || bytes[0x147]==0x11 ||
       bytes[0x147]==0x19 || bytes[0x147]==0x1c)ram_size=0;
    cart.ram_bytes=ram_size;
    if(cart.mbc==MBC_NONE)cart.enableram=1;
    unsigned length=bytes[0x143]&0x80?11:16;
    memcpy(title,bytes+0x134,length);title[length]=0;
    for(unsigned i=0;i<length;++i)if((uint8_t)title[i]<32 || (uint8_t)title[i]==255){title[i]=0;break;}
    if(!*title)memcpy(title,"UNNAMED",8);
    checksum=gb_port_crc32(0,bytes,size);rtc_timestamp=wall_time();loaded=true;
    return NULL;
}

/* RTC uses wall time when read/written/saved; long running games sync once
 * per emulated second. No per-instruction imports or native pointers in saves. */
void gnuboy_rtc_sync(void) {
    if(!cart.has_rtc)return;
    uint64_t now=wall_time();
    uint64_t elapsed=now>=rtc_timestamp?now-rtc_timestamp:0;
    rtc_timestamp=now;
    if(!elapsed || (cart.rtc.flags&0x40))return;
    uint64_t seconds=cart.rtc.s+60u*(cart.rtc.m+60u*(cart.rtc.h+24u*(uint64_t)cart.rtc.d))+elapsed;
    uint64_t days=seconds/86400;
    if(days>=512)cart.rtc.flags|=0x80;
    cart.rtc.d=(int)(days&511);cart.rtc.h=(int)(seconds/3600%24);
    cart.rtc.m=(int)(seconds/60%60);cart.rtc.s=(int)(seconds%60);
}
static uint32_t get32(const uint8_t *p) {
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put32(uint8_t *p,uint32_t value) {
    for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(value>>(8*i));
}
size_t gb_core_sram_size(void){return loaded?ram_size:0;}
const uint8_t *gb_core_sram(void){return loaded && ram_size?(const uint8_t*)cart.rambanks:NULL;}
void gb_core_sram_import(const uint8_t *data,size_t size) {
    if(loaded && data && ram_size)memcpy(cart.rambanks,data,size<ram_size?size:ram_size);
}
size_t gb_core_save_size(void){return loaded?ram_size+(cart.has_rtc?48:0):0;}
bool gb_core_save_export(uint8_t *data,size_t capacity) {
    size_t size=gb_core_save_size();if(!size || !data || capacity<size)return false;
    if(ram_size)memcpy(data,cart.rambanks,ram_size);
    if(cart.has_rtc){
        gnuboy_rtc_sync();uint8_t *p=data+ram_size;
        const uint32_t values[5]={(unsigned)cart.rtc.s,(unsigned)cart.rtc.m,(unsigned)cart.rtc.h,
            (unsigned)cart.rtc.d&255,((unsigned)cart.rtc.flags&0xc0)|((unsigned)cart.rtc.d>>8)};
        for(unsigned i=0;i<5;++i){put32(p+i*4,values[i]);put32(p+20+i*4,cart.rtc.regs[i]);}
        put32(p+40,(uint32_t)rtc_timestamp);put32(p+44,(uint32_t)(rtc_timestamp>>32));
    }
    return true;
}
bool gb_core_save_import(const uint8_t *data,size_t size) {
    size_t expected=gb_core_save_size();
    if(!loaded || !data || !size || size!=expected)return false;
    if(ram_size)memcpy(cart.rambanks,data,ram_size);
    if(cart.has_rtc){
        const uint8_t *p=data+ram_size;
        cart.rtc.s=get32(p)%60;cart.rtc.m=get32(p+4)%60;cart.rtc.h=get32(p+8)%24;
        cart.rtc.flags=get32(p+16)&0xc1;cart.rtc.d=(get32(p+12)&255)|((cart.rtc.flags&1)<<8);
        for(unsigned i=0;i<5;++i)cart.rtc.regs[i]=get32(p+20+4*i)&255;
        rtc_timestamp=get32(p+40)|((uint64_t)get32(p+44)<<32);gnuboy_rtc_sync();
    }
    return true;
}
void gb_core_frame_render(uint16_t pad,bool render) {
    if(!loaded)return;
    gnuboy_set_pad(pad&255);gnuboy_run(render);
    if(render && !(R_LCDC&0x80))for(unsigned y=0;y<144;++y)
        memset(GB.video.buffer16+y*GB.video.stride,0xff,160*2);
}
void gb_core_frame(uint16_t pad){gb_core_frame_render(pad,true);}
void gb_core_frame_to(uint16_t pad,uint16_t *target,size_t stride) {
    if(!loaded || !target || stride<160)return;
    GB.video.buffer16=target;GB.video.stride=stride;
    gb_core_frame(pad);GB.video.buffer16=video;GB.video.stride=160;
}
const uint16_t *gb_core_video(void){return video;}
const int16_t *gb_core_audio(size_t *frames){if(frames)*frames=loaded && audio_enabled?GB.audio.pos/2:0;return audio;}
uint32_t gb_core_checksum(void){return checksum;}
unsigned gb_core_model(void){return loaded && GB.hwtype==GB_HW_CGB?GB_BOARD_CGB:GB_BOARD_DMG;}
unsigned gb_core_rate(void){return 60;}
const char *gb_core_title(void){return title;}
