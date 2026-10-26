/* SPDX-License-Identifier: GPL-2.0-or-later
 * PXA platform boundary for SDL Sopwith; original core is unchanged in vendor.
 * swmain's blocking outer loop is replaced with Host clock event scheduling.
 */
#include <stdarg.h>
#include <string.h>
#include "pxa_framebuffer.h"
#include "vendor/swmain.c"
#define swrestart sopwith_blocking_swrestart
#include "vendor/swinit.c"
#undef swrestart
#include "vendor/swtext.h"
#include "vendor/pcsound.h"
#include "vendor/font.h"

#define PORT_WIDTH 320
#define PORT_HEIGHT 240
static uint8_t pixels[PORT_WIDTH * PORT_HEIGHT];
static uint8_t upload[PXA_RASTER_UPLOAD_HEADER_BYTES + sizeof(pixels)];
static pxfb_t fb;
static uint8_t playing, fire_enabled;
static uint8_t awarding;
static int award_next, award_inc;
static int nexttic;
static int reported_present;
bool vid_fullscreen, snd_tinnyfilter;
int keybindings[NUM_KEYS], controller_bindings[NUM_KEYS];

static void text_at(int x, int y, const char *s, uint8_t color) {
    for (; *s; ++s, x += 8) {
        for (int yy = 0; yy < 8; ++yy) {
            uint8_t row = font_data[(uint8_t)*s * 8 + yy];
            for (int xx = 0; xx < 8; ++xx)
                if ((row & (0x80u >> xx)) && x + xx >= 0 &&
                    x + xx < PORT_WIDTH && y + yy >= 0 && y + yy < PORT_HEIGHT)
                    pixels[(y + yy) * PORT_WIDTH + x + xx] = color;
        }
    }
}

static void control_bar(void) {
    static const char *labels[8] = {"UP", "DOWN", "THR", "FLIP", "GUN", "BOMB", "HOME", "MENU"};
    memset(pixels + SCR_WDTH * SCR_HGHT, 0, PORT_WIDTH * 40);
    for (int i = 0; i < 8; ++i) {
        for (int x = i * 40; x < (i + 1) * 40; ++x)
            pixels[201 * PORT_WIDTH + x] = 3;
        for (int y = 201; y < PORT_HEIGHT; ++y)
            pixels[y * PORT_WIDTH + i * 40] = 1;
        text_at(i * 40 + 3, 216, labels[i], i == 4 && fire_enabled ? 1 : 3);
    }
}

static void menu_draw(void) {
    swtitln();
    /* Keep upstream's title drawing; replace only its blocking menu area. */
    for (int y = 104; y < SCR_HGHT; ++y) memset(pixels + y * PORT_WIDTH, 0, PORT_WIDTH);
    text_at(56, 118, "NOVICE - TAP TO START", 1);
    text_at(56, 150, "COMBAT - TAP TO START", 2);
    text_at(24, 180, "PXA TOUCH / ORIGINAL GAME CORE", 3);
    memset(pixels + SCR_WDTH * SCR_HGHT, 0, PORT_WIDTH * 40);
    text_at(24, 216, "HOLD UP: TAKE OFF / GUN: TOGGLE", 3);
    int32_t status = pxfb_present(&fb);
    if (!reported_present) {
        char line[150];
        snprintf(line, sizeof(line), "Sopwith FRAMEBUFFER status=%d screen=%dx%d texture=%dx%d",
                 status, fb.screen_width, fb.screen_height, fb.width, fb.height);
        (void)pxa_log_write(PXA_LOG_LEVEL_INFO, line);
        reported_present = 1;
    }
}

static void show_menu(void) {
    playing = 0; fire_enabled = 0; awarding = 0;
    memset(keysdown, 0, sizeof(keysdown));
    (void)pxa_log_write(PXA_LOG_LEVEL_INFO, "Sopwith MENU");
    menu_draw();
}

static void start_game(playmode_t mode) {
    playmode = mode; gamenum = 0;
    restart_flag = false; fire_enabled = 0; awarding = 0; have_savescore = false;
    memset(keysdown, 0, sizeof(keysdown));
    swinitlevel();
    nexttic = Timer_GetMS(); skip_time = 0; playing = 1;
    (void)pxa_log_write(PXA_LOG_LEVEL_INFO,
        mode == PLAYMODE_NOVICE ? "Sopwith START novice" : "Sopwith START combat");
    swdisp();
}

