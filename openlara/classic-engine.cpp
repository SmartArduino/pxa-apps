// PXA adapter for the complete BSD-2-Clause OpenLara gameplay engine.
#include "engine.h"
#include "storage.h"
#include "music-stream.h"
#include "levels.h"
#include "vendor/classic/game.h"
#include <stdio.h>
#include <string.h>
#include <new>

uint8_t *ol_level_data;
alignas(16) uint8_t ol_tracks_data[OL_TRACKS_CAPACITY];
uint8_t ol_title_data[320*240];
unsigned ol_title_width=240,ol_title_height=160;
uint8_t ol_title_palette_data[512];
uint32_t ol_tracks_size;
uint16_t ol_palette[256];
static uint16_t fallback_pixels[320*240];
static uint16_t *pixels=fallback_pixels;
static unsigned input_bytes;
static uint64_t current_us;
static bool initialized,swap_actions,show_fps;
static unsigned gamma_value,current_id;
static uint8_t video_mode;
static unsigned video_width() {return video_mode==1?240u:video_mode==2?160u:320u;}
static void log_video() {
    char s[80];snprintf(s,sizeof(s),"OpenLara video gameplay=%ux%u ui=320x240",video_width(),video_width()*3/4);
    pxa_log_write(2,s);
}
static int fps_value,requested=-1;
static Stream *pending_stream;
static uint8_t *pending_write[2];
static unsigned pending_size[2];
static bool restore_valid=true;
static uint32_t levels_mask=(1u<<OL_LEVEL_COUNT)-1;
static bool validate_save_file(const uint8_t *data,unsigned bytes);
static void reset_audio();

bool olClassicBorrow(const void *pointer,size_t bytes) {
    uintptr_t p=reinterpret_cast<uintptr_t>(pointer),b=reinterpret_cast<uintptr_t>(ol_level_data);
    return ol_level_data && p>=b && p-b<input_bytes && bytes<=input_bytes-(p-b);
}
void *operator new(size_t n) {
    void *p=malloc(n?n:1);
    if(!p) {pxa_log_write(4,"OpenLara classic: allocation failed");__builtin_trap();}
    return p;
}
void *operator new[](size_t n) {return ::operator new(n);}
void operator delete(void *p) noexcept {if(!olClassicBorrow(p,0))free(p);}
void operator delete[](void *p) noexcept {::operator delete(p);}
void operator delete(void *p,size_t) noexcept {::operator delete(p);}
void operator delete[](void *p,size_t) noexcept {::operator delete(p);}
void olClassicBadData() {pxa_log_write(4,"OpenLara classic: invalid PHD or save range");__builtin_trap();}

void *osMutexInit() {return nullptr;}
void osMutexFree(void*) {}
void osMutexLock(void*) {}
void osMutexUnlock(void*) {}
int osGetTimeMS() {return int(current_us/1000);}
bool osJoyReady(int index) {return index==0;}
void osJoyVibrate(int,float,float) {}

static int file_id(const char *name) {
    if(!name)return -1;
    const char *base=strrchr(name,'/');base=base?base+1:name;
    char normalized[32];unsigned n=0;
    while(base[n] && n<sizeof(normalized)-1) {
        char c=base[n];normalized[n]=(c>='A' && c<='Z')?c+32:c;++n;
    }
    normalized[n]=0;
    if(n<5 || strcmp(normalized+n-4,".phd"))return -1;
    strcpy(normalized+n-4,".pkd");
    for(unsigned i=0;i<OL_LEVEL_COUNT;++i)if(!strcmp(normalized,OL_LEVELS[i].file))return i;
    return -1;
}
bool ol_level_available(unsigned id) {return id<OL_LEVEL_COUNT && (levels_mask&(1u<<id));}
void ol_set_levels_mask(uint32_t mask) {levels_mask=mask&((1u<<OL_LEVEL_COUNT)-1);}
int ol_first_level() {
    for(unsigned i=0;i<OL_LEVEL_COUNT;++i)if(OL_LEVELS[i].kind==2 && ol_level_available(i))return i;
    return -1;
}
int olClassicFirstLevel() {int id=ol_first_level();return id<0?int(TR::LVL_TR1_TITLE):id+int(TR::LVL_TR1_TITLE);}
bool olClassicExists(const char *name) {int id=file_id(name);return id>=0 && ol_level_available(id);}
void olClassicOpen(Stream *stream) {
    int id=file_id(stream->name);
    if(id>=0) {
        if(pending_stream)delete pending_stream;
        pending_stream=stream;requested=id;return;
    }
    if(stream->callback) {stream->callback(nullptr,stream->userData);delete stream;}
}
const char *ol_level_file(unsigned id) {
    static char name[32];
    if(id>=OL_LEVEL_COUNT)return "";
    strcpy(name,OL_LEVELS[id].file);strcpy(name+strlen(name)-4,".phd");return name;
}

