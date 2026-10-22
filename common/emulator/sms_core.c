/* Memory-backed SMS Plus adapter. GPL-2.0-or-later. */
#include "core.h"
#include "engines/smsplus/shared.h"
#include <stdarg.h>
static uint8_t indices[256*256];
static uint16_t rgb[256*240];
static uint16_t palette[256];
static int16_t pcm[2048*2];
static bool loaded,enabled=true,indexed_output,rgb_dirty;
uint32_t pxa_sms_crc(uint32_t,const void*,size_t);
static uint8_t *small_rom;
static rom_cache_t cartridge_cache;
static rom_slice_t cartridge_slice;
const rom_cache_t *emu_core_rom_cache(void){return &cartridge_cache;}
uint8_t *pxa_sms_rom_map(uint8_t *base,unsigned offset,unsigned owner){
    return cartridge_cache.source.read && base==cart.rom?rom_cache_get(&cartridge_cache,offset,owner):base+offset;
}
uint32_t pxa_sms_cart_crc(const void *data,size_t size){
    return cartridge_cache.source.read?cartridge_cache.source.checksum:pxa_sms_crc(0,data,size);
}
void pxa_sms_log(int level,const char *format,...){
    if(level>2)return;
    char line[240];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);emu_core_log("%s",line);
}
uint32_t pxa_sms_crc(uint32_t seed,const void *data,size_t size){return emu_port_crc32(seed,data,size);}
const char *emu_core_validate(const uint8_t *data,size_t size){
    if(!data || size<0x2000 || size>EMU_ROM_LIMIT)return "Invalid cartridge size";
    size_t effective=size-(size%0x4000==512?512:0);
    if(effective%0x4000 && effective!=0x2000)return "Truncated cartridge bank";
    return NULL;
}
void emu_core_close(void){
    if(loaded)system_shutdown();
    free(cart.sram);free(small_rom);small_rom=NULL;
    rom_cache_close(&cartridge_cache);
    memset(&smsplus,0,sizeof(smsplus));memset(&snd,0,sizeof(snd));loaded=false;
    memset(rgb,0,sizeof(rgb));rgb_dirty=false;
}
static const char *load_cartridge(uint8_t *data,size_t size,const rom_source_t *source,size_t budget){
    const char *error=emu_core_validate(data,size);if(error)return error;
    emu_core_close();emu_core_defaults();
    uint32_t checksum=source?source->checksum:emu_port_crc32(0,data,size);
    if(source){
        unsigned skip=size%0x4000==512?512:0;
        cartridge_slice=(rom_slice_t){*source,skip};
        rom_source_t logical={&cartridge_slice,rom_slice_read,size-skip,checksum};
        if(skip && !rom_source_crc(source,skip,size-skip,&logical.checksum))return "Cannot read cartridge checksum";
        if(!rom_cache_init(&cartridge_cache,&logical,4096,budget,64))return "Cannot allocate cartridge cache";
    }
    if(source){
        small_rom=malloc(32768);
        if(!small_rom){emu_core_close();return "Cannot allocate cartridge header";}
        size_t n=cartridge_cache.source.size<32768?cartridge_cache.source.size:32768;
        if(!rom_source_read(&cartridge_cache.source,0,small_rom,n)){emu_core_close();return "Cannot read cartridge header";}
        data=small_rom;size-=cartridge_slice.offset;
    }else if(size%0x4000==512){data+=512;size-=512;}
    if(size==0x2000){if(!source)small_rom=malloc(0x4000);if(!small_rom)return "Not enough cartridge memory";memcpy(small_rom,data,size);memcpy(small_rom+size,data,size);data=small_rom;size=0x4000;}
    void *reserve=malloc(1024u*1024u);
    if(!reserve){emu_core_close();return "Not enough emulator memory";}free(reserve);
    system_reset_config();option.console=EMU_SMS_GEAR?3:2;option.sndrate=32000;option.overscan=0;option.extra_gg=0;option.use_bios=0;
    if(!load_rom(data,(int)size,(int)size)){emu_core_close();return "Failed to map cartridge";}
    bitmap.width=256;bitmap.height=256;bitmap.pitch=256;bitmap.data=indices;
    loaded=true;system_poweron();
    emu_core_info.width=bitmap.viewport.w;emu_core_info.height=bitmap.viewport.h;
    emu_core_info.stride=bitmap.viewport.w;emu_core_info.aspect_num=EMU_SMS_GEAR?10:4;emu_core_info.aspect_den=EMU_SMS_GEAR?9:3;
    emu_core_info.rate_num=sms.display==DISPLAY_NTSC?CLOCK_NTSC:CLOCK_PAL;
    emu_core_info.rate_den=CYCLES_PER_LINE*(sms.display==DISPLAY_NTSC?262:313);
    emu_core_info.audio_rate=32000;emu_core_info.checksum=checksum;emu_core_info.model=EMU_SMS_GEAR;
    snprintf(emu_core_info.title,sizeof(emu_core_info.title),"%s cartridge",EMU_SYSTEM_TAG);
    return NULL;
}
const char *emu_core_load(uint8_t *data,size_t size){return load_cartridge(data,size,NULL,0);}
const char *emu_core_load_source(const rom_source_t *source,size_t budget){
    uint8_t header[512];
    if(!source || !rom_source_read(source,0,header,sizeof(header)))return "Cannot read cartridge header";
    return load_cartridge(header,(size_t)source->size,source,budget);
}

