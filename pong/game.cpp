/* PXA adapter MIT; original Pong code copyright 2017 Taylor Hansen, MIT. */
#include "../common/retro-games/arduboy-batch-20261010/backend.h"
#include "pxa_port_log.h"
#include "original/game.cpp"
#include "original/vector.cpp"
#include "original/paddle.cpp"
#include "original/ball.cpp"
#include "original/pong.ino"
static unsigned last_logged_frame;
extern "C" void arduboy_game_boot() { pong_reset();sound.noTone();setup();last_logged_frame=~0u; }
extern "C" void arduboy_game_step() {
 loop();unsigned frame=Arduboy2Base::frameCount;
 if(frame!=last_logged_frame && frame%15==0) {
  last_logged_frame=frame;pxlog_t l;pxlog_init(&l);pxlog_text(&l,"PONG");
  pxlog_field(&l,"phase",pong_phase());pxlog_field(&l,"x",ball.getPosition().getX());pxlog_field(&l,"y",ball.getPosition().getY());
  pxlog_field(&l,"p1y",player1.getPosition().getY());pxlog_field(&l,"p2y",player2.getPosition().getY());
  pxlog_field(&l,"score1",player1.getScore());pxlog_field(&l,"score2",player2.getScore());pxlog_send(&l);
 }
}
extern "C" const char *arduboy_game_name() {return "Pong";}
extern "C" size_t arduboy_game_pack(uint8_t *out,size_t capacity) {
 const size_t n=16;if(!out)return n;if(capacity<n)return 0;
 int16_t v[8]={(int16_t)pong_phase(),ball.getPosition().getX(),ball.getPosition().getY(),player1.getPosition().getY(),player2.getPosition().getY(),player1.getScore(),player2.getScore(),(int16_t)twoPlayerGame()};
 for(unsigned i=0;i<8;i++){out[i*2]=(uint8_t)v[i];out[i*2+1]=(uint8_t)((uint16_t)v[i]>>8);}return n;
}
