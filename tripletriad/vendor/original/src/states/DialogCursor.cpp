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
#include "DialogCursor.hpp"

namespace tt {

DialogCursor::DialogCursor() :
  cursor_position_(0)
{
  // NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE );
}

DialogCursor::~DialogCursor()
{
  // NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE );
}

void DialogCursor::set_position_map( const std::vector<nom::IntRect>& map )
{
  this->position_map_ = map;
  this->cursor_position_ = 0;
}

int DialogCursor::cursor_position()
{
  return this->cursor_position_;
}

int DialogCursor::first()
{
  return 0;
}

int DialogCursor::last()
{
  return this->position_map_.size();
}

bool DialogCursor::prev()
{
  int pos = this->cursor_position();          // Element position
  int first_pos = this->first();

  if( this->position().y > this->position_map_[first_pos].y ) {

    nom::Point2i offset;
    offset.x = this->position().x;
    offset.y = this->position().y - this->position_map_[pos].h;
    this->set_position(offset);

    --cursor_position_;
    return true;
  }

  // NOM_DUMP_VAR( TTCARDS_LOG_CATEGORY_TEST, this->position().x );
  NOM_DUMP_VAR( TTCARDS_LOG_CATEGORY_TEST, this->position().y );
  return false;
}

bool DialogCursor::next()
{
  int pos = this->cursor_position();          // Element position
  int last_pos = this->last() - 1;

  if( this->position().y < this->position_map_[last_pos].y ) {

    nom::Point2i offset;
    offset.x = this->position().x;
    offset.y = this->position().y + this->position_map_[pos].h;
    this->set_position(offset);

    ++cursor_position_;
    return true;
  }

  // NOM_DUMP_VAR( TTCARDS_LOG_CATEGORY_TEST, this->position().x );
  NOM_DUMP_VAR( TTCARDS_LOG_CATEGORY_TEST, this->position().y );
  return false;
}

void DialogCursor::set_cursor_position(int pos)
{
  NOM_ASSERT( pos <= this->last() );

  if( pos <= this->last() ) {
    this->cursor_position_ = pos;

    // Update rendering coordinates
    this->set_position( this->position_map_[pos].position() );
  }
}

} // namespace tt
