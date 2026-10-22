#include "core.h"
#include "nes/nes.h"
#include "mappers/mappers.h"
#include "palettes.h"
#include <time.h>
static uint64_t scan_us;
static nes_core_profile_t profile[2];
void nes_port_phase_time(unsigned draw,uint64_t cpu,uint64_t ppu,uint64_t apu) {
    nes_core_profile_t *p=&profile[!!draw];
    p->cpu_bus_us+=cpu;p->ppu_us+=ppu;p->apu_us+=apu;++p->samples;
}
void nes_core_profile_take(nes_core_profile_t out[2]) {
    memcpy(out,profile,sizeof(profile));memset(profile,0,sizeof(profile));
}
uint64_t nes_port_time(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000; }
void nes_port_scan_time(uint64_t us) { scan_us=us; }
uint64_t nes_core_scan_time(void) { return scan_us; }
static uint8_t video[NES_SCREEN_PITCH * NES_SCREEN_HEIGHT] __attribute__((aligned(8)));
static bool loaded, initialized;
static rom_cache_t cartridge_cache;
static rom_slice_t cartridge_slice;
static uint8_t cartridge_header[528];
static size_t cartridge_prg;
const rom_cache_t *nes_core_rom_cache(void){return &cartridge_cache;}
uint32_t nes_port_rom_checksum(void){return cartridge_cache.source.checksum;}
bool nes_port_rom_cached_address(const uint8_t *p){
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)cartridge_cache.data,f=(uintptr_t)cartridge_cache.fault_page;
    return (a>=b && a-b<(size_t)cartridge_cache.count*cartridge_cache.page_size) ||
        (cartridge_cache.fault_page && a>=f && a-f<cartridge_cache.page_size+16u);
}
bool nes_port_rom_paged(void){return cartridge_cache.source.read!=NULL;}
uint8_t *nes_port_rom_page(unsigned chr,size_t offset,unsigned owner){
    return rom_cache_get(&cartridge_cache,(chr?cartridge_prg:0)+offset,owner);
}
bool nes_port_rom_offset(const uint8_t *p,unsigned *chr,uint32_t *offset){
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)cartridge_cache.data;
    if(!nes_port_rom_paged() || a<b || a-b>=(size_t)cartridge_cache.count*cartridge_cache.page_size)return false;
    size_t k=(a-b)/cartridge_cache.page_size;
    size_t at=(size_t)cartridge_cache.slots[k].page*cartridge_cache.page_size+(a-b)%cartridge_cache.page_size;
    *chr=at>=cartridge_prg;*offset=(uint32_t)(at-(*chr?cartridge_prg:0));return true;
}

