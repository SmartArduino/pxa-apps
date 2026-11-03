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
#ifndef GAMEAPP_GAMEOVER_HEADERS
#define GAMEAPP_GAMEOVER_HEADERS

#include <memory>

#include <nomlib/graphics.hpp>
#include <nomlib/actions.hpp>
#include <nomlib/gui.hpp>
#include <nomlib/system.hpp>

#include "config.hpp"
#include "types.hpp"
#include "Card.hpp"
#include "GameOverStateCursor.hpp"

namespace tt {

const nom::Point2i TOP_GAMEOVER_ORIGIN =
  nom::Point2i( BOARD_ORIGIN_X - ( CARD_DIMS.w ),
                BOARD_ORIGIN_Y + ( CARD_DIMS.h / 3 ) );

const nom::Point2i BOTTOM_GAMEOVER_ORIGIN =
  nom::Point2i( BOARD_ORIGIN_X - ( CARD_DIMS.w ),
                BOARD_ORIGIN_Y + ( CARD_DIMS.h ) +
                ( CARD_DIMS.h / 2 ) + ( CARD_DIMS.h / 4 ) );

const nom::Point2i TOP_GAMEOVER_CURSOR_ORIGIN =
  nom::Point2i(TOP_GAMEOVER_ORIGIN.x, TOP_GAMEOVER_ORIGIN.y * 2);

// Forward declarations
class Game;

class GameOverState: public nom::IState
{
  public:
    GameOverState(nom::SDLApp* object, nom::void_ptr state);

    virtual ~GameOverState();

  private:
    /// \todo Change return type to bool
    void on_init(nom::void_ptr data);
    void on_exit(nom::void_ptr data);

    void on_pause(nom::void_ptr data);
    void on_resume(nom::void_ptr data);

    /// \brief The default event handler for this state.
    bool on_event(const nom::Event& ev) override;

    /// \brief Method callback for mouse button actions.
    ///
    /// \see nom::InputMapper.
    void on_mouse_button_down(const nom::Event& ev);

    /// \brief User events handler for this state.
    void on_user_event(const nom::Event& ev);

    void on_update(nom::real32 delta_time);
    void on_draw(nom::RenderWindow& target);

    std::shared_ptr<nom::IActionObject>
    create_scale_card_action(const std::shared_ptr<nom::Sprite>& sp);

    // Set the card rendering layout for the player and opponent
    bool update_player_positions();

    void play_gameover_animation( CardCollection* db, CardHand* phand,
                                  int card_pos );

    Game* game;

    nom::uint32 gameover_state_;

    // Input
    GameOverStateCursor cursor_;
    Card selected_card_;

    // Animations
    std::shared_ptr<nom::Sprite> flash_action_sprite_;
    std::shared_ptr<nom::Sprite> scale_action_sprite_;

    // nom::uint32 num_trade_cards_ = 0;
    // STUB
    nom::uint32 num_trade_cards_ = 1;
};

// Convenience declarations for changing state
typedef std::unique_ptr<GameOverState> GameOverStatePtr;

} // namespace tt

#endif // GAMEAPP_GAMEOVER_HEADERS defined
