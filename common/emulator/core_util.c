/* PXA console adapter utilities. GPL-2.0-or-later. */
#include "core.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
emu_core_info_t emu_core_info={.width=256,.height=224,.stride=256,
    .aspect_num=4,.aspect_den=3,.rate_num=60,.rate_den=1,.audio_rate=32000};
static emu_log_sink sink;
void emu_core_set_log_sink(emu_log_sink value){sink=value;}
void emu_core_log(const char *format,...){
    if(!sink)return;
    char line[240];va_list args;va_start(args,format);
    vsnprintf(line,sizeof(line),format,args);va_end(args);sink(line);
}
void emu_core_defaults(void){
    emu_core_info=(emu_core_info_t){.width=256,.height=224,.stride=256,
        .aspect_num=4,.aspect_den=3,.rate_num=60,.rate_den=1,.audio_rate=32000};
}
uint32_t emu_port_crc32(uint32_t seed,const void *data,size_t size){
    static uint32_t table[256];
    if(!data || !size)return seed;
    if(!table[1])for(unsigned i=0;i<256;++i){
        uint32_t v=i;for(unsigned j=0;j<8;++j)v=(v>>1)^((0u-(v&1))&0xedb88320u);table[i]=v;
    }
    uint32_t crc=~seed;const uint8_t *p=data;
    for(size_t i=0;i<size;++i)crc=(crc>>8)^table[(crc^p[i])&255];
    return ~crc;
}
void emu_core_frame_to(uint16_t p1,uint16_t p2,uint16_t *target,size_t stride){
    if(!target)return;
    emu_core_frame_render(p1,p2,true);if(stride<EMU_VIDEO_WIDTH)return;
    const uint16_t *source=emu_core_video();
    if(!source)return;
    for(unsigned y=0;y<EMU_VIDEO_HEIGHT;++y)
        memcpy(target+y*stride,source+y*emu_core_info.stride,EMU_VIDEO_WIDTH*2u);
}

__attribute__((weak)) void emu_core_set_indexed_output(bool enabled){(void)enabled;}
__attribute__((weak)) const uint8_t *emu_core_indexed_video(unsigned *stride,const uint16_t **palette){(void)stride;(void)palette;return NULL;}
