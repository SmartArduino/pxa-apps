#include "engine.h"
#include "storage.h"
bool osValidateSave();
#include "game.h"

alignas(16) uint8_t ol_level_data[OL_LEVEL_CAPACITY];
alignas(16) uint8_t ol_tracks_data[OL_TRACKS_CAPACITY];
uint8_t ol_title_data[240*160];
uint32_t ol_tracks_size;
const void* TRACKS_AD4 = ol_tracks_data;
uint16_t ol_palette[256];
int32 fps;
static uint64_t time_us;
static bool initialized, restore_pending;
static uint8_t save_blob[SAVEGAME_SIZE];
static uint8_t title_screen[FRAME_WIDTH * FRAME_HEIGHT];

int32 osGetSystemTimeMS() { return int32(time_us / 1000); }
void ol_set_time(uint64_t us) { time_us = us; }
void ol_set_fps(int value) { fps = value; }
bool osSaveSettings() { return store_write(0,&gSettings,sizeof(gSettings)); }
bool osLoadSettings() {
    Settings settings;unsigned n;
    if(!store_read(0,&settings,sizeof(settings),&n) || n!=sizeof(settings) || settings.version!=SETTINGS_VER)return false;
    gSettings=settings;return true;
}
static bool saved_header(SaveGame &sg) {
    unsigned n;
    if(!store_read(1,save_blob,sizeof(save_blob),&n) || n<sizeof(SaveGame))return false;
    memcpy(&sg,save_blob,sizeof(sg));
    return sg.version==SAVEGAME_VER && sg.level>=1 && sg.level<=3 &&
        sg.dataSize==n-sizeof(SaveGame) && sg.dataSize>0 && sg.track>=-1 && sg.track<64;
}
bool osCheckSave() {SaveGame sg;return saved_header(sg);}
bool osSaveGame() {
    if(gLevelID<LVL_TR1_GYM || int(gLevelID)>3 || gSaveGame.dataSize>sizeof(gSaveData))return false;
    memcpy(save_blob,&gSaveGame,sizeof(gSaveGame));
    memcpy(save_blob+sizeof(gSaveGame),gSaveData,gSaveGame.dataSize);
    return store_write(1,save_blob,sizeof(gSaveGame)+gSaveGame.dataSize);
}
bool osLoadGame() {
    SaveGame sg;if(!saved_header(sg) || sg.level!=gLevelID)return false;
    gSaveGame=sg;memcpy(gSaveData,save_blob+sizeof(sg),sg.dataSize);return true;
}
bool osValidateSave() {
    // Check every serialized record before invoking any virtual load method.
    uint8_t scratch[256];unsigned offset=0;
    for(unsigned i=0;i<level.itemsCount;++i) {
        unsigned n=items[i].save(scratch)-scratch;
        if(n>sizeof(scratch) || offset>gSaveGame.dataSize || n>gSaveGame.dataSize-offset)return false;
        const ItemSave *sg=reinterpret_cast<const ItemSave*>(gSaveData+offset);
        unsigned animCount=(reinterpret_cast<const uint8_t*>(level.animStates)-reinterpret_cast<const uint8_t*>(level.anims))/sizeof(Anim);
        if(sg->roomIndex>=level.roomsCount || sg->animIndex>=animCount)return false;
        if(items[i].type==ITEM_LARA) {
            const auto *ls=reinterpret_cast<const Lara::LaraSave*>(gSaveData+offset+sizeof(ItemSave));
            if(ls->cameraRoom>=level.roomsCount || ls->armR.animIndex>=animCount || ls->armL.animIndex>=animCount)return false;
            for(unsigned j=0;j<JOINT_MAX;++j)if(ls->meshes[j]>=level.meshesCount)return false;
        }
        offset+=n;
    }
    return offset==gSaveGame.dataSize;
}
void osJoyVibrate(int32, int32, int32) {}
void osSetPalette(const uint16* p) {
    for (unsigned i = 0; i < 256; ++i) {
        unsigned c = p[i]; // Upstream uses BGR555; PXA expects RGB565.
        ol_palette[i] = ((c & 31u) << 11) | (((c >> 5) & 31u) << 6) |
                        (((c >> 5) & 31u) >> 4 << 5) | ((c >> 10) & 31u);
    }
}
const void* osLoadScreen(LevelID) { return title_screen; }
const void* osLoadLevel(LevelID) { return ol_level_data; }
// The upstream bank is 10512 Hz. Interpolate to the negotiated Host rate.
static int8_t mix_block[SND_SAMPLES];
static unsigned mix_pos=SND_SAMPLES, phase;
static int previous_sample, next_sample;
static bool primed;
static int source_sample() {
    if(mix_pos==SND_SAMPLES) {sndFill(mix_block);mix_pos=0;}
    return int(mix_block[mix_pos++])*256;
}
extern int32 channelsCount;
bool ol_audio_active() {
    if(channelsCount || sndTrackIsPlaying() || previous_sample || next_sample)return true;
    for(unsigned i=mix_pos;i<SND_SAMPLES;++i)if(mix_block[i])return true;
    // Silence need not occupy the Host FIFO. A future voice starts at the
    // current game time; dropping only these zero samples cannot cut a tail.
    mix_pos=SND_SAMPLES;phase=0;primed=false;return false;
}
void ol_audio(int16_t *out,unsigned count,unsigned rate) {
    if(!primed) {previous_sample=source_sample();next_sample=source_sample();primed=true;}
    for(unsigned i=0;i<count;++i) {
        out[i]=int16_t(previous_sample+(next_sample-previous_sample)*int(phase)/int(rate));
        phase+=SND_OUTPUT_FREQ;
        while(phase>=rate) {phase-=rate;previous_sample=next_sample;next_sample=source_sample();}
    }
}
const uint8_t *ol_pixels() { return reinterpret_cast<const uint8_t*>(fb); }
void ol_start(int id) {
    gLevelID = LevelID(id);
    gNextLevel = LVL_MAX;
    keys = 0;mix_pos=SND_SAMPLES;phase=0;primed=false;
    for(unsigned y=0;y<FRAME_HEIGHT;++y)for(unsigned x=0;x<FRAME_WIDTH;++x)
        title_screen[y*FRAME_WIDTH+x]=ol_title_data[(y*160/FRAME_HEIGHT)*240+x*240/FRAME_WIDTH];
    if (!initialized) {
        gameInit();
        initialized = true;
    } else {
        inventory.state = INV_STATE_NONE;
        startLevel(gLevelID);
    }
    if(restore_pending) {
        restore_pending=false;
        bool ok=gameLoad();pxa_log_write(ok?2:4,ok?"OpenLara save restored":"OpenLara invalid save; level restarted");
    }
}
void ol_update(unsigned frames, uint32_t buttons) {
    keys = buttons;
    if (frames) gameUpdate(int32(frames));
}
void ol_render() { gameRender(); }
void ol_free() { if (initialized) gameFree(); initialized = false; }
int ol_next_level() {
    if (inventory.state == INV_STATE_NONE && gNextLevel != LVL_MAX) {
        int id = int(gNextLevel);
        gNextLevel = LVL_MAX;
        if(id==LVL_LOAD) {SaveGame sg;if(!saved_header(sg))return -1;restore_pending=true;return sg.level;}
        return id;
    }
    return -1;
}
void ol_player_position(int32_t out[4]) {
    if (!players[0]) { memset(out, 0, 4 * sizeof(*out)); return; }
    out[0] = players[0]->pos.x; out[1] = players[0]->pos.y;
    out[2] = players[0]->pos.z; out[3] = players[0]->angle.y;
}

