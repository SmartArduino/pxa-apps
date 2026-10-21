/* Deterministic gnuboy state and throughput; not a display FPS measurement. */
#include "core.h"
#include "vendor/gnuboy/hw.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
void gb_core_test_clock(uint64_t value);
static uint64_t now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*1000000000+ts.tv_nsec;}
int main(int argc,char **argv){
    if(argc<2)return 2;
    unsigned frames=argc>2?(unsigned)atoi(argv[2]):600,interval=argc>3?(unsigned)atoi(argv[3]):1;
    if(!frames || !interval)return 2;
    FILE *fp=fopen(argv[1],"rb");if(!fp)return 2;fseek(fp,0,SEEK_END);long size=ftell(fp);rewind(fp);
    if(size<0x8000 || size>GB_ROM_LIMIT)return 2;
    uint8_t *rom=malloc((size_t)size);if(!rom || fread(rom,1,size,fp)!=(size_t)size)return 2;fclose(fp);
    gb_core_test_clock(1700000000);
    if(getenv("GB_CORE_MUTED"))gb_core_set_audio_enabled(false);
    const char *error=gb_core_load(rom,size);if(error){fprintf(stderr,"%s\n",error);return 1;}
    unsigned peak=0,drawn=0;uint64_t samples=0,draw_ns=0,skip_ns=0;uint32_t pcm_crc=0;
    uint64_t started=now();
    for(unsigned i=0;i<frames;++i){
        unsigned phase=i%600;
        uint16_t pad=phase==150?GB_PAD_START:phase>300?GB_PAD_RIGHT|((phase%80)<20?GB_PAD_A:0):0;
        if(getenv("GB_TEST_UNMUTE_AT") && i==(unsigned)atoi(getenv("GB_TEST_UNMUTE_AT")))gb_core_set_audio_enabled(true);
        bool render=i%interval==0 || i+1==frames;
        uint64_t start=now();gb_core_frame_render(pad,render);uint64_t elapsed=now()-start;
        if(render){draw_ns+=elapsed;++drawn;}else skip_ns+=elapsed;
        size_t count;const int16_t *pcm=gb_core_audio(&count);samples+=count;
        for(size_t j=0;j<count*2;++j){unsigned v=pcm[j]<0?-(int)pcm[j]:pcm[j];if(v>peak)peak=v;}
        pcm_crc=gb_port_crc32(pcm_crc,pcm,count*4);
    }
    uint64_t elapsed=now()-started;
    uint32_t ram=gb_port_crc32(0,GB.rambanks,32768)^gb_port_crc32(0,GB.vbanks,16384);
    printf("{\"engine\":\"gnuboy\",\"model\":%u,\"frames\":%u,\"drawn\":%u,\"elapsed_us\":%llu,\"draw_avg_us\":%llu,\"skip_avg_us\":%llu,"
        "\"samples\":%llu,\"peak\":%u,\"pcm_crc\":\"%08x\",\"video_crc\":\"%08x\",\"ram_crc\":\"%08x\",\"pc\":%u,\"sp\":%u,\"af\":%u,\"bc\":%u,\"de\":%u,\"hl\":%u}\n",
        gb_core_model(),frames,drawn,(unsigned long long)(elapsed/1000),
        (unsigned long long)(drawn?draw_ns/drawn/1000:0),(unsigned long long)(frames>drawn?skip_ns/(frames-drawn)/1000:0),
        (unsigned long long)samples,peak,pcm_crc,gb_port_crc32(0,gb_core_video(),160*144*2),ram,
        GB.cpu->pc.w,GB.cpu->sp.w,GB.cpu->af.w,GB.cpu->bc.w,GB.cpu->de.w,GB.cpu->hl.w);
    gb_core_close();free(rom);return 0;
}
