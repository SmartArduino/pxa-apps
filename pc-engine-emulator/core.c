/* PXA HuCard adapter. GPL-2.0-or-later. */
#include "core.h"
#include "vendor/pce-go/pce.h"
#include "vendor/pce-go/psg.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
static rom_cache_t cartridge_cache;
static rom_slice_t cartridge_slice;
const rom_cache_t *emu_core_rom_cache(void){return &cartridge_cache;}
bool pxa_pce_rom_paged(void){return cartridge_cache.source.read!=NULL;}
uint8_t *pxa_pce_rom_map(unsigned bank,unsigned owner){return rom_cache_get(&cartridge_cache,(uint64_t)bank*8192,owner);}
static uint8_t reverse_bits(uint8_t v){v=(uint8_t)((v>>4)|(v<<4));v=(uint8_t)(((v&0xcc)>>2)|((v&0x33)<<2));return (uint8_t)(((v&0xaa)>>1)|((v&0x55)<<1));}
static bool encoded;
void pxa_pce_rom_encoded(bool value){encoded=value;}
static int read_card(void *context,uint64_t offset,void *out,size_t n){
    if(rom_slice_read(context,offset,out,n))return -1;
    if(encoded){uint8_t *p=out;for(size_t i=0;i<n;++i)p[i]=reverse_bits(p[i]);}return 0;
}
static uint16_t video[XBUF_WIDTH*XBUF_HEIGHT+32];
static int16_t pcm[2048*2];
static bool loaded,enabled=true,drawing;
static size_t frames;
static uint64_t sample_phase;
static uint16_t pads[2];
void pxa_pce_log(int level,const char *format,...){
    if(level>2)return;char line[240];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);emu_core_log("%s",line);
}
uint32_t pxa_pce_crc(uint32_t seed,const void *data,size_t size){return emu_port_crc32(seed,data,size);}
uint16_t *osd_gfx_framebuffer(int width,int height){
    if(!drawing || width<1 || width>512 || height<1 || height>240)return NULL;
    emu_core_info.width=(unsigned)width;emu_core_info.height=(unsigned)height;emu_core_info.stride=XBUF_WIDTH;
    return video+16;
}
static uint8_t buttons(uint16_t p){
    return ((p&EMU_PAD_A)?JOY_A:0)|((p&EMU_PAD_B)?JOY_B:0)|
        ((p&EMU_PAD_START)?JOY_RUN:0)|((p&EMU_PAD_SELECT)?JOY_SELECT:0)|
        ((p&EMU_PAD_UP)?JOY_UP:0)|((p&EMU_PAD_DOWN)?JOY_DOWN:0)|
        ((p&EMU_PAD_LEFT)?JOY_LEFT:0)|((p&EMU_PAD_RIGHT)?JOY_RIGHT:0);
}
void osd_input_read(uint8_t joypads[8]){memset(joypads,0,8);joypads[0]=buttons(pads[0]);joypads[1]=buttons(pads[1]);}
void osd_vsync(void){}
const char *emu_core_validate(const uint8_t *rom,size_t size){
    if(!rom || size<0x2000 || size>EMU_ROM_LIMIT)return "Invalid PC Engine HuCard size";
    if(size%0x2000 && size%0x2000!=512)return "Truncated HuCard bank";
    return NULL;
}
void emu_core_close(void){
    if(loaded){PCE.ROM=NULL;ShutdownPCE();}
    rom_cache_close(&cartridge_cache);
    memset(&PCE,0,sizeof(PCE));loaded=false;frames=0;sample_phase=0;memset(video,0,sizeof(video));
}
static const char *load_cartridge(uint8_t *rom,size_t size,const rom_source_t *source,size_t budget){
    const char *error=emu_core_validate(rom,size);if(error)return error;
    emu_core_close();emu_core_defaults();uint32_t crc=source?source->checksum:emu_port_crc32(0,rom,size);
    if(source){
        unsigned skip=size%8192;uint8_t last;
        cartridge_slice=(rom_slice_t){*source,skip};encoded=false;
        if(!rom_source_read(source,skip+8191,&last,1))return "Cannot read HuCard header";
        encoded=last<0xe0;
        rom_source_t logical={&cartridge_slice,read_card,size-skip,crc};
        if(!rom_cache_init(&cartridge_cache,&logical,8192,budget,8))return "Cannot allocate HuCard cache";
    }
    if(InitPCE(32000,true)){emu_core_close();return "PC Engine initialization failed";}
    loaded=true;
    if(source?LoadCardPaged(size,crc):LoadCard(rom,size)){emu_core_close();return "HuCard mapping failed";}
    emu_core_info.width=256;emu_core_info.height=224;emu_core_info.stride=XBUF_WIDTH;
    /* CPU CSH/CSL changes do not change the video frame clock. */
    emu_core_info.rate_num=60;emu_core_info.rate_den=1;
    emu_core_info.audio_rate=32000;emu_core_info.checksum=crc;
    snprintf(emu_core_info.title,sizeof(emu_core_info.title),"PC Engine HuCard");return NULL;
}
const char *emu_core_load(uint8_t *rom,size_t size){return load_cartridge(rom,size,NULL,0);}
const char *emu_core_load_source(const rom_source_t *source,size_t budget){
    uint8_t header[512];
    if(!source || !rom_source_read(source,0,header,sizeof(header)))return "Cannot read HuCard header";
    return load_cartridge(header,(size_t)source->size,source,budget);
}

void emu_core_set_audio_enabled(bool value){enabled=value;}
void emu_core_frame_render(uint16_t p1,uint16_t p2,bool draw){
    if(!loaded)return;pads[0]=p1;pads[1]=p2;drawing=draw;
    osd_input_read(PCE.Joypad.regs);pce_run();
    sample_phase+=(uint64_t)emu_core_audio_rate()*EMU_FRAME_CYCLES;
    frames=sample_phase/EMU_CLOCK_HZ;sample_phase%=EMU_CLOCK_HZ;
    if(frames>2048)frames=2048;
    psg_update(pcm,frames,0x3f);
}
const uint16_t *emu_core_video(void){return video+16;}
const int16_t *emu_core_audio(size_t *n){if(n)*n=loaded&&enabled?frames:0;return pcm;}
size_t emu_core_save_size(void){return loaded&&PCE.ExRAM?0x8000:0;}
bool emu_core_save_export(uint8_t *data,size_t size){if(!data || !emu_core_save_size() || size<0x8000)return false;memcpy(data,PCE.ExRAM,0x8000);return true;}
bool emu_core_save_import(const uint8_t *data,size_t size){if(!data || !emu_core_save_size() || size!=0x8000)return false;memcpy(PCE.ExRAM,data,size);return true;}
