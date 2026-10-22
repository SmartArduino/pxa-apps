/* PXA memory-backed Snes9x adapter. Core license in vendor/snes9x/src/LICENSE. */
#include "core.h"
#include "rom_pager.h"
#include "vendor/snes9x/snes9x.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "vendor/snes9x/src/srtc.h"
int pxa_snes_printf(const char *format,...){char line[240];va_list a;va_start(a,format);vsnprintf(line,sizeof(line),format,a);va_end(a);emu_core_log("%s",line);return 0;}
static uint8_t *color_buffers,*depth_buffers;
static int16_t pcm[2048*2];
static size_t audio_frames,ram_bytes;
static uint64_t sample_phase;
static uint16_t pads[2];
static bool loaded,enabled=true,memory_ready,apu_ready,gfx_ready,sound_ready;
bool S9xInitDisplay(void){
    color_buffers=calloc(1,512u*512u*4u);depth_buffers=calloc(1,512u*512u*2u);
    if(!color_buffers || !depth_buffers)return false;
    GFX.Pitch=512*2;GFX.ZPitch=512;
    GFX.Screen=color_buffers;GFX.SubScreen=color_buffers+512*512*2;
    GFX.ZBuffer=depth_buffers;GFX.SubZBuffer=depth_buffers+512*512;
    return true;
}
void S9xDeinitDisplay(void){free(color_buffers);free(depth_buffers);color_buffers=depth_buffers=NULL;memset(&GFX,0,sizeof(GFX));}
uint32_t S9xReadJoypad(int32_t which){
    if(which<0 || which>1)return 0;uint16_t p=pads[which];
    return 0x80000000u|((p&EMU_PAD_B)?0x8000:0)|((p&EMU_PAD_Y)?0x4000:0)|
        ((p&EMU_PAD_SELECT)?0x2000:0)|((p&EMU_PAD_START)?0x1000:0)|
        ((p&EMU_PAD_UP)?0x800:0)|((p&EMU_PAD_DOWN)?0x400:0)|
        ((p&EMU_PAD_LEFT)?0x200:0)|((p&EMU_PAD_RIGHT)?0x100:0)|
        ((p&EMU_PAD_A)?0x80:0)|((p&EMU_PAD_X)?0x40:0)|
        ((p&EMU_PAD_L)?0x20:0)|((p&EMU_PAD_R)?0x10:0);
}
bool S9xReadMousePosition(int32_t which,int32_t *x,int32_t *y,uint32_t *buttons){(void)which;(void)x;(void)y;(void)buttons;return false;}
bool S9xReadSuperScopePosition(int32_t *x,int32_t *y,uint32_t *buttons){(void)x;(void)y;(void)buttons;return false;}
bool JustifierOffscreen(void){return true;}
void JustifierButtons(uint32_t *buttons){*buttons=0;}
const char *emu_core_validate(const uint8_t *rom,size_t size){
    if(!rom || size<0x8000 || size>EMU_ROM_LIMIT)return "Invalid SNES cartridge size";
    if(size%0x2000 && size%0x2000!=512)return "Truncated SNES cartridge bank";
    return NULL;
}
void emu_core_close(void){
    if(gfx_ready)S9xDeinitGFX();
    if(sound_ready)S9xDeinitSound();
    if(apu_ready)S9xDeinitAPU();
    if(memory_ready){Memory.ROM=NULL;Memory.ROM_Offset=0;S9xDeinitMemory();}
    S9xDeinitDisplay();loaded=memory_ready=apu_ready=gfx_ready=sound_ready=false;
    snes_rom_close();
    ram_bytes=audio_frames=0;sample_phase=0;memset(pads,0,sizeof(pads));
}
static const char *load_cartridge(uint8_t *rom,size_t size,const rom_source_t *source,size_t budget){
    const char *error=emu_core_validate(rom,size);if(error)return error;
    emu_core_close();emu_core_defaults();memset(&Settings,0,sizeof(Settings));
    if(source && !snes_rom_open(source,budget))return "Cannot allocate SNES cartridge cache";
    emu_core_info.checksum=source?source->checksum:emu_port_crc32(0,rom,size);
    Settings.APUEnabled=Settings.NextAPUEnabled=true;Settings.Shutdown=true;
    Settings.CyclesPercentage=100;Settings.H_Max=SNES_CYCLES_PER_SCANLINE;
    Settings.HBlankStart=(256*Settings.H_Max)/SNES_HCOUNTER_MAX;
    Settings.FrameTimeNTSC=16667;Settings.FrameTimePAL=20000;Settings.ControllerOption=SNES_JOYPAD;
    Settings.SoundPlaybackRate=Settings.SoundInputRate=32000;Settings.InterpolatedSound=true;
    Memory.ROM=source?(uint8_t*)(uintptr_t)0x10000000:rom;Memory.ROM_AllocSize=size+EMU_ROM_PADDING;Memory.ROM_Offset=0;
    /* The frontend provides the writable mapping guard; the core borrows it. */
    memory_ready=true;
    if(!S9xInitMemory()){Memory.ROM=NULL;memory_ready=false;return "SNES memory allocation failed";}
    if(!S9xInitDisplay()){emu_core_close();return "SNES graphics allocation failed";}
    apu_ready=S9xInitAPU();sound_ready=apu_ready && S9xInitSound(0,0);
    gfx_ready=sound_ready && S9xInitGFX();
    if(!gfx_ready){emu_core_close();return "SNES chip initialization failed";}
    if(!LoadROMMemory(size)){emu_core_close();return "SNES cartridge mapping failed";}
    if(Settings.SuperFX || Settings.SDD1 || Settings.SPC7110 || Settings.SETA ||
        (Memory.ROMType&0xf0)==0x30){emu_core_close();return "This SNES enhancement chip is not supported";}
    S9xSetPlaybackRate(32000);
    emu_core_info.width=256;emu_core_info.height=PPU.ScreenHeight;emu_core_info.stride=512;
    emu_core_info.rate_num=Settings.PAL?50:60;emu_core_info.rate_den=1;
    emu_core_info.audio_rate=32000;emu_core_info.model=Settings.PAL;
    snprintf(emu_core_info.title,sizeof(emu_core_info.title),"%s",Memory.ROMName);
    ram_bytes=Memory.SRAMSize && Memory.SRAMSize<16?((size_t)1024<<Memory.SRAMSize):0;
    if(ram_bytes>SRAM_SIZE)ram_bytes=SRAM_SIZE;
    loaded=true;return NULL;
}
const char *emu_core_load(uint8_t *rom,size_t size){return load_cartridge(rom,size,NULL,0);}
const char *emu_core_load_source(const rom_source_t *source,size_t budget){
    uint8_t header[512];if(!source || !rom_source_read(source,0,header,sizeof(header)))return "Cannot read SNES cartridge header";
    return load_cartridge(header,(size_t)source->size,source,budget);
}

