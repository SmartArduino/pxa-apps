/* SPDX-License-Identifier: AGPL-3.0-or-later
 * PXA platform frontend for the unmodified upstream Brogue CE game.
 * Binaryen Asyncify retains the original synchronous C input call stack.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "GlobalsBase.h"
#include "pxa_framebuffer.h"
#include "font.h"

#define CELL_W 8
#define CELL_H 12
#define SCREEN_W (COLS * CELL_W)
#define GAME_H (ROWS * CELL_H)
#define SCREEN_H (GAME_H + 144)
#define ASYNC_STACK_BYTES 1048576

struct brogueConsole currentConsole;
char dataDirectory[BROGUE_FILENAME_MAX] = ".";
boolean serverMode = false, nonInteractivePlayback = false, hasGraphics = false;
boolean isCsvFormat = false, noMenu = false;
enum graphicsModes graphicsMode = TEXT_GRAPHICS;

static uint8_t pixels[SCREEN_W * SCREEN_H];
static uint8_t upload[SCREEN_W * SCREEN_H + PXA_RASTER_UPLOAD_HEADER_BYTES];
static pxfb_t fb;
static uint8_t async_stack[ASYNC_STACK_BYTES] __attribute__((aligned(16)));
/* Asyncify retains Wasm locals, while the original C stack stays live in
 * linear memory. Host event callbacks and exported malloc use another stack. */
uint32_t br_game_stack_pointer;
void br_call_game(void);
void br_save_game_stack(void);
static struct { uint32_t pos, end; } async_data;
static int suspended, rewinding, started, completed, dirty, waiting_input, uppercase;
static uint32_t deadline, previous_buttons;
static unsigned long logged_turn;
static short logged_x, logged_y, logged_hp;
static uint64_t logged_seed;
static int logged_active, logged_ended;
static rogueEvent event_queue[32];
static unsigned queue_read, queue_write;

/* The PXA lifecycle owns process termination; an upstream fatal exit is a Guest trap. */
_Noreturn void exit(int status) {
    fprintf(stderr,"PXA_BROGUE fatal upstream exit=%d\n",status);
    __builtin_trap();
}
_Noreturn void _Exit(int status) { exit(status); }

__attribute__((import_module("asyncify"), import_name("start_unwind")))
void async_start_unwind(void *data);
__attribute__((import_module("asyncify"), import_name("stop_unwind")))
void async_stop_unwind(void);
__attribute__((import_module("asyncify"), import_name("start_rewind")))
void async_start_rewind(void *data);
__attribute__((import_module("asyncify"), import_name("stop_rewind")))
void async_stop_rewind(void);

/* Retain this leaf in Asyncify analysis so its pause/input callers are detected. */
__attribute__((noinline)) static void yield_to_pxa(void) {
    if (rewinding) {
        async_stop_rewind(); rewinding = 0; suspended = 0; return;
    }
    suspended = 1;
    br_save_game_stack();
    async_start_unwind(&async_data);
}

static uint8_t rgb_index(int r, int g, int b) {
    r = clamp(r, 0, 100); g = clamp(g, 0, 100); b = clamp(b, 0, 100);
    return (uint8_t)(((r * 7 + 50) / 100 << 5) |
                     ((g * 7 + 50) / 100 << 2) | ((b * 3 + 50) / 100));
}

static void draw_code(uint32_t code, int px, int py, uint8_t fg, uint8_t bg) {
    const uint8_t *rows = NULL;
    for (unsigned i = 0; i < sizeof(brogue_font)/sizeof(brogue_font[0]); ++i)
        if (brogue_font[i].code == code) { rows = brogue_font[i].rows; break; }
    for (int y = 0; y < CELL_H; ++y)
        for (int x = 0; x < CELL_W; ++x)
            pixels[(py+y)*SCREEN_W+px+x] = rows && (rows[y] & (1 << (7-x))) ? fg : bg;
}

static void draw_label(const char *s, int px, int py, uint8_t fg, uint8_t bg) {
    for (; *s; ++s, px += CELL_W) draw_code((unsigned char)*s, px, py, fg, bg);
}

