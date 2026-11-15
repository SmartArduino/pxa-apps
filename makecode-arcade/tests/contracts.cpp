// MIT. Failure paths must not corrupt the currently loaded game or its save.
extern "C" {
#include "core.h"
void arcade_settings_reset();
size_t arcade_settings_size();
bool arcade_settings_export(void *,size_t);
bool arcade_settings_import(const void *,size_t);
}
#include "pxt.h"
#include "cartridge.hpp"
#include <assert.h>
#include <vector>
#ifdef __linux__
#include <malloc.h>
#endif
namespace settings { int _set(String,Buffer); Buffer _get(String); bool _exists(String); }
static void write32(uint8_t *p,uint32_t x) {for(unsigned i=0;i<4;++i)p[i]=x>>(8*i);}
int main(int argc,char **argv) {
#if defined(__linux__) && UINTPTR_MAX == 0xffffffffu
    mallopt(M_MMAP_MAX,0);
#endif
    assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);
    fseek(file,0,SEEK_END);size_t size=ftell(file);rewind(file);
    std::vector<uint8_t> rom(size);assert(fread(rom.data(),1,size,file)==size);fclose(file);
    ArcadeCartridge info{};assert(!arcade_cartridge_inspect(rom.data(),rom.size(),&info));
    for(size_t length=0;length<info.image_size;length+=101)
        assert(arcade_cartridge_inspect(rom.data(),length,nullptr));
    for(size_t at=0;at<info.image_size;) {
        uint32_t length;memcpy(&length,rom.data()+at+4,4);
        auto broken=rom;write32(broken.data()+at+4,UINT32_MAX);
        assert(arcade_cartridge_inspect(broken.data(),broken.size(),nullptr));
        broken=rom;broken[at]=0xff;assert(arcade_cartridge_inspect(broken.data(),broken.size(),nullptr));
        at+=length;
    }
    assert(!emu_core_load(rom.data(),size));assert(emu_core_rom_cache() && !emu_core_rom_cache()->error);
    for(unsigned i=0;i<120;++i)emu_core_frame_render(0,0,true);
    const auto runtime=pxt::current_time_ms();assert(runtime==2000);
    assert(emu_core_load(nullptr,0)); // Invalid selection leaves the live game intact.
    emu_core_frame_render(0,0,true);assert(pxt::current_time_ms()>runtime);
    arcade_settings_reset(); // Game startup has already created internal keys.
    auto score=pxt::mkString("score");pxt::registerGCObj(score);
    uint8_t value[]={0,1,2,3,4};auto buffer=pxt::mkBuffer(value,sizeof(value));pxt::registerGCObj(buffer);
    assert(settings::_set(nullptr,buffer)==-1);
    assert(settings::_set(score,nullptr)==-1);
    assert(settings::_set(pxt::mkString(""),buffer)==-1);
    assert(settings::_set(score,buffer)==0);
    for(unsigned i=1;i<32;++i) {
        char name[24];snprintf(name,sizeof(name),"key-%u",i);
        assert(settings::_set(pxt::mkString(name),buffer)==0);
    }
    assert(settings::_set(pxt::mkString("overflow"),buffer)==-1);
    std::vector<uint8_t> original(arcade_settings_size()),other(original.size()+1);
    assert(arcade_settings_export(original.data(),original.size()));
    assert(!arcade_settings_export(nullptr,original.size()));assert(!arcade_settings_import(nullptr,original.size()));
    memcpy(other.data()+1,original.data(),original.size());
    assert(arcade_settings_import(other.data()+1,original.size())); // Unaligned file bytes.
    other[25]^=0x80;assert(!arcade_settings_import(other.data()+1,original.size()));
    std::vector<uint8_t> after(original.size());assert(arcade_settings_export(after.data(),after.size()));assert(after==original);
    // Legacy saves migrate, but duplicate keys must be rejected atomically.
    std::vector<uint8_t> legacy(4+32*(128+4+1024));write32(legacy.data(),0x3158434d);
    memcpy(legacy.data()+4,"score",6);write32(legacy.data()+4+128,5);memcpy(legacy.data()+4+132,value,5);
    assert(arcade_settings_import(legacy.data(),legacy.size()));assert(settings::_exists(score));
    auto data=settings::_get(score);assert(data && data->length==5 && !memcmp(data->data,value,5));
    memcpy(legacy.data()+4+1156,"score",6);assert(!arcade_settings_import(legacy.data(),legacy.size()));
    pxt::unregisterGCObj(buffer);pxt::unregisterGCObj(score);
    emu_core_close();assert(!emu_core_load(rom.data(),size));assert(pxt::current_time_ms()==0);
    emu_core_close();puts("PASS cartridge bounds, live-game preservation, clock reset, save migration/checksum/atomicity");
}
