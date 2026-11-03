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
#ifndef GAMEAPP_GAMEOVER_STATE_CURSOR_HPP
#define GAMEAPP_GAMEOVER_STATE_CURSOR_HPP

#include <iostream>
#include <string>
#include <memory>

#include <nomlib/graphics.hpp>
#include <nomlib/math.hpp>
#include <nomlib/system.hpp>

#include "config.hpp"
#include "CardHand.hpp"

namespace tt {

class GameOverStateCursor: public nom::SpriteBatch
{
  public:
    GameOverStateCursor();

    virtual ~GameOverStateCursor();

    /// Set a new positioning object for this instance to use
    void set_position_map(CardHand* position);

    /// \brief Get the current position, relative to the card element (index) in
    /// the player's hand.
    int cursor_position();

    /// Move the cursor to the left.
    ///
    /// Returns the X coordinate position of the cursor after it has been moved.
    nom::int32 move_left();

    /// Move the cursor to the right.
    ///
    /// Returns the X coordinate position of the cursor after it has been moved.
    nom::int32 move_right();

    // NOTE: The event handler object **must** outlive the destruction of this
    // object!
    void set_event_handler(nom::EventHandler& evt_handler);

  private:
    void next();
    void previous();

    // Non-owned pointer
    CardHand* cards_;

    // Non-owned pointer
    nom::EventHandler* evt_handler_;
};

} // namespace tt

#endif // include guard defined
