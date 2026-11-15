/* PXA frontend adapter. GPL-2.0-or-later. Runtime itself remains MIT. */
extern "C" {
#include "core.h"
void arcade_settings_reset();
size_t arcade_settings_size();
bool arcade_settings_export(void *, size_t);
bool arcade_settings_import(const void *, size_t);
uint64_t pxt_vm_frame_request();
void pxt_screen_get_rgb565(uint16_t *, size_t);
void pxt_raise_event(int, int);
void pxt_get_audio_samples(int16_t *, unsigned);
}
#include "pxt.h"
#include "cartridge.hpp"
namespace pxt { void pxa_vm_step(); void pxa_vm_cleanup(); void pxa_image_identity(const uint8_t *, unsigned); }
namespace pxt { extern bool pxaResetRequested; }
namespace {
uint8_t *original_rom;
size_t original_size;
uint16_t pixels[160*120];
int16_t pcm[735*2], mono[735];
uint16_t previous[2];
const rom_cache_t direct_cache = {};
bool audio = true, loaded;
char load_error[160];

}
extern "C" {
const char *emu_core_validate(const uint8_t *rom, size_t size) {
    return arcade_cartridge_inspect(rom,size,nullptr);
}

const char *emu_core_load(uint8_t *rom, size_t size) {
    ArcadeCartridge cartridge{};
    const char *problem=arcade_cartridge_inspect(rom,size,&cartridge);
    if (problem) return problem;
    emu_core_close();
    original_rom=rom;original_size=size;pxt::pxaResetRequested=false;
    const auto checksum=cartridge.checksum;
    size=cartridge.image_size;
    auto copy = static_cast<uint8_t *>(calloc(1, size+16)); if (!copy) return "No memory for PXT64 image";
    memcpy(copy, rom, size); pxt::pxa_image_identity(copy, size);
    pxt::gcPreStartup(); pxt::panicCode = 0;
    auto state = pxt::vm_alloc_patch_state();
    if (!state) { free(copy); return "No memory for PXT64 relocations"; }
    problem = pxt::vm_patch_image(state, copy, size);
    if (problem) { snprintf(load_error, sizeof(load_error), "PXT64 patch: %s", problem); pxt::vm_finish_patch(state); free(copy); return load_error; }
    pxt::vm_finish_patch(state);
    auto img = pxt::loadVMImage(copy, size);
    if (img->errorCode) { snprintf(load_error, sizeof(load_error), "PXT64 validation %d at 0x%x", img->errorCode, img->errorOffset); pxt::unloadVMImage(img); return load_error; }
    pxt::vmImg = img;
    if (pxt::getConfig(CFG_DISPLAY_WIDTH,160)!=160 || pxt::getConfig(CFG_DISPLAY_HEIGHT,128)!=120) {
        pxt::unloadVMImage(img); pxt::vmImg=nullptr;
        return "This cartridge requires a display other than 160x120";
    }
    pxt::gcStartup(); pxt_vm_set_online(1);
    arcade_settings_reset(); memset(previous,0,sizeof(previous)); loaded = true;
    emu_core_defaults(); emu_core_info.width=160; emu_core_info.height=120; emu_core_info.stride=160;
    emu_core_info.audio_rate=44100; emu_core_info.checksum=checksum;
    emu_core_info.aspect_num=4; emu_core_info.aspect_den=3;
    const auto *name=img->infoHeader->name;
    size_t length=0;while(length<sizeof(img->infoHeader->name) && name[length])++length;
    if(length && length<sizeof(emu_core_info.title)) {
        memcpy(emu_core_info.title,name,length); emu_core_info.title[length]=0;
    } else strcpy(emu_core_info.title, "MakeCode Arcade");
    pxt::globals = static_cast<pxt::TValue *>(pxt::app_alloc(sizeof(pxt::TValue)*pxt::getNumGlobals()));
    memset(pxt::globals, 0, sizeof(pxt::TValue)*pxt::getNumGlobals());
    pxt::initRuntime();
    return nullptr;
}
const char *emu_core_load_source(const rom_source_t *, size_t) { return "PXT64 cartridges must fit in memory"; }
const rom_cache_t *emu_core_rom_cache() { return &direct_cache; }
void emu_core_close() {
    if (!loaded) return;
    pxt::pxa_vm_cleanup(); pxt::unloadVMImage(pxt::vmImg); pxt::vmImg=nullptr;
    original_rom=nullptr;original_size=0;pxt::pxaResetRequested=false;
    loaded=false; memset(previous,0,sizeof(previous)); memset(pixels,0,sizeof(pixels)); memset(pcm,0,sizeof(pcm));
}
void emu_core_set_audio_enabled(bool enabled) { audio=enabled; }
void emu_core_frame_render(uint16_t player1, uint16_t player2, bool render) {
    if (!loaded) return;
    static const uint16_t keys[] = {EMU_PAD_LEFT,EMU_PAD_UP,EMU_PAD_RIGHT,EMU_PAD_DOWN,EMU_PAD_A,EMU_PAD_B,EMU_PAD_START};
    const uint16_t pads[]={player1,player2};
    for(unsigned player=0;player<2;++player) {
        for (unsigned i=0;i<7;++i) if ((pads[player]^previous[player])&keys[i])
            pxt_raise_event(pads[player]&keys[i]?2051:2050,i+1+player*7);
        previous[player]=pads[player];
    }
    pxt_vm_frame_request(); pxt::pxa_vm_step();
    if(pxt::pxaResetRequested) {
        // The frontend owns the borrowed cartridge until close. Preserve only
        // settings, as a real device reset discards all transient VM state.
        uint8_t saved[16+32*(128+4+1024)];
        const auto bytes=arcade_settings_size();
        const bool keep=bytes<=sizeof(saved) && arcade_settings_export(saved,bytes);
        auto source=original_rom;auto size=original_size;
        const char *failure=emu_core_load(source,size);
        if(failure) emu_core_log("ARCADE reset failed: %s",failure);
        else if(keep) arcade_settings_import(saved,bytes);
        return;
    }
    if (render) {
        pxt_screen_get_rgb565(pixels,160);
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