static const char *command_labels[] = {"New", "Inv", "Equip", "Apply", "Throw", "Search", "Rest", "Down", "Up", "New!", "Quit", "Esc", "Yes", "No", "Space", "Enter"};
static const long command_keys[] = {'n','i','e','a','t','s','z','>','<','N','Q',ESCAPE_KEY,'y','n',' ',RETURN_KEY};
static const long direction_keys[] = {'y','k','u','h','.','l','b','j','n'};
static const char *direction_labels[] = {"NW","N","NE","W","Wait","E","SW","S","SE"};

static void button(int x,int y,int width,int height,const char *label,uint8_t color) {
    for(int yy=0;yy<height-2;++yy) memset(pixels+(y+yy)*SCREEN_W+x,color,width-2);
    draw_label(label,x+(width-(int)strlen(label)*CELL_W)/2,y+(height-CELL_H)/2,255,color);
}

static void draw_controls(void) {
    memset(pixels + GAME_H * SCREEN_W, 0, (SCREEN_H-GAME_H)*SCREEN_W);
    draw_label("Touch: 8 directions + commands + item letters. HOME closes. Saves are volatile.", 8, GAME_H+4, 255, 0);
    for(unsigned i=0;i<9;i++) button((i%3)*66,GAME_H+20+(i/3)*30,66,30,direction_labels[i],41);
    for (unsigned i=0;i<16;++i) {
        int x=204+(i%8)*74, y=GAME_H+20+(i/8)*18;
        button(x,y,74,18,command_labels[i],i%2 ? 37 : 41);
    }
    for(unsigned i=0;i<26;i++) {char label[2]={(uppercase?'A':'a')+i,0};button(204+(i%13)*45,GAME_H+56+(i/13)*18,45,18,label,37);}
    for(unsigned i=0;i<10;i++) {char label[2]={'0'+i,0};button(204+i*45,GAME_H+92,45,18,label,41);}
    button(654,GAME_H+92,54,18,"?",37);
    button(710,GAME_H+92,43,18,uppercase?"abc":"ABC",41);
    button(755,GAME_H+92,43,18,"Del",37);
}

static void queue_event(enum eventTypes type, long p1, long p2) {
    if (queue_write - queue_read >= 32) return;
    event_queue[queue_write++ % 32] = (rogueEvent){type,p1,p2,false,false};
}

static void command_button(unsigned index) {
    if(index==9 && rogue.gameInProgress) {
        /* CE 1.15.1 normalizes uppercase N to southeast before its NEW_GAME
         * case. A dedicated platform restart requests the same junction that
         * that original case selects; all cleanup and generation stay upstream. */
        rogue.nextGame=NG_NEW_GAME;rogue.gameHasEnded=true;
        fprintf(stderr,"PXA_BROGUE frontend restart requested\n");
        queue_event(KEYSTROKE,ESCAPE_KEY,0);
    } else queue_event(KEYSTROKE,command_keys[index],0);
}

static void pxa_plot(enum displayGlyph ch, short x, short y,
        short fr, short fg, short fblue, short br, short bg, short bblue) {
    if(x<0 || x>=COLS || y<0 || y>=ROWS) return;
    draw_code(glyphToUnicode(ch),x*CELL_W,y*CELL_H,
              rgb_index(fr,fg,fblue),rgb_index(br,bg,bblue));
    dirty=1;
}

static void present(void) {
    if (!dirty) return;
    draw_controls();
    int32_t status=pxfb_present(&fb);
    if (status >= 0) dirty=0;
}

static boolean pxa_pause(short milliseconds, PauseBehavior behavior) {
    (void)behavior;
    present();
    if (queue_write != queue_read) return true;
    waiting_input=0; deadline=fb.time_ms+(milliseconds>0 ? milliseconds : 1);
    yield_to_pxa();
    return queue_write != queue_read;
}

static void pxa_next_event(rogueEvent *out, boolean textInput, boolean colorsDance) {
    (void)textInput;
    while (queue_read == queue_write) {
        if(colorsDance) { shuffleTerrainColors(3,true); commitDraws(); }
        present(); waiting_input=1; yield_to_pxa();
    }
    *out=event_queue[queue_read++ % 32];
    fprintf(stderr,"PXA_BROGUE input type=%d key=%ld x=%ld\n",out->eventType,out->param1,out->param2);
}