struct PersistedSettings {
    uint32_t magic,version;
    Core::Settings settings;
    uint8_t swap,show_fps,gamma,reserved;
};
static bool write_private(unsigned kind,const void *data,unsigned n) {
    if(!store_busy())return store_write(kind,data,n);
    auto *copy=static_cast<uint8_t*>(malloc(n));if(!copy)return false;
    memcpy(copy,data,n);free(pending_write[kind]);pending_write[kind]=copy;pending_size[kind]=n;return true;
}
static bool save_settings() {
    PersistedSettings p{};p.magic=0x33434c4f;p.version=1;p.settings=Core::settings;
    p.swap=swap_actions;p.show_fps=show_fps;p.gamma=gamma_value;p.reserved=video_mode;
    return write_private(0,&p,sizeof(p));
}
void osCacheRead(Stream *stream) {
    unsigned n=0;const uint8_t *p=store_view(0,&n);PersistedSettings settings;
    bool ok=p && n==sizeof(settings);
    if(ok) {
        memcpy(&settings,p,n);ok=settings.magic==0x33434c4f && settings.version==1 &&
            settings.settings.version==SETTINGS_VERSION && settings.gamma<32 && settings.swap<=1 && settings.show_fps<=1 && settings.reserved<3;
    }
    if(ok) {swap_actions=settings.swap;show_fps=settings.show_fps;gamma_value=settings.gamma;video_mode=settings.reserved;}
    if(stream->callback)stream->callback(ok?new Stream(stream->name,&settings.settings,sizeof(settings.settings)):nullptr,stream->userData);
    delete stream;
}
void osCacheWrite(Stream *stream) {
    bool ok=stream->size==sizeof(Core::Settings) && save_settings();
    if(stream->callback)stream->callback(ok?new Stream(stream->name,nullptr,0):nullptr,stream->userData);
    delete stream;
}
void osReadSlot(Stream *stream) {
    unsigned n=0;const uint8_t *p=store_view(1,&n);
    if(p && !validate_save_file(p,n)) {pxa_log_write(3,"OpenLara: rejected invalid classic save records");p=nullptr;}
    if(stream->callback)stream->callback(p?new Stream(stream->name,p,n):nullptr,stream->userData);
    delete stream;
}
void osWriteSlot(Stream *stream) {
    bool ok=stream->size>0 && stream->size<=128*1024 && write_private(1,stream->data,stream->size);
    if(stream->callback)stream->callback(ok?new Stream(stream->name,stream->data,stream->size):nullptr,stream->userData);
    if(!ok)delete[]stream->data;
    delete stream;
}
void ol_set_time(uint64_t us) {
    current_us=us;
    if(!store_busy())for(unsigned i=0;i<2;++i)if(pending_write[i]) {
        if(store_write(i,pending_write[i],pending_size[i])) {free(pending_write[i]);pending_write[i]=nullptr;}
        break;
    }
}
void ol_set_fps(int value) {fps_value=value;}