void emu_core_set_audio_enabled(bool value){enabled=value;}
static uint8_t buttons(uint16_t p){
    return ((p&EMU_PAD_UP)?INPUT_UP:0)|((p&EMU_PAD_DOWN)?INPUT_DOWN:0)|
        ((p&EMU_PAD_LEFT)?INPUT_LEFT:0)|((p&EMU_PAD_RIGHT)?INPUT_RIGHT:0)|
        ((p&EMU_PAD_A)?INPUT_BUTTON2:0)|((p&EMU_PAD_B)?INPUT_BUTTON1:0);
}
void emu_core_frame_render(uint16_t p1,uint16_t p2,bool draw){
    if(!loaded)return;
    input.pad[0]=buttons(p1);input.pad[1]=buttons(p2);
    input.system=(p1&EMU_PAD_START)?(EMU_SMS_GEAR?INPUT_START:INPUT_PAUSE):0;
    system_frame(!draw);
    unsigned w=bitmap.viewport.w,h=bitmap.viewport.h;
    if(!w || !h || w>256 || h>240 || bitmap.viewport.x<0 || bitmap.viewport.x+(int)w>256 || bitmap.viewport.y<0 || bitmap.viewport.y+(int)h>256)return;
    emu_core_info.width=w;emu_core_info.height=h;emu_core_info.stride=w;
    if(draw){
        render_copy_palette(palette);
        rgb_dirty=true;
        if(!indexed_output)emu_core_video();
    }
    if(enabled)for(int i=0;i<snd.sample_count && i<2048;++i){
        /* Integer gain avoids float PCM work; preserve stereo Game Gear. */
        int l=(int)snd.stream[0][i]*11/4,r=(int)snd.stream[1][i]*11/4;
        pcm[i*2]=(int16_t)(l>32767?32767:l< -32768?-32768:l);
        pcm[i*2+1]=(int16_t)(r>32767?32767:r< -32768?-32768:r);
    }
}
void emu_core_set_indexed_output(bool enabled){indexed_output=enabled;}
const uint8_t *emu_core_indexed_video(unsigned *stride,const uint16_t **colors){
    if(!loaded)return NULL;if(stride)*stride=256;if(colors)*colors=palette;
    return indices+bitmap.viewport.y*256+bitmap.viewport.x;
}
const uint16_t *emu_core_video(void){
    if(rgb_dirty){
        unsigned w=EMU_VIDEO_WIDTH,h=EMU_VIDEO_HEIGHT;
        for(unsigned y=0;y<h;++y){const uint8_t *src=indices+(y+bitmap.viewport.y)*256+bitmap.viewport.x;
            for(unsigned x=0;x<w;++x)rgb[y*w+x]=palette[src[x]];}
        rgb_dirty=false;
    }
    return rgb;
}
const int16_t *emu_core_audio(size_t *frames){if(frames)*frames=loaded&&enabled?(unsigned)snd.sample_count:0;return pcm;}
size_t emu_core_save_size(void){return loaded?0x8000:0;}
bool emu_core_save_export(uint8_t *data,size_t size){if(!loaded || !data || size<0x8000)return false;memcpy(data,cart.sram,0x8000);return true;}
bool emu_core_save_import(const uint8_t *data,size_t size){if(!loaded || !data || size!=0x8000)return false;memcpy(cart.sram,data,size);return true;}

#include "sms_state.inc"
