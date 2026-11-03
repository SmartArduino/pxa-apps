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
#ifndef GAMEAPP_PLAYSTATE_HEADERS
#define GAMEAPP_PLAYSTATE_HEADERS

#include <string>
#include <memory>

#include <nomlib/graphics.hpp>
#include <nomlib/actions.hpp>
#include <nomlib/system.hpp>

#include "config.hpp"
#include "types.hpp"

namespace tt {

// Forward declarations
class Game;
class IPlayer;
struct Card;

class PlayState: public nom::IState
{
  public:
    PlayState(nom::SDLApp* object);
    virtual ~PlayState();

    void on_init(nom::void_ptr data) override;
    void on_exit(nom::void_ptr data) override;
    void on_pause(nom::void_ptr data) override;
    void on_resume(nom::void_ptr data) override;

    void on_update(float delta_time) override;
    void on_draw(nom::RenderWindow& target) override;

  private:
    /// \brief The default event handler for this state.
    bool on_event(const nom::Event& ev) override;

    /// \brief Method callback for mouse button actions.
    ///
    /// \see nom::InputMapper.
    void on_mouse_button_down(const nom::Event& ev);

    PlayerIndex player_turn() const;

    void set_player_turn(PlayerIndex player_id);

    // Helper method for incrementing to next player's turn
    void end_turn();

    // ...Helper methods for game cursor input...

    bool cursor_locked();
    void lock_cursor(bool state);
    void reset_cursor();
    void unlock_selected_card();
    void lock_selected_card();

    /// \brief Helper method for updating board with player's selected card.
    void move_to(const nom::Point2i& rel_board_pos);

    void
    flip_cards( const nom::Point2i& rel_board_pos,
                const std::function<void()>& on_completion_func );

    nom::uint32 cursor_position();

    void set_cursor_position(nom::uint32 cursor_pos);

    void move_cursor_left();
    void move_cursor_right();
    void move_cursor_up();
    void move_cursor_down();
    void update_cursor();

    /// Interface Helper method; shows Card's ID number in a message box for
    /// both cursor states; player's hand and placed board cards -- debug
    /// handling included.
    void on_update_info_dialogs();

    /// \brief Update the players' scoreboard.
    void update_score();

    bool save_game(const std::string& filename);
    bool load_game(const std::string& filename);

    Game* game;

    /// \brief Game players
    std::unique_ptr<IPlayer> players_[PlayerIndex::TOTAL_PLAYERS];

    /// \brief Rendering bounds for the starting origin of the game cursor.
    nom::Point2i player_cursor_coords_[PlayerIndex::TOTAL_PLAYERS];

    /// \brief Rendering bounds for the starting origin of the scoreboard.
    nom::Point2i player_scoreboard_[PlayerIndex::TOTAL_PLAYERS];

    /// \brief Rendering bounds for the game cursor.
    nom::Point2i cursor_bounds_[MAX_PLAYER_HAND];

    PlayerIndex player_turn_ = PlayerIndex::TOTAL_PLAYERS;

    enum CursorState
    {
      /// \brief Player hand
      PLAYER = 0,

      /// \brief Board positions
      BOARD
    };

    /// \brief Cursor state
    enum CursorState cursor_state_ = CursorState::PLAYER;

    /// locks cursor state to board placement
    bool cursor_locked_ = false;

    nom::uint32 last_delta_ = 0;

    /// \brief Simulates the CPU player spending time in thought about their
    /// next action.
    nom::real32 cpu_hand_delay_seconds_ = 0.0f;

    /// Debug option -- when toggled on, we are able to control both players.
    bool skip_turn_ = false;

    enum GameOverType gameover_state_;

    std::shared_ptr<nom::Sprite> move_card_up_sprite_;
    std::shared_ptr<nom::Sprite> flash_action_sprite_;

    std::shared_ptr<nom::Sprite> text_action_sprite_;
    std::shared_ptr<nom::Sprite> gameover_text_action_sprite_;

    typedef std::function<void(const Card& card)> move_card_up_action_callback;

    void
    move_card_up_action(  const nom::Point2i& rel_board_pos,
                          const move_card_up_action_callback&
                          on_completion_func );

    void check_gameover_conditions();

    std::shared_ptr<nom::IActionObject>
    create_gameover_text_action(  GameOverType type,
                                  const std::string& action_name );

    std::shared_ptr<nom::IActionObject>
    create_text_action(const std::shared_ptr<nom::Sprite>& sp);

    void initialize_cpu_player_turn();
};

// Convenience declarations for changing state
typedef std::unique_ptr<PlayState> PlayStatePtr;

} // namespace tt

#endif // include guard defined