static void controls() {
    auto &c=Core::settings.controls[0];
    c.joyIndex=0;c.keys[cAction].joy=swap_actions?jkB:jkA;c.keys[cJump].joy=swap_actions?jkA:jkB;
}
static void brightness() {
    if(!Game::level)return;
    GAPI::initPalette(Game::level->level.palette,Game::level->level.lightmap);
    if(!gamma_value)return;
    for(unsigned i=0;i<256;++i) {
        auto brighten=[](uint16_t c) {
            unsigned r=c>>11,g=(c>>5)&63,b=c&31;
            r+=(31-r)*gamma_value/64;g+=(63-g)*gamma_value/64;b+=(31-b)*gamma_value/64;
            return uint16_t((r<<11)|(g<<5)|b);
        };
        GAPI::swPaletteColor[i]=brighten(GAPI::swPaletteColor[i]);
        GAPI::swPaletteWater[i]=brighten(GAPI::swPaletteWater[i]);
    }
#if OL_SHADED_PALETTE_CACHE
    GAPI::refreshShadedPalettes();
#endif
}
bool ol_allocate_level(unsigned n) {
    if(!n || n>OL_LEVEL_CAPACITY)return false;
    if(initialized && Game::level) {delete Game::level;Game::level=nullptr;}
    music_stream_stop();reset_audio();free(ol_level_data);ol_level_data=nullptr;input_bytes=0;
    ol_level_data=static_cast<uint8_t*>(malloc(n));if(!ol_level_data)return false;
    input_bytes=n;return true;
}
const char *ol_validate_level(unsigned n) {
    return n==input_bytes && n>=10 && ol_level_data && ol_level_data[0]==0x20 &&
        ol_level_data[1]==0 && ol_level_data[2]==0 && ol_level_data[3]==0?nullptr:"Expected original PC TR1 PHD data";
}
void ol_start(int id) {
    current_id=id;
    Stream *stream=pending_stream;pending_stream=nullptr;requested=-1;
    if(!stream)stream=new Stream(ol_level_file(id),ol_level_data,input_bytes);
    else {stream->data=reinterpret_cast<char*>(ol_level_data);stream->size=input_bytes;stream->pos=0;}
    bool restoring=initialized && loadSlot>=0;restore_valid=true;
    if(!initialized) {
        Core::width=320;Core::height=240;Core::defLang=0;inventory=new Inventory();
        Game::init(stream);initialized=true;
    } else Game::startLevel(stream);
    brightness();GAPI::resize();GAPI::swColor=pixels;Core::resetTime();
    log_video();
    if(restoring)pxa_log_write(restore_valid?2:3,restore_valid?"OpenLara save restored":"OpenLara invalid save; level restarted");
}
void ol_update(unsigned,uint32_t keys) {
    if(!initialized || !Game::level)return;
    const JoyKey map[]={jkUp,jkRight,jkDown,jkLeft,jkA,jkY,jkNone,jkB,jkX,jkNone,jkLB,jkRB,jkNone,jkNone,jkNone,jkStart};
    for(unsigned i=0;i<16;++i)if(map[i]!=jkNone)Input::setJoyDown(0,map[i],(keys&(1u<<i))!=0);
    // Game::update already caps catch-up at 200 ms. Resetting its clock on
    // every slow frame prevented any simulation/input update below 4 FPS.
    Game::update();
}
void ol_idle_time() {if(initialized)Core::resetTime();}
void olClassicTitleBackground(unsigned alpha) {
    // Render-target clearing is deferred until state validation. Finish it
    // before writing the direct RGB565 background, as normal draw calls do.
    Core::validateRenderState();
    uint16_t colors[256];
    for(unsigned i=0;i<256;++i) {
        uint16_t c=pxa_load_u16(ol_title_palette_data+2*i);
        unsigned r=((c>>11)*alpha+127)/255,g=(((c>>5)&63)*alpha+127)/255,b=((c&31)*alpha+127)/255;
        colors[i]=uint16_t((r<<11)|(g<<5)|b);
    }
    unsigned source_x[320];
    for(unsigned x=0;x<320;++x)source_x[x]=x*ol_title_width/320;
    for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x) {
        pixels[y*320+x]=colors[ol_title_data[(y*ol_title_height/240)*ol_title_width+source_x[x]]];
    }
}
static void text_begin() {UI::begin(4.0f/3.0f);Game::level->atlasGlyphs->bind(sDiffuse);}
static void text(unsigned x,unsigned y,const char *s,bool centered=true) {
    UI::textOut(vec2(x*2,y*2),s,centered?UI::aCenter:UI::aLeft,centered?640:0,255,UI::SHADE_ORANGE);
}
void ol_render() {
    if(!initialized || !Game::level)return;
    // Rendering alone uses the smaller target. Restore native dimensions
    // before the next simulation tick and before any platform overlay.
    unsigned width=ol_playable_level(current_id) && !inventory->isActive()?video_width():320;
    Core::width=width;Core::height=width*3/4;
    Game::render();
    if(width!=320) {
        ol_classic_upscale(pixels,width);
        Core::width=320;Core::height=240;
        Core::setViewport(Core::x,Core::y,320,240);
        Core::setScissor(Core::viewport);
    }
    if(show_fps) {char s[24];snprintf(s,sizeof(s),"%d FPS",fps_value);text_begin();text(8,8,s,false);UI::end();}
}
static uint8_t *overlay_lightmap;
static decltype(GAPI::swPalette) overlay_palette;
void ol_overlay_begin() {
    overlay_lightmap=GAPI::swLightmap;overlay_palette=GAPI::swPalette;
    Game::frameBegin();
    // Core::reset does not mark the scissor dirty. Restore the full target
    // explicitly instead of inheriting the last 3D portal's clipping box.
    Core::setTarget(nullptr,nullptr,RT_STORE_COLOR);
    Core::setViewport(Core::x,Core::y,Core::width,Core::height);
    Core::setScissor(Core::viewport);Core::validateRenderState();
    GAPI::setPalette(GAPI::swPaletteColor);GAPI::setShading(false);
}
void ol_overlay_end() {
    Game::frameEnd();GAPI::swLightmap=overlay_lightmap;GAPI::setPalette(overlay_palette);
}
const uint8_t *ol_pixels() {return reinterpret_cast<const uint8_t*>(pixels);}
void ol_set_framebuffer(uint16_t *output) {
    pixels=output?output:fallback_pixels;
    GAPI::swColor=pixels;
}
int ol_next_level() {int id=requested;requested=-1;return id;}
void ol_player_position(int32_t out[4]) {
    auto *lara=initialized && Game::level?Game::level->players[0]:nullptr;
    if(!lara) {memset(out,0,16);return;}
    out[0]=int32_t(lara->pos.x);out[1]=int32_t(lara->pos.y);out[2]=int32_t(lara->pos.z);
    out[3]=int16_t(lara->angle.y*(65536.0f/PI2));
}
bool ol_quick_save() {
    if(!initialized || !ol_playable_level(current_id) || store_busy())return false;
    Game::quickSave();return saveResult!=SAVE_RESULT_ERROR;
}
bool ol_quick_load() {
    if(!initialized || store_busy())return false;
    int index=-1;
    for(int i=0;i<saveSlots.length;++i)if(saveSlots[i].isCheckpoint())index=i;
    if(index<0)return false;
    int id=int(saveSlots[index].getLevelID())-int(TR::LVL_TR1_TITLE);
    if(!ol_playable_level(id))return false;
    loadSlot=index;requested=id;return true;
}
void ol_show_title() {loadSlot=-1;requested=0;}
void ol_draw_message(const char *message) {if(initialized && Game::level && message && *message) {text_begin();text(0,216,message);UI::end();}}
void ol_draw_menu(unsigned selected,bool help,bool settings) {
    memset(pixels,0,320u*240u*sizeof(*pixels));text_begin();
    if(help) {
        const char *lines[]={"CONTROLS","D-pad: move / turn","A: action  B: jump","X: weapon  Y: roll","L: look  R: walk","Start: inventory  Select: menu","PC: arrows  Z/X  A/S  Q/W","PC: Enter/Tab = Start/Select","Bottom centre: menu"};
        for(unsigned i=0;i<9;++i)text(0,20+i*22,lines[i]);
    } else if(settings) {
        const char *labels[]={"Sound effects","Music","Swap action/jump","Show FPS","Brightness","3D resolution","Back"};
        unsigned values[]={Core::settings.audio.sound!=0,Core::settings.audio.music!=0,swap_actions,show_fps,gamma_value};
        text(0,16,"SETTINGS");
        for(unsigned i=0;i<7;++i) {char s[64];if(i<4)snprintf(s,sizeof(s),"%s: %s",labels[i],values[i]?"ON":"OFF");else if(i==4)snprintf(s,sizeof(s),"Brightness: %u",gamma_value);else if(i==5)snprintf(s,sizeof(s),"3D resolution: %ux%u",video_width(),video_width()*3/4);else strcpy(s,labels[i]);text(0,40+i*25,s);if(i==selected)text(12,40+i*25,">",false);}
    } else {
        const char *lines[]={"Resume","Save game","Load game","New game","City of Vilcabamba","Lara's home","Title","Settings","Controls","All levels","Import game data"};
        text(0,16,"OPENLARA - TR1");for(unsigned i=0;i<11;++i) {text(0,35+i*18,lines[i]);if(i==selected)text(12,35+i*18,">",false);}
    }
    UI::end();
}
void ol_draw_levels(unsigned selected) {
    memset(pixels,0,320u*240u*sizeof(*pixels));text_begin();unsigned page=selected/8;
    text(0,16,page?"LEVELS 2/2":"LEVELS 1/2");
    for(unsigned row=0;row<8;++row) {
        unsigned i=page*8+row;if(i>=OL_PLAYABLE_COUNT)break;unsigned id=OL_PLAYABLE_IDS[i];
        char s[80];snprintf(s,sizeof(s),"%s%s",OL_LEVELS[id].name,ol_level_available(id)?"":" (missing)");
        text(0,38+row*20,s);if(i==selected)text(12,38+row*20,">",false);
    }
    text(0,218,"Left/right: page  B: back");UI::end();
}
void ol_draw_data(unsigned installed,bool sd) {
    memset(pixels,0,320u*240u*sizeof(*pixels));text_begin();
    char count[48];snprintf(count,sizeof(count),"Installed campaign levels: %u / 15",installed);
    const char *lines[]={"ORIGINAL GAME DATA",count,"Copy your original PC DATA folder", "into OpenLara's SD media folder.",
        "Keep the original *.PHD filenames.","PC importer also accepts GAME.GOG.",sd?"A: Scan DATA folder and reload":"SD media unavailable; use PC importer","B: Back"};
    for(unsigned i=0;i<8;++i)text(0,22+i*26,lines[i]);UI::end();
}
bool ol_toggle_setting(unsigned index) {
    if(store_busy())return false;
    switch(index) {
        case 0:Core::settings.audio.sound=Core::settings.audio.sound?0:SND_MAX_VOLUME;break;
        case 1:
            Core::settings.audio.music=Core::settings.audio.music?0:SND_MAX_VOLUME;
            olClassicTrack(Core::settings.audio.music?Game::level->level.state.flags.track:255,false);break;
        case 2:swap_actions=!swap_actions;controls();break;
        case 3:show_fps=!show_fps;break;
        case 4:gamma_value=(gamma_value+4)&31;brightness();break;
        case 5:video_mode=(video_mode+1)%3;log_video();break;
        default:return false;
    }
    return save_settings();
}
void ol_free() {
    if(initialized)Game::deinit();initialized=false;
    free(ol_level_data);ol_level_data=nullptr;input_bytes=0;
    if(pending_stream)delete pending_stream;pending_stream=nullptr;
    for(auto &p:pending_write) {free(p);p=nullptr;}
}

