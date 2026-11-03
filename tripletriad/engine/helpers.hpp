/* SPDX-License-Identifier: MIT
 * Declarations for unchanged upstream helper functions in core_helpers.cpp.
 */
#pragma once
#include "Card.hpp"
#include "CardHand.hpp"
namespace tt {
PlayerID player_id(PlayerIndex);
PlayerID flip_player_id(PlayerIndex);
void set_card_id(Card&,PlayerID);
void increase_card_rank(CardRank,Card&);
void decrease_card_rank(CardRank,Card&);
}
