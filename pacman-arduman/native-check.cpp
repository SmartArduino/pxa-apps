/* MIT. Behavioral probes of the real ported owner, no device-only test hook. */
#include "backend.h"
#include "Arduboy.h"
#include "ardu_main.h"
#include "Player.h"
#include "Ghost.h"
#include "Highscores.h"
#include <assert.h>
#include <stdio.h>
#include <chrono>
extern Player player;
extern uint8_t g_transition;
extern int8_t g_selection;
static void advance(unsigned milliseconds) {
    while(milliseconds) {
        unsigned slice=milliseconds>16?16:milliseconds;
        auto start=std::chrono::steady_clock::now();
        arduboy_pxa_advance(slice);
        auto elapsed=std::chrono::steady_clock::now()-start;
        assert(elapsed<std::chrono::milliseconds(100));
        assert(!arduboy_pxa_state.fault);milliseconds-=slice;
    }
}
static void button(uint8_t mask,unsigned duration=60) {
    arduboy_pxa_pointer(mask,0);advance(duration);
    arduboy_pxa_pointer(0,1);advance(60);
}
static void startGame() {
    assert(g_mode==MODE_MENU&&g_selection==0);
    button(A_BUTTON);advance(3000);
    assert(g_mode==MODE_PLAYING&&g_level==1&&player.m_livesLeft==3);
}
static void forceGhostCollision() {
    player.m_powerTimer=0;
    ghosts[0].m_x=player.m_x;ghosts[0].m_y=player.m_y;
    ghosts[0].m_pauseTimer=0;ghosts[0].Update();
    assert(player.m_wasHurt);
    advance(40);assert(g_mode==MODE_DYING&&g_transition);
}
int main() {
    arduboy_pxa_boot(12345);advance(60);
    assert(g_mode==MODE_MENU&&player.m_score==0);
    startGame();
    float before=player.m_x;
    button(LEFT_BUTTON,600);
    assert(player.m_x<before&&player.m_pelletsEaten>0&&player.m_score>0);
    printf("MOVE x=%.2f score=%u pellets=%u\n",player.m_x,player.m_score,player.m_pelletsEaten);
    forceGhostCollision();advance(1500);
    assert(g_mode==MODE_PLAYING&&player.m_livesLeft==2&&!player.m_wasHurt);
    forceGhostCollision();advance(1500);
    assert(g_mode==MODE_PLAYING&&player.m_livesLeft==1);
    forceGhostCollision();advance(3200);
    assert(g_mode==MODE_ENTER_INITIALS&&player.m_livesLeft==0);
    uint32_t finalScore=player.m_score;
    button(DOWN_BUTTON);button(A_BUTTON);button(A_BUTTON);button(A_BUTTON);advance(50);
    assert(g_mode==MODE_HIGHSCORES&&player.m_score==finalScore);
    unsigned saved=((unsigned)EEPROM.read(400)<<8)|EEPROM.read(401);
    assert(saved==finalScore&&EEPROM.read(402)=='A');
    printf("DEATH / SCORE score=%u eeprom=%u\n",finalScore,saved);
    button(RIGHT_BUTTON);button(B_BUTTON);assert(g_mode==MODE_MENU);
    startGame();assert(player.m_score==0&&player.m_pelletsEaten==0);
    player.m_pelletsEaten=TOTAL_PELLETS;advance(30);
    assert(g_level==2&&g_transition==7);advance(6500);
    assert(g_mode==MODE_PLAYING&&g_level==2&&player.m_pelletsEaten==0);
    // Impossible turn location must return instead of entering random infinite loops.
    arduboy.fillScreen(1);ghosts[0].m_x=10;ghosts[0].m_y=10;
    ghosts[0].m_pauseTimer=0;player.m_x=80;player.m_y=44;ghosts[0].Update();
    arduboy_pxa_new();advance(60);
    assert(g_level==1&&g_mode==MODE_MENU&&!g_transition);
    assert(player.m_score==0&&player.m_livesLeft==3&&player.m_pelletsEaten==0);
    assert((((unsigned)EEPROM.read(400)<<8)|EEPROM.read(401))==saved);
    uint8_t state[32];assert(arduboy_game_pack(state,sizeof(state))==32);
    printf("PASS intro, movement/pellets, ghost death, 3 lives, initials, scores, level transition, bounded ghost path, full reset\n");
}
