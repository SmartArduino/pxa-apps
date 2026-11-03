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
#ifndef GAMEAPP_CFG_HEADERS
#define GAMEAPP_CFG_HEADERS

#include <string>

#include <nomlib/config.hpp>
#include <nomlib/math.hpp>

#include <cassert>

#if defined(NOM_ASSERT) && ! defined(NDEBUG)
  #undef NOM_ASSERT
  #define NOM_ASSERT(expr) assert(expr)
#endif

#include "debug.hpp"

/// Scale factor of rendered graphics
///
/// Scale factor is used in computing width, height and origin positioning for
/// the game data used at the time of the compile.
///
/// It is also used to determine which game assets configuration file to use to
/// locate the resource files from.
///
/// If set to one (1), this will use the original resource data, which is quite
/// low in resolution (384x224).
///
/// If set to two (2), this will use the rescaled resource data (using Scale2X
/// algorithm), giving us a game resolution of 768x448. This is the preferred
/// default.
#define SCALE_FACTOR 2

namespace tt {

// Global configuration
const std::string APP_NAME = "TTcards";
const std::string APP_WINDOW_TITLE = "Triple Triad";
const nom::int32 SCREEN_WIDTH = 768;
const nom::int32 SCREEN_HEIGHT = 448;
const auto SCREEN_RESOLUTION =
  nom::Size2i(SCREEN_WIDTH, SCREEN_HEIGHT);

#if defined(SCALE_FACTOR) && SCALE_FACTOR == 1
  const nom::Size2i GAME_RESOLUTION = nom::Size2i(384,224);
#else
  const nom::Size2i GAME_RESOLUTION = nom::Size2i(768,448);
#endif

/// Invalid Card identifier -- NULL value, if you will.
const nom::int32 BAD_CARD_ID = -1;

/// Minimum number of cards a player can hold in a game round
const nom::int32 MIN_PLAYER_HAND = 0;

/// Maximum number of cards a player can hold in a game round
const nom::int32 MAX_PLAYER_HAND = 5;

// Board grid is a dynamically generated 2D array
const nom::int32 BOARD_GRID_WIDTH = 3;
const nom::int32 BOARD_GRID_HEIGHT = 3;
const nom::uint16 BOARD_COUNT_MAX = BOARD_GRID_WIDTH * BOARD_GRID_HEIGHT;

/// Sprite sheet: faces.json
const nom::int32 NOFACE_ID = 110;

/// Sprite sheet: cursors.json
const nom::int32 SHEET_CURSOR_NONE = 0;
const nom::int32 SHEET_CURSOR_LEFT = 1;
const nom::int32 SHEET_CURSOR_RIGHT = 3;

const nom::int32 INTERFACE_CURSOR_HIDDEN = 0;
const nom::int32 INTERFACE_CURSOR_SHOWN = 1;

/// Sprite sheet: menu_elements.json
const nom::int32 INTERFACE_MENU_ELEMENT = 0;
const nom::int32 INTERFACE_MENU_ELEMENT_USED = 1;
const nom::int32 INTERFACE_MENU_ELEMENT_PAGE_LEFT = 2;
const nom::int32 INTERFACE_MENU_ELEMENT_PAGE_RIGHT = 3;

/// Additional resource data; width, height, origin coordinates
const nom::int32 CARD_WIDTH = 64 * SCALE_FACTOR;
const nom::int32 CARD_HEIGHT = 64 * SCALE_FACTOR;
const nom::Size2i CARD_DIMS = nom::Size2i(CARD_WIDTH, CARD_HEIGHT);

const nom::int32 ELEMENT_WIDTH = 16 * SCALE_FACTOR;
const nom::int32 ELEMENT_HEIGHT = 16 * SCALE_FACTOR;

const nom::int32 CURSOR_WIDTH = 26 * SCALE_FACTOR;
const nom::int32 CURSOR_HEIGHT = 16 * SCALE_FACTOR;

// top-left of board grid
const nom::int32 BOARD_ORIGIN_X = 96 * SCALE_FACTOR;
const nom::int32 BOARD_ORIGIN_Y = 16 * SCALE_FACTOR;

const nom::int32 PLAYER2_ORIGIN_X = ( BOARD_ORIGIN_X / 2 ) - ( CARD_WIDTH / 2 ); // 16
const nom::int32 PLAYER2_ORIGIN_Y = BOARD_ORIGIN_Y; // 16
const nom::Point2i PLAYER2_ORIGIN =
  nom::Point2i(PLAYER2_ORIGIN_X, PLAYER2_ORIGIN_Y);

const nom::int32 PLAYER1_ORIGIN_X = ( BOARD_ORIGIN_X / 2 ) - ( CARD_WIDTH / 2 ) + BOARD_ORIGIN_X + ( CARD_WIDTH * 3 );
const nom::int32 PLAYER1_ORIGIN_Y = BOARD_ORIGIN_Y;
const nom::Point2i PLAYER1_ORIGIN =
  nom::Point2i(PLAYER1_ORIGIN_X, PLAYER1_ORIGIN_Y);

const nom::int32 CURSOR_ORIGIN_X = ( GAME_RESOLUTION.w - CURSOR_WIDTH ) / 2 ;
const nom::int32 CURSOR_ORIGIN_Y = ( GAME_RESOLUTION.h - CURSOR_HEIGHT ) / 2;

const nom::int32 PLAYER2_CURSOR_ORIGIN_X = BOARD_ORIGIN_X - PLAYER2_ORIGIN_X; // 80
const nom::int32 PLAYER2_CURSOR_ORIGIN_Y = BOARD_ORIGIN_Y + ( CARD_HEIGHT / 2 ); // 48

const nom::int32 PLAYER1_CURSOR_ORIGIN_X = BOARD_ORIGIN_X + ( CARD_WIDTH * 3 ) - ( CURSOR_WIDTH / 2 ); // 275
const nom::int32 PLAYER1_CURSOR_ORIGIN_Y = BOARD_ORIGIN_Y + ( CARD_HEIGHT / 2 ); // 48

// TODO (player scores):
//
// SCREEN_HEIGHT - 48
// 64 * 5
const nom::int32 PLAYER1_SCORE_ORIGIN_X = 320 * SCALE_FACTOR;
const nom::int32 PLAYER1_SCORE_ORIGIN_Y = 176 * SCALE_FACTOR;

const nom::int32 PLAYER2_SCORE_ORIGIN_X = 32 * SCALE_FACTOR;
const nom::int32 PLAYER2_SCORE_ORIGIN_Y = 176 * SCALE_FACTOR;

const nom::Point2i ELEMENT_ORIGIN =
  nom::Point2i(42 * SCALE_FACTOR, 5 * SCALE_FACTOR);

const nom::Point2i RANK_NORTH_ORIGIN =
  nom::Point2i(8 * SCALE_FACTOR, 3 * SCALE_FACTOR);

const nom::Point2i RANK_EAST_ORIGIN =
  nom::Point2i(12 * SCALE_FACTOR, 11 * SCALE_FACTOR);

const nom::Point2i RANK_SOUTH_ORIGIN =
  nom::Point2i(RANK_NORTH_ORIGIN.x, 20 * SCALE_FACTOR);

const nom::Point2i RANK_WEST_ORIGIN =
  nom::Point2i(4 * SCALE_FACTOR, RANK_EAST_ORIGIN.y);

const nom::Point2i PLAYER1_TRIAD_ORIGIN =
  nom::Point2i(320 * SCALE_FACTOR, 0 * SCALE_FACTOR);

const nom::Point2i PLAYER2_TRIAD_ORIGIN =
  nom::Point2i(40 * SCALE_FACTOR, 0 * SCALE_FACTOR);

} // namespace tt

#endif // GAMEAPP_CFG_HEADERS defined
