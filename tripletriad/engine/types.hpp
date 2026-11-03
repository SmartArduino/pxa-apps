/******************************************************************************

  TTcards - Triple Triad remake

Copyright (c) 2013, 2014, 2015 Jeffrey Carpenter <i8degrees@gmail.com>
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
#ifndef TTCARDS_TYPES_HPP
#define TTCARDS_TYPES_HPP

#include <string>
#include <vector>

#include <nomlib/math/Color4.hpp>

namespace tt {

typedef nom::int32 CardID;

// NOTE: The player's index is variable and may well change at any given time;
// you should think of this as a stable array index and nothing more!
enum PlayerIndex: nom::uint32
{
  PLAYER_1 = 0,
  PLAYER_2 = 1,
  TOTAL_PLAYERS,
  PLAYER_INVALID,
};

// NOTE: Unique identifier for a player
enum PlayerID: nom::uint32
{
  PLAYER_ID_INVALID = 0,
  PLAYER_ID_1,
  PLAYER_ID_2,
  TOTAL_PLAYER_IDS,
};

/// \brief The type of player.
///
/// \see IPlayer::type
enum PlayerType: nom::uint32
{
  OBSERVER_PLAYER = 0,
  HUMAN_PLAYER,
  CPU_PLAYER,
};

/// \brief Card elemental types
enum CardElement: nom::uint32
{
  CARD_ELEMENT_NONE = 0,
  CARD_ELEMENT_EARTH,
  CARD_ELEMENT_FIRE,
  CARD_ELEMENT_HOLY,
  CARD_ELEMENT_ICE,
  CARD_ELEMENT_POISON,
  CARD_ELEMENT_THUNDER,
  CARD_ELEMENT_WATER,
  CARD_ELEMENT_WIND,
  TOTAL_CARD_ELEMENTS,
};

/// \brief Card type categories
enum CardType: nom::uint32
{
  CARD_TYPE_INVALID = 0,
  CARD_TYPE_MONSTER,
  CARD_TYPE_BOSS,
  CARD_TYPE_GF,
  CARD_TYPE_PLAYER,
  CARD_TYPE_CUSTOM,
  TOTAL_CARD_TYPES,
};

/// \brief Card rank indices.
enum CardRank: nom::uint32
{
  RANK_NORTH = 0,
  RANK_EAST,
  RANK_SOUTH,
  RANK_WEST,
  TOTAL_RANKS,
};

enum GameOverType: nom::uint32
{
  NotOver = 0,
  Tie,
  Won,
  Lost
};

enum BoardPosition: nom::int32
{
  TopLeft = 0,
  TopCenter,
  TopRight,
  MiddleLeft,
  MiddleCenter,
  MiddleRight,
  BottomLeft,
  BottomCenter,
  BottomRight
};

enum GameEvent: nom::int32
{
  AnimationEvent,
  DebugEvent,
  GUIEvent,
  AudioEvent,

  /// \remarks Starting event number for Game::State enum (to ensure unique
  /// code identifiers).
  NUM_EVENTS
};

// The gradient colors used to render card backgrounds.

const nom::Color4iColors CARD_BG_PLAYER1 = {{
  // light to dark blue
  nom::Color4i( 208, 223, 255 ), nom::Color4i( 50, 59, 114 )
}};

const nom::Color4iColors CARD_BG_PLAYER2 = {{
  // light to dark pink
  nom::Color4i( 251, 222, 232 ), nom::Color4i( 114, 66, 66 )
}};

const nom::Color4iColors CARD_BG_NO_PLAYER = {{
  // light to dark gray
  nom::Color4i( 197, 197, 197 ), nom::Color4i( 84, 84, 84 )
}};

typedef std::vector<std::string> string_list;

// CardsMenuState UI column titles
enum ColumnTitle
{
  CARDS_COLUMN_STATUS = 0,
  CARDS_COLUMN_TITLE,
  CARDS_COLUMN_NUM,
};

/// \brief The game cursor response choices.
///
/// \see tt::ConfirmationDialogState, ::on_resume
enum GameResponse: nom::uint32
{
  /// nullptr
  GAME_RESPONSE_NO = 0,
  GAME_RESPONSE_YES = 1,
  TOTAL_GAME_RESPONSES
};

} // namespace tt

#endif // defined include guard
