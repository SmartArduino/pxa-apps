/* GPL-2.0-or-later. Pointer-free PCE hardware and all mutable memory. */
#include "../common/state/snapshot.h"
#include <stddef.h>
#undef CPU
void gfx_state_get(int values[6]);
void gfx_state_set(const int values[6]);
typedef struct {
    PCE_t machine;
    uint64_t sample_phase;
    int32_t raster[6];
    uint8_t ram[8192];
    uint16_t vram[32768];
} pce_snapshot_t;
static uint32_t pce_state_layout(void) {
    return 0x10000000u ^ (uint32_t)sizeof(pce_snapshot_t) ^ ((uint32_t)sizeof(void*)<<24) ^
        ((uint32_t)sizeof(PCE_t)<<8) ^ (uint32_t)offsetof(pce_snapshot_t,ram);
}
__attribute__((noinline,cold)) size_t emu_state_size(void) {return loaded?PXA_SNAPSHOT_HEADER+sizeof(pce_snapshot_t)+(PCE.ExRAM?32768:0):0;}
__attribute__((noinline,cold)) const char *emu_state_save(void *data,size_t size) {
    if(!loaded || !data || size!=emu_state_size())return "No loaded HuCard or wrong snapshot buffer";
    pxa_snapshot_begin(data,size,6,emu_core_checksum(),pce_state_layout());
    pce_snapshot_t *s=(pce_snapshot_t*)((uint8_t*)data+PXA_SNAPSHOT_HEADER);
    /* Keep scalar hardware globals visible to LLVM GlobalOpt. Copying the
     * whole PCE (or CPU) object disables scalar replacement in every frame. */
    s->machine.ROM_SIZE=PCE.ROM_SIZE;
    s->machine.ROM_CRC=PCE.ROM_CRC;
    s->machine.Scanline=PCE.Scanline;
    s->machine.ScrollYDiff=PCE.ScrollYDiff;
    s->machine.Cycles=PCE.Cycles;
    s->machine.MaxCycles=PCE.MaxCycles;
    s->machine.SF2=PCE.SF2;
    s->machine.io_buffer=PCE.io_buffer;
    s->machine.CPU.PC=PCE.CPU.PC;
    s->machine.CPU.A=PCE.CPU.A;
    s->machine.CPU.X=PCE.CPU.X;
    s->machine.CPU.Y=PCE.CPU.Y;
    s->machine.CPU.P=PCE.CPU.P;
    s->machine.CPU.S=PCE.CPU.S;
    s->machine.CPU.irq_mask=PCE.CPU.irq_mask;
    s->machine.CPU.irq_mask_delay=PCE.CPU.irq_mask_delay;
    s->machine.CPU.irq_lines=PCE.CPU.irq_lines;
    s->machine.CPU.cycles=PCE.CPU.cycles;
    s->machine.CPU.halted=PCE.CPU.halted;
    s->machine.Timer=PCE.Timer;
    s->machine.Joypad=PCE.Joypad;
    s->machine.VCE=PCE.VCE;
    s->machine.VDC=PCE.VDC;
    s->machine.PSG=PCE.PSG;
    memcpy(s->machine.SPRAM,PCE.SPRAM,sizeof(PCE.SPRAM));
    memcpy(s->machine.Palette,PCE.Palette,sizeof(PCE.Palette));
    memcpy(s->machine.MMR,PCE.MMR,sizeof(PCE.MMR));
    s->sample_phase=sample_phase;gfx_state_get(s->raster);
    memcpy(s->ram,PCE.RAM,sizeof(s->ram));memcpy(s->vram,PCE.VRAM,sizeof(s->vram));
    if(PCE.ExRAM)memcpy(s+1,PCE.ExRAM,32768);
    pxa_snapshot_finish(data,size,emu_port_crc32);return NULL;
}
__attribute__((noinline,cold)) const char *emu_state_load(const void *data,size_t size) {
    if(!loaded)return "No loaded HuCard";
    const char *error=pxa_snapshot_check(data,size,emu_state_size(),6,emu_core_checksum(),pce_state_layout(),emu_port_crc32);
    if(error)return error;
    const pce_snapshot_t *s=(const pce_snapshot_t*)((const uint8_t*)data+PXA_SNAPSHOT_HEADER);
    const PCE_t *m=&s->machine;
    if(m->ROM_SIZE!=PCE.ROM_SIZE || m->ROM_CRC!=PCE.ROM_CRC || m->SF2>3 || m->VCE.reg>=512 || m->VDC.reg>=32 ||
       m->PSG.ch>=6 || m->VDC.screen_width>512 || m->VDC.screen_height>240 || s->sample_phase>=EMU_CLOCK_HZ ||
       m->RAM || m->VRAM || m->ExRAM || m->ROM || m->ROM_DATA || m->IOAREA || m->NULLRAM || m->MemoryMapR || m->MemoryMapW)
        return "Invalid PCE snapshot machine state";
    /* Keep allocation/ROM pointers immutable between cartridge loads. A whole
     * PCE assignment would also replace runtime-only pointer fields. */
    memcpy(PCE.SPRAM,m->SPRAM,sizeof(PCE.SPRAM));memcpy(PCE.Palette,m->Palette,sizeof(PCE.Palette));
    PCE.Scanline=m->Scanline;PCE.ScrollYDiff=m->ScrollYDiff;PCE.Cycles=m->Cycles;PCE.MaxCycles=m->MaxCycles;
    memcpy(PCE.MMR,m->MMR,sizeof(PCE.MMR));PCE.SF2=m->SF2;PCE.io_buffer=m->io_buffer;
    PCE.Timer=m->Timer;PCE.Joypad=m->Joypad;PCE.VCE=m->VCE;PCE.VDC=m->VDC;PCE.PSG=m->PSG;
    PCE.CPU.PC=m->CPU.PC;
    PCE.CPU.A=m->CPU.A;
    PCE.CPU.X=m->CPU.X;
    PCE.CPU.Y=m->CPU.Y;
    PCE.CPU.P=m->CPU.P;
    PCE.CPU.S=m->CPU.S;
    PCE.CPU.irq_mask=m->CPU.irq_mask;
    PCE.CPU.irq_mask_delay=m->CPU.irq_mask_delay;
    PCE.CPU.irq_lines=m->CPU.irq_lines;
    PCE.CPU.cycles=m->CPU.cycles;
    PCE.CPU.halted=m->CPU.halted;

    memcpy(PCE.RAM,s->ram,sizeof(s->ram));memcpy(PCE.VRAM,s->vram,sizeof(s->vram));
    if(PCE.ExRAM)memcpy(PCE.ExRAM,s+1,32768);
    if(PCE.ROM_SIZE>=0xc0)for(unsigned i=0x40;i<0x80;++i) {
        pxa_pce_rom_banks[i]=(PCE.SF2*64+i)%PCE.ROM_SIZE;
        if(!pxa_pce_rom_paged())PCE.MemoryMapR[i]=PCE.ROM_DATA+(size_t)pxa_pce_rom_banks[i]*8192;
    }
    for(unsigned i=0;i<8;++i)pce_bank_set(i,PCE.MMR[i]);
    gfx_state_set(s->raster);sample_phase=s->sample_phase;frames=0;
    if(PCE.VDC.screen_width && PCE.VDC.screen_height) {
        emu_core_info.width=PCE.VDC.screen_width;emu_core_info.height=PCE.VDC.screen_height;
    }
    return NULL;
}

#define CPU PCE.CPU