void Vid_Init(void) { vid_vram = pixels; vid_pitch = PORT_WIDTH; }
void Vid_Reset(void) {}
void Vid_Update(void) { if (playing) control_bar(); (void)pxfb_present(&fb); }
bool Vid_GetCtrlBreak(void) { return false; }
int Vid_GetKey(void) { return 0; }
int Vid_GetChar(void) { return '\n'; }
const char *Vid_KeyName(int key) { (void)key; return "TOUCH"; }
void Vid_StartTextInput(void) {}
void Vid_StopTextInput(void) {}
char *Vid_GetPrefPath(void) { return NULL; }
int Vid_GetNumVideoPalettes(void) { return 1; }
const char *Vid_GetVideoPaletteName(int palette) { (void)palette; return "CGA 1"; }
void Vid_SetVideoPalette(int palette) { (void)palette; }
void Vid_DrawTouchControls(void) {}
const struct touch_button *Vid_GetTouchButton(int x, int y) { (void)x; (void)y; return NULL; }
void Vid_TouchButtonPress(const struct touch_button *b, bool pressed) { (void)b; (void)pressed; }
void Vid_ShowTouchGameControls(void) {}
void Vid_ShowTouchKeys(const char *keys) { (void)keys; }
enum menukey Vid_ControllerMenuKey(void) { return MENUKEY_NONE; }
bool Vid_HaveController(void) { return false; }
const char *Vid_ControllerButtonName(enum gamekey key) { (void)key; return NULL; }
void Timer_Init(void) {}
int Timer_GetMS(void) { return (int)fb.time_ms; }
void Timer_Sleep(int ms) { (void)ms; } /* Dormant upstream blocking menus. */
void Speaker_Init(void) {}
void Speaker_Off(void) {}
void Speaker_Output(unsigned short count) { (void)count; }
void ErrorExit(char *fmt, ...) {
    char msg[240]; va_list args; va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args); va_end(args);
    (void)pxa_log_write(PXA_LOG_LEVEL_ERROR, msg); abort();
}
/* Reactor modules must not import WASI proc_exit. Unexpected upstream fatal
   paths trap this component, leaving lifecycle control with the PXA Host. */
void abort(void) { __builtin_trap(); }
void exit(int status) { (void)status; __builtin_trap(); }
/* Optional desktop high score name entry is blocking and intentionally omitted. */
void InitHighScores(void) {}
void LoadHighScoreTable(void) {}
void SaveHighScoreTable(void) {}
void DrawHighScoreTable(void) {}
bool NewHighScore(score_t *s) { (void)s; return false; }

/* Original swrestart's winner scoring transition, retaining its 200 ms pace
   while allowing PXA to deliver inputs/lifecycle events between awards. */
void swrestart(void) {
    OBJECTS *ob = planes[player];
    if (consoleplayer->ob_endsts == WINNER) {
        GetEndLevel(ob);
        awarding = 1; award_inc = 0; award_next = Timer_GetMS();
    } else {
        (void)NewHighScore(&ob->ob_score);
        gamenum = starting_level; have_savescore = false;
        playmode = PLAYMODE_UNSET; restart_flag = true;
    }
}

static void set_touch(enum gamekey key, int first_press) {
    keysdown[key] |= KEYDOWN_TOUCH;
    if (first_press) keysdown[key] |= KEYDOWN_WAS_PRESSED;
}

static void pointer_input(void) {
    int down = fb.pointer_phase == PXA_POINTER_DOWN;
    if (down) {
        char line[100];
        snprintf(line, sizeof(line), "Sopwith POINTER x=%d y=%d", fb.pointer_x, fb.pointer_y);
        (void)pxa_log_write(PXA_LOG_LEVEL_INFO, line);
    }
    if (fb.pointer_phase == PXA_POINTER_CANCEL) {
        memset(keysdown, 0, sizeof(keysdown)); fire_enabled = 0; return;
    }
    for (int i = 0; i < NUM_KEYS; ++i) keysdown[i] &= ~KEYDOWN_TOUCH;
    if (fb.pointer_phase == PXA_POINTER_UP) return;
    if (!playing) {
        if (down && fb.pointer_y >= 108 && fb.pointer_y < 176)
            start_game(fb.pointer_y < 142 ? PLAYMODE_NOVICE : PLAYMODE_COMPUTER);
        return;
    }
    if (fb.pointer_y < 200 || fb.pointer_x < 0 || fb.pointer_x >= 320) return;
    switch (fb.pointer_x / 40) {
    case 0: set_touch(KEY_PULLUP, down); set_touch(KEY_ACCEL, down); break;
    case 1: set_touch(KEY_PULLDOWN, down); set_touch(KEY_ACCEL, down); break;
    case 2: set_touch(KEY_ACCEL, down); break;
    case 3: if (down) keysdown[KEY_FLIP] |= KEYDOWN_WAS_PRESSED; break;
    case 4:
        if (down) {
            fire_enabled = !fire_enabled;
            keysdown[KEY_FIRE] = fire_enabled ? KEYDOWN_KEYBOARD : 0;
            (void)pxa_log_write(PXA_LOG_LEVEL_INFO,
                fire_enabled ? "Sopwith GUN ON" : "Sopwith GUN OFF");
        }
        break;
    case 5: set_touch(KEY_BOMB, down); break;
    case 6: set_touch(KEY_HOME, down); break;
    case 7: if (down) show_menu(); break;
    }
}

