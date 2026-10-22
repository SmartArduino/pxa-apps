/* Snes9x license: vendor/snes9x/src/LICENSE. Complete pause-only PXA state.
 * Runtime maps, opcode tables, screen buffers and sample pointers stay live. */
#include "../common/state/snapshot.h"
#include "vendor/snes9x/src/cpuexec.h"
#include "vendor/snes9x/src/apu.h"
#include "vendor/snes9x/src/soundux.h"
#include "vendor/snes9x/src/snapshot.h"
#include <stddef.h>
size_t S9xSoundSnapshotSize(void);
void S9xSoundSnapshotSave(void *);
void S9xSoundSnapshotLoad(const void *,uint8_t,uint8_t);
size_t S9xDSPSnapshotSize(void);
void S9xDSPSnapshotSave(void *);
void S9xDSPSnapshotLoad(const void *);
typedef struct {
    SCPUState cpu;
    SICPU icpu;
    SPPU ppu;
    InternalPPU ippu;
    SDMA dma[8];
    SAPU apu;
    SIAPU iapu;
    SSoundData sound;
    SoundStatus sound_status;
    SRTC_DATA clock;
    uint64_t sample_phase;
    uint32_t pc,opcode,wait,apu_pc,apu_direct,apu_wait1,apu_wait2,sound_size,dsp_size;
    uint8_t blocks,echoes;
    uint8_t ram[RAM_SIZE],vram[VRAM_SIZE],sram[SRAM_SIZE],fill[FILLRAM_SIZE],apu_ram[65536];
} snes_snapshot_t;
static uint32_t snes_state_layout(void) {
    return 0x10000000u ^ (uint32_t)sizeof(snes_snapshot_t) ^ ((uint32_t)sizeof(void*)<<24) ^
        ((uint32_t)sizeof(long)<<20) ^ ((uint32_t)sizeof(SoundStatus)<<16) ^ (uint32_t)offsetof(snes_snapshot_t,ram);
}
static uint32_t snes_state_offset(const uint8_t *p,const uint8_t *base,size_t size) {
    if(!p)return UINT32_MAX;uintptr_t a=(uintptr_t)p,b=(uintptr_t)base;
    return a>=b && a-b<size?(uint32_t)(a-b):UINT32_MAX;
}
static uint8_t *snes_state_pointer(uint32_t offset,uint8_t *base) {return offset==UINT32_MAX?NULL:base+offset;}
size_t emu_state_size(void) {
    return loaded?PXA_SNAPSHOT_HEADER+sizeof(snes_snapshot_t)+S9xSoundSnapshotSize()+S9xDSPSnapshotSize():0;
}
const char *emu_state_save(void *data,size_t size) {
    if(!loaded || !data || size!=emu_state_size())return "No loaded SNES cartridge or wrong snapshot buffer";
    pxa_snapshot_begin(data,size,2,emu_core_checksum(),snes_state_layout());
    snes_snapshot_t *s=(snes_snapshot_t*)((uint8_t*)data+PXA_SNAPSHOT_HEADER);
    s->cpu=CPU;s->icpu=ICPU;s->ppu=PPU;s->ippu=IPPU;memcpy(s->dma,DMA,sizeof(s->dma));
    s->pc=(uint32_t)((uintptr_t)CPU.PC-(uintptr_t)CPU.PCBase)&65535;
    s->opcode=snes_state_offset(CPU.PCAtOpcodeStart,CPU.PCBase,65536);s->wait=snes_state_offset(CPU.WaitAddress,CPU.PCBase,65536);
    s->cpu.PC=s->cpu.PCBase=s->cpu.PCAtOpcodeStart=s->cpu.WaitAddress=NULL;
    s->icpu.UNUSED1=NULL;s->icpu.S9xOpcodes=NULL;
    s->ippu.TileCache=s->ippu.TileCached=NULL;s->ippu.XB=NULL;s->ippu.ScreenColors=s->ippu.DirectColors=NULL;
    s->apu=APU;s->iapu=IAPU;s->sound=SoundData;s->sound_status=so;s->clock=rtc;
    s->apu_pc=snes_state_offset(IAPU.PC,IAPU.RAM,65536);s->apu_direct=snes_state_offset(IAPU.DirectPage,IAPU.RAM,65536);
    s->apu_wait1=snes_state_offset(IAPU.WaitAddress1,IAPU.RAM,65536);s->apu_wait2=snes_state_offset(IAPU.WaitAddress2,IAPU.RAM,65536);
    s->iapu.PC=s->iapu.RAM=s->iapu.DirectPage=s->iapu.WaitAddress1=s->iapu.WaitAddress2=NULL;
    for(unsigned n=0;n<NUM_CHANNELS;++n) {
        if(SoundData.channels[n].block)s->blocks|=1u<<n;
        if(SoundData.channels[n].echo_buf_ptr)s->echoes|=1u<<n;
        s->sound.channels[n].block=NULL;s->sound.channels[n].echo_buf_ptr=NULL;
    }
    s->sample_phase=sample_phase;s->sound_size=S9xSoundSnapshotSize();s->dsp_size=S9xDSPSnapshotSize();
    memcpy(s->ram,Memory.RAM,sizeof(s->ram));memcpy(s->vram,Memory.VRAM,sizeof(s->vram));
    memcpy(s->sram,Memory.SRAM,sizeof(s->sram));memcpy(s->fill,Memory.FillRAM,sizeof(s->fill));memcpy(s->apu_ram,IAPU.RAM,sizeof(s->apu_ram));
    uint8_t *p=(uint8_t*)(s+1);S9xSoundSnapshotSave(p);S9xDSPSnapshotSave(p+s->sound_size);
    pxa_snapshot_finish(data,size,emu_port_crc32);return NULL;
}
const char *emu_state_load(const void *data,size_t size) {
    if(!loaded)return "No loaded SNES cartridge";
    const char *error=pxa_snapshot_check(data,size,emu_state_size(),2,emu_core_checksum(),snes_state_layout(),emu_port_crc32);
    if(error)return error;
    const snes_snapshot_t *s=(const snes_snapshot_t*)((const uint8_t*)data+PXA_SNAPSHOT_HEADER);
    if(s->pc>65535 || (s->opcode!=UINT32_MAX && s->opcode>65535) || (s->wait!=UINT32_MAX && s->wait>65535) ||
       s->apu_pc>65535 || s->apu_direct>65535 || (s->apu_wait1!=UINT32_MAX && s->apu_wait1>65535) ||
       (s->apu_wait2!=UINT32_MAX && s->apu_wait2>65535) || s->sound_size!=S9xSoundSnapshotSize() || s->dsp_size!=S9xDSPSnapshotSize() ||
       s->sample_phase>=EMU_CLOCK_HZ || s->ppu.Brightness>15 || s->ppu.BGMode>7 || s->ppu.OBJSizeSelect>7 ||
       s->ppu.ScreenHeight<1 || s->ppu.ScreenHeight>239 || s->cpu.WhichEvent>NO_EVENT || s->cpu.InDMA ||
       s->sound_status.playback_rate!=so.playback_rate || s->sound.echo_buffer_size<0 || s->sound.echo_buffer_size>24000 ||
       s->sound.echo_ptr<0 || (s->sound.echo_buffer_size && s->sound.echo_ptr>=s->sound.echo_buffer_size))
        return "Invalid SNES snapshot machine state";
    for(unsigned n=0;n<NUM_CHANNELS;++n) {
        const Channel *c=&s->sound.channels[n];
        if(c->state<0 || c->state>10 || c->type<0 || c->type>1 || c->sample_pointer>16 || c->block_pointer>65535 || c->block || c->echo_buf_ptr)
            return "Invalid SNES snapshot sound state";
    }
    InternalPPU live_ppu=IPPU;uint8_t *apu_ram=IAPU.RAM;
    CPU=s->cpu;ICPU=s->icpu;PPU=s->ppu;IPPU=s->ippu;memcpy(DMA,s->dma,sizeof(s->dma));
    IPPU.TileCache=live_ppu.TileCache;IPPU.TileCached=live_ppu.TileCached;
    IPPU.ScreenColors=live_ppu.ScreenColors;IPPU.DirectColors=live_ppu.DirectColors;IPPU.XB=live_ppu.XB;
    APU=s->apu;IAPU=s->iapu;IAPU.RAM=apu_ram;
    IAPU.PC=snes_state_pointer(s->apu_pc,apu_ram);IAPU.DirectPage=snes_state_pointer(s->apu_direct,apu_ram);
    IAPU.WaitAddress1=snes_state_pointer(s->apu_wait1,apu_ram);IAPU.WaitAddress2=snes_state_pointer(s->apu_wait2,apu_ram);
    SoundData=s->sound;so=s->sound_status;rtc=s->clock;if(Settings.SRTC)rtc.system_timestamp=time(NULL);
    memcpy(Memory.RAM,s->ram,sizeof(s->ram));memcpy(Memory.VRAM,s->vram,sizeof(s->vram));
    memcpy(Memory.SRAM,s->sram,sizeof(s->sram));memcpy(Memory.FillRAM,s->fill,sizeof(s->fill));memcpy(IAPU.RAM,s->apu_ram,sizeof(s->apu_ram));
    const uint8_t *p=(const uint8_t*)(s+1);S9xSoundSnapshotLoad(p,s->blocks,s->echoes);S9xDSPSnapshotLoad(p+s->sound_size);
    S9xSetPCBase(ICPU.ShiftedPB|s->pc);CPU.PCAtOpcodeStart=snes_state_pointer(s->opcode,CPU.PCBase);CPU.WaitAddress=snes_state_pointer(s->wait,CPU.PCBase);
    S9xFixCycles();memset(IPPU.TileCached,0,MAX_2BIT_TILES);S9xFixColourBrightness();
    IPPU.ColorsChanged=true;IPPU.OBJChanged=true;PPU.RecomputeClipWindows=true;
    sample_phase=s->sample_phase;audio_frames=0;
    if(IPPU.RenderedScreenWidth>0 && IPPU.RenderedScreenHeight>0) {
        emu_core_info.width=IPPU.RenderedScreenWidth;emu_core_info.height=IPPU.RenderedScreenHeight;
    }
    return NULL;
}
