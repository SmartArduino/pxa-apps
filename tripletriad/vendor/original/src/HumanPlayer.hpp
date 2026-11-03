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
#ifndef GAMEAPP_HUMAN_PLAYER_HPP
#define GAMEAPP_HUMAN_PLAYER_HPP

#include <nomlib/math/Point2.hpp>

#include "config.hpp"
#include "IPlayer.hpp"

namespace tt {

// Forward declarations
class CardHand;

class HumanPlayer: public IPlayer
{
  public:
    HumanPlayer(CardHand* hand);

    virtual ~HumanPlayer();

    virtual PlayerType type() const override;

    virtual PlayerID player_id() const override;
    virtual void set_player_id(PlayerID id) override;

    void update(nom::real32 delta_time) override;
    void draw(nom::IDrawable::RenderTarget& target) override;

  private:
    /// \remarks This object pointer is **not** owned by us; do not free.
    CardHand* hand_;

    /// \brief The unique identifier for the player.
    PlayerID player_id_;
};

} // namespace tt

#endif // GAMEAPP_PLAYER_HEADERS defined
