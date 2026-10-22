/* GPL-2.0-or-later. SMS/GG snapshot adapter. No pointers or callbacks on disk. */
#include "../state/snapshot.h"
#include <stddef.h>
#include <math.h>
uint64_t sound_state_fraction(void);
void sound_state_set_fraction(uint64_t value);
void render_state_reset(void);
void pio_state_get(uint8 values[3]);
void pio_state_set(const uint8 values[3]);
typedef struct {
    sms_t hardware;
    vdp_t display;
    Z80_Regs cpu;
    SN76489_Context psg;
    __typeof__(bitmap.viewport) viewport;
    input_t pads;
    uint64_t sample_fraction;
    int32_t sample_count,done_so_far,cycles;
    uint8_t fcr[4],peripheral[3];
    uint8_t wram[8192],vram[16384],sram[32768];
} sms_snapshot_t;
/* Keep scalar CPU accesses visible to LLVM. A whole-struct byte copy makes
 * the live register bank escape and can pessimize every emulated opcode.
 * Callbacks belong to the current machine; they are never serialized. */
static inline __attribute__((always_inline)) void sms_state_copy_cpu(Z80_Regs *dst,const Z80_Regs *src) {
#define COPY_PAIR(field) dst->field.d=src->field.d
    COPY_PAIR(pc);COPY_PAIR(sp);COPY_PAIR(af);COPY_PAIR(bc);COPY_PAIR(de);COPY_PAIR(hl);
    COPY_PAIR(ix);COPY_PAIR(iy);COPY_PAIR(wz);COPY_PAIR(af2);COPY_PAIR(bc2);COPY_PAIR(de2);COPY_PAIR(hl2);
#undef COPY_PAIR
#define COPY_BYTE(field) dst->field=src->field
    COPY_BYTE(r);COPY_BYTE(r2);COPY_BYTE(iff1);COPY_BYTE(iff2);COPY_BYTE(halt);COPY_BYTE(im);COPY_BYTE(i);
    COPY_BYTE(nmi_state);COPY_BYTE(nmi_pending);COPY_BYTE(irq_state);COPY_BYTE(after_ei);
#undef COPY_BYTE
}
static uint32_t sms_state_layout(void) {
    return 0x10000000u ^ (uint32_t)sizeof(sms_snapshot_t) ^ ((uint32_t)sizeof(void*)<<24) ^
        ((uint32_t)sizeof(Z80_Regs)<<16) ^ (uint32_t)offsetof(sms_snapshot_t,wram);
}
size_t emu_state_size(void) {return loaded?PXA_SNAPSHOT_HEADER+sizeof(sms_snapshot_t):0;}
const char *emu_state_save(void *buffer,size_t size) {
    if(!loaded || !buffer || size!=emu_state_size())return "No loaded cartridge or wrong snapshot buffer";
    pxa_snapshot_begin(buffer,size,EMU_SMS_GEAR?5:4,emu_core_checksum(),sms_state_layout());
    sms_snapshot_t *s=(sms_snapshot_t*)((uint8_t*)buffer+PXA_SNAPSHOT_HEADER);
    s->hardware=sms;s->hardware.wram=NULL;s->display=vdp;s->display.vram=NULL;sms_state_copy_cpu(&s->cpu,&Z80);
    s->cpu.daisy=NULL;s->cpu.irq_callback=NULL;s->cpu.mem_read=NULL;s->cpu.mem_write=NULL;s->cpu.port_read=NULL;s->cpu.port_write=NULL;
    memcpy(&s->psg,SN76489_GetContextPtr(0),sizeof(s->psg));s->viewport=bitmap.viewport;s->pads=input;
    s->sample_fraction=sound_state_fraction();s->sample_count=snd.sample_count;s->done_so_far=snd.done_so_far;s->cycles=z80_cycle_count;
    memcpy(s->fcr,cart.fcr,4);pio_state_get(s->peripheral);
    memcpy(s->wram,sms.wram,sizeof(s->wram));memcpy(s->vram,vdp.vram,sizeof(s->vram));memcpy(s->sram,cart.sram,sizeof(s->sram));
    pxa_snapshot_finish(buffer,size,emu_port_crc32);return NULL;
}
const char *emu_state_load(const void *buffer,size_t size) {
    if(!loaded)return "No loaded cartridge";
    const char *error=pxa_snapshot_check(buffer,size,emu_state_size(),EMU_SMS_GEAR?5:4,emu_core_checksum(),sms_state_layout(),emu_port_crc32);
    if(error)return error;
    const sms_snapshot_t *s=(const sms_snapshot_t*)((const uint8_t*)buffer+PXA_SNAPSHOT_HEADER);
    if(s->hardware.console!=sms.console || s->hardware.display!=sms.display || s->hardware.wram || s->display.vram ||
       s->display.extended>2 || s->display.mode>15 || (s->display.lpf!=262 && s->display.lpf!=313) ||
       s->display.height<144 || s->display.height>240 || s->cpu.im>2 || s->cpu.iff1>1 || s->cpu.iff2>1 ||
       s->viewport.w<1 || s->viewport.h<1 || s->viewport.w>256 || s->viewport.h>240 ||
       s->viewport.x<0 || s->viewport.y<0 || s->viewport.x+s->viewport.w>256 || s->viewport.y+s->viewport.h>256 ||
       s->sample_fraction>=snd.psg_clock || s->sample_count<1 || s->sample_count>snd.buffer_size/2 ||
       s->done_so_far || !isfinite(s->psg.Clock) || !isfinite(s->psg.dClock) || s->psg.VolumeArray<0 || s->psg.VolumeArray>3)
        return "Invalid SMS/GG snapshot machine state";
    /* Recreate only the live allocation/maps. Validate the complete file first. */
    system_reset();uint8 *ram=sms.wram,*vram=vdp.vram;
    sms=s->hardware;sms.wram=ram;vdp=s->display;vdp.vram=vram;sms_state_copy_cpu(&Z80,&s->cpu);
    memcpy(sms.wram,s->wram,sizeof(s->wram));memcpy(vdp.vram,s->vram,sizeof(s->vram));memcpy(cart.sram,s->sram,sizeof(s->sram));
    memcpy(cart.fcr,s->fcr,4);slot.rom=cart.rom;slot.pages=cart.pages;slot.mapper=cart.mapper;slot.fcr=cart.fcr;mapper_reset();
    cpu_readmap[0]=pxa_sms_rom_map(slot.rom,0,0);
    for(unsigned i=0;i<4;++i) {
        if(slot.mapper==MAPPER_KOREA_MSX)mapper_8k_w(i,s->fcr[i]);else mapper_16k_w(i,s->fcr[i]);
    }
    vdp_init();bitmap.viewport=s->viewport;render_state_reset();input=s->pads;pio_state_set(s->peripheral);
    memcpy(SN76489_GetContextPtr(0),&s->psg,sizeof(s->psg));snd.sample_count=s->sample_count;snd.done_so_far=s->done_so_far;
    sound_state_set_fraction(s->sample_fraction);z80_cycle_count=s->cycles;
    render_copy_palette(palette);rgb_dirty=true;
    emu_core_info.width=bitmap.viewport.w;emu_core_info.height=bitmap.viewport.h;emu_core_info.stride=bitmap.viewport.w;
    return NULL;
}