// Music uses the same bounded AD4 media FIFO as the fixed renderer. SFX use
// the complete engine's original mixer, resampled from 44.1 kHz to Host mono.
static uint32_t music_remaining;
static int music_id=-1,z1,z2,tap,quant=2048;
static bool music_pcm,music_loop,music_complete=true;
static int8_t music_samples[352];
static unsigned music_pos=352,music_phase;
static int music_prev,music_next;
static bool music_primed;
static Sound::Frame sfx_samples[882];
static unsigned sfx_pos=882,sfx_phase;
static int sfx_prev,sfx_next;
static bool sfx_primed;
static void reset_audio() {
    music_remaining=0;music_pos=352;music_phase=0;music_prev=music_next=0;music_primed=false;
    music_complete=true;music_loop=false;memset(music_samples,0,sizeof(music_samples));
    sfx_pos=882;sfx_phase=0;sfx_prev=sfx_next=0;sfx_primed=false;memset(sfx_samples,0,sizeof(sfx_samples));
}
void olClassicTrack(int track,bool) {
    if(track==255 || track<0 || track>=64 || !Core::settings.audio.music) {
        music_remaining=0;music_complete=true;music_loop=false;music_stream_stop();return;
    }
    music_loop=track==TR::LEVEL_INFO[TR::LVL_TR1_TITLE+current_id].track;
    if(track==2 && !pxa_load_u32(ol_tracks_data+track*8+4))track=4;
    unsigned offset=pxa_load_u32(ol_tracks_data+track*8),n=pxa_load_u32(ol_tracks_data+track*8+4);
    music_pcm=(n>>31)!=0;n&=0x7fffffffu;music_complete=!n;
    music_id=track;music_remaining=n;music_pos=352;music_phase=0;music_prev=music_next=0;music_primed=false;
    z1=z2=tap=0;quant=2048;
    if(n)music_stream_begin(offset,n);else music_stream_stop();
}
static int music_source() {
    if(music_pos==352) {
        if(!music_remaining && !music_complete) {
            music_complete=true;
            int ambient=TR::LEVEL_INFO[TR::LVL_TR1_TITLE+current_id].track;
            if(ambient!=TR::NO_TRACK && (music_loop || music_id!=ambient))olClassicTrack(ambient,false);
        }
        uint8_t encoded[352];unsigned n=music_remaining,limit=music_pcm?352:176;if(n>limit)n=limit;
        if(!n || music_stream_failed() || music_stream_read(encoded,n)!=n) return 0;
        static const int adapt[]={192,192,136,136,128,128,128,128,112,128,128,128,128,136,136,192};
        unsigned p=0;
        if(music_pcm) {memcpy(music_samples,encoded,n);p=n;}
        else for(unsigned i=0;i<n;++i)for(unsigned shift=0;shift<8;shift+=4) {
            tap=z2+tap-(tap>>3);music_samples[p++]=int8_t(clamp(tap>>8,-128,127));
            int code=((encoded[i]>>shift)&15)^8;code-=8;
            int out=code*quant+(z1-z2);z2=z1;z1=out;quant=(quant*adapt[code+8]+127)>>7;
        }
        memset(music_samples+p,0,352-p);music_remaining-=n;music_pos=0;
    }
    return int(music_samples[music_pos++])*256;
}
static int sfx_source() {
    if(sfx_pos==882) {Sound::fill(sfx_samples,882);sfx_pos=0;}
    auto f=sfx_samples[sfx_pos++];return (int(f.L)+int(f.R))/2;
}
bool ol_audio_active() {
    if(!initialized)return false;
    if(Sound::channelsCount || (!music_complete && Core::settings.audio.music && !music_stream_failed()) || music_prev || music_next || sfx_prev || sfx_next)return true;
    for(unsigned i=music_pos;i<352;++i)if(music_samples[i])return true;
    for(unsigned i=sfx_pos;i<882;++i)if(sfx_samples[i].L || sfx_samples[i].R)return true;
    return false;
}
void ol_audio(int16_t *out,unsigned count,unsigned rate) {
    if(!sfx_primed) {sfx_prev=sfx_source();sfx_next=sfx_source();sfx_primed=true;}
    if(!music_primed) {music_prev=music_source();music_next=music_source();music_primed=true;}
    for(unsigned i=0;i<count;++i) {
        int sfx=sfx_prev+(sfx_next-sfx_prev)*int(sfx_phase)/int(rate);
        int song=music_prev+(music_next-music_prev)*int(music_phase)/int(rate);
        out[i]=int16_t(clamp(sfx+song*Core::settings.audio.music/SND_MAX_VOLUME,-32768,32767));
        sfx_phase+=44100;while(sfx_phase>=rate) {sfx_phase-=rate;sfx_prev=sfx_next;sfx_next=sfx_source();}
        music_phase+=10512;while(music_phase>=rate) {music_phase-=rate;music_prev=music_next;music_next=music_source();}
    }
}

