// MIT. Container bounds and ABI checks independent of VM allocation/state.
#include "cartridge.hpp"
#include "app_config.h"
#include <string.h>
extern "C" uint32_t emu_port_crc32(uint32_t,const void *,size_t);
namespace {
uint32_t read32(const uint8_t *p) { return p[0]|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
}
const char *arcade_cartridge_inspect(const uint8_t *rom,size_t size,ArcadeCartridge *result) {
    if (!rom || size<264 || size>EMU_ROM_LIMIT) return "Invalid PXT64 cartridge size";
    static const uint8_t magic[]={10,'P','X','T','6','4',10,0,0x54,0x71,0xfe,0x2b,0x5e,0x21,0x37,0x68};
    if (rom[0]!=1 || memcmp(rom+8,magic,sizeof(magic))) return "Not a MakeCode Arcade PXT64 cartridge";
    size_t at=0; unsigned seen=0,functions=0;
    while (at<size) {
        static const uint8_t zero[32]={};
        bool trailer=false;
        for(unsigned padding=0;padding<=32;padding+=8)
            if(size-at>=padding+4 && !memcmp(rom+at,zero,padding) && !memcmp(rom+at+padding,"SRCM",4)) trailer=true;
        if(trailer) break;
        if (size-at<8) return "Truncated PXT64 section header";
        unsigned type=rom[at],length=read32(rom+at+4);
        if (length<16 || length>=32000 || (length&7) || length>size-at) return "Invalid PXT64 section bounds";
        if (type>=1 && type<=6) {
            unsigned bit=1u<<type;
            if (seen&bit) return "Duplicate PXT64 metadata section";
            seen|=bit;
        }
        switch (type) {
        case 1:
            if (at || length<264) return "Invalid PXT64 program header";
            break;
        case 2:
            if (rom[at+length-1]) return "Unterminated PXT64 opcode table";
            break;
        case 3: case 5: break;
        case 4:
            if (read32(rom+at+length-8)) return "Unterminated PXT64 configuration";
            break;
        case 6:
            if ((length-8)%12 && (length-12)%12) return "Invalid PXT64 boxed numbers";
            break;
        case 0x20:
            if (length<32) return "Truncated PXT64 function";
            ++functions;break;
        case 0x21:
            if (length<24) return "Truncated PXT64 literal";
            break;
        case 0x22:
            if (length<64) return "Truncated PXT64 class table";
            break;
        default: return "Unknown PXT64 section type";
        }
        at+=length;
    }
    if ((seen&0x7e)!=0x7e || !functions) return "Incomplete PXT64 cartridge";
    // Preserve 0.1 save identities; debug/source-map trailers are not gameplay.
    if (result) *result={at,emu_port_crc32(0,rom,at)};
    return nullptr;
}
