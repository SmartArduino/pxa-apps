/* PXA gpSP adapter. GPL-2.0-or-later. */
#include "core.h"
#include "common.h"
#include "streams/file_stream.h"
#include <limits.h>
#include "pxa_gpsp_profile.h"

#ifdef PXA_GBA_PROFILE
uint32_t gba_profile_instructions[2][16];
#endif
#ifdef PXA_EMU_PROFILE_PHASES
static uint64_t profile_core_us;
static unsigned profile_frames, profile_draws, profile_total;
static void profile_reset(void) {
#ifdef PXA_GBA_PROFILE
    memset(gba_profile_instructions,0,sizeof(gba_profile_instructions));
#endif
    emu_profile_reset();profile_core_us=profile_frames=profile_draws=profile_total=0;
}
static void profile_frame(uint64_t started,bool render) {
    profile_core_us+=emu_profile_now()-started;
    profile_draws+=render;
    ++profile_total;
    if(++profile_frames<60)return;
    emu_core_log("GBA PROFILE total=%u frames=%u drawn=%u core_us=%llu raster_us=%llu apu_us=%llu output_us=%llu",
        profile_total,profile_frames,profile_draws,(unsigned long long)profile_core_us,
        (unsigned long long)emu_profile_phases[EMU_PROFILE_RASTER].us,
        (unsigned long long)emu_profile_phases[EMU_PROFILE_APU].us,
        (unsigned long long)emu_profile_phases[EMU_PROFILE_OUTPUT].us);
#ifdef PXA_GBA_PROFILE
    for(unsigned i=0;i<16;++i)if(gba_profile_instructions[0][i]||gba_profile_instructions[1][i])
        emu_core_log("GBA CPU region=%u arm=%u thumb=%u",i,gba_profile_instructions[0][i],gba_profile_instructions[1][i]);
    memset(gba_profile_instructions,0,sizeof(gba_profile_instructions));
#endif
    emu_profile_reset();profile_core_us=profile_frames=profile_draws=0;
}
#endif

u32 skip_next_frame, netplay_num_clients, netplay_client_id;
u32 idle_loop_target_pc, translation_gate_targets;
u32 translation_gate_target_pc[MAX_TRANSLATION_GATES];
int dynarec_enable, sprite_limit = 1;
boot_mode selected_boot_mode = boot_game;
static uint16_t pixels[240*161];
static int16_t pcm[1100*2];
static rom_cache_t cartridge;
static RFILE stream;
static unsigned sample_remainder, pcm_frames;
static uint16_t buttons;
static bool loaded, audio_enabled = true;

#ifdef PXA_GBA_BENCHMARK
/* Compare identical emulated frame ranges, independently of the frontend's
 * adaptive drawing cadence. Checksumming/logging is outside measured time.
 * Never use this mode to report release LCD FPS: it forces drawing and input. */
#include <time.h>
#ifdef PXA_GBA_BENCHMARK_INPUT
#include "gba_bench_input.h"
#endif
static unsigned bench_frames;
static uint64_t bench_us;
static uint32_t bench_pcm;
static uint64_t bench_now(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;
}
static void bench_frame(uint64_t start) {
    bench_us+=bench_now()-start;
    bench_pcm=emu_port_crc32(bench_pcm,pcm,pcm_frames*4);
    if(++bench_frames%60)return;
    emu_core_log("GBA BENCH rom=%08x end=%u frames=60 core_us=%llu pcm=%08x video=%08x cpu=%08x",
        emu_core_checksum(),bench_frames,(unsigned long long)bench_us,bench_pcm,
        emu_port_crc32(0,pixels,240*160*2),emu_port_crc32(0,reg,REG_ARCH_COUNT*sizeof(*reg)));
    bench_us=0;bench_pcm=0;
}
#endif

