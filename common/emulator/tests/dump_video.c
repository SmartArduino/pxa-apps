/* Native reference image output. GPL-2.0-or-later. */
#include "core.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
 if(argc<3)return 2;FILE *f=fopen(argv[1],"rb");if(!f)return 2;fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
 uint8_t *rom=calloc(1,EMU_ROM_CAPACITY(n));if(!rom || fread(rom,1,n,f)!=n)return 2;fclose(f);
 const char *error=emu_core_load(rom,n);if(error){fprintf(stderr,"%s\n",error);return 1;}
 unsigned frames=argc>3?atoi(argv[3]):180;
 for(unsigned i=0;i<frames;++i)emu_core_frame_render(0,0,true);
 f=fopen(argv[2],"wb");if(!f)return 2;fprintf(f,"P6\n%u %u\n255\n",EMU_VIDEO_WIDTH,EMU_VIDEO_HEIGHT);
 const uint16_t *p=emu_core_video();printf("center=%04x stride=%u\n",p[EMU_VIDEO_HEIGHT/2*emu_core_info.stride+EMU_VIDEO_WIDTH/2],emu_core_info.stride);
 for(unsigned y=0;y<EMU_VIDEO_HEIGHT;++y)for(unsigned x=0;x<EMU_VIDEO_WIDTH;++x){unsigned v=p[y*emu_core_info.stride+x];uint8_t rgb[]={((v>>11)*255+15)/31,(((v>>5)&63)*255+31)/63,((v&31)*255+15)/31};fwrite(rgb,1,3,f);}
 fclose(f);emu_core_close();free(rom);return 0;
}
