/* MIT. PXA owner for the preserved original Ardu-man game. */
#include "backend.h"
#include "Arduboy.h"
#include "ardu_main.h"
#include "Player.h"
#include "Ghost.h"
#include "Highscores.h"
#ifdef __wasm__
#include "pxa_port_log.h"
#endif
extern Player player;
extern int8_t g_selection;
extern uint8_t g_transition, g_deathLine;
extern uint32_t g_transitionDue;
extern void DrawMenu();
static uint32_t lastLog;
static GAME_MODE lastMode;
static uint32_t lastScore;
static void traceState() {
#ifdef __wasm__
    if (g_mode==lastMode && player.m_score==lastScore && millis()-lastLog<1000) return;
    lastLog=millis();lastMode=g_mode;lastScore=player.m_score;
    pxlog_t log;pxlog_init(&log);pxlog_text(&log,"CLASSIC PACMAN");
    pxlog_field(&log,"mode",g_mode);pxlog_field(&log,"level",g_level);
    pxlog_field(&log,"score",player.m_score);pxlog_field(&log,"pellets",player.m_pelletsEaten);
    pxlog_field(&log,"lives",player.m_livesLeft);pxlog_field(&log,"x10",(int)(player.m_x*10));
    pxlog_field(&log,"y10",(int)(player.m_y*10));pxlog_field(&log,"transition",g_transition);
    pxlog_send(&log);
#endif
}
extern "C" const char *arduboy_game_name(void) { return "ARDU-MAN"; }
extern "C" void arduboy_game_boot(void) {
    g_level=1;g_selection=0;g_mode=MODE_MENU;g_transition=0;g_transitionDue=0;g_deathLine=0;
    for(unsigned i=0;i<GHOST_MAX;++i) { ghosts[i]=Ghost();ghosts[i].m_x=ghosts[i].m_y=0; }
    GHOST_COUNT=STARTING_GHOSTS;player.ResetGame();ResetHighScoresUI();
    main_setup();DrawMenu();lastLog=0;lastMode=MODE_NONE;lastScore=UINT32_MAX;traceState();
}
extern "C" void arduboy_game_step(void) { main_loop();traceState(); }
extern "C" size_t arduboy_game_pack(uint8_t *out,size_t capacity) {
    constexpr size_t count=32;
    if(!out) return count;
    if(capacity<count) return 0;
    memset(out,0,count);uint8_t *p=out;
    auto u32=[&](uint32_t x){for(unsigned i=0;i<4;++i)*p++=(uint8_t)(x>>(8*i));};
    u32(g_mode);u32(g_level);u32(player.m_score);u32(player.m_pelletsEaten);
    u32(player.m_livesLeft);u32((uint32_t)(int32_t)(player.m_x*100));
    u32((uint32_t)(int32_t)(player.m_y*100));u32(g_transition);return count;
}
