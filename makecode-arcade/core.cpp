/* PXA frontend adapter. GPL-2.0-or-later. Runtime itself remains MIT. */
extern "C" {
#include "core.h"
void arcade_settings_reset();
size_t arcade_settings_size();
bool arcade_settings_export(void *, size_t);
bool arcade_settings_import(const void *, size_t);
uint64_t pxt_vm_frame_request();
void pxt_screen_get_pixels(int, int, uint32_t *);
void pxt_raise_event(int, int);
void pxt_get_audio_samples(int16_t *, unsigned);
}
#include "pxt.h"
namespace pxt { void pxa_vm_step(); void pxa_vm_cleanup(); void pxa_image_identity(const uint8_t *, unsigned); }
namespace {
uint32_t rgb[160*120];
uint16_t pixels[160*120];
int16_t pcm[735*2], mono[735];
uint16_t previous;
bool audio = true, loaded;
char load_error[160];
size_t image_size(const uint8_t *rom, size_t size) {
    size_t at=0;
    while (size-at>=8) {
        if (!memcmp(rom+at,"SRCM",4)) return at;
        static const uint8_t padding[16]={};
        if (size-at>=20 && !memcmp(rom+at,padding,16) && !memcmp(rom+at+16,"SRCM",4)) return at;
        uint32_t n; memcpy(&n,rom+at+4,4);
        if(n<16 || n>=32000 || (n&7) || n>size-at) return 0;
        at+=n;
    }
    return at==size?at:0;
}
}
extern "C" {
const char *emu_core_validate(const uint8_t *rom, size_t size) {
    if (!rom || size < 64 || size > EMU_ROM_LIMIT) return "Invalid PXT64 image size";
    uint64_t magic; memcpy(&magic, rom+8, 8); if (magic != VM_MAGIC0) return "Not a PXT64 cartridge; export the VM target in MakeCode Arcade";
    size=image_size(rom,size);
    if(size<64) return "Invalid or truncated PXT64 section chain";
    size_t offset = 0;
    while (offset < size) {
        if (size-offset < 8) return "Truncated PXT64 section";
        uint32_t n; memcpy(&n, rom+offset+4, 4);
        if (n < 16 || n >= 32000 || (n & 7) || n > size-offset) return "Invalid or unsupported PXT64 section";
        offset += n;
    }
    return nullptr;
}
const char *emu_core_load(uint8_t *rom, size_t size) {
    emu_core_close(); const char *problem = emu_core_validate(rom, size); if (problem) return problem;
    size=image_size(rom,size);
    auto copy = static_cast<uint8_t *>(calloc(1, size+16)); if (!copy) return "No memory for PXT64 image";
    memcpy(copy, rom, size); pxt::pxa_image_identity(copy, size);
    pxt::gcPreStartup(); pxt::panicCode = 0;
    auto state = pxt::vm_alloc_patch_state();
    problem = pxt::vm_patch_image(state, copy, size);
    if (problem) { snprintf(load_error, sizeof(load_error), "PXT64 patch: %s", problem); pxt::vm_finish_patch(state); free(copy); return load_error; }
    pxt::vm_finish_patch(state);
    auto img = pxt::loadVMImage(copy, size);
    if (img->errorCode) { snprintf(load_error, sizeof(load_error), "PXT64 validation %d at 0x%x", img->errorCode, img->errorOffset); pxt::unloadVMImage(img); return load_error; }
    pxt::vmImg = img;
    pxt::gcStartup(); pxt_vm_set_online(1);
    arcade_settings_reset(); previous = 0; loaded = true;
    emu_core_defaults(); emu_core_info.width=160; emu_core_info.height=120; emu_core_info.stride=160;
    emu_core_info.audio_rate=44100; emu_core_info.checksum=emu_port_crc32(0,rom,size);
    strcpy(emu_core_info.title, "MakeCode Arcade");
    pxt::globals = static_cast<pxt::TValue *>(pxt::app_alloc(sizeof(pxt::TValue)*pxt::getNumGlobals()));
    memset(pxt::globals, 0, sizeof(pxt::TValue)*pxt::getNumGlobals());
    pxt::initRuntime();
    return nullptr;
}
const char *emu_core_load_source(const rom_source_t *, size_t) { return "PXT64 cartridges must fit in memory"; }
const rom_cache_t *emu_core_rom_cache() { return nullptr; }
void emu_core_close() {
    if (!loaded) return;
    pxt::pxa_vm_cleanup(); pxt::unloadVMImage(pxt::vmImg); pxt::vmImg=nullptr;
    loaded=false; previous=0; memset(pixels,0,sizeof(pixels)); memset(pcm,0,sizeof(pcm));
}
void emu_core_set_audio_enabled(bool enabled) { audio=enabled; }
void emu_core_frame_render(uint16_t player1, uint16_t player2, bool render) {
    if (!loaded) return;
    static const uint16_t keys[] = {EMU_PAD_LEFT,EMU_PAD_UP,EMU_PAD_RIGHT,EMU_PAD_DOWN,EMU_PAD_A,EMU_PAD_B,EMU_PAD_START};
    // The second controller supplies the same first-player controls for now.
    uint16_t buttons=player1|player2;
    for (unsigned i=0;i<7;++i) if ((buttons^previous)&keys[i]) pxt_raise_event(buttons&keys[i]?2051:2050,i+1);
    previous=buttons;
    pxt_vm_frame_request(); pxt::pxa_vm_step();
    if (render) {
        pxt_screen_get_pixels(160,120,rgb);
        for (unsigned i=0;i<160*120;++i) { auto c=rgb[i]; pixels[i]=((c>>8)&0xf800)|((c>>5)&0x7e0)|((c>>3)&31); }
    }
    // Always advance the mixer, including while output is muted.
    pxt_get_audio_samples(mono,735);
    for (unsigned i=0;i<735;++i) pcm[i*2]=pcm[i*2+1]=audio?mono[i]:0;
}
const uint16_t *emu_core_video() { return pixels; }
const int16_t *emu_core_audio(size_t *frames) { *frames=loaded?735:0; return pcm; }
size_t emu_core_save_size() { return loaded?arcade_settings_size():0; }
bool emu_core_save_export(uint8_t *data,size_t size) { return arcade_settings_export(data,size); }
bool emu_core_save_import(const uint8_t *data,size_t size) { return arcade_settings_import(data,size); }
size_t emu_state_size() { return 0; }
const char *emu_state_save(void *,size_t) { return "Instant snapshots are unavailable for MakeCode Arcade"; }
const char *emu_state_load(const void *,size_t) { return "Instant snapshots are unavailable for MakeCode Arcade"; }
}
