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
#ifndef GAMEAPP_BOARD_TILE_HPP
#define GAMEAPP_BOARD_TILE_HPP

#include <iostream>
#include <string>

#include <nomlib/math.hpp>
#include <nomlib/system.hpp>

#include "config.hpp"
#include "Card.hpp"

namespace tt {

class BoardTile
{
  public:
    static const BoardTile null;

    /// Default constructor
    BoardTile();

    /// Destructor
    ~BoardTile();

    /// Construct a new board tile
    BoardTile(  const Card& card, const nom::IntRect& bounds,
                const nom::uint32 element );

    /// Getter for tile_card instance var
    const Card& tile ( void ) const;

    /// \brief Get the tile's X and Y coordinate.
    nom::Point2i position() const;

    /// Getter for tile_pos instance var
    const nom::IntRect& bounds ( void ) const;

    void set_position(const nom::Point2i& pos);

    void set_bounds(const nom::IntRect& bounds);

    /// Getter for tile_element instance var
    nom::uint32 element ( void ) const;

    void set_element(nom::uint32 element);

    /// Update an existing tile
    void set_tile(const Card& card);

  private:
    friend class Board;
    Card tile_card;
    nom::IntRect bounds_;
    nom::uint32 element_;
    // enum BoardPosition board_tile;
};

bool operator ==(const BoardTile& lhs, const BoardTile& rhs);
bool operator !=(const BoardTile& lhs, const BoardTile& rhs);

struct BoardResult
{
  BoardTile tile;
  nom::uint32 applied_ruleset = 0;
};

typedef std::vector<BoardTile> board_tiles;
typedef std::vector<BoardResult> board_tiles_result;

} // namespace tt

#endif // include guard defined
