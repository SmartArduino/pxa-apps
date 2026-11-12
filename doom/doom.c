// SPDX-License-Identifier: GPL-3.0-or-later
// PureDOOM backend for the shared PXA Core 2.1 game runtime.
#include "port.h"
#include "pxa_window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define DOOM_IMPLEMENTATION
#include "vendor/PureDOOM.h"
#ifdef PORT_FREEDOOM
#include "music.h"
#include "audio_clock.h"
static musicinfo_t *current_music;
static const void *current_music_data;
static void *owned_music;
static int current_music_lump=-1;
static unsigned music_error;
// PureDOOM synthesizes these labels from fixed columns of Doom's menu art.
// Freedoom's glyph widths differ; use its own HUD font without slicing patches.
static void freedoom_draw_options(void) {
    M_DrawOptions();
    M_WriteText(OptionsDef.x,OptionsDef.y+LINEHEIGHT*crosshair_opt,"CROSSHAIR:");
    M_WriteText(OptionsDef.x,OptionsDef.y+LINEHEIGHT*always_run_opt,"ALWAYS RUN:");
    M_WriteText(OptionsDef.x,OptionsDef.y+LINEHEIGHT*mouseoptions,"MOUSE OPTIONS");
}
static void freedoom_draw_mouse_options(void) {
    M_WriteText(74,45,"MOUSE OPTIONS");
    M_WriteText(MouseOptionsDef.x,MouseOptionsDef.y+LINEHEIGHT*mousemov,"MOUSE MOVE:");
    V_DrawPatchDirect(MouseOptionsDef.x+149,MouseOptionsDef.y+LINEHEIGHT*mousemov,0,
        W_CacheLumpName(msgNames[mousemove],PU_CACHE));
    M_DrawThermo(MouseOptionsDef.x,MouseOptionsDef.y+LINEHEIGHT*(mousesens+1),10,mouseSensitivity);
}
static void freedoom_menu_init(void) {
    for(unsigned i=0;i<sizeof(OptionsMenuFull)/sizeof(*OptionsMenuFull);++i)
        if(!strncmp(OptionsMenuFull[i].name,"TXT_",4))OptionsMenuFull[i].name[0]=0;
    MouseOptionsMenu[mousemov].name[0]=0;
    OptionsDef.routine=freedoom_draw_options;
    MouseOptionsDef.routine=freedoom_draw_mouse_options;
}
static void sync_music(void) {
    int lump=mus_playing_s_sound?mus_playing_s_sound->lumpnum:-1;
    char episode4[9]={0};
    // Freedoom supplies distinct E4 tracks, whereas vanilla Doom reuses E1-E3.
    if(mus_playing_s_sound && gamestate==GS_LEVEL && gamemode!=commercial && gameepisode==4 && gamemap>=1 && gamemap<=9) {
        snprintf(episode4,sizeof(episode4),"D_E4M%d",gamemap);
        int replacement=W_CheckNumForName(episode4);
        if(replacement>=0)lump=replacement;
    }
    const void *data=mus_playing_s_sound?mus_playing_s_sound->data:NULL;
    if(current_music==mus_playing_s_sound && current_music_data==data && current_music_lump==lump)return;
    fd_music_stop();
    if(owned_music) {Z_ChangeTag(owned_music,PU_CACHE);owned_music=NULL;}
    current_music=mus_playing_s_sound;
    current_music_data=data;current_music_lump=lump;
    music_error=0;
    if(!current_music)return;
    if(lump!=current_music->lumpnum)data=owned_music=W_CacheLumpNum(lump,PU_MUSIC);
    int ok=fd_music_start(data,(size_t)W_LumpLength(lump),mus_loop);
    port_log("Freedoom MUSIC song=%s format=MIDI playing=%d loop=%d",
        lump!=current_music->lumpnum?episode4:current_music->name,ok,mus_loop);
}
#endif

