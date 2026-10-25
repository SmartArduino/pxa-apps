/* GPL-2.0-or-later. Real-ROM probe; keeps only the bounded gpSP page cache. */
#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
static uint64_t read_calls, read_bytes;
static void log_core(const char *message) {fprintf(stderr,"%s\n",message);}
static int read_file(void *context,uint64_t offset,void *dst,size_t bytes) {
    FILE *f=context;++read_calls;read_bytes+=bytes;
    return fseeko(f,(off_t)offset,SEEK_SET)||fread(dst,1,bytes,f)!=bytes ? -1 : 0;
}
static uint64_t now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
static uint32_t advance(unsigned first,unsigned count,unsigned interval,uint64_t *samples,unsigned *signal) {
    uint32_t crc=0;
    for(unsigned i=first;i<first+count;++i) {
        unsigned keys=i==60||i==180?EMU_PAD_START:i>210&&i%60<15?EMU_PAD_A:0;
        emu_core_frame_render(keys,0,i%interval==0);
        size_t n;const int16_t *pcm=emu_core_audio(&n);*samples+=n;
        for(size_t k=0;k<n*2;++k)*signal|=pcm[k]!=0;
        crc=emu_port_crc32(crc,pcm,n*4);
        assert(!emu_core_rom_cache()->error);
    }
    for(unsigned y=0;y<EMU_VIDEO_HEIGHT;++y)crc=emu_port_crc32(crc,emu_core_video()+y*emu_core_info.stride,EMU_VIDEO_WIDTH*2);
    return crc;
}
int main(int argc,char **argv) {
    emu_core_set_log_sink(log_core);
    if(argc<2)return 2;FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    fseeko(f,0,SEEK_END);size_t size=ftello(f);rewind(f);
    uint8_t block[32768];size_t n;uint32_t crc=0;
    while((n=fread(block,1,sizeof(block),f)))crc=emu_port_crc32(crc,block,n);
    if(ferror(f))return 2;
    rom_source_t source={f,read_file,size,crc};const char *error=emu_core_load_source(&source,EMU_ROM_CACHE_BUDGET);
    if(error){fprintf(stderr,"%s\n",error);return 1;}
    unsigned frames=argc>2?strtoul(argv[2],NULL,10):300,interval=argc>3?strtoul(argv[3],NULL,10):1;
    assert(frames&&interval);uint64_t samples=0;unsigned signal=0;
    uint64_t start=now();uint32_t fingerprint=advance(0,frames,interval,&samples,&signal);uint64_t elapsed=now()-start;
    size_t state_size=emu_state_size();uint8_t *state=malloc(state_size);assert(state&&!emu_state_save(state,state_size));
    uint32_t replay1=advance(frames,30,1,&samples,&signal);assert(!emu_state_load(state,state_size));
    uint32_t replay2=advance(frames,30,1,&samples,&signal);assert(replay1==replay2);
    if(argc>4){FILE *out=fopen(argv[4],"wb");assert(out);fprintf(out,"P6\n240 160\n255\n");
        const uint16_t *pixels=emu_core_video();for(unsigned i=0;i<240*160;++i){uint16_t c=pixels[i];uint8_t rgb[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,1,3,out);}fclose(out);}
    printf("{\"bytes\":%zu,\"rom_crc\":\"%08x\",\"frames\":%u,\"draw_interval\":%u,\"elapsed_us\":%llu,\"fingerprint\":\"%08x\",\"audio_signal\":%u,\"read_calls\":%llu,\"read_bytes\":%llu,\"cache_bytes\":%u,\"state_bytes\":%zu,\"replay_equal\":true}\n",size,crc,frames,interval,(unsigned long long)elapsed,fingerprint,signal,(unsigned long long)read_calls,(unsigned long long)read_bytes,emu_core_rom_cache()->count*emu_core_rom_cache()->page_size,state_size);
    free(state);emu_core_close();fclose(f);return 0;
}
