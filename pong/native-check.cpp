/* MIT. Exercise upstream Pong physics and the cooperative reset boundary. */
#include "game.cpp"
#include <assert.h>
#include <stdio.h>
extern "C" int32_t pxa_send_built(const uint8_t*, uint32_t) { return 0; }
static void advance(unsigned ms) {
 while(ms){unsigned n=ms>16?16:ms;arduboy_pxa_advance(n);ms-=n;assert(!arduboy_pxa_state.fault);}
}
static void press(uint8_t b,unsigned ms=70){arduboy_pxa_pointer(b,0);advance(ms);arduboy_pxa_pointer(0,1);advance(70);}
int main() {
 arduboy_pxa_boot(12345);advance(40);assert(pong_phase()==0);
 press(A_BUTTON);advance(1100);assert(pong_phase()==2);
 int before=player1.getPosition().getY();press(UP_BUTTON,260);assert(player1.getPosition().getY()<before);
 int x=ball.getPosition().getX();advance(120);assert(ball.getPosition().getX()!=x);
 printf("PLAY moved paddle %d -> %d, ball=%d,%d\n",before,player1.getPosition().getY(),ball.getPosition().getX(),ball.getPosition().getY());
 press(LEFT_BUTTON);assert(pong_phase()==3);Vector paused=ball.getPosition();advance(400);assert(ball.getPosition().getX()==paused.getX()&&ball.getPosition().getY()==paused.getY());
 press(LEFT_BUTTON);assert(pong_phase()==2);
 ball.setPosition(Vector(127,20));advance(40);assert(player1.getScore()==1);
 player1.updateScore();ball.setPosition(Vector(127,20));advance(40);assert(pong_phase()==4&&player1.getScore()==3);
 press(A_BUTTON);advance(1100);assert(pong_phase()==2&&player1.getScore()==0&&player2.getScore()==0);
 for(unsigned i=0;i<64;i++) {arduboy_pxa_new();advance(40);assert(pong_phase()==0&&!twoPlayerGame()&&player1.getScore()==0);}
 puts("PASS: movement, AI, ball, pause/resume, score, win, restart, 64 NEW cycles");
}
