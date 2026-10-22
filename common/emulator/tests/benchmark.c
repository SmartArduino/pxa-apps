/* Native core throughput is not device display FPS. GPL-2.0-or-later. */
#include "core.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
int main(int argc,char **argv){
 if(argc<2)return 2;FILE *f=fopen(argv[1],"rb");if(!f)return 2;fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);uint8_t *rom=calloc(1,EMU_ROM_CAPACITY(size));if(!rom || fread(rom,1,size,f)!=size)return 2;fclose(f);
 const char *error=emu_core_load(rom,size);if(error){fprintf(stderr,"%s\n",error);return 1;}
 unsigned frames=argc>2?atoi(argv[2]):600,interval=argc>3?atoi(argv[3]):1;if(!frames || !interval)return 2;
 uint64_t total=now(),draw_us=0,skip_us=0,samples=0;unsigned drawn=0;uint32_t audio=0;
 for(unsigned i=0;i<frames;++i){bool draw=i%interval==0 || i+1==frames;uint64_t t=now();emu_core_frame_render(i>100?EMU_PAD_RIGHT|EMU_PAD_A:0,0,draw);t=now()-t;if(draw){draw_us+=t;++drawn;}else skip_us+=t;size_t n;const int16_t *pcm=emu_core_audio(&n);samples+=n;audio=emu_port_crc32(audio,pcm,n*4);}
 uint32_t screen=0;const uint16_t *pixels=emu_core_video();
 for(unsigned y=0;y<EMU_VIDEO_HEIGHT;++y)screen=emu_port_crc32(screen,pixels+y*emu_core_info.stride,EMU_VIDEO_WIDTH*2u);
 printf("{\"system\":\"%s\",\"frames\":%u,\"drawn\":%u,\"elapsed_us\":%llu,\"draw_avg_us\":%llu,\"skip_avg_us\":%llu,\"samples\":%llu,\"pcm_crc\":\"%08x\",\"video_crc\":\"%08x\",\"width\":%u,\"height\":%u}\n",EMU_SYSTEM_TAG,frames,drawn,(unsigned long long)(now()-total),(unsigned long long)(drawn?draw_us/drawn:0),(unsigned long long)(frames>drawn?skip_us/(frames-drawn):0),(unsigned long long)samples,audio,screen,EMU_VIDEO_WIDTH,EMU_VIDEO_HEIGHT);
 emu_core_close();free(rom);return 0;
}