bool ol_quick_save() {return initialized && int(gLevelID)>0 && gameSave();}
bool ol_quick_load() {if(!osCheckSave())return false;inventory.state=INV_STATE_NONE;nextLevel(LVL_LOAD);return true;}
void ol_show_title() {inventory.state=INV_STATE_NONE;nextLevel(LVL_TR1_TITLE);}
void ol_draw_message(const char *message) {if(initialized && message && *message) {drawText(0,FRAME_HEIGHT-24,message,TEXT_ALIGN_CENTER);flush();}}

void ol_draw_menu(unsigned selected,bool help,bool settings) {
    unsigned black=0;
    for(unsigned i=1;i<256;++i)if(ol_palette[i]<ol_palette[black])black=i;
    memset(fb,black,FRAME_WIDTH*FRAME_HEIGHT);
    if(help) {
        const char *lines[]={"CONTROLS", "D-pad: move / turn", "A: action   B: jump", "X: weapon   Y: roll", "L: look   R: walk", "Start: inventory   Select: menu", "PC: arrows  Z/X  A/S  Q/W", "PC: Enter/Tab = Start/Select", "Touch: left pad, right actions", "Bottom centre: this menu"};
        for(unsigned i=0;i<sizeof(lines)/sizeof(lines[0]);++i)drawText(0,20+i*21,lines[i],TEXT_ALIGN_CENTER);
    } else if(settings) {
        const char *labels[]={"Sound effects", "Music", "Swap action/jump", "Show FPS", "Brightness", "Back"};
        unsigned values[]={gSettings.audio_sfx,gSettings.audio_music,gSettings.controls_swap,gSettings.video_fps,gSettings.video_gamma};
        drawText(0,20,"SETTINGS",TEXT_ALIGN_CENTER);
        for(unsigned i=0;i<6;++i) {
            char text[64];
            if(i<4)sprintf(text,"%s: %s",labels[i],values[i]?"ON":"OFF");
            else if(i==4)sprintf(text,"Brightness: %u",values[i]);
            else strcpy(text,labels[i]);
            drawText(0,40+i*25,text,TEXT_ALIGN_CENTER);
            if(i==selected)drawText(12,40+i*25,">",TEXT_ALIGN_LEFT);
        }
    } else {
        const char *lines[]={"Resume", "Save game", "Load game", "New game: Caves", "City of Vilcabamba", "Lara's home", "Title", "Settings", "Controls"};
        drawText(0,20,"OPENLARA",TEXT_ALIGN_CENTER);
        for(unsigned i=0;i<9;++i) {drawText(0,38+i*20,lines[i],TEXT_ALIGN_CENTER);if(i==selected)drawText(12,38+i*20,">",TEXT_ALIGN_LEFT);}
    }
    flush();
}

bool ol_toggle_setting(unsigned index) {
    if(store_busy())return false;
    Settings old=gSettings;
    switch(index) {
        case 0:gSettings.audio_sfx^=1;break;
        case 1:gSettings.audio_music^=1;break;
        case 2:gSettings.controls_swap^=1;break;
        case 3:gSettings.video_fps^=1;break;
        case 4:gSettings.video_gamma=(gSettings.video_gamma+4)&31;palSet(level.palette,gSettings.video_gamma<<4,gBrightness);break;
        default:return false;
    }
    if(!osSaveSettings()) {gSettings=old;return false;}
    return true;
}
