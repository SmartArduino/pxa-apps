/* PXA Mega Drive cartridge adapter. PicoDrive license: vendor/picodrive/COPYING. */
#include "core.h"
#include "rom_pager.h"
#include "md_profile.h"
#include "vendor/picodrive/pico/pico_int.h"
#include <string.h>
#include <stdio.h>
static uint16_t video[320*256];
static int16_t pcm[2][2048*2];
static unsigned pcm_completed;
static unsigned first_line;
static size_t audio_frames;
static bool loaded,enabled=true;
void emu_video_mode_change(int start_line,int lines,int start_col,int columns){
    (void)start_col;
    if(start_line<0 || lines<1 || lines>240 || columns<1 || columns>320 || start_line+lines>256)return;
    first_line=(unsigned)start_line;emu_core_info.width=columns;emu_core_info.height=lines;emu_core_info.stride=columns;
    memset(video,0,sizeof(video));PicoDrawSetOutBuf(video,columns*2);
}
void emu_32x_startup(void){}
static void sound_written(int bytes){
    audio_frames=bytes>0?(size_t)bytes/4:0;if(audio_frames>2048)audio_frames=2048;
    /* PicoDrive clears sndOut immediately after this callback. Hand it the
     * next buffer so the frontend can borrow the completed PCM without a copy. */
    pcm_completed=PicoIn.sndOut==pcm[1];
    PicoIn.sndOut=pcm[pcm_completed^1u];
}
const char *emu_core_validate(const uint8_t *rom,size_t size){
    if(!rom || size<0x200 || size>EMU_ROM_LIMIT || (size&3))return "Invalid Mega Drive cartridge size";
    if(size%0x4000==512 && size>=0x4200)return NULL;
    if(memcmp(rom+0x100,"SEGA",4))return "Not a Mega Drive cartridge";
    if(size>=0x108 && !memcmp(rom+0x100,"SEGA 32X",8))return "32X cartridges are not supported";
    return NULL;
}
void emu_core_close(void){
    if(loaded)PicoExit();md_rom_close();loaded=false;audio_frames=0;first_line=0;memset(video,0,sizeof(video));
}
static const char *load_cartridge(uint8_t *rom,size_t size,const rom_source_t *source,size_t budget){
    const char *error=emu_core_validate(rom,size);if(error)return error;
    emu_core_close();emu_core_defaults();uint32_t crc=source?source->checksum:emu_port_crc32(0,rom,size);size_t capacity=EMU_ROM_CAPACITY(size);
    if(!source && size%0x4000==512){
        size-=512;memmove(rom,rom+512,size);uint8_t temp[0x4000];
        for(size_t at=0;at<size;at+=0x4000){memcpy(temp,rom+at,0x4000);for(unsigned j=0;j<0x2000;++j){rom[at+j*2]=temp[j+0x2000];rom[at+j*2+1]=temp[j];}}
    }
    if(source){
        if(!md_rom_open(source,budget))return "Cannot allocate Mega Drive cartridge cache";
        if(size%0x4000==512)size-=512;
        uint8_t normal_header[512];memcpy(normal_header,md_rom_pointer(0,UINT32_MAX),512);
        for(unsigned i=0;i<512;i+=2){uint8_t v=normal_header[i];normal_header[i]=normal_header[i+1];normal_header[i+1]=v;}
        error=emu_core_validate(normal_header,size);
    }else error=emu_core_validate(rom,size);
    if(error){emu_core_close();return error;}
    memset(&PicoIn,0,sizeof(PicoIn));
    PicoIn.opt=POPT_EN_FM|POPT_EN_PSG|POPT_EN_Z80|POPT_EN_STEREO|POPT_DIS_32C_BORDER;
    memset(pcm,0,sizeof(pcm));pcm_completed=0;
    PicoIn.sndRate=32000;PicoIn.sndOut=pcm[0];PicoIn.writeSound=sound_written;
    PicoIn.autoRgnOrder=0x184;
    PicoInit();loaded=true;
    if(source)rom=(uint8_t*)(uintptr_t)0x10000000;else Byteswap(rom,rom,(int)size);
    PicoCartSetBorrowed((unsigned)capacity);
    if(PicoCartInsert(rom,(unsigned)size,NULL)){emu_core_close();return "Mega Drive mapping failed";}
    PicoSetInputDevice(0,PICO_INPUT_PAD_6BTN);PicoSetInputDevice(1,PICO_INPUT_PAD_6BTN);
    PicoDrawSetOutFormat(PDF_RGB555,0);PicoDrawSetOutBuf(video,320*2);PicoLoopPrepare();
    emu_core_info.width=320;emu_core_info.height=224;emu_core_info.stride=320;
    emu_core_info.rate_num=Pico.m.pal?50:60;emu_core_info.rate_den=1;
    emu_core_info.audio_rate=32000;emu_core_info.model=Pico.m.pal;emu_core_info.checksum=crc;
    for(unsigned i=0;i<48;++i){unsigned c=source?*md_rom_pointer((0x150+i)^1,UINT32_MAX):rom[(0x150+i)^1];emu_core_info.title[i]=c>=32 && c<127?(char)c:' ';}emu_core_info.title[48]=0;
    md_profile_reset();return NULL;
}
const char *emu_core_load(uint8_t *rom,size_t size){return load_cartridge(rom,size,NULL,0);}
const char *emu_core_load_source(const rom_source_t *source,size_t budget){
    uint8_t header[512];if(!source || !rom_source_read(source,0,header,sizeof(header)))return "Cannot read Mega Drive header";
    return load_cartridge(header,(size_t)source->size,source,budget);
}

void emu_core_set_audio_enabled(bool value){enabled=value;}
static uint16_t buttons(uint16_t p){
    /* PicoDrive: MXYZ SACB RLDU. Common Y is the third MD face button C. */
    return ((p&EMU_PAD_UP)?1:0)|((p&EMU_PAD_DOWN)?2:0)|((p&EMU_PAD_LEFT)?4:0)|((p&EMU_PAD_RIGHT)?8:0)|
        ((p&EMU_PAD_B)?0x10:0)|((p&EMU_PAD_Y)?0x20:0)|((p&EMU_PAD_A)?0x40:0)|((p&EMU_PAD_START)?0x80:0)|
        ((p&EMU_PAD_R)?0x100:0)|((p&EMU_PAD_L)?0x200:0)|((p&EMU_PAD_X)?0x400:0)|((p&EMU_PAD_SELECT)?0x800:0);
}
void emu_core_frame_render(uint16_t p1,uint16_t p2,bool draw){
    if(!loaded)return;audio_frames=0;PicoIn.pad[0]=buttons(p1);PicoIn.pad[1]=buttons(p2);PicoIn.skipFrame=!draw;PicoFrame();md_profile_frame();
}
const uint16_t *emu_core_video(void){return video+(size_t)first_line*emu_core_info.stride;}
const int16_t *emu_core_audio(size_t *frames){if(frames)*frames=loaded&&enabled?audio_frames:0;return pcm[pcm_completed];}
size_t emu_core_save_size(void){return loaded&&Pico.sv.data?Pico.sv.size:0;}
bool emu_core_save_export(uint8_t *data,size_t size){size_t n=emu_core_save_size();if(!n || !data || size<n)return false;memcpy(data,Pico.sv.data,n);return true;}
bool emu_core_save_import(const uint8_t *data,size_t size){if(!data || !size || size!=emu_core_save_size())return false;memcpy(Pico.sv.data,data,size);return true;}

#include "state_snapshot.inc"