static const uint32_t crc_table[256] = {
    0x00000000u, 0x77073096u, 0xee0e612cu, 0x990951bau, 0x076dc419u, 0x706af48fu, 0xe963a535u, 0x9e6495a3u,
    0x0edb8832u, 0x79dcb8a4u, 0xe0d5e91eu, 0x97d2d988u, 0x09b64c2bu, 0x7eb17cbdu, 0xe7b82d07u, 0x90bf1d91u,
    0x1db71064u, 0x6ab020f2u, 0xf3b97148u, 0x84be41deu, 0x1adad47du, 0x6ddde4ebu, 0xf4d4b551u, 0x83d385c7u,
    0x136c9856u, 0x646ba8c0u, 0xfd62f97au, 0x8a65c9ecu, 0x14015c4fu, 0x63066cd9u, 0xfa0f3d63u, 0x8d080df5u,
    0x3b6e20c8u, 0x4c69105eu, 0xd56041e4u, 0xa2677172u, 0x3c03e4d1u, 0x4b04d447u, 0xd20d85fdu, 0xa50ab56bu,
    0x35b5a8fau, 0x42b2986cu, 0xdbbbc9d6u, 0xacbcf940u, 0x32d86ce3u, 0x45df5c75u, 0xdcd60dcfu, 0xabd13d59u,
    0x26d930acu, 0x51de003au, 0xc8d75180u, 0xbfd06116u, 0x21b4f4b5u, 0x56b3c423u, 0xcfba9599u, 0xb8bda50fu,
    0x2802b89eu, 0x5f058808u, 0xc60cd9b2u, 0xb10be924u, 0x2f6f7c87u, 0x58684c11u, 0xc1611dabu, 0xb6662d3du,
    0x76dc4190u, 0x01db7106u, 0x98d220bcu, 0xefd5102au, 0x71b18589u, 0x06b6b51fu, 0x9fbfe4a5u, 0xe8b8d433u,
    0x7807c9a2u, 0x0f00f934u, 0x9609a88eu, 0xe10e9818u, 0x7f6a0dbbu, 0x086d3d2du, 0x91646c97u, 0xe6635c01u,
    0x6b6b51f4u, 0x1c6c6162u, 0x856530d8u, 0xf262004eu, 0x6c0695edu, 0x1b01a57bu, 0x8208f4c1u, 0xf50fc457u,
    0x65b0d9c6u, 0x12b7e950u, 0x8bbeb8eau, 0xfcb9887cu, 0x62dd1ddfu, 0x15da2d49u, 0x8cd37cf3u, 0xfbd44c65u,
    0x4db26158u, 0x3ab551ceu, 0xa3bc0074u, 0xd4bb30e2u, 0x4adfa541u, 0x3dd895d7u, 0xa4d1c46du, 0xd3d6f4fbu,
    0x4369e96au, 0x346ed9fcu, 0xad678846u, 0xda60b8d0u, 0x44042d73u, 0x33031de5u, 0xaa0a4c5fu, 0xdd0d7cc9u,
    0x5005713cu, 0x270241aau, 0xbe0b1010u, 0xc90c2086u, 0x5768b525u, 0x206f85b3u, 0xb966d409u, 0xce61e49fu,
    0x5edef90eu, 0x29d9c998u, 0xb0d09822u, 0xc7d7a8b4u, 0x59b33d17u, 0x2eb40d81u, 0xb7bd5c3bu, 0xc0ba6cadu,
    0xedb88320u, 0x9abfb3b6u, 0x03b6e20cu, 0x74b1d29au, 0xead54739u, 0x9dd277afu, 0x04db2615u, 0x73dc1683u,
    0xe3630b12u, 0x94643b84u, 0x0d6d6a3eu, 0x7a6a5aa8u, 0xe40ecf0bu, 0x9309ff9du, 0x0a00ae27u, 0x7d079eb1u,
    0xf00f9344u, 0x8708a3d2u, 0x1e01f268u, 0x6906c2feu, 0xf762575du, 0x806567cbu, 0x196c3671u, 0x6e6b06e7u,
    0xfed41b76u, 0x89d32be0u, 0x10da7a5au, 0x67dd4accu, 0xf9b9df6fu, 0x8ebeeff9u, 0x17b7be43u, 0x60b08ed5u,
    0xd6d6a3e8u, 0xa1d1937eu, 0x38d8c2c4u, 0x4fdff252u, 0xd1bb67f1u, 0xa6bc5767u, 0x3fb506ddu, 0x48b2364bu,
    0xd80d2bdau, 0xaf0a1b4cu, 0x36034af6u, 0x41047a60u, 0xdf60efc3u, 0xa867df55u, 0x316e8eefu, 0x4669be79u,
    0xcb61b38cu, 0xbc66831au, 0x256fd2a0u, 0x5268e236u, 0xcc0c7795u, 0xbb0b4703u, 0x220216b9u, 0x5505262fu,
    0xc5ba3bbeu, 0xb2bd0b28u, 0x2bb45a92u, 0x5cb36a04u, 0xc2d7ffa7u, 0xb5d0cf31u, 0x2cd99e8bu, 0x5bdeae1du,
    0x9b64c2b0u, 0xec63f226u, 0x756aa39cu, 0x026d930au, 0x9c0906a9u, 0xeb0e363fu, 0x72076785u, 0x05005713u,
    0x95bf4a82u, 0xe2b87a14u, 0x7bb12baeu, 0x0cb61b38u, 0x92d28e9bu, 0xe5d5be0du, 0x7cdcefb7u, 0x0bdbdf21u,
    0x86d3d2d4u, 0xf1d4e242u, 0x68ddb3f8u, 0x1fda836eu, 0x81be16cdu, 0xf6b9265bu, 0x6fb077e1u, 0x18b74777u,
    0x88085ae6u, 0xff0f6a70u, 0x66063bcau, 0x11010b5cu, 0x8f659effu, 0xf862ae69u, 0x616bffd3u, 0x166ccf45u,
    0xa00ae278u, 0xd70dd2eeu, 0x4e048354u, 0x3903b3c2u, 0xa7672661u, 0xd06016f7u, 0x4969474du, 0x3e6e77dbu,
    0xaed16a4au, 0xd9d65adcu, 0x40df0b66u, 0x37d83bf0u, 0xa9bcae53u, 0xdebb9ec5u, 0x47b2cf7fu, 0x30b5ffe9u,
    0xbdbdf21cu, 0xcabac28au, 0x53b39330u, 0x24b4a3a6u, 0xbad03605u, 0xcdd70693u, 0x54de5729u, 0x23d967bfu,
    0xb3667a2eu, 0xc4614ab8u, 0x5d681b02u, 0x2a6f2b94u, 0xb40bbe37u, 0xc30c8ea1u, 0x5a05df1bu, 0x2d02ef8du,
};
uint32_t nes_port_crc32(uint32_t seed, const void *data, size_t size) {
    const uint8_t *p = data;
    uint32_t crc = ~seed;
    while (size--) crc = (crc >> 8) ^ crc_table[(crc ^ *p++) & 255u];
    return ~crc;
}

