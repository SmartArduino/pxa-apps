/* GPL-2.0-or-later. Included by core.c to keep runtime allocations private.
 * No maps, callbacks, FILE handles or video/audio target pointers are persisted. */
#include "../common/state/snapshot.h"
#include <stddef.h>
typedef struct {
    gb_cpu_t cpu;
    gb_snd_t sound;
    gb_rtc_t rtc;
    int32_t cycles,ilines,pad,serial,hdma,hwtype,frames,window_offset;
    int32_t bankmode,enableram,rombank,rambank,frame_carry,window[2];
    uint8_t ioregs[256],oam[256],palette[128];
    uint16_t video_palette[64];
    uint8_t wram[8*4096],vram[2*8192];
    uint16_t framebuffer[GB_VIDEO_WIDTH*GB_VIDEO_HEIGHT];
} gb_snapshot_t;
static uint32_t gb_state_layout(void) {
    return 0x10000000u ^ (uint32_t)sizeof(gb_snapshot_t) ^ ((uint32_t)sizeof(gb_snd_t)<<16) ^
        (uint32_t)offsetof(gb_snapshot_t,wram);
}
__attribute__((noinline,cold)) size_t gb_state_size(void) {return loaded?PXA_SNAPSHOT_HEADER+sizeof(gb_snapshot_t)+ram_size:0;}
__attribute__((noinline,cold)) const char *gb_state_save(void *buffer,size_t size) {
    if(!loaded || !buffer || size!=gb_state_size())return "No loaded GB cartridge or wrong snapshot buffer";
    gnuboy_rtc_sync();pxa_snapshot_begin(buffer,size,1,checksum,gb_state_layout());
    gb_snapshot_t *s=(gb_snapshot_t*)((uint8_t*)buffer+PXA_SNAPSHOT_HEADER);

    s->cpu.pc.w=GB.cpu->pc.w;
    s->cpu.sp.w=GB.cpu->sp.w;
    s->cpu.af.w=GB.cpu->af.w;
    s->cpu.bc.w=GB.cpu->bc.w;
    s->cpu.de.w=GB.cpu->de.w;
    s->cpu.hl.w=GB.cpu->hl.w;
    s->cpu.timer=GB.cpu->timer;
    s->cpu.div=GB.cpu->div;
    s->cpu.ime=GB.cpu->ime;
    s->cpu.ima=GB.cpu->ima;
    s->cpu.halted=GB.cpu->halted;
    s->cpu.double_speed=GB.cpu->double_speed;
    s->cpu.disassemble=GB.cpu->disassemble;
s->sound=*GB.snd;s->rtc=cart.rtc;
    s->cycles=GB.cycles;s->ilines=GB.ilines;s->pad=GB.pad;s->serial=GB.serial;s->hdma=GB.hdma;
    s->hwtype=GB.hwtype;s->frames=GB.frames;s->window_offset=GB.compat.window_offset;
    s->bankmode=cart.bankmode;s->enableram=cart.enableram;s->rombank=cart.rombank;s->rambank=cart.rambank;
    s->frame_carry=gnuboy_state_frame_carry();gb_lcd_state_get(s->window);
    memcpy(s->ioregs,GB.ioregs,sizeof(s->ioregs));memcpy(s->oam,GB.oam,sizeof(s->oam));
    memcpy(s->palette,GB.pal,sizeof(s->palette));memcpy(s->video_palette,GB.video.palette,sizeof(s->video_palette));
    memcpy(s->wram,GB.rambanks,sizeof(s->wram));memcpy(s->vram,GB.vbanks,sizeof(s->vram));
    memcpy(s->framebuffer,video,sizeof(video));if(ram_size)memcpy(s+1,cart.rambanks,ram_size);
    pxa_snapshot_finish(buffer,size,gb_port_crc32);return NULL;
}
__attribute__((noinline,cold)) const char *gb_state_load(const void *buffer,size_t size) {
    if(!loaded)return "No loaded GB cartridge";
    const char *error=pxa_snapshot_check(buffer,size,gb_state_size(),1,checksum,gb_state_layout(),gb_port_crc32);
    if(error)return error;
    const gb_snapshot_t *s=(const gb_snapshot_t*)((const uint8_t*)buffer+PXA_SNAPSHOT_HEADER);
    if(s->hwtype!=GB.hwtype || s->sound.rate!=GB.snd->rate ||
       s->cpu.double_speed>1 || s->cpu.ime>1 || s->cpu.ima>1 || s->cpu.halted>1 ||
       s->frame_carry<0 || s->frame_carry>64 || s->rombank<0 || s->rombank>511 ||
       s->rambank<0 || s->rambank>15 || s->bankmode<0 || s->bankmode>1 ||
       s->window[0]<-7 || s->window[0]>255 || s->window[1]<0 || s->window[1]>255 ||
       s->rtc.s<0 || s->rtc.s>59 || s->rtc.m<0 || s->rtc.m>59 || s->rtc.h<0 || s->rtc.h>23 ||
       s->rtc.d<0 || s->rtc.d>511)return "Invalid GB snapshot machine state";

    GB.cpu->pc.w=s->cpu.pc.w;
    GB.cpu->sp.w=s->cpu.sp.w;
    GB.cpu->af.w=s->cpu.af.w;
    GB.cpu->bc.w=s->cpu.bc.w;
    GB.cpu->de.w=s->cpu.de.w;
    GB.cpu->hl.w=s->cpu.hl.w;
    GB.cpu->timer=s->cpu.timer;
    GB.cpu->div=s->cpu.div;
    GB.cpu->ime=s->cpu.ime;
    GB.cpu->ima=s->cpu.ima;
    GB.cpu->halted=s->cpu.halted;
    GB.cpu->double_speed=s->cpu.double_speed;
    GB.cpu->disassemble=s->cpu.disassemble;
*GB.snd=s->sound;cart.rtc=s->rtc;
    GB.cycles=s->cycles;GB.ilines=s->ilines;GB.pad=s->pad;GB.serial=s->serial;GB.hdma=s->hdma;
    GB.frames=s->frames;GB.compat.window_offset=s->window_offset;
    cart.bankmode=s->bankmode;cart.enableram=s->enableram;cart.rombank=s->rombank;cart.rambank=s->rambank;
    memcpy(GB.ioregs,s->ioregs,sizeof(s->ioregs));memcpy(GB.oam,s->oam,sizeof(s->oam));
    memcpy(GB.pal,s->palette,sizeof(s->palette));memcpy(GB.video.palette,s->video_palette,sizeof(s->video_palette));
    memcpy(GB.rambanks,s->wram,sizeof(s->wram));memcpy(GB.vbanks,s->vram,sizeof(s->vram));
    memcpy(video,s->framebuffer,sizeof(video));if(ram_size)memcpy(cart.rambanks,s+1,ram_size);
    gnuboy_state_set_frame_carry(s->frame_carry);gb_lcd_state_set(s->window);gb_hw_updatemap();
    rtc_timestamp=wall_time();GB.audio.pos=0;GB.audio.buffer_ptr=GB.audio.buffer;
    cart.sram_dirty++;return NULL;
}