static boolean modifiers(int modifier) { (void)modifier; return false; }
static void pxa_loop(void) { (void)rogueMain(); }
static void remap(const char *a,const char *b) {(void)a;(void)b;}

/* Called from an uninstrumented runtime frame, so every resume enters the same game root. */
__attribute__((noinline)) void br_run_game(void) {
    if (suspended) { rewinding=1; async_start_rewind(&async_data); }
    pxa_loop();
    if (suspended) { async_stop_unwind(); }
    else { completed=1; }
}

/* Called only after the ASM bridge has restored the frontend C stack. */
static void report_game_state(void) {
    if(rogue.seed!=logged_seed || rogue.playerTurnNumber!=logged_turn ||
       player.loc.x!=logged_x || player.loc.y!=logged_y || player.currentHP!=logged_hp ||
       rogue.gameInProgress!=logged_active || rogue.gameHasEnded!=logged_ended) {
        fprintf(stderr,"PXA_BROGUE state seed=%llu turn=%lu pos=%d,%d hp=%d depth=%d next=%d active=%d ended=%d\n",
            (unsigned long long)rogue.seed,rogue.playerTurnNumber,player.loc.x,player.loc.y,
            player.currentHP,rogue.depthLevel,rogue.nextGame,rogue.gameInProgress,rogue.gameHasEnded);
        logged_seed=rogue.seed;logged_turn=rogue.playerTurnNumber;
        logged_x=player.loc.x;logged_y=player.loc.y;logged_hp=player.currentHP;
        logged_active=rogue.gameInProgress;logged_ended=rogue.gameHasEnded;
        if(rogue.seed && rogue.depthLevel && monsters) {
            for(creatureIterator it=iterateCreatures(monsters);hasNextCreature(it);) {
                creature *monst=nextCreature(&it);
                if(canSeeMonster(monst)) {
                    windowpos w=mapToWindow(monst->loc);
                    fprintf(stderr,"PXA_BROGUE visible turn=%lu pos=%d,%d cell=%d,%d hp=%d name=%s\n",
                        rogue.playerTurnNumber,monst->loc.x,monst->loc.y,w.window_x,w.window_y,monst->currentHP,monst->info.monsterName);
                }
            }
            for(unsigned row=0;row<MESSAGE_LINES;row++) {
                char clean[COLS*2];unsigned n=0;
                const unsigned char *s=(const unsigned char *)displayedMessage[row];
                for(unsigned i=0;s[i] && n<sizeof(clean)-1;i++) {
                    if(s[i]==COLOR_ESCAPE && s[i+1] && s[i+2] && s[i+3]) {i+=3;continue;}
                    clean[n++]=s[i];
                }
                clean[n]=0;
                if(n) fprintf(stderr,"PXA_BROGUE message turn=%lu %s\n",rogue.playerTurnNumber,clean);
            }
        }
    }
}

/* Retained upstream CLI helper used by seed input in MainMenu.c. */
boolean tryParseUint64(char *str, uint64_t *num) {
    unsigned long long n; char buf[100];
    if (strlen(str) && sscanf(str,"%llu",&n) && sprintf(buf,"%llu",n) && !strcmp(buf,str)) {
        *num=(uint64_t)n; return true;
    }
    return false;
}

int32_t br_app_start(const uint8_t *config,uint32_t length) {
    memset(&fb,0,sizeof(fb)); fb.pixels=pixels; fb.upload=upload;
    fb.upload_bytes=sizeof(upload); fb.width=SCREEN_W; fb.height=SCREEN_H;
    for(unsigned i=0;i<256;++i) {
        unsigned r=(i>>5)*31/7,g=((i>>2)&7)*63/7,b=(i&3)*31/3;
        fb.palette[i]=(uint16_t)((r<<11)|(g<<5)|b);
    }
    async_data.pos=(uint32_t)(uintptr_t)async_stack;
    async_data.end=async_data.pos+sizeof(async_stack);
    suspended=rewinding=started=completed=waiting_input=uppercase=0; dirty=1;
    queue_read=queue_write=0; previous_buttons=0;
    logged_seed=UINT64_MAX;logged_turn=~0ul;logged_x=logged_y=logged_hp=-1;
    logged_active=logged_ended=-1;
    currentConsole=(struct brogueConsole){pxa_loop,pxa_pause,pxa_next_event,pxa_plot,remap,modifiers,NULL,NULL,NULL};
    rogue.nextGame=NG_NOTHING; rogue.nextGamePath[0]='\0'; rogue.nextGameSeed=0;
    rogue.mode=GAME_MODE_NORMAL; rogue.trueColorMode=false;
    return pxfb_start(&fb,config,length,33);
}

