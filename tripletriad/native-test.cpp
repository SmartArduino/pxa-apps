// SPDX-License-Identifier: BSD-2-Clause
// Checks observable gameplay and UI/lifecycle transitions against actual core.
#include "ui.hpp"
#include "helpers.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace pxa_tt;
static unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"CHECK FAILED line=%d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void picture(UI&u,const std::string&path){u.draw();FILE*f=fopen(path.c_str(),"wb");CHECK(f);fprintf(f,"P6\n%d %d\n255\n",Width,Height);for(uint8_t c:u.pixels){uint8_t rgb[3]={uint8_t(((c>>5)&7)*255/7),uint8_t(((c>>2)&7)*255/7),uint8_t((c&3)*255/3)};CHECK(fwrite(rgb,1,3,f)==3);}CHECK(fclose(f)==0);}
static tt::Card fixture(int id,tt::PlayerID owner,unsigned rank){tt::Card c;c.id=id;c.player_id=c.player_owner=owner;c.num=1;c.name="Fixture";for(auto&r:c.ranks)r=rank;return c;}
static void capture(unsigned mode,unsigned attacking_rank,bool expected){
 Match m;m.start(5,mode);m.board.clear();m.hands[0].clear();m.turn=0;
 auto enemy=fixture(2,tt::PLAYER_ID_2,4);m.board.update(nom::Point2i(0,1),enemy);
 m.hands[0].push_back(fixture(4,tt::PLAYER_ID_1,attacking_rank));
 m.hands[0].push_back(fixture(8,tt::PLAYER_ID_1,2));
 CHECK(m.place(0,0,1,1));CHECK(m.board.getPlayerID(0,1)==(expected?tt::PLAYER_ID_1:tt::PLAYER_ID_2));CHECK(m.moves==1);
}
int main(int argc,char**argv){
 CHECK(card_def_count==103);capture(0,7,true);capture(0,4,false);capture(0,2,false);
 // This deliberately pins upstream's existing single-equal SAME behavior,
 // rather than claiming that it implements the full standard FF8 Same rule.
 capture(2,4,true);
 // Pin the actual upstream greedy AI, which chooses the first capturing hand
 // card before its strongest-card corner fallback.
 {Match m;m.start(1,0);m.board.clear();m.hands[1].clear();m.turn=1;
  auto target=fixture(8,tt::PLAYER_ID_1,1);m.board.update(nom::Point2i(0,1),target);
  m.hands[1].push_back(fixture(2,tt::PLAYER_ID_2,2));m.hands[1].push_back(fixture(4,tt::PLAYER_ID_2,9));
  m.tick(600);CHECK(m.moves==1);CHECK(m.board.getStatus(0,0)==2);CHECK(m.board.getPlayerID(0,1)==tt::PLAYER_ID_2);}
 {Match m;m.start(1,0);m.board.clear();m.hands[1].clear();m.turn=1;
  m.hands[1].push_back(fixture(2,tt::PLAYER_ID_2,2));m.hands[1].push_back(fixture(4,tt::PLAYER_ID_2,9));
  m.tick(600);CHECK(m.moves==1);CHECK(m.board.getStatus(0,0)==4);}
 unsigned sessions=0,total_ai=0,total_captures=0,wins=0,losses=0,draws=0;
 for(unsigned mode=0;mode<3;mode++)for(unsigned seed=1;seed<=100;seed++){
  Match m;m.start(seed,mode);CHECK(m.hands[0].size()==5&&m.hands[1].size()==5);CHECK(m.score[0]+m.score[1]==10);
  CHECK(!m.place(2,0,0,0));CHECK(!m.place(m.turn,0,-1,0));CHECK(!m.place(m.turn,5,0,0));
  unsigned limit=0;
  while(m.stage==Playing&&limit++<100){
   if(m.turn==1){unsigned before=m.moves;m.paused=true;m.tick(6000);CHECK(before==m.moves);m.paused=false;m.background=true;m.tick(6000);CHECK(before==m.moves);m.background=false;m.tick(600);CHECK(m.moves==before+1);}
   else{auto free=m.board.free_tiles();CHECK(!free.empty());auto pos=free.front().position();CHECK(m.place(0,0,pos.x,pos.y));}
   CHECK(m.score[0]+m.score[1]==10);CHECK(m.board.getCount()==m.moves);CHECK(m.moves<=9);
  }
  CHECK(m.stage==Result);CHECK(m.moves==9);CHECK(m.board.getCount()==9);CHECK(m.hands[0].size()+m.hands[1].size()==1);
  CHECK(m.ai_moves>=4&&m.ai_moves<=5);CHECK(m.winner==(m.score[0]>m.score[1]?1:m.score[0]<m.score[1]?2:0));
  total_ai+=m.ai_moves;total_captures+=m.captures;sessions++;if(m.winner==1)wins++;else if(m.winner==2)losses++;else draws++;
 }
 UI u;CHECK(u.game.stage==Menu);u.draw();u.pointer(110,194);CHECK(u.rules_mode==1);u.pointer(110,138);CHECK(u.game.stage==Playing);
 unsigned guard=0;
 while(u.game.stage==Playing&&guard++<100){
  if(u.game.turn==1){u.game.paused=true;unsigned before=u.game.moves;u.tick(10000);CHECK(u.game.moves==before);u.game.paused=false;u.tick(600);}
  else{auto free=u.game.board.free_tiles();auto p=free.front().position();u.pointer(20,330);CHECK(u.board_focus);u.pointer(92+p.x*72+20,72+p.y*72+20);}
  u.draw();
 }
 CHECK(u.game.stage==Result);CHECK(u.game.moves==9);
 if(argc>1)picture(u,std::string(argv[1])+"/result.ppm");
 unsigned gen=u.game.generation;u.pointer(110,194);CHECK(u.game.stage==Playing);CHECK(u.game.generation==gen+1);CHECK(u.game.moves==0);
 u.pointer(300,18);CHECK(u.game.paused);u.pointer(110,194);CHECK(!u.game.paused);
 u.pointer(16,18);CHECK(u.game.stage==Menu);u.buttons(16);CHECK(u.game.stage==Playing);
 while(u.game.turn==1){u.tick(600);}u.buttons(16);CHECK(u.board_focus);u.buttons(32);CHECK(!u.board_focus);
 if(argc>1)picture(u,std::string(argv[1])+"/playing.ppm");
 printf("PASS checks=%u sessions=%u modes=3 ai_moves=%u captures=%u wins=%u losses=%u draws=%u UI_complete_match=9 pause_background_restart=passed\n",checks,sessions,total_ai,total_captures,wins,losses,draws);
 return 0;
}