void emu_core_set_audio_enabled(bool value){enabled=value;}
void emu_core_frame_render(uint16_t p1,uint16_t p2,bool draw){
    if(!loaded)return;pads[0]=p1;pads[1]=p2;IPPU.RenderThisFrame=draw;
    S9xMainLoop();
    if(draw){
        unsigned w=IPPU.RenderedScreenWidth,h=IPPU.RenderedScreenHeight;
        if(w && h && w<=512 && h<=512){emu_core_info.width=w;emu_core_info.height=h;emu_core_info.stride=512;}
    }
    sample_phase+=(uint64_t)emu_core_audio_rate()*EMU_FRAME_CYCLES;
    audio_frames=sample_phase/EMU_CLOCK_HZ;sample_phase%=EMU_CLOCK_HZ;
    if(audio_frames>2048)audio_frames=2048;
    /* Mixing also advances envelopes and echo; muting only discards output. */
    S9xMixSamples(pcm,(int32_t)audio_frames*2);
}
const uint16_t *emu_core_video(void){return (const uint16_t *)color_buffers;}
const int16_t *emu_core_audio(size_t *frames){if(frames)*frames=loaded&&enabled?audio_frames:0;return pcm;}
/* New development format: SRAM followed by portable S-RTC registers and timestamp. */
size_t emu_core_save_size(void){return loaded?ram_bytes+(Settings.SRTC?32:0):0;}
bool emu_core_save_export(uint8_t *data,size_t size){
    size_t n=emu_core_save_size();if(!n || !data || size<n)return false;
    memcpy(data,Memory.SRAM,ram_bytes);
    if(Settings.SRTC){
        S9xUpdateSrtcTime();uint8_t *r=data+ram_bytes;memset(r,0,32);
        memcpy(r,rtc.data,13);r[13]=rtc.needs_init;r[14]=rtc.count_enable;
        r[15]=(uint8_t)rtc.index;r[16]=rtc.mode;
        uint64_t t=(uint64_t)rtc.system_timestamp;
        for(unsigned i=0;i<8;++i)r[24+i]=(uint8_t)(t>>(8*i));
    }
    return true;
}
bool emu_core_save_import(const uint8_t *data,size_t size){
    size_t n=emu_core_save_size();if(!n || !data || size!=n)return false;
    if(Settings.SRTC){
        const uint8_t *r=data+ram_bytes;
        if(r[13]>1 || r[14]>1 || (int8_t)r[15]<-1 || (int8_t)r[15]>13 || r[16]>MODE_COMMAND_DONE)return false;
        for(unsigned i=0;i<13;++i)if(r[i]>15)return false;
        uint64_t t=0;for(unsigned i=0;i<8;++i)t|=(uint64_t)r[24+i]<<(8*i);
        if(t>INT64_MAX)return false;
        memcpy(rtc.data,r,13);rtc.needs_init=r[13];rtc.count_enable=r[14];
        rtc.index=(int8_t)r[15];rtc.mode=r[16];rtc.system_timestamp=(time_t)t;
        S9xUpdateSrtcTime();
    }
    memcpy(Memory.SRAM,data,ram_bytes);return true;
}

#include "state_snapshot.inc"
