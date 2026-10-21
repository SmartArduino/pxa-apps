/* Library smoke/differential probe. Native timings are not device FPS. */
#include "core.h"
#include "nes/nes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void nes_port_log(const char *format, ...) { (void)format; }
static void capture(const char *prefix, unsigned frame) {
    if (!prefix) return;
    char path[4096];
    if (snprintf(path,sizeof(path),"%s-%u.ppm",prefix,frame)>=(int)sizeof(path)) exit(2);
    FILE *f=fopen(path,"wb"); if(!f) exit(2);
    fprintf(f,"P6\n256 240\n255\n");
    const uint8_t *v=nes_core_video();
    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<256;++x) {
        unsigned c=nes_core_color(v[y*NES_SCREEN_PITCH+x]);
        unsigned r=(c>>11)&31,g=(c>>5)&63,b=c&31;
        uint8_t rgb[]={(r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)};
        if(fwrite(rgb,1,3,f)!=3) exit(2);
    }
    fclose(f);
}
int main(int argc,char **argv) {
    if(argc<2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    fseek(f,0,SEEK_END);long length=ftell(f);rewind(f);if(length<0)return 2;
    size_t size=(size_t)length;uint8_t *rom=malloc(size);
    if(!rom||fread(rom,1,size,f)!=size)return 2;fclose(f);
    const char *error=nes_core_load(rom,size);
    if(error){fprintf(stderr,"%s\n",error);free(rom);return 3;}
    unsigned frames=argc>2?(unsigned)strtoul(argv[2],0,10):1800;
    unsigned interval=argc>3?(unsigned)strtoul(argv[3],0,10):3;
    if(!interval||!frames)return 2;
    uint64_t draw_us=0,skip_us=0,energy=0,samples=0;
    unsigned drawn=0,skipped=0,jammed=0,fast=0,changes=0;uint32_t signature=0,previous=0;
    for(unsigned i=0;i<frames;++i){
        uint8_t pad=(i==600||i==601)?8:0;
        if(i>=900)pad=0x80|((i%63)<12?1:0);
        bool draw=i%interval==0;
        fast+=nes_getptr()->cpu_idle_next||nes6502_idle_hint();
        uint64_t start=nes_port_time();nes_core_frame(draw,pad,0);
        uint64_t us=nes_port_time()-start;
        if(draw){draw_us+=us;++drawn;}else{skip_us+=us;++skipped;}
        nes6502_t c;nes6502_getcontext(&c);jammed+=c.jammed;
        uint32_t state[]={c.pc_reg,c.a_reg,c.x_reg,c.y_reg,c.s_reg,c.p_reg,(uint32_t)c.total_cycles};
        signature=nes_port_crc32(signature,state,sizeof(state));
        signature=nes_port_crc32(signature,nes_getptr()->mem->ram,2048);
        rom_t *cart=nes_getptr()->cart;ppu_t *p=nes_getptr()->ppu;
        signature=nes_port_crc32(signature,cart->prg_ram,cart->prg_ram_banks*8192u);
        signature=nes_port_crc32(signature,p->nametab,4096);
        signature=nes_port_crc32(signature,p->oam,sizeof(p->oam));
        signature=nes_port_crc32(signature,p->palette,sizeof(p->palette));
        if(draw){uint32_t v=nes_port_crc32(0,nes_core_video()-8,NES_SCREEN_PITCH*240);
            changes+=v!=previous;previous=v;signature=nes_port_crc32(signature,&v,sizeof(v));}
        size_t count;const int16_t *pcm=nes_core_audio(&count);samples+=count;
        signature=nes_port_crc32(signature,pcm,count*sizeof(*pcm));
        for(size_t j=0;j<count;++j)energy+=pcm[j]<0?-(int)pcm[j]:pcm[j];
        if(argc>4 && (i==300||i==frames-1))capture(argv[4],i);
    }
    printf("{\"frames\":%u,\"hz\":%u,\"mapper\":%u,\"drawn\":%u,\"skipped\":%u,\"draw_us\":%llu,\"skip_us\":%llu,\"fast_frames\":%u,\"jammed_frames\":%u,\"video_changes\":%u,\"audio_samples\":%llu,\"audio_energy\":%llu,\"signature\":\"%08x\"}\n",
        frames,nes_core_rate(),nes_core_mapper(),drawn,skipped,(unsigned long long)draw_us,(unsigned long long)skip_us,fast,jammed,changes,(unsigned long long)samples,(unsigned long long)energy,signature);
    nes_core_close();free(rom);return 0;
}
