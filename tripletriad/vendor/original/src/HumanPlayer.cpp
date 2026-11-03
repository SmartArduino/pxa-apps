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
#include "HumanPlayer.hpp"

// Private headers
#include "CardRenderer.hpp"

// Forward declarations
#include "CardHand.hpp"

using namespace nom;

namespace tt {

HumanPlayer::HumanPlayer(CardHand* hand)
{
  //NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);

  this->hand_ = hand;
  this->player_id_ = PlayerID::PLAYER_ID_INVALID;

  NOM_ASSERT(this->hand_ != nullptr);
}

HumanPlayer::~HumanPlayer()
{
  // NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);
}

PlayerType HumanPlayer::type() const
{
  return PlayerType::HUMAN_PLAYER;
}

PlayerID HumanPlayer::player_id() const
{
  return this->player_id_;
}

void HumanPlayer::set_player_id(PlayerID id)
{
  this->player_id_ = id;
}

void HumanPlayer::update(nom::real32 delta_time)
{
  // Stub
}

void HumanPlayer::draw(nom::IDrawable::RenderTarget& target)
{
  NOM_ASSERT(this->hand_ != nullptr);
  if( this->hand_ == nullptr ) {
    return;
  }

  auto hand_idx = 0;
  nom::Point2i pos(nom::Point2i::zero);
  for( auto itr = this->hand_->begin(); itr != this->hand_->end(); ++itr ) {

    pos.x = this->position().x;
    pos.y = this->position().y + ( CARD_HEIGHT / 2 ) * hand_idx;

    if( this->hand_->position() == hand_idx) {
      pos.x -= 16;
    }

    ++hand_idx;

    auto card_renderer =
      itr->card_renderer;
    if( card_renderer != nullptr && card_renderer->valid() == true ) {
      // TODO: Update the element position only when we need to -- this will
      // help ease further integration of animations!
      card_renderer->set_position(pos);
      card_renderer->render(target);
    }
  } // end for this->hand loop
}

} // namespace tt