static void game_step(void) {
    int nowtime = Timer_GetMS();
    if (!playing) { menu_draw(); return; }
    if (awarding) {
        OBJECTS *ob = planes[player];
        if (nowtime < award_next) return;
        if (ob->ob_crashcnt < maxcrash) {
            ++ob->ob_crashcnt; award_inc += 25;
            ob->ob_score.score += award_inc;
            swdisp(); award_next = nowtime + 200;
        } else {
            ++gamenum; savescore = ob->ob_score; have_savescore = true;
            awarding = 0; restart_flag = true;
        }
        return;
    }
    if (restart_flag) {
        if (playmode == PLAYMODE_UNSET) { show_menu(); return; }
        restart_flag = false; swinitlevel(); nexttic = nowtime; skip_time = 0;
    }
    /* Same command generation, lag handling, move, draw and collision order
       as upstream swmain(), with the Host providing the outer scheduling. */
    if (nowtime > nexttic && latest_player_time[player] - countmove < MAX_NET_LAG) {
        NewMove();
        nexttic = nowtime - nexttic > 1000 ? nowtime + (1000 / FPS) : nexttic + (1000 / FPS);
        nexttic += skip_time; skip_time = 0;
    }
    asynupdate(); swsndupdate();
    if (CanMove()) {
        CalculateLag(); swmove(); swdisp(); swcollsn(); swsound();
        if (countmove % 10 == 0) {
            int shots = 0, activebombs = 0, enemyplanes = 0;
            for (OBJECTS *object = objtop; object; object = object->ob_next) {
                if (object->ob_owner == consoleplayer) {
                    shots += object->ob_type == SHOT;
                    activebombs += object->ob_type == BOMB;
                }
                enemyplanes += object->ob_type == PLANE && object != consoleplayer;
            }
            char line[220];
            snprintf(line, sizeof(line), "Sopwith t=%d x=%d y=%d speed=%d ammo=%d bombs=%d state=%d keys=%d shots=%d activebombs=%d enemyplanes=%d",
                countmove, consoleplayer->ob_x, consoleplayer->ob_y,
                consoleplayer->ob_speed, consoleplayer->ob_rounds,
                consoleplayer->ob_bombs, consoleplayer->ob_state,
                latest_player_commands[player][(countmove - 1) % MAX_NET_LAG],
                shots, activebombs, enemyplanes);
            (void)pxa_log_write(PXA_LOG_LEVEL_INFO, line);
        }
    }
}

int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
    pxa_ui_environment_t initial_environment;
    int initial_ok = pxa_ui_parse_start_environment(config, length, &initial_environment);
    memset(&fb, 0, sizeof(fb));
    fb.pixels = pixels; fb.upload = upload; fb.upload_bytes = sizeof(upload);
    fb.width = PORT_WIDTH; fb.height = PORT_HEIGHT;
    fb.palette[0] = 0; fb.palette[1] = 0x07ff; fb.palette[2] = 0xf81f; fb.palette[3] = 0xffff;
    playing = fire_enabled = 0; reported_present = 0;
    char *args[] = {"sopwith", "-q"};
    swinit(2, args);
    (void)pxa_log_write(PXA_LOG_LEVEL_INFO, "Sopwith PXA: upstream c4e034109e28, touch frontend, silent");
    fb.pointer_controller=1;int32_t status = pxfb_start(&fb, config, length, 33);
    char line[100];
    snprintf(line, sizeof(line), "Sopwith CONFIG length=%d first=%d second=%d parsed-before=%d",
        length, length > 1 ? pxa_read_u16(config) : -1,
        length > 3 ? pxa_read_u16(config + 2) : -1, initial_ok);
    (void)pxa_log_write(PXA_LOG_LEVEL_INFO, line);
    return status;
}

int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t length) {
    uint32_t events = pxfb_event(&fb, bytes, length);
    if (events & PXFB_ERROR) {
        (void)pxa_log_write(PXA_LOG_LEVEL_ERROR, "Sopwith framebuffer error");
        return PXA_EVENT_HANDLED;
    }
    if (events & PXFB_READY) show_menu();
    if (events & PXFB_LIFECYCLE) {
        memset(keysdown, 0, sizeof(keysdown)); fire_enabled = 0;
    }
    if (events & PXFB_POINTER) pointer_input();
    if (events & PXFB_TICK) game_step();
    if (events & PXFB_BACK) {
        if (playing) { show_menu(); return PXA_EVENT_HANDLED; }
        return PXA_EVENT_UNHANDLED;
    }
    return events ? PXA_EVENT_HANDLED : PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; playing = 0; pxfb_stop(&fb); }