/* Link and wireless accessories are intentionally disconnected. */
void netpacket_send(uint16_t client,const void *data,size_t size) {(void)client;(void)data;(void)size;}
void netpacket_poll_receive(void) {}
void set_fastforward_override(bool enabled) {(void)enabled;}

RFILE *filestream_open(const char *path,unsigned mode,unsigned hints) {
    (void)hints;
    if (!path || strcmp(path,"cartridge.gba") || mode!=RETRO_VFS_FILE_ACCESS_READ) return NULL;
    stream.offset=0; return &stream;
}
int filestream_close(RFILE *file) {(void)file;return 0;}
int64_t filestream_get_size(RFILE *file) {return file ? (int64_t)cartridge.source.size : -1;}
int64_t filestream_seek(RFILE *file,int64_t offset,int whence) {
    if (!file) return -1;
    int64_t base=whence==SEEK_SET ? 0 : whence==SEEK_CUR ? (int64_t)file->offset :
        whence==SEEK_END ? (int64_t)cartridge.source.size : -1;
    if (base<0 || offset < -base || offset>INT64_MAX-base) return -1;
    file->offset=(uint64_t)(base+offset);return (int64_t)file->offset;
}
int64_t filestream_read(RFILE *file,void *out,int64_t bytes) {
    if (!file || !out || bytes<0) return -1;
    if (file->offset>=cartridge.source.size) return 0;
    uint64_t left=cartridge.source.size-file->offset;
    if ((uint64_t)bytes>left) bytes=(int64_t)left;
    if (!rom_source_read(&cartridge.source,file->offset,out,(size_t)bytes)) {
        cartridge.error=1;return -1;
    }
    file->offset+=(uint64_t)bytes;cartridge.bytes+=(uint64_t)bytes;++cartridge.misses;
    return bytes;
}