bool olClassicValidateSlot(const void *blob,size_t bytes,const void *level_ptr) {
    const auto *data=static_cast<const uint8_t*>(blob);
    const auto *level=static_cast<const TR::Level*>(level_ptr);
    auto reject=[]() {restore_valid=false;pxa_log_write(3,"OpenLara: invalid classic save slot");return false;};
    if(!data || bytes<sizeof(SaveStats)+4)return reject();
    uint32_t flags=pxa_load_u32(data),id=flags&0x7fffffffu;
    int catalog_id=int(id)-int(TR::LVL_TR1_TITLE);
    if(!ol_playable_level(catalog_id))return reject();
    size_t pos=sizeof(SaveStats);unsigned count=pxa_load_u32(data+pos);pos+=4;
    if(count>256 || count>(bytes-pos)/sizeof(SaveItem))return reject();
    pos+=count*sizeof(SaveItem);
    if(!(flags>>31))return pos==bytes;
    if(bytes-pos<sizeof(SaveState)+4)return reject();
    pos+=sizeof(SaveState);count=pxa_load_u32(data+pos);pos+=4;
    if(!count || count>1024 || (level && count>unsigned(level->entitiesCount)))return reject();
    constexpr unsigned base=sizeof(SaveEntity)-sizeof(SaveEntity::Extra);
    for(unsigned i=0;i<count;++i) {
        if(bytes-pos<base)return reject();
        SaveEntity e{};memcpy(&e,data+pos,base);
        if(e.extraSize>sizeof(e.extra) || e.extraSize>bytes-pos-base)return reject();
        if(level) {
            unsigned room=e.room,expected=0;
            if(i<unsigned(level->entitiesBaseCount)) {
                auto *controller=static_cast<Controller*>(level->entities[i].controller);
                SaveEntity original{};
                if(!controller || !controller->getSaveData(original) || e.type)return reject();
                expected=original.extraSize;room^=level->entities[i].room;
            } else {
                TR::Entity entity{};entity.type=TR::Entity::Type(e.type);
                if(!e.type || !level->getModelIndex(entity.type))return reject();
                expected=entity.isLara()?sizeof(e.extra.lara):(entity.isEnemy()?sizeof(e.extra.enemy):0);
            }
            if(room>=unsigned(level->roomsCount) || e.extraSize!=expected || e.animIndex>=level->animsCount)return reject();
        }
        pos+=base+e.extraSize;
    }
    return pos==bytes;
}
static bool validate_save_file(const uint8_t *data,unsigned bytes) {
    if(bytes<4 || pxa_load_u32(data)!=SAVE_MAGIC)return false;
    unsigned pos=4,count=0;
    while(pos<bytes) {
        if(bytes-pos<4 || ++count>64)return false;
        unsigned n=pxa_load_u32(data+pos);pos+=4;
        if(n>bytes-pos || !olClassicValidateSlot(data+pos,n,nullptr))return false;
        pos+=n;
    }
    return true;
}