const char *nes_core_validate(const uint8_t *b, size_t size) {
    if (!b || size < 16 || size > NES_ROM_LIMIT) return "ROM size invalid (max 6 MiB)";
    if (memcmp(b, "NES\032", 4)) return "Expected an iNES .nes file";
    if ((b[7] & 12) == 8) return "NES 2.0 is not supported by this core";
    if (b[7] & 3) return "VS/PlayChoice console is not supported";
    if (!b[4]) return "ROM has no PRG banks";
    size_t required = 16u + ((b[6] & 4) ? 512u : 0u) + b[4] * 16384u + b[5] * 8192u;
    if (required > size) return "ROM is truncated";
    unsigned mapper = (b[6] >> 4) | (b[7] & 0xf0);
    if (b[12] || b[13] || b[14] || b[15]) mapper &= 15;
    for (unsigned i = 0; mappers[i]; ++i)
        if (mappers[i]->number == mapper) return NULL;
    return "Mapper is not supported by Nofrendo";
}
static const char *load_cartridge(uint8_t *bytes,size_t size,const rom_source_t *source,size_t budget) {
    const char *error = nes_core_validate(bytes, size);
    if (error) return error;
    nes_core_close();
    if(source){
        unsigned skip=16+((bytes[6]&4)?512:0);
        cartridge_prg=(size_t)bytes[4]*16384;
        cartridge_slice=(rom_slice_t){*source,skip};
        rom_source_t logical={&cartridge_slice,rom_slice_read,size-skip,source->checksum};
        if(!rom_cache_init(&cartridge_cache,&logical,2048,budget,48))return "Cannot allocate cartridge cache";
    }
    if (!nes_init(SYS_DETECT, 16000, false, NULL)) return "NES initialization failed";
    initialized = true;
    rom_t *cart = rom_loadmem(bytes, size);
    if (!cart) { nes_core_close(); return "ROM load/database size check failed"; }
    if (cart->system == SYS_UNKNOWN) cart->system = (bytes[9] & 1) ? SYS_NES_PAL : SYS_NES_NTSC;
    if (nes_insertcart(cart) < 0) {
        /* nes_insertcart already shuts down on failure. */
        initialized = false;
        return "Mapper initialization failed";
    }
    if (cart->trainer) memcpy(cart->prg_ram + 0x1000, cart->trainer, 512);
    loaded = true;
    nes_setvidbuf(video);
    input_connect(0, NES_JOYPAD);
    input_connect(1, NES_JOYPAD);
    memset(video, 0, sizeof(video));
    return NULL;
}
const char *nes_core_load(uint8_t *bytes,size_t size){return load_cartridge(bytes,size,NULL,0);}
const char *nes_core_load_source(const rom_source_t *source,size_t budget){
    if(!source || !rom_source_read(source,0,cartridge_header,sizeof(cartridge_header)))return "Cannot read iNES header";
    return load_cartridge(cartridge_header,(size_t)source->size,source,budget);
}

void nes_core_close(void) {
    if (initialized) nes_shutdown();
    loaded = initialized = false;rom_cache_close(&cartridge_cache);
}
__attribute__((noinline)) void nes_core_frame(bool draw, uint8_t pad1, uint8_t pad2) {
    if (!loaded) return;
    input_update(0, pad1); input_update(1, pad2);
    if (nes_getptr()->cpu_idle_next || nes6502_idle_hint()) nes_emulate_idle(draw);
    else nes_emulate(draw);
}
const uint8_t *nes_core_video(void) { return video + 8; }
const int16_t *nes_core_audio(size_t *samples) {
    *samples = loaded ? nes_getptr()->apu->samples_per_frame : 0;
    return loaded ? nes_getptr()->apu->buffer : NULL;
}
uint8_t *nes_core_sram(size_t *bytes) {
    rom_t *cart = loaded ? nes_getptr()->cart : NULL;
    *bytes = cart && cart->battery ? cart->prg_ram_banks * 8192u : 0;
    return *bytes ? cart->prg_ram : NULL;
}
uint32_t nes_core_checksum(void) { return loaded ? nes_getptr()->cart->checksum : 0; }
unsigned nes_core_rate(void) { return loaded ? nes_getptr()->refresh_rate : 60; }
unsigned nes_core_mapper(void) { return loaded ? nes_getptr()->cart->mapper_number : 0; }
uint16_t nes_core_color(unsigned i) {
    const uint8_t *rgb = nes_palettes[1] + (i & 63u) * 3u;
    return ((rgb[0] >> 3) << 11) | ((rgb[1] >> 2) << 5) | (rgb[2] >> 3);
}

#ifdef __wasm__
_Noreturn void abort(void) { __builtin_trap(); }
#endif
