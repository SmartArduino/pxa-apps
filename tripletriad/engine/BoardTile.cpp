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
#include "BoardTile.hpp"

using namespace nom;

namespace tt {

// Static initialization
const BoardTile BoardTile::null = BoardTile();

BoardTile::BoardTile() :
  element_(::NONE)
{
  //NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);
}

BoardTile::~BoardTile()
{
  //NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);
}

BoardTile::BoardTile( const Card& card, const nom::IntRect& bounds,
                      nom::uint32 element ) :
  tile_card(card),
  bounds_(bounds),
  element_(element)
{
  //NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);
}

const Card& BoardTile::tile ( void ) const
{
  return this->tile_card;
}

Point2i BoardTile::position() const
{
  return this->bounds_.position();
}

const nom::IntRect& BoardTile::bounds() const
{
  return this->bounds_;
}

void BoardTile::set_position(const Point2i& pos)
{
  this->bounds_.x = pos.x;
  this->bounds_.y = pos.y;
  // this->bounds_.w = CARD_WIDTH;
  // this->bounds_.h = CARD_HEIGHT;
}

void BoardTile::set_bounds(const nom::IntRect& bounds)
{
  this->bounds_ = bounds;
}

nom::uint32 BoardTile::element() const
{
  return this->element_;
}

void BoardTile::set_element(nom::uint32 element)
{
  this->element_ = element;
}

void BoardTile::set_tile(const Card& card)
{
  this->tile_card = card;
}

bool operator ==(const BoardTile& lhs, const BoardTile& rhs)
{
  return  ( lhs.tile() == rhs.tile() )        &&
          ( lhs.bounds() == rhs.bounds() )    &&
          ( lhs.element() == rhs.element() );
          // ( lhs.board_tile == rhs.board_tile );
}

bool operator !=( const BoardTile& lhs, const BoardTile& rhs )
{
  return !( lhs == rhs );
}

} // namespace tt
