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
#ifndef GAMEAPP_BOARD_HEADERS
#define GAMEAPP_BOARD_HEADERS

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <fstream>

#include <nomlib/config.hpp>
#include <nomlib/math.hpp>

#include "config.hpp"
#include "BoardTile.hpp"
#include "Card.hpp"

namespace tt {

// Forward declarations
struct RegionRuleSet;
class CardResourceLoader;

/// \brief The game board
///
/// \note A new renderer for the card is generated on every call to ::update.
class Board
{
  public:
    /// \brief Default constructor.
    ///
    /// \see ::initialize.
    Board();

    /// \brief Destructor.
    ~Board();

    /// \brief Initialize the game board with a rule set and card rendering
    /// components.
    ///
    /// \remarks The game board has two prerequisite dependencies before
    /// successful construction -- both the card rule-sets and card rendering
    /// components must be fully constructed.
    bool
    initialize(tt::RegionRuleSet* ruleset, CardResourceLoader* res);

    /// \brief Empty the board of its stored cards.
    void clear();

    /// Translates local board coordinates into positioning coordinates used by
    /// display rendering (units are naturally in pixels) -- usefulness ranges
    /// from collision inputs detection to interface cursor navigation.
    ///
    /// Input x, y arguments is in local board grid coordinates:
    ///
    ///   0, 0    1, 0    2, 0
    ///   0, 1    1, 1    2, 1
    ///   0, 2    1, 2    2, 2
    ///
    /// Returns a nom::IntRect filled with the global coordinate values; the
    /// width and height member values are the bounding boxes of each local grid
    /// coordinate. The x & y member values are the input grid coordinates.
    ///
    /// Returns -1, -1 when undefined.
    const nom::IntRect getGlobalBounds ( nom::int32 x, nom::int32 y ) const;

    board_tiles find_adjacent(nom::int32 x, nom::int32 y) const;

    board_tiles_result check_board(const nom::Point2i& rel_board_pos);

    /// Getter helper method for obtaining total count of placed cards on board
    nom::uint32 getCount ( void );

    /// get card count by player's cards on board
    nom::uint32 getPlayerCount(PlayerID player_id);

    /// Getter helper method for obtaining card ID at x, y coords
    CardID getStatus ( nom::int32 x, nom::int32 y ) const;

    CardID status(const nom::Point2i& rel_board_pos) const;

    /// \brief Helper method for placing a card on the board.
    ///
    /// \note A new card renderer is generated.
    ///
    /// \see ::getGlobalBounds
    void update(const nom::Point2i& grid_pos, Card& pcard);

    /// \brief ...
    ///
    /// \param
    ///
    /// \remarks ...
    bool update(const Cards& cards);

    /// Getter helper method for obtaining player owner tag / ID on a card at x, y
    /// coords
    PlayerID getPlayerID(nom::int32 x, nom::int32 y) const;

    /// \brief Helper method for swapping player owner tags / IDs on a card.
    void flip_card(const nom::Point2i& rel_board_pos, PlayerID player_id);

    /// (Private) Getter helper method for obtaining card placed at x, y coords;
    /// Used within Board::Draw(), Game::showCardInfoBox() method calls
    const Card& get(nom::int32 x, nom::int32 y) const;

    const BoardTile& tile(nom::int32 x, nom::int32 y) const;

    /// Draws our active board grid based on their values (card IDs)
    void draw(nom::IDrawable::RenderTarget& target);

    /// \brief Get the unoccupied tiles on the board.
    board_tiles free_tiles() const;

    /// \brief Diagnostic output of the current board tiles.
    void dump_values();

  private:
    void initialize_board_elements();

    /// \brief The current game rules in effect.
    ///
    /// \note This is a non-owned pointer, so we **must not** free it!
    tt::RegionRuleSet* rules_;

    /// \brief Card rendering.
    ///
    /// \note This is a non-owned pointer, so we **must not** free it!
    CardResourceLoader* card_res_;

    /// 2D vector of BoardTiles
    ///
    /// \todo Re-implement this as a one-dimensional array.
    std::vector<std::vector<BoardTile>> grid;

    /// \brief The board's local grid positions mapped to screen coordinates.
    nom::IntRect board_map_[9];

    /// \brief The board's local grid positions mapped to screen coordinates
    /// with the whole rectangle bounds of the position for ease of collision
    /// detection with mouse input.
    nom::IntRect board_mouse_map_[9];
};

nom::Value
serialize_board(const Board* board);

tt::Cards
deserialize_board(const nom::Value& objects);

} // namespace tt

#endif // GAMEAPP_BOARD_HEADERS defined