static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id) {
    (void)device;(void)index;
    if (port || id!=RETRO_DEVICE_ID_JOYPAD_MASK) return 0;
    static const unsigned masks[]={EMU_PAD_B,EMU_PAD_Y,EMU_PAD_SELECT,EMU_PAD_START,
        EMU_PAD_UP,EMU_PAD_DOWN,EMU_PAD_LEFT,EMU_PAD_RIGHT,EMU_PAD_A,EMU_PAD_X,EMU_PAD_L,EMU_PAD_R};
    uint16_t out=0;
    for(unsigned i=0;i<12;++i) if(buttons&masks[i]) out|=1u<<i;
    return (int16_t)out;
}
const char *emu_core_validate(const uint8_t *rom,size_t size) {
    if (!rom || size<0xc0 || size>EMU_ROM_LIMIT) return "GBA ROM must be 192 bytes to 32 MiB";
    return NULL;
}
void emu_core_close(void) {
    memory_term();loaded=false;pcm_frames=sample_remainder=0;buttons=0;
    gba_screen_pixels=NULL;memset(&cartridge,0,sizeof(cartridge));
}
const char *emu_core_load_source(const rom_source_t *source,size_t budget) {
    if (!source || !source->read || source->size<0xc0 || source->size>EMU_ROM_LIMIT)
        return "Invalid GBA cartridge source";
    if (budget<1024u*1024u) return "gpSP needs a 1 MiB cartridge cache";
    rom_source_t copy=*source;emu_core_close();cartridge.source=copy;
    cartridge.page_size=32768;
    cartridge.pages=(unsigned)((copy.size+32767)/32768);
    size_t blocks=(copy.size+1048575u)/1048576u;
    if (blocks>budget/1048576u) blocks=budget/1048576u;
    init_gamepak_buffer_with_limit((unsigned)blocks);
    cartridge.count=gamepak_buffer_count*32;
    if (!gamepak_buffer_count) {emu_core_close();return "Cannot allocate GBA ROM cache";}
    memset(pixels,0,sizeof(pixels));gba_screen_pixels=pixels;
    memcpy(bios_rom,open_gba_bios_rom,sizeof(bios_rom));
    sound_frequency=32768;sound_freq_bits=15;init_sound();
    memset(gamepak_backup,0xff,sizeof(gamepak_backup));
    struct retro_game_info info={.path="cartridge.gba"};
    if (load_gamepak(&info,info.path,FEAT_AUTODETECT,FEAT_DISABLE,SERIAL_MODE_DISABLED) || cartridge.error) {
        emu_core_close();return "Cannot read GBA cartridge";
    }
    emu_core_info=(emu_core_info_t){.width=240,.height=160,.stride=240,
        .aspect_num=3,.aspect_den=2,.rate_num=16777216,.rate_den=280896,
        .audio_rate=32768,.checksum=copy.checksum};
    uint8_t title[12];
    if (!rom_source_read(&copy,0xa0,title,sizeof(title))) {emu_core_close();return "Cannot read GBA header";}
    for(unsigned i=0;i<12;++i) emu_core_info.title[i]=title[i]>=32 && title[i]<127 ? title[i] : ' ';
    emu_core_info.title[12]=0;
    libretro_supports_bitmasks=true;retro_set_input_state(input);
    buttons=0;skip_next_frame=0;update_input();reset_gba();
#ifdef PXA_GBA_BENCHMARK
    bench_frames=bench_us=bench_pcm=0;
#endif
#ifdef PXA_EMU_PROFILE_PHASES
    profile_reset();
#endif
    loaded=true;return NULL;
}
static int read_memory_rom(void *context,uint64_t offset,void *out,size_t size) {
    memcpy(out,(uint8_t*)context+(size_t)offset,size);return 0;
}
const char *emu_core_load(uint8_t *rom,size_t size) {
    const char *error=emu_core_validate(rom,size);if(error)return error;
    rom_source_t source={rom,read_memory_rom,size,emu_port_crc32(0,rom,size)};
    return emu_core_load_source(&source,EMU_ROM_CACHE_BUDGET);
}
const rom_cache_t *emu_core_rom_cache(void) {return &cartridge;}
void emu_core_set_audio_enabled(bool enabled) {audio_enabled=enabled;}
void emu_core_frame_render(uint16_t p1,uint16_t p2,bool render) {
    (void)p2;pcm_frames=0;if(!loaded || cartridge.error)return;
#ifdef PXA_GBA_BENCHMARK
    p1=0;render=true;
#ifdef PXA_GBA_BENCHMARK_INPUT
    p1=gba_bench_input(bench_frames);
#endif
    uint64_t bench_start=bench_now();
#endif
#ifdef PXA_EMU_PROFILE_PHASES
    uint64_t started=emu_profile_now();
#endif
    buttons=p1;skip_next_frame=!render;update_input();clear_gamepak_stickybits();
    execute_arm(execute_cycles);
    sample_remainder+=280896;
    unsigned wanted=sample_remainder/512;sample_remainder%=512;
    pcm_frames=sound_read_samples(pcm,wanted);
#ifdef PXA_GBA_BENCHMARK
    bench_frame(bench_start);
#endif
#ifdef PXA_EMU_PROFILE_PHASES
    profile_frame(started,render);
#endif
}
const uint16_t *emu_core_video(void) {return pixels;}
const int16_t *emu_core_audio(size_t *frames) {if(frames)*frames=loaded&&audio_enabled?pcm_frames:0;return pcm;}
size_t emu_core_save_size(void) {return loaded?sizeof(gamepak_backup):0;}
bool emu_core_save_export(uint8_t *out,size_t size) {
    if(!loaded||!out||size!=sizeof(gamepak_backup))return false;
    memcpy(out,gamepak_backup,size);return true;
}
bool emu_core_save_import(const uint8_t *data,size_t size) {
    if(!loaded||!data||size!=sizeof(gamepak_backup))return false;
    memcpy(gamepak_backup,data,size);return true;
}

