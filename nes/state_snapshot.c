/* GPL-2.0-or-later. Snapshot layout version 1 is local to a core ABI.
 * Pointers are never persisted: maps use bounded region offsets, callbacks and
 * allocations are rebound to the live ROM. Validate the entire file before
 * changing the running machine. CPU/PPU/APU hot functions remain unchanged. */
#include "state_snapshot.h"
#include "core.h"
#include "nes/nes.h"
#include <stddef.h>

typedef struct {uint32_t region,offset;} mapping_t;
typedef struct {
    uint8_t magic[8];
    uint32_t version,bytes,crc,rom,mapper,layout;
    uint32_t prg_ram,chr_ram,prg_rom,chr_rom;
    int32_t scanline,cycles,timer_period;
    uint32_t apu_fraction,input_strobe;
    nes6502_t cpu;
    ppu_t ppu;
    apu_t apu;
    input_t input[2];
    uint8_t mapper_state[512];
    mapping_t cpu_map[MEM_PAGECOUNT],ppu_map[PPU_PAGECOUNT];
} snapshot_t;
typedef struct {uint8_t *p;size_t n;} region_t;
static void regions(nes_t *m,region_t r[8]) {
    r[0]=(region_t){0,0};r[1]=(region_t){m->mem->ram,2048};
    r[2]=(region_t){m->cart->prg_rom,m->cart->prg_rom_banks*8192u};
    r[3]=(region_t){m->cart->chr_rom,m->cart->chr_rom_banks*8192u};
    r[4]=(region_t){m->cart->prg_ram,m->cart->prg_ram_banks*8192u};
    r[5]=(region_t){m->cart->chr_ram,m->cart->chr_ram_banks*8192u};
    r[6]=(region_t){m->ppu->nametab,4096};r[7]=(region_t){m->mem->dummy,2048};
}
static uint32_t layout(void) {
    /* Raw scalar structures require the same pointer/long ABI and field layout.
     * Layout changes must bump the format, never silently reinterpret a save. */
    return sizeof(snapshot_t) ^ (sizeof(void*)<<24) ^ (sizeof(long)<<16) ^
        (sizeof(ppu_t)<<8) ^ sizeof(apu_t) ^ offsetof(snapshot_t,cpu_map);
}
static int supported(nes_t *m) {
    if(m->apu->ext)return 0; /* External audio chips need their own state. */
    unsigned n=m->mapper->number;
    return (m->mapper->get_state && m->mapper->set_state) || n==0 || n==2 || n==3 || n==7 || n==66;
}
size_t nes_state_size(void) {
    nes_t *m=nes_getptr();
    if(!m->cart || !m->cart->prg_rom)return 0;
    return sizeof(snapshot_t)+2048+4096+2048+
        (m->cart->prg_ram_banks+m->cart->chr_ram_banks+m->cart->chr_rom_banks)*8192u+
        NES_SCREEN_PITCH*NES_SCREEN_HEIGHT;
}
static int encode(mapping_t *out,const uint8_t *p,size_t span,const region_t r[8]) {
    *out=(mapping_t){0,0};if(!p)return 1;
    for(unsigned n=1;n<8;++n) {
        uintptr_t a=(uintptr_t)p,b=(uintptr_t)r[n].p;
        if(r[n].p && a>=b && a-b<=r[n].n && span<=r[n].n-(a-b)) {
            *out=(mapping_t){n,(uint32_t)(a-b)};return 1;
        }
    }
    return 0;
}
static int valid(mapping_t p,size_t span,const region_t r[8]) {
    return p.region<8 && (!p.region ? !p.offset :
        r[p.region].p && p.offset<=r[p.region].n && span<=r[p.region].n-p.offset);
}
static uint8_t *decode(mapping_t p,const region_t r[8]) {return p.region?r[p.region].p+p.offset:NULL;}
static uint32_t checksum(const snapshot_t *s,size_t n) {
    size_t offset=offsetof(snapshot_t,rom);
    return nes_port_crc32(0,(const uint8_t*)s+offset,n-offset);
}
const char *nes_state_save(void *data,size_t size) {
    nes_t *m=nes_getptr();size_t need=nes_state_size();
    if(!need || size!=need || !data)return "Invalid snapshot buffer";
    if(!supported(m))return "Snapshot is not supported for this mapper/audio chip";
    snapshot_t *s=data;memset(s,0,sizeof(*s));memcpy(s->magic,"PXANESST",8);
    s->version=1;s->bytes=need;s->rom=nes_core_checksum();s->mapper=nes_core_mapper();s->layout=layout();
    region_t r[8];regions(m,r);
    s->prg_ram=r[4].n;s->chr_ram=r[5].n;s->prg_rom=r[2].n;s->chr_rom=r[3].n;
    s->scanline=m->scanline;s->cycles=m->cycles;s->timer_period=m->timer_period;
    s->apu_fraction=apu_get_fraction();s->input_strobe=input_get_strobe();
    s->cpu=*m->cpu;s->cpu.zp=NULL;s->cpu.stack=NULL;s->cpu.pages=NULL;
    s->ppu=*m->ppu;s->ppu.nametab=NULL;s->ppu.latchfunc=NULL;s->ppu.vreadfunc=NULL;
    memset(s->ppu.page,0,sizeof(s->ppu.page));
    s->apu=*m->apu;s->apu.buffer=NULL;s->apu.ext=NULL;
    memcpy(s->input,m->input,sizeof(s->input));
    if(m->mapper->get_state)m->mapper->get_state(s->mapper_state);
    for(unsigned n=0;n<MEM_PAGECOUNT;++n)
        if(!encode(&s->cpu_map[n],mem_getpage(n),MEM_PAGESIZE,r))return "Unrecognized CPU mapping";
    for(unsigned n=0;n<PPU_PAGECOUNT;++n)
        if(!encode(&s->ppu_map[n],ppu_getpage(n),PPU_PAGESIZE,r))return "Unrecognized PPU mapping";
    uint8_t *p=(uint8_t*)(s+1);
    /* This core can corrupt pattern memory on illegal in-render writes.
     * Preserve CHR bytes as well, including that existing emulated behavior. */
    const unsigned ram[]={1,4,5,6,7,3};
    for(unsigned n=0;n<6;++n){region_t a=r[ram[n]];if(a.n)memcpy(p,a.p,a.n);p+=a.n;}
    memcpy(p,nes_core_video()-8,NES_SCREEN_PITCH*NES_SCREEN_HEIGHT);
    s->crc=checksum(s,size);return NULL;
}
const char *nes_state_load(const void *data,size_t size) {
    nes_t *m=nes_getptr();size_t need=nes_state_size();
    if(!data || !need || size!=need || size<sizeof(snapshot_t))return "Snapshot size mismatch";
    /* The FS buffer is malloc-aligned. Callers may also pass unaligned data. */
    snapshot_t copy;memcpy(&copy,data,sizeof(copy));const snapshot_t *s=&copy;
    if(memcmp(s->magic,"PXANESST",8) || s->version!=1 || s->bytes!=size || s->layout!=layout())return "Unsupported snapshot format";
    if(s->rom!=nes_core_checksum() || s->mapper!=nes_core_mapper())return "Snapshot belongs to another ROM";
    if(s->crc!=nes_port_crc32(0,(const uint8_t*)data+offsetof(snapshot_t,rom),size - offsetof(snapshot_t,rom)))return "Snapshot checksum failed";
    if(!supported(m))return "Snapshot is not supported for this mapper/audio chip";
    region_t r[8];regions(m,r);
    if(s->prg_ram!=r[4].n || s->chr_ram!=r[5].n || s->prg_rom!=r[2].n || s->chr_rom!=r[3].n)return "Snapshot memory mismatch";
    if(s->cpu.pc_reg>65535 || s->scanline<0 || s->scanline>m->scanlines_per_frame || s->input_strobe>1 ||
       s->apu_fraction>=(unsigned)m->refresh_rate || s->apu.sample_rate!=m->apu->sample_rate ||
       s->apu.stereo!=m->apu->stereo || s->apu.samples_per_frame<0 || s->apu.samples_per_frame>400)return "Invalid snapshot machine state";
    for(unsigned n=0;n<MEM_PAGECOUNT;++n)if(!valid(s->cpu_map[n],MEM_PAGESIZE,r))return "Invalid CPU mapping";
    for(unsigned n=0;n<PPU_PAGECOUNT;++n)if(!s->ppu_map[n].region || !valid(s->ppu_map[n],PPU_PAGESIZE,r))return "Invalid PPU mapping";
    const uint8_t *p=(const uint8_t*)data+sizeof(snapshot_t);
    /* This core can corrupt pattern memory on illegal in-render writes.
     * Preserve CHR bytes as well, including that existing emulated behavior. */
    const unsigned ram[]={1,4,5,6,7,3};
    for(unsigned n=0;n<6;++n){region_t a=r[ram[n]];if(a.n)memcpy(a.p,p,a.n);p+=a.n;}
    if(m->mapper->set_state)m->mapper->set_state(copy.mapper_state);
    for(unsigned n=0;n<MEM_PAGECOUNT;++n)mem_setpage(n,decode(s->cpu_map[n],r));
    copy.cpu.zp=m->cpu->zp;copy.cpu.stack=m->cpu->stack;copy.cpu.pages=m->cpu->pages;*m->cpu=copy.cpu;
    copy.ppu.nametab=m->ppu->nametab;copy.ppu.latchfunc=m->ppu->latchfunc;copy.ppu.vreadfunc=m->ppu->vreadfunc;
    for(unsigned n=0;n<PPU_PAGECOUNT;++n)copy.ppu.page[n]=decode(s->ppu_map[n],r)-(n<<PPU_PAGESHIFT);
    /* Runtime rendering options and ROM geometry belong to the running app,
     * not to the emulated machine. Keep them live, also preserving the core's
     * constant-option specialization under whole-program optimization. */
    copy.ppu.scanlines=m->ppu->scanlines;copy.ppu.vram_present=m->ppu->vram_present;
    memcpy(m->ppu,&copy.ppu,offsetof(ppu_t,options));ppu_invalidate_caches();
    copy.apu.buffer=m->apu->buffer;copy.apu.ext=m->apu->ext;
    copy.apu.sample_rate=m->apu->sample_rate;copy.apu.stereo=m->apu->stereo;
    copy.apu.cycle_rate=m->apu->cycle_rate;
    memcpy(copy.apu.decay_lut,m->apu->decay_lut,sizeof(copy.apu.decay_lut));
    memcpy(copy.apu.vbl_lut,m->apu->vbl_lut,sizeof(copy.apu.vbl_lut));
    memcpy(copy.apu.trilength_lut,m->apu->trilength_lut,sizeof(copy.apu.trilength_lut));
    memcpy(m->apu,&copy.apu,offsetof(apu_t,options));
    apu_set_fraction(s->apu_fraction);input_set_strobe(s->input_strobe);memcpy(m->input,s->input,sizeof(s->input));
    m->scanline=s->scanline;m->cycles=s->cycles;m->timer_period=s->timer_period;m->cpu_idle_next=false;
    memcpy((uint8_t*)nes_core_video()-8,p,NES_SCREEN_PITCH*NES_SCREEN_HEIGHT);
    return NULL;
}
