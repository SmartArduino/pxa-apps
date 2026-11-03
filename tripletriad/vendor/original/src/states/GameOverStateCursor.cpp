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
#include "GameOverStateCursor.hpp"

// Private headers
#include "GameOverState.hpp"

namespace tt {

GameOverStateCursor::GameOverStateCursor()
{
  // NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE );
}

GameOverStateCursor::~GameOverStateCursor()
{
  // NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE );
}

void GameOverStateCursor::set_position_map(CardHand* position)
{
  this->cards_ = position;
}

int GameOverStateCursor::cursor_position()
{
  int result = -1;
  if( this->cards_ != nullptr ) {
    result = this->cards_->position();
  }

  return result;
}

nom::int32 GameOverStateCursor::move_left()
{
  nom::Point2i offset( this->position() );

  if( this->position().x > tt::TOP_GAMEOVER_ORIGIN.x + (CARD_DIMS.w / 2) ) {
    offset.x -= CARD_DIMS.w;
    this->set_position(offset);

    this->previous();
  }

  return this->position().x;
}

nom::int32 GameOverStateCursor::move_right()
{
  nom::Point2i offset( this->position() );

  if( this->position().x < tt::TOP_GAMEOVER_ORIGIN.x +
      ( CARD_DIMS.w * ( this->cards_->size() - 1 ) ) )
  {
    offset.x += CARD_DIMS.w;
    this->set_position(offset);

    this->next();
  }

  return this->position().x;
}

void GameOverStateCursor::set_event_handler(nom::EventHandler& evt_handler)
{
  this->evt_handler_ = &evt_handler;
}

void GameOverStateCursor::next()
{
  if( this->cards_ != nullptr ) {
    this->cards_->next();

    nom::Event ev =
      nom::create_user_event(GameEvent::GUIEvent, nullptr, nullptr, 0);

    NOM_ASSERT(this->evt_handler_ != nullptr);
    this->evt_handler_->push_event(ev);
  }
}

void GameOverStateCursor::previous()
{
  if( this->cards_ != nullptr ) {
    this->cards_->previous();

    nom::Event ev =
      nom::create_user_event(GameEvent::GUIEvent, nullptr, nullptr, 0);

    NOM_ASSERT(this->evt_handler_ != nullptr);
    this->evt_handler_->push_event(ev);
  }
}

} // namespace tt
