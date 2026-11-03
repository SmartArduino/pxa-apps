/******************************************************************************

  TTcards - Triple Triad remake

Copyright (c) 2013, 2014 Jeffrey Carpenter <i8degrees@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
#include "helpers.hpp"
using namespace nom;
namespace tt {

PlayerID player_id(PlayerIndex player_index)
{
  auto result = PlayerID::PLAYER_ID_INVALID;

  if( player_index == PlayerIndex::PLAYER_1 ) {
    result = PlayerID::PLAYER_ID_1;
  } else if( player_index == PlayerIndex::PLAYER_2 ) {
    result = PlayerID::PLAYER_ID_2;
  } else if( player_index == PlayerIndex::PLAYER_INVALID ) {
    result = PlayerID::PLAYER_ID_INVALID;
  } else {
    // NOTE: Invalid player!
    NOM_ASSERT_INVALID_PATH();
  }

  return result;
}

PlayerID flip_player_id(PlayerIndex player_index)
{
  auto result = PlayerID::PLAYER_ID_INVALID;

  result = NOM_SCAST(PlayerID, player_index + 1);

  if( result == PlayerID::TOTAL_PLAYER_IDS ) {
    // NOTE: Invalid player!
    NOM_ASSERT_INVALID_PATH();
  }

  return result;
}

void set_card_id(Card& card, PlayerID pid)
{
  if( card != Card::null ) {
    card.player_id = pid;
    card.player_owner = pid;
  }
}

void increase_card_rank(CardRank rank, Card& card)
{
  uint32 dir = (card.ranks[rank] + 1);
  card.ranks[rank] = nom::minimum(dir, MAX_RANK);
}

void decrease_card_rank(CardRank rank, Card& card)
{
  uint32 dir = (card.ranks[rank] - 1);
  card.ranks[rank] = nom::maximum(dir, MIN_RANK);
}

} // namespace tt
