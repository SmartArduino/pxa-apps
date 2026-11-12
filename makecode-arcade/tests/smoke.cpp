extern "C" {
#include "core.h"
}
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "pxt.h"
#include "mixer---ext/SoundOutput.h"
#include "mixer---ext/melody.h"
namespace music { void queuePlayInstructions(int, Buffer); }
#ifdef __linux__
#include <malloc.h>
#endif
static void log_line(const char *line) { fprintf(stderr,"%s\n",line); }
int main(int argc,char **argv) {
    if(argc<2) return 2;
#if defined(__linux__) && UINTPTR_MAX == 0xffffffffu
    // Upstream's 32-bit GC reserves the top address nibble as object flags.
    // Linux mmap normally returns 0xf..., unlike the bounded Wasm arena.
    mallopt(M_MMAP_MAX,0);
#endif
    if(getenv("ARCADE_LOG")) emu_core_set_log_sink(log_line);
    FILE *f=fopen(argv[1],"rb"); assert(f); fseek(f,0,SEEK_END); size_t size=ftell(f); rewind(f);
    auto rom=static_cast<uint8_t *>(calloc(1,size+16)); assert(rom); assert(fread(rom,1,size,f)==size); fclose(f);
    assert(emu_core_validate(nullptr,0));
    assert(emu_core_validate(rom,63));
    uint32_t first_video=0,first_audio=0;
    for(unsigned run=0;run<2;++run) {
        const char *error=emu_core_load(rom,size); if(error) {fprintf(stderr,"%s\n",error); return 1;}
        auto start=clock(); uint32_t crc=0,pcm=0; unsigned frames=argc>2?atoi(argv[2]):600, audible=0;
        for(unsigned i=0;i<frames;++i) {
            if(i==120) {
                SoundInstruction tone={SW_SINE,0,440,300,512,512,440};
                auto buffer=pxt::mkBuffer(reinterpret_cast<uint8_t *>(&tone),sizeof(tone));
                music::queuePlayInstructions(0,buffer);
            }
            emu_core_frame_render(i>100?EMU_PAD_RIGHT|EMU_PAD_A:0,0,true);
            crc=emu_port_crc32(crc,emu_core_video(),160*120*2);
            size_t n; auto audio=emu_core_audio(&n); assert(n==735); pcm=emu_port_crc32(pcm,audio,n*4);
            for(size_t j=0;j<n*2;++j)audible+=audio[j]!=0;
        }
        unsigned colors=0; const auto *v=emu_core_video(); for(unsigned i=1;i<160*120;++i) colors+=v[i]!=v[0];
        printf("run=%u pointer_bits=%zu frames=%u cpu_ms=%.2f video=%08x audio=%08x nonuniform=%u\n",run,sizeof(void*)*8,frames,1000.0*(clock()-start)/CLOCKS_PER_SEC,crc,pcm,colors);
        unsigned nonzero=0;for(unsigned i=0;i<160*120;++i)nonzero+=v[i]!=0;
        assert(nonzero); // Button probes intentionally render a uniform color.
        if(frames>140)assert(audible);
        if(strstr(argv[1],"probe.pxt64") && frames>200)assert(v[0]==0xf904);
        if(!run) {first_video=crc;first_audio=pcm;} else {assert(crc==first_video);assert(pcm==first_audio);}
        auto bytes=emu_core_save_size();assert(bytes);
        auto save=static_cast<uint8_t *>(malloc(bytes));assert(save);
        assert(emu_core_save_export(save,bytes));assert(emu_core_save_import(save,bytes));
        save[0]^=1;assert(!emu_core_save_import(save,bytes));free(save);
        emu_core_close();
    }
    free(rom);
}
