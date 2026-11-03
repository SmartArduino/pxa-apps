// SPDX-License-Identifier: BSD-2-Clause
// PXA match/lifecycle adapter for Jeffrey Carpenter's TTcards core.
#pragma once
#include <cstdint>
#include "Board.hpp"
#include "CardHand.hpp"
#include "CardRules.hpp"
#include "CPU_Player.hpp"
#include "CardResourceLoader.hpp"
namespace pxa_tt {
struct CardDef { int id,level,type,element,num; const char* name; unsigned ranks[4]; };
extern const CardDef card_defs[];
extern const unsigned card_def_count;
extern const uint8_t card_faces[111][64*64];
enum Stage { Menu, Playing, Result };
struct Match {
 tt::RegionRuleSet rules;
 tt::Board board;
 tt::CardHand hands[2];
 tt::CardResourceLoader resources;
 tt::CPU_Player cpu;
 Stage stage=Menu;
 unsigned turn=0, mode=0, moves=0, captures=0, ai_moves=0, generation=0;
 unsigned score[2]={5,5};
 int winner=0;
 bool paused=false, background=false;
 uint32_t rng=1, thinking_ms=0;
 Match();
 uint32_t random();
 void start(uint32_t seed, unsigned rules_mode);
 bool place(unsigned player,unsigned hand_index,int x,int y);
 void tick(uint32_t milliseconds);
 void update_score();
 void flip(const nom::Point2i& p);
};
}