int32_t pxa_app_on_event(const uint8_t *event,uint32_t length) {
    uint32_t flags=pxfb_event(&fb,event,length);
    if(flags & PXFB_ERROR) return fb.error;
    if(flags & PXFB_BACK) return PXA_EVENT_UNHANDLED;
    if(flags & PXFB_CONTROLLER) {
        uint32_t pressed=fb.buttons & ~previous_buttons; previous_buttons=fb.buttons;
        if(pressed & PXA_CONTROLLER_UP) queue_event(KEYSTROKE,UP_ARROW,0);
        if(pressed & PXA_CONTROLLER_DOWN) queue_event(KEYSTROKE,DOWN_ARROW,0);
        if(pressed & PXA_CONTROLLER_LEFT) queue_event(KEYSTROKE,LEFT_ARROW,0);
        if(pressed & PXA_CONTROLLER_RIGHT) queue_event(KEYSTROKE,RIGHT_ARROW,0);
        if(pressed & PXA_CONTROLLER_A) queue_event(KEYSTROKE,' ',0);
        if(pressed & PXA_CONTROLLER_B) queue_event(KEYSTROKE,ESCAPE_KEY,0);
        if(pressed & PXA_CONTROLLER_START) queue_event(KEYSTROKE,RETURN_KEY,0);
        if(pressed & PXA_CONTROLLER_SELECT) queue_event(KEYSTROKE,'i',0);
    }
    if(flags & PXFB_POINTER) {
        if(fb.pointer_y >= GAME_H+20 && fb.pointer_phase==PXA_POINTER_UP) {
            int y=fb.pointer_y-GAME_H-20,x=fb.pointer_x;
            if(x>=0 && x<198 && y<90) queue_event(KEYSTROKE,direction_keys[(y/30)*3+x/66],0);
            else if(x>=204 && x<796 && y<36) command_button((y/18)*8+(x-204)/74);
            else if(x>=204 && x<789 && y>=36 && y<72) queue_event(KEYSTROKE,(uppercase?'A':'a')+((y-36)/18)*13+(x-204)/45,0);
            else if(x>=204 && x<654 && y>=72 && y<90) queue_event(KEYSTROKE,'0'+(x-204)/45,0);
            else if(x>=654 && y>=72 && y<90) {
                if(x>=710 && x<753) {uppercase=!uppercase;dirty=1;present();}
                else queue_event(KEYSTROKE,x<708?'?':DELETE_KEY,0);
            }
        } else if(fb.pointer_y < GAME_H) {
            enum eventTypes type=fb.pointer_phase==PXA_POINTER_DOWN ? MOUSE_DOWN :
                fb.pointer_phase==PXA_POINTER_UP ? MOUSE_UP : MOUSE_ENTERED_CELL;
            queue_event(type,fb.pointer_x/CELL_W,fb.pointer_y/CELL_H);
        }
    }
    if((flags & PXFB_READY) && !started) { started=1; suspended=0; br_call_game(); report_game_state(); }
    else if(started && !completed && (flags & (PXFB_TICK|PXFB_CONTROLLER|PXFB_POINTER)) &&
            ((!waiting_input && (int32_t)(fb.time_ms-deadline)>=0) || queue_write!=queue_read)) {
        br_call_game(); report_game_state();
    }
    if(completed) {
        draw_label("Brogue exited. Press HOME to close this PXA package.", 8, 8, 255, 0);
        dirty=1; present();
    }
    return flags ? PXA_EVENT_HANDLED : PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; pxfb_stop(&fb); }
