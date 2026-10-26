/* GPL-2.0-or-later. Deterministic gameplay input and complete-state traces.
 * Usage: ROM frames draw_interval input.csv output_prefix
 * CSV rows are frame,held_core_pad_mask (decimal); final state/PPM and trace
 * belong in local artifacts. Host time is never a target-device FPS result. */
#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* Freeze the cartridge RTC epoch for byte-exact cross-process traces. */
time_t time(time_t *out) { time_t value=1700000000; if(out)*out=value; return value; }
extern uint64_t gba_cpu_fusion_hits[6];
static int read_file(void *ctx,uint64_t offset,void *out,size_t size) {
    FILE *f=ctx;return fseeko(f,(off_t)offset,SEEK_SET)||fread(out,1,size,f)!=size?-1:0;
}
static FILE *output(const char *prefix,const char *ext) {
    char path[4096];assert(snprintf(path,sizeof(path),"%s.%s",prefix,ext)<(int)sizeof(path));
    FILE *f=fopen(path,"wb");assert(f);return f;
}
int main(int argc,char **argv) {
    assert(argc==6 || argc==7);FILE *rom=fopen(argv[1],"rb"),*input=fopen(argv[4],"r");assert(rom&&input);
    fseeko(rom,0,SEEK_END);size_t bytes=ftello(rom);rewind(rom);
    uint8_t block[32768];size_t n;uint32_t crc=0;
    while((n=fread(block,1,sizeof(block),rom)))crc=emu_port_crc32(crc,block,n);
    assert(!ferror(rom));rom_source_t source={rom,read_file,bytes,crc};assert(!emu_core_load_source(&source,EMU_ROM_CACHE_BUDGET));
    if(argc==7) {
        FILE *battery=fopen(argv[6],"rb");size_t n=emu_core_save_size();uint8_t *data=malloc(n);assert(battery&&data);
        assert(fread(data,1,n,battery)==n && fgetc(battery)==EOF);assert(emu_core_save_import(data,n));free(data);fclose(battery);
    }
    size_t state_size=emu_state_size();uint8_t *state=malloc(state_size);assert(state);
    unsigned frames=strtoul(argv[2],NULL,10),interval=strtoul(argv[3],NULL,10),held=0,at=0,mask=0;
    assert(frames&&interval);int event=fscanf(input,"%u,%u",&at,&mask);uint32_t audio=0;
    for(unsigned i=0;i<frames;++i) {
        while(event==2&&at==i) {held=mask;event=fscanf(input,"%u,%u",&at,&mask);assert(event!=2||at>i);}
        emu_core_frame_render(held,0,i%interval==0);assert(!emu_core_rom_cache()->error);
        size_t samples;const int16_t *pcm=emu_core_audio(&samples);audio=emu_port_crc32(audio,pcm,samples*4);
        if((i+1)%60==0) {
            assert(!emu_state_save(state,state_size));
            printf("{\"frame\":%u,\"state\":\"%08x\",\"video\":\"%08x\",\"audio\":\"%08x\"}\n",i+1,
                emu_port_crc32(0,state,state_size),emu_port_crc32(0,emu_core_video(),240*160*2),audio);audio=0;
        }
    }
    assert(!emu_state_save(state,state_size));FILE *out=output(argv[5],"state");assert(fwrite(state,1,state_size,out)==state_size);fclose(out);
    out=output(argv[5],"ppm");fprintf(out,"P6\n240 160\n255\n");const uint16_t *p=emu_core_video();
    for(unsigned i=0;i<240*160;++i){uint16_t c=p[i];uint8_t rgb[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,1,3,out);}fclose(out);
    fprintf(stderr,"fusion head=%llu tail=%llu copy=%llu direct=%llu counted_copy=%llu half_copy=%llu\n",(unsigned long long)gba_cpu_fusion_hits[0],(unsigned long long)gba_cpu_fusion_hits[1],(unsigned long long)gba_cpu_fusion_hits[2],(unsigned long long)gba_cpu_fusion_hits[3],(unsigned long long)gba_cpu_fusion_hits[4],(unsigned long long)gba_cpu_fusion_hits[5]);
    free(state);emu_core_close();fclose(rom);fclose(input);return 0;
}