enum { STATE_HEADER=32, MIX_BYTES=BUFFER_SIZE*sizeof(s32),
    MIX_OFFSET=GBA_STATE_MEM_SIZE+128*1024,
    STATE_BODY=MIX_OFFSET+MIX_BYTES+4 };
static uint32_t get32(const uint8_t *p) {return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
/* Validate bounds before passing a state to gpSP's BSON readers. */
static bool bson_valid(const uint8_t *p,size_t n,unsigned depth,bool array) {
    if(depth>16||n<5||get32(p)!=n||p[n-1])return false;
    size_t i=4;
    while(i<n-1) {
        unsigned type=p[i++];size_t start=i;
        while(i<n-1&&p[i])++i;
        if(i==n-1)return false;
        size_t key=i-start;++i;
        if(array&&(type!=0x10||key!=2))return false;
        if(type==0x10) {if(n-1-i<4)return false;i+=4;}
        else if(type==3||type==4) {
            if(n-1-i<4)return false;size_t len=get32(p+i);
            if(len>n-1-i||!bson_valid(p+i,len,depth+1,type==4))return false;i+=len;
        } else if(type==5) {
            if(n-1-i<5)return false;size_t len=get32(p+i);
            if(len>n-1-i-5)return false;i+=len+5;
        } else return false;
    }
    return i==n-1;
}
size_t emu_state_size(void) {return loaded?STATE_HEADER+STATE_BODY:0;}
const char *emu_state_save(void *data,size_t size) {
    if(!loaded||!data||size!=emu_state_size())return "Invalid GBA snapshot buffer";
    uint8_t *p=data;memset(p,0,size);memcpy(p,"PXAGBA1",7);
    put32(p+8,emu_core_checksum());put32(p+12,STATE_BODY);
    gba_save_state(p+STATE_HEADER);
    memcpy(p+STATE_HEADER+GBA_STATE_MEM_SIZE,gamepak_backup,sizeof(gamepak_backup));
    sound_save_mix(p+STATE_HEADER+MIX_OFFSET);
    put32(p+size-4,sample_remainder);
    put32(p+16,emu_port_crc32(0,p+STATE_HEADER,STATE_BODY));return NULL;
}
const char *emu_state_load(const void *data,size_t size) {
    const uint8_t *p=data;
    if(!loaded||!p||size!=emu_state_size()||memcmp(p,"PXAGBA1",8)||
       get32(p+8)!=emu_core_checksum()||get32(p+12)!=STATE_BODY||
       get32(p+16)!=emu_port_crc32(0,p+STATE_HEADER,STATE_BODY)||get32(p+size-4)>=512||
       !bson_valid(p+STATE_HEADER,GBA_STATE_MEM_SIZE,0,false))return "Invalid or mismatched GBA snapshot";
    uint8_t *previous=calloc(1,GBA_STATE_MEM_SIZE+MIX_BYTES);if(!previous)return "Cannot allocate snapshot rollback";
    gba_save_state(previous);
    sound_save_mix(previous+GBA_STATE_MEM_SIZE);
    bool ok=gba_load_state(p+STATE_HEADER);
    if(!ok) {gba_load_state(previous);sound_load_mix(previous+GBA_STATE_MEM_SIZE);}
    free(previous);if(!ok)return "Unsupported GBA snapshot";
    sound_load_mix(p+STATE_HEADER+MIX_OFFSET);
    memcpy(gamepak_backup,p+STATE_HEADER+GBA_STATE_MEM_SIZE,sizeof(gamepak_backup));
    sample_remainder=get32(p+size-4);pcm_frames=0;
#ifdef PXA_GBA_BENCHMARK
    /* Start a reproducible frame sequence after loading a known snapshot. */
    bench_frames=bench_us=bench_pcm=0;
#endif
#ifdef PXA_EMU_PROFILE_PHASES
    profile_reset();
#endif
    return NULL;
}