static int initialized;
#ifdef PORT_FREEDOOM
static fd_audio_clock_t audio_clock;
static int audio_busy;
static uint64_t audio_mix_us,audio_frames;
#endif
static char last_message[256];
static int held[256];
static unsigned audio_fraction, audio_cursor=512, audio_due;
static const short *audio_block;
#ifdef PORT_FREEDOOM
// PureDOOM discovers game modes by filename. Translate its probes in the file
// callback so upstream Freedoom IWADs keep their original names and bytes.
static const char *wad_names[]={"freedoom2.wad","freedoom1.wad"};
#define PORT_IWAD_LIMIT (64*1024*1024)
#else
static const char *wad_names[]={"doom2.wad","plutonia.wad","tnt.wad","doomu.wad","doom.wad","doom1.wad"};
#define PORT_IWAD_LIMIT (20*1024*1024)
#endif
static const char *selected_wad;
typedef struct {FILE *stream; int writing, failed; char target[96];} GameFile;

static void print_message(const char *s) {
    if(s && strspn(s,"\r\n \t")!=strlen(s))snprintf(last_message,sizeof(last_message),"%s",s);
    port_log("%s",s);
}
static void *allocate(int n) {
    void *p=n>0?malloc((size_t)n):NULL;
    if(!p)port_error("DOOM ran out of memory.\nTry the shareware IWAD or a larger memory profile.");
    return p;
}
static void game_exit(int code) {
    if(!code)port_error("DOOM exited.\nUse Home to return to the launcher.");
    port_error(last_message[0]?last_message:"DOOM stopped after an engine error.");
}
static void game_time(int *s,int *us) {
#ifdef PORT_FREEDOOM
    if(initialized)port_game_audio(); // NetUpdate calls this between renderer phases.
#endif
    uint64_t t=port_time_us();*s=(int)(t/1000000);*us=(int)(t%1000000);
}
static char *game_env(const char *name) {
    if(!strcmp(name,"HOME") || !strcmp(name,"DOOMWADDIR"))return "/data";
    return NULL;
}
static void *game_open(const char *path,const char *mode) {
    const char *base=strrchr(path,'/');base=base?base+1:path;
    char target[96];
    if(strstr(base,".wad")) {
#ifdef PORT_FREEDOOM
        const char *probe=selected_wad && !strcmp(selected_wad,"freedoom1.wad")?"doomu.wad":"doom2.wad";
        if(!selected_wad || strcmp(base,probe) || strcmp(mode,"rb"))return NULL;
        snprintf(target,sizeof(target),"/data/%s",selected_wad);
#else
        if(!selected_wad || strcmp(base,selected_wad))return NULL;
        snprintf(target,sizeof(target),"/data/%s",base);
#endif
    } else if(!strcmp(base,"default.cfg") || !strcmp(base,".doomrc")) {
        snprintf(target,sizeof(target),"/data/default.cfg");
    } else if(strlen(base)==12 && !strncmp(base,"doomsav",7) && base[7]>='0' && base[7]<='5' && !strcmp(base+8,".dsg")) {
        snprintf(target,sizeof(target),"/data/%s",base);
    } else return NULL;
#ifdef PORT_FREEDOOM
    // The phases share an application identity, but never config/save slots.
    if(!strstr(base,".wad")) {
        const char *leaf=strrchr(target,'/')+1;
        char isolated[96];
        snprintf(isolated,sizeof(isolated),"/data/freedoom%c-%s",selected_wad[8],leaf);
        snprintf(target,sizeof(target),"%s",isolated);
    }
#endif
    GameFile *f=calloc(1,sizeof(*f));if(!f)return NULL;
    snprintf(f->target,sizeof(f->target),"%s",target);
    f->writing=strchr(mode,'w')!=NULL;
    f->stream=fopen(f->writing?"/data/doom-pending.tmp":target,mode);
    if(!f->stream) {free(f);return NULL;}
    return f;
}
static void game_close(void *handle) {
    GameFile *f=handle;
    if(fclose(f->stream))f->failed=1;
    if(f->writing) {
        if(f->failed || port_replace("/data/doom-pending.tmp",f->target)) {
            port_log("DOOM save failed; previous file retained: %s",f->target);
            if(initialized && players[consoleplayer].mo)players[consoleplayer].message="PXA SAVE FAILED";
        } else port_log("DOOM saved %s",f->target);
    }
    free(f);
}
static int game_read(void *handle,void *out,int count) {GameFile *f=handle;return count>0?(int)fread(out,1,(size_t)count,f->stream):0;}
static int game_write(void *handle,const void *bytes,int count) {
    GameFile *f=handle;if(count<=0)return 0;
    int n=(int)fwrite(bytes,1,(size_t)count,f->stream);if(n!=count)f->failed=1;return n;
}
static int game_seek(void *handle,int offset,doom_seek_t origin) {return fseek(((GameFile*)handle)->stream,offset,(int)origin);}
static int game_tell(void *handle) {return (int)ftell(((GameFile*)handle)->stream);}
static int game_eof(void *handle) {return feof(((GameFile*)handle)->stream);}
static unsigned le32(const unsigned char *p) {return p[0]|p[1]<<8|p[2]<<16|(unsigned)p[3]<<24;}
static void validate_iwad(const char *name) {
    char path[96];snprintf(path,sizeof(path),"/data/%s",name);
    FILE *f=fopen(path,"rb");unsigned char h[16];
    if(!f || fseek(f,0,SEEK_END))port_error("Cannot read the selected IWAD.");
    long size=ftell(f);rewind(f);
    if(size<12 || size>PORT_IWAD_LIMIT || fread(h,1,12,f)!=12 || memcmp(h,"IWAD",4))
        port_error("Invalid IWAD header or size.\nRun tools/prepare-data.py again.");
    unsigned count=le32(h+4),dir=le32(h+8),required=0;
    if(!count || count>65536 || dir<12 || dir>(unsigned)size || count>((unsigned)size-dir)/16 || fseek(f,dir,SEEK_SET))
        port_error("Invalid IWAD directory.");
    for(unsigned i=0;i<count;++i) {
        if(fread(h,1,16,f)!=16)port_error("Cannot read the IWAD directory.");
        unsigned off=le32(h),len=le32(h+4);
        if(off>(unsigned)size || len>(unsigned)size-off || (len && off<12))port_error("Invalid IWAD lump boundaries.");
        if(!memcmp(h+8,"PLAYPAL\0",8) && len>=768)required|=1;
        if(!memcmp(h+8,"COLORMAP",8) && len>=256)required|=2;
        if(!memcmp(h+8,"TITLEPIC",8) && len>=8)required|=4;
        if(!memcmp(h+8,"E1M1\0\0\0\0",8) || !memcmp(h+8,"MAP01\0\0\0",8))required|=8;
    }
    fclose(f);
    if(required!=15)port_error("IWAD lacks required game resources.");
    port_log("DOOM IWAD validated name=%s bytes=%ld lumps=%u",name,size,count);
}
int port_game_in_menu(void) {return initialized && (menuactive || messageToPrint || saveStringEnter || gamestate!=GS_LEVEL);}
void port_game_key(int key,int down) {
    if(!initialized || key<0 || key>=256)return;
    if(!down) {if(held[key])doom_key_up((doom_key_t)held[key]);held[key]=0;return;}
    if(held[key])return;
    int mapped=0,menu=port_game_in_menu();
    switch(key) {
    case 128:case 'w':mapped=DOOM_KEY_UP_ARROW;break;
    case 129:case 's':mapped=DOOM_KEY_DOWN_ARROW;break;
    case 130:mapped=DOOM_KEY_LEFT_ARROW;break;
    case 131:mapped=DOOM_KEY_RIGHT_ARROW;break;
    case 'a':mapped=menu?DOOM_KEY_LEFT_ARROW:DOOM_KEY_COMMA;break;
    case 'd':mapped=menu?DOOM_KEY_RIGHT_ARROW:DOOM_KEY_PERIOD;break;
    case 200:mapped=menu?DOOM_KEY_ENTER:DOOM_KEY_CTRL;break;
    case ' ':mapped=menu?DOOM_KEY_ESCAPE:DOOM_KEY_SPACE;break;
    case 'e':mapped=DOOM_KEY_SHIFT;break;
    case 9:mapped=DOOM_KEY_TAB;break;
    case 13:mapped=menu?DOOM_KEY_ENTER:DOOM_KEY_TAB;break;
    case 27:mapped=DOOM_KEY_ESCAPE;break;
    case 'q': {
        int w=players[consoleplayer].readyweapon;
        for(int i=1;i<=NUMWEAPONS;++i) {
            int next=(w+i)%NUMWEAPONS;
            if(players[consoleplayer].weaponowned[next]) {mapped= next==wp_chainsaw?'1':next==wp_supershotgun?'3':'1'+next;break;}
        }
        break;
    }
    default:break;
    }
    if(messageToPrint && messageNeedsInput) {
        if(mapped==DOOM_KEY_ENTER)mapped='y';
        else if(mapped==DOOM_KEY_ESCAPE)mapped='n';
    }
    if(mapped) {held[key]=mapped;doom_key_down((doom_key_t)mapped);}
}
void port_game_motion(int dx,int dy) {if(initialized && !port_game_in_menu())doom_mouse_move(dx*2,0);(void)dy;}
void port_game_audio(void) {
#ifdef PORT_FREEDOOM
    if(!initialized || audio_busy)return;
    if(!port_audio_enabled()) {audio_clock.ready=0;return;}
    audio_busy=1;
    unsigned epoch=port_audio_epoch();
    if(!audio_clock.ready || audio_clock.epoch!=epoch)audio_cursor=512;
    audio_due=fd_audio_clock_take(&audio_clock,port_time_us(),epoch);
    if(!audio_due) {audio_busy=0;return;}
#endif
    while(audio_due) {
        if(audio_cursor==512) {audio_block=doom_get_sound_buffer();audio_cursor=0;}
        unsigned n=512-audio_cursor;if(n>audio_due)n=audio_due;
#ifdef PORT_FREEDOOM
        short mixed[1024];
        memcpy(mixed,audio_block+audio_cursor*2,n*2*sizeof(*mixed));
        uint64_t mix_began=port_time_us();
        // Loading a level can yield while its WAD stream is in use. Only mix
        // an already loaded track here; never seek/read that stream recursively.
        if(current_music==mus_playing_s_sound &&
           current_music_data==(mus_playing_s_sound?mus_playing_s_sound->data:NULL))
            fd_music_mix(mixed,n,(unsigned)snd_MusicVolume,mus_paused);
        audio_mix_us+=port_time_us()-mix_began;audio_frames+=n;
        if(fd_music_faults()!=music_error) {
            music_error=fd_music_faults();port_log("Freedoom MUSIC malformed track stopped");
        }
        port_pcm(mixed,n,11025,2);
        if(epoch!=port_audio_epoch() || !port_audio_enabled()) {
            audio_clock.ready=0;audio_due=0;audio_busy=0;return;
        }
#else
        port_pcm(audio_block+audio_cursor*2,n,11025,2);
#endif
        audio_cursor+=n;audio_due-=n;
    }
#ifdef PORT_FREEDOOM
    audio_busy=0;
#endif
}
void port_game_stats(void) {
    player_t *p=&players[consoleplayer];
    port_log("DOOM state=%d menu=%d tic=%d map=%d hp=%d x=%d y=%d zone=%dMiB",
        gamestate,menuactive,gametic,gamemap,p->health,p->mo?p->mo->x>>16:0,p->mo?p->mo->y>>16:0,mb_used);
#ifdef PORT_FREEDOOM
    port_log("Freedoom MUSIC samples=%llu loops=%u peak=%u volume=%d paused=%d",
        (unsigned long long)fd_music_position(),fd_music_loops(),fd_music_peak(),snd_MusicVolume,mus_paused);
    port_log("Freedoom PCM frames=%llu mix_us=%llu clock_resets=%u",
        (unsigned long long)audio_frames,(unsigned long long)audio_mix_us,audio_clock.resets);
    audio_frames=audio_mix_us=0;
#endif
}
void port_game_main(void) {
    memset(held,0,sizeof(held));
    for(unsigned i=0;i<sizeof(wad_names)/sizeof(*wad_names);++i) {
        char path[96];snprintf(path,sizeof(path),"/data/%s",wad_names[i]);FILE *f=fopen(path,"rb");
        if(f) {fclose(f);selected_wad=wad_names[i];break;}
    }
    if(!selected_wad) {
#ifdef PORT_FREEDOOM
        port_log("Freedoom missing IWAD: run freedoom/tools/prepare-data.py");
        port_error("No Freedoom IWAD found.\nRun freedoom/tools/prepare-data.py\nand install its media files.");
#else
        port_log("DOOM missing IWAD: prepare doom1.wad in the app media root");
        port_error("No DOOM IWAD found.\nPrepare doom1.wad and copy it to\nthe app's fixed SD media folder.");
#endif
    }
    validate_iwad(selected_wad);
    doom_set_print(print_message);doom_set_malloc(allocate,free);
    doom_set_file_io(game_open,game_close,game_read,game_write,game_seek,game_tell,game_eof);
    doom_set_gettime(game_time);doom_set_getenv(game_env);doom_set_exit(game_exit);
#ifdef PORT_FREEDOOM
    mb_used=8;
#else
    mb_used=4;
#endif
    doom_set_default_int("screenblocks",10);
    doom_set_default_int("snd_channels",8);
    char *argv[]={"doom","-config","/data/default.cfg","-noserial",NULL};
#ifdef PORT_FREEDOOM
    doom_init(4,argv,0);
    freedoom_menu_init();
    int bank_lump=W_CheckNumForName("GENMIDI");
    if(bank_lump<0 || !fd_music_init(W_CacheLumpNum(bank_lump,PU_STATIC),(size_t)W_LumpLength(bank_lump)))
        port_log("Freedoom MUSIC invalid GENMIDI bank; continuing with sound effects");
#else
    doom_init(4,argv,DOOM_FLAG_HIDE_MUSIC_OPTIONS);
#endif
    initialized=1;
    port_log("DOOM ready IWAD=%s render=320x200 tick=35 zone=%dMiB",selected_wad,mb_used);
    uint64_t next=port_time_us();unsigned remainder=0;int previous_menu=0;
    for(;;) {
        // Advance exactly one tic. Do not catch up through a long background pause.
        port_poll_input();
        doom_force_update();
#ifdef PORT_FREEDOOM
        audio_busy=1;
        sync_music(); // WAD access is safe after the engine has finished its tic.
        audio_busy=0;
#endif
        if(previous_menu && !menuactive)M_SaveDefaults();
        previous_menu=menuactive;
        if(saveStringEnter && !savegamestrings[saveSlot][0]) {
            snprintf(savegamestrings[saveSlot],SAVESTRINGSIZE,"PXA SAVE %d",saveSlot+1);
            saveCharIndex=(int)strlen(savegamestrings[saveSlot]);
        }
        port_palette(screen_palette,0);
#ifndef PORT_FREEDOOM
        audio_fraction+=11025;audio_due+=audio_fraction/35;audio_fraction%=35;
#endif
        port_frame(doom_get_framebuffer(1),320,200,320);
        remainder+=1000000;next+=remainder/35;remainder%=35;
        uint64_t now=port_time_us();
        if(now>next+100000)next=now;
        port_sleep(now<next?(unsigned)((next-now+999)/1000):0);
    }
}
