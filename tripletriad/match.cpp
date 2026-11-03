// SPDX-License-Identifier: BSD-2-Clause
// Host replacement. Board, CardHand and CPU_Player are prepared upstream source.
// Placement/flip ordering follows TTcards PlayState.cpp:796-944; desktop action
// callbacks execute synchronously here, while the UI keeps a timed CPU delay.
#include "match.hpp"
#include "helpers.hpp"
namespace pxa_tt {
Match::Match():cpu(tt::CPU_Player::Easy,&board,&hands[1],[this](tt::BoardTile& tile){
 const tt::Card selected=hands[1].getSelectedCard();
 const int index=hands[1].at(selected);
 if(index>=0)place(1,static_cast<unsigned>(index),tile.position().x,tile.position().y);
}) { hands[0].init(&resources,tt::PLAYER_1); hands[1].init(&resources,tt::PLAYER_2); }
uint32_t Match::random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
void Match::start(uint32_t seed,unsigned rules_mode){
 rng=seed?seed:1;nom::seed_random(rng);mode=rules_mode%3;rules.rule_set=tt::OPEN_RULE;
 if(mode==1)rules.rule_set|=tt::ELEMENTAL_RULESET;
 if(mode==2)rules.rule_set|=tt::ELEMENTAL_RULESET|tt::SAME_RULESET;
 board.initialize(&rules,&resources);hands[0].clear();hands[1].clear();
 // Same source card metadata and five-card hands as the upstream; deal unique
 // cards per hand to avoid its documented duplicate-id selection ambiguity.
 for(unsigned p=0;p<2;p++)while(hands[p].size()<5){
  const auto& d=card_defs[random()%card_def_count];tt::Card c;
  c.id=d.id;c.level=d.level;c.type=d.type;c.element=d.element;c.num=d.num;c.name=d.name;
  for(unsigned r=0;r<4;r++)c.ranks[r]=d.ranks[r];
  if(!hands[p].exists(c))hands[p].push_back(c);
 }
 turn=random()%2;moves=captures=ai_moves=thinking_ms=0;winner=0;paused=false;background=false;
 stage=Playing;generation++;update_score();
}
void Match::update_score(){for(unsigned i=0;i<2;i++)score[i]=board.getPlayerCount(tt::player_id(static_cast<tt::PlayerIndex>(i)))+hands[i].size();}
void Match::flip(const nom::Point2i& p){
 // The original two-level flip/Combo flow, with only renderer/audio actions removed.
 const auto grid=board.check_board(p);
 for(const auto& result:grid){
  const auto pos=result.tile.position();const auto owner=tt::flip_player_id(static_cast<tt::PlayerIndex>(turn));
  if(board.getPlayerID(pos.x,pos.y)!=owner)captures++;
  board.flip_card(pos,owner);update_score();
  if(tt::is_card_rule_set(&rules,tt::COMBO_RULE)){
   const auto secondary=board.check_board(pos);
   for(const auto& inner:secondary){const auto q=inner.tile.position();
    if(board.getPlayerID(q.x,q.y)!=owner)captures++;
    board.flip_card(q,owner);update_score();
   }
  }
 }
}
bool Match::place(unsigned player,unsigned hand_index,int x,int y){
 if(stage!=Playing||paused||background||player!=turn||x<0||y<0||x>=3||y>=3||hand_index>=hands[player].size())return false;
 if(board.status(nom::Point2i(x,y))!=tt::BAD_CARD_ID)return false;
 hands[player].set_position(hand_index);tt::Card selected=hands[player].getSelectedCard();
 if(selected.id==tt::BAD_CARD_ID)return false;
 board.update(nom::Point2i(x,y),selected);hands[player].erase(selected);flip(nom::Point2i(x,y));
 moves++;if(player==1)ai_moves++;update_score();
 if(board.getCount()==9||hands[0].empty()||hands[1].empty()){
  winner=score[0]>score[1]?1:score[0]<score[1]?2:0;stage=Result;
 }else turn=1-turn;
 thinking_ms=0;return true;
}
void Match::tick(uint32_t ms){
 if(stage!=Playing||paused||background||turn!=1)return;
 thinking_ms+=ms;
 if(thinking_ms>=600){thinking_ms=0;cpu.update(static_cast<float>(ms)/1000.0f);}
}
}
