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
#include "Card.hpp"

// Private headers
#include <nomlib/ptree/Value.hpp>

// Forward declarations
#include "CardRenderer.hpp"

using namespace nom;

namespace tt {

// Static initializations
Card Card::null = Card();

Card::Card() :
  id(BAD_CARD_ID),
  level(MIN_LEVEL),
  type(CARD_TYPE_INVALID),
  element(CARD_ELEMENT_NONE),
  ranks( {{MIN_RANK-1}} ),
  player_id(PlayerID::PLAYER_ID_INVALID),
  player_owner(PlayerID::PLAYER_ID_INVALID),
  num(0),
  face_down(false),
  card_renderer(nullptr),
  texture_path("\0")
{
  NOM_LOG_TRACE_PRIO( TTCARDS_LOG_CATEGORY_TRACE,
                      NOM_LOG_PRIORITY_VERBOSE );
}

Card::~Card()
{
  NOM_LOG_TRACE_PRIO( TTCARDS_LOG_CATEGORY_TRACE,
                      NOM_LOG_PRIORITY_VERBOSE );
}

nom::Value serialize_card(const Card& card)
{
  nom::Value card_obj;

  card_obj["id"] = card.id;
  card_obj["name"] = card.name;
  card_obj["level"] = card.level;
  card_obj["type"] = card.type;
  card_obj["element"] = card.element;
  card_obj["num"] = card.num;

  auto ranks = card.ranks;

  for( auto itr = ranks.begin(); itr != ranks.end(); ++itr ) {

    // NOTE: If we do not insert the array elements as an integer here,
    // nom::Value gets confused and assigns the values as boolean!
    uint32 attr = *itr;
    card_obj["ranks"].push_back(attr);
  }

  if( card.texture_path.length() > 0 ) {
    card_obj["texture"] = card.texture_path;
  }

  return card_obj;
}

Card deserialize_card(const nom::Value& obj)
{
  nom::Value ranks;
  Card result;
  std::string tex_path;

  result.id = obj["id"].get_int();
  result.name = obj["name"].get_string();
  result.level = obj["level"].get_int();
  result.type = obj["type"].get_int();
  result.element = obj["element"].get_int();

  ranks = obj["ranks"].array();

  nom::uint32 rank_idx = 0;
  for( auto rank = ranks.begin(); rank != ranks.end(); ++rank ) {

    NOM_ASSERT(ranks.size() == CardRank::TOTAL_RANKS);
    result.ranks[rank_idx] = rank->get_int();
    ++rank_idx;
  }

  result.num = obj["num"].get_int();
  result.texture_path = obj["texture"].get_string();

  return result;
}

std::ostream& operator <<(std::ostream& os, const Card& rhs)
{
  os  << "[name="
      << rhs.name
      << ", card_id="
      << rhs.id
      << ", element_id="
      << rhs.element
      << ", player_id="
      << rhs.player_id
      << ", player_owner="
      << rhs.player_owner
      << ", num="
      << rhs.num
      << ", texture_path="
      << rhs.texture_path
      << ", ranks="
      << rhs.ranks[RANK_NORTH]
      << ", "
      << rhs.ranks[RANK_EAST]
      << ", "
      << rhs.ranks[RANK_SOUTH]
      << ", "
      << rhs.ranks[RANK_WEST]
      << "] ";

  return os;
}

bool operator ==(const Card& lhs, const Card& rhs)
{
  return( lhs.id == rhs.id );
}

bool operator <(const Card& lhs, const Card& rhs)
{
  return( lhs.id < rhs.id );
}

bool operator >(const Card& lhs, const Card& rhs)
{
  return( rhs.id > lhs.id );
}

bool operator !=(const Card& lhs, const Card& rhs)
{
  return ! ( lhs == rhs );
}

bool operator <=(const Card& lhs, const Card& rhs)
{
  return ! (rhs < lhs );
}

bool operator >=(const Card& lhs, const Card& rhs)
{
  return ! ( lhs < rhs );
}

nom::int32 card_strength(const Card& rhs)
{
  nom::int32 total_strength_value = 0;

  total_strength_value += rhs.ranks[RANK_NORTH];
  total_strength_value += rhs.ranks[RANK_EAST];
  total_strength_value += rhs.ranks[RANK_WEST];
  total_strength_value += rhs.ranks[RANK_SOUTH];

  return total_strength_value;
}

bool strongest_card(const Card& lhs, const Card& rhs)
{
  nom::int32 lhs_total_strengths = 0;
  nom::int32 rhs_total_strengths = 0;

  lhs_total_strengths += lhs.ranks[RANK_NORTH];
  lhs_total_strengths += lhs.ranks[RANK_EAST];
  lhs_total_strengths += lhs.ranks[RANK_WEST];
  lhs_total_strengths += lhs.ranks[RANK_SOUTH];

  rhs_total_strengths += rhs.ranks[RANK_NORTH];
  rhs_total_strengths += rhs.ranks[RANK_EAST];
  rhs_total_strengths += rhs.ranks[RANK_WEST];
  rhs_total_strengths += rhs.ranks[RANK_SOUTH];

  if( rhs_total_strengths == lhs_total_strengths ) {
    return( rhs.id < lhs.id );
  } else {
    return( rhs_total_strengths < lhs_total_strengths );
  }
}

bool weakest_card(const Card& lhs, const Card& rhs)
{
  nom::int32 lhs_total_strengths = 0;
  nom::int32 rhs_total_strengths = 0;

  lhs_total_strengths += lhs.ranks[RANK_NORTH];
  lhs_total_strengths += lhs.ranks[RANK_EAST];
  lhs_total_strengths += lhs.ranks[RANK_WEST];
  lhs_total_strengths += lhs.ranks[RANK_SOUTH];

  rhs_total_strengths += rhs.ranks[RANK_NORTH];
  rhs_total_strengths += rhs.ranks[RANK_EAST];
  rhs_total_strengths += rhs.ranks[RANK_WEST];
  rhs_total_strengths += rhs.ranks[RANK_SOUTH];

  if( lhs_total_strengths == rhs_total_strengths ) {
    return( lhs.id < rhs.id );
  } else {
    return( lhs_total_strengths < rhs_total_strengths );
  }
}

} // namespace tt
