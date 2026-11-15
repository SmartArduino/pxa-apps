// MIT. Runs compiler-generated fixtures through the actual PXT bytecode VM.
extern "C" {
#include "core.h"
}
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <string>
#ifdef __linux__
#include <malloc.h>
#endif
int main(int argc,char **argv) {
#if defined(__linux__) && UINTPTR_MAX == 0xffffffffu
    mallopt(M_MMAP_MAX,0);
#endif
    assert(argc==2);
    for(const char *name:{"exceptions","scheduler","multiplayer","reset"}) {
        std::string path=std::string(argv[1])+"/"+name+".pxt64";
        FILE *f=fopen(path.c_str(),"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
        std::vector<uint8_t> bytes(n);assert(fread(bytes.data(),1,n,f)==n);fclose(f);
        const char *problem=emu_core_load(bytes.data(),n);if(problem){fprintf(stderr,"%s: %s\n",name,problem);return 1;}
        const bool multiplayer=std::string(name)=="multiplayer";
        for(unsigned frame=0;frame<2000;++frame) {
            auto p1=multiplayer && frame>=60 && frame<80?EMU_PAD_A:0;
            auto p2=multiplayer && frame>=120 && frame<140?EMU_PAD_A:0;
            emu_core_frame_render(p1,p2,true);
            if(multiplayer && frame==100)assert(emu_core_video()[0]==0xf904);
        }
        auto color=emu_core_video()[0];
        if(color!=0x7eea){fprintf(stderr,"%s: expected green, got %04x\n",name,color);return 1;}
        emu_core_close();printf("PASS %s\n",name);
    }
}
