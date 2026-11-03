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
#include "PlayState.hpp"

// Private headers
#include "CardRenderer.hpp"
#include "Board.hpp"
#include "helpers.hpp"

// Forward declarations
#include "Game.hpp"
#include "HumanPlayer.hpp"
#include "CPU_Player.hpp"
#include "Card.hpp"

// Create keyboard bindings for a game board position
#define CREATE_MOVE_TO_KEY_BINDING(var_name, x, y) \
  auto var_name( [=](const nom::Event& evt) { \
    auto player_turn = this->player_turn(); \
    if( this->game->debug_game_ == true ) { \
      this->move_to( nom::Point2i(x, y) ); \
    } else if(  this->game->debug_game_ == false && \
                player_turn != PlayerIndex::PLAYER_2 ) \
    { \
      this->move_to( nom::Point2i(x, y) ); \
    } \
  });

using namespace nom;

namespace tt {

PlayState::PlayState(nom::SDLApp* object) :
  nom::IState(Game::State::Play),
  game( NOM_SCAST(Game*, object) )
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );
}

PlayState::~PlayState()
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE_STATES);

  NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_TRACE_STATES, "num_actions:",
                  this->game->actions_.num_actions() );
}

void PlayState::on_exit(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  this->game->info_box_.close();
  this->game->debug_box_.close();

  Rocket::Core::Factory::ClearStyleSheetCache();
  Rocket::Core::Factory::ClearTemplateCache();

  this->game->actions_.cancel_actions();
}

void PlayState::on_pause(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  if( this->game->info_box_.enabled() == true ) {
    this->game->info_box_.hide();
  }

  if( this->game->debug_box_.enabled() == true ) {
    this->game->debug_box_.hide();
  }
}

void PlayState::on_resume(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  this->game->input_mapper.activate_only( "PlayState" );
  this->game->input_mapper.activate( "Game" );
}

void PlayState::on_init(nom::void_ptr data)
{
  uint16 platform_key_mod = KMOD_LCTRL;

  NOM_ASSERT(this->game != nullptr);

  this->game->board_.reset( new Board() );
  NOM_ASSERT(this->game->board_ != nullptr);

  this->gameover_state_ = GameOverType::NotOver;
  this->cursor_state_ = CursorState::PLAYER;
  this->skip_turn_ = false;
  this->player_turn_ = PlayerIndex::TOTAL_PLAYERS;
  this->cursor_locked_ = false;

  this->text_action_sprite_ =
    std::make_shared<Sprite>();
  NOM_ASSERT(text_action_sprite_ != nullptr);

  // ...Initialize player hands...

  auto& rules = this->game->rules_;
  auto& p1_hand = this->game->hand[PlayerIndex::PLAYER_1];
  auto& p2_hand = this->game->hand[PlayerIndex::PLAYER_2];

  CPU_Player::
  action_callback cpu_player_action_callback( [=](BoardTile& tile) {
    this->move_to( tile.bounds().position() );
  });

  if( tt::is_card_rule_set(&rules, CardRuleset::OPEN_RULESET) == false ) {
    // ...No peeking at the opponent's cards!!
    tt::set_face_down(&this->game->hand[PlayerIndex::PLAYER_2], true);
  } else {
    tt::set_face_down(&this->game->hand[PlayerIndex::PLAYER_2], false);
  }

  // ...Initialize game board...

  this->game->board_->initialize( &rules,
                                  this->game->card_res_.get() );

  this->game->cursor_->set_position( Point2i(PLAYER1_CURSOR_ORIGIN_X, PLAYER1_CURSOR_ORIGIN_Y) );
  this->game->cursor_->set_frame(INTERFACE_CURSOR_HIDDEN);

  this->players_[PlayerIndex::PLAYER_1].reset( new HumanPlayer(&this->game->hand[PlayerIndex::PLAYER_1]) );
  // this->players_[PlayerIndex::PLAYER_1].reset( new CPU_Player( CPU_Player::Difficulty::Easy,
  //                                   &this->game->board,
  //                                   &this->game->hand[PlayerIndex::PLAYER_1],
  //                                   cpu_player_action_callback ) );

  this->players_[PlayerIndex::PLAYER_1]->set_position( Point2i(PLAYER1_ORIGIN_X, PLAYER1_ORIGIN_Y) );

  this->players_[PlayerIndex::PLAYER_2].reset( new CPU_Player(  CPU_Player::Difficulty::Easy,
                                            this->game->board_.get(),
                                            &this->game->hand[PlayerIndex::PLAYER_2],
                                            cpu_player_action_callback ) );

  this->players_[PlayerIndex::PLAYER_2]->set_position( Point2i(PLAYER2_ORIGIN_X, PLAYER2_ORIGIN_Y) );

  // Starting origins for the game cursor
  this->player_cursor_coords_[PlayerIndex::PLAYER_1] =
    nom::Point2i(PLAYER1_CURSOR_ORIGIN_X, PLAYER1_CURSOR_ORIGIN_Y);

  this->player_cursor_coords_[PlayerIndex::PLAYER_2] =
    nom::Point2i(PLAYER2_CURSOR_ORIGIN_X, PLAYER2_CURSOR_ORIGIN_Y);

  // Starting origins for player scoreboard
  this->player_scoreboard_[PlayerIndex::PLAYER_1] =
    Point2i(PLAYER1_SCORE_ORIGIN_X, PLAYER1_SCORE_ORIGIN_Y);

  this->player_scoreboard_[PlayerIndex::PLAYER_2] =
    Point2i(PLAYER2_SCORE_ORIGIN_X, PLAYER2_SCORE_ORIGIN_Y);

  for( nom::int32 idx = 0; idx < MAX_PLAYER_HAND; idx++ ) {
    this->cursor_bounds_[idx] =
      nom::Point2i( idx, this->player_cursor_coords_[0].y + ( CARD_HEIGHT / 2 * idx ) );
  }

  // Northern message box

  if( this->game->debug_box_.set_context(&this->game->gui_window_) == false )
  {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION, "Could set GUI desktop." );
    // return false;
  }

  const auto GUI_DEBUG =
    this->game->res_cfg_->get_string("GUI_DEBUG");
  if( this->game->debug_box_.load_document_file(GUI_DEBUG) == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:", GUI_DEBUG );
    exit(NOM_EXIT_FAILURE);
    // return false;
  }

  this->game->debug_box_.show();

  if( this->game->debug_game_ == false ) {
    this->game->debug_box_.disable();
  }

  // Southern message box

  if( this->game->info_box_.set_context(&this->game->gui_window_) == false )
  {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION, "Could set GUI desktop." );
    // return false;
  }

  const auto GUI_MBOX =
    this->game->res_cfg_->get_string("GUI_MBOX");
  if( this->game->info_box_.load_document_file(GUI_MBOX) == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:", GUI_MBOX );
    exit(NOM_EXIT_FAILURE);
    // return false;
  }

  this->game->info_box_.show();

  // Update both player scores now that we have the player scoreboard X, Y
  // origins calculated for rendering.
  //
  // Assuming a player hand of five, this should always assign a starting score
  // value of 5 for each player initially.
  this->update_score();

  // Set whose turn it is initially using a random number generator with equal
  // odds -- 50/50 chance that you will have the first move!
  uint32 random_choice =
    nom::uniform_int_rand<nom::uint32>(0, PlayerIndex::TOTAL_PLAYERS - 1);

  PlayerIndex chosen_player = NOM_SCAST(PlayerIndex, random_choice);
#if 0
  this->set_player_turn(chosen_player);
#else
  this->set_player_turn(PlayerIndex::PLAYER_1);
#endif
  this->reset_cursor();

  if( this->player_turn() == PlayerIndex::PLAYER_2 ) {
    this->initialize_cpu_player_turn();
  }

  this->cpu_hand_delay_seconds_ =
    this->game->config_->get_real32("CPU_HAND_DELAY_SECONDS", 1.0f);

  // Convert to milliseconds
  this->cpu_hand_delay_seconds_ = this->cpu_hand_delay_seconds_ * 1000;

  // this->game->input_mapper.clear();
  nom::InputActionMapper state;

  // ...Keyboard mappings...

  CREATE_MOVE_TO_KEY_BINDING(move_to_1, 0, 0);
  CREATE_MOVE_TO_KEY_BINDING(move_to_2, 1, 0);
  CREATE_MOVE_TO_KEY_BINDING(move_to_3, 2, 0);
  CREATE_MOVE_TO_KEY_BINDING(move_to_4, 0, 1);
  CREATE_MOVE_TO_KEY_BINDING(move_to_5, 1, 1);
  CREATE_MOVE_TO_KEY_BINDING(move_to_6, 2, 1);
  CREATE_MOVE_TO_KEY_BINDING(move_to_7, 0, 2);
  CREATE_MOVE_TO_KEY_BINDING(move_to_8, 1, 2);
  CREATE_MOVE_TO_KEY_BINDING(move_to_9, 2, 2);

  state.insert("moveto_1", nom::KeyboardAction(SDLK_1), move_to_1);
  state.insert("moveto_2", nom::KeyboardAction(SDLK_2), move_to_2);
  state.insert("moveto_3", nom::KeyboardAction(SDLK_3), move_to_3);
  state.insert("moveto_4", nom::KeyboardAction(SDLK_4), move_to_4);
  state.insert("moveto_5", nom::KeyboardAction(SDLK_5), move_to_5);
  state.insert("moveto_6", nom::KeyboardAction(SDLK_6), move_to_6);
  state.insert("moveto_7", nom::KeyboardAction(SDLK_7), move_to_7);
  state.insert("moveto_8", nom::KeyboardAction(SDLK_8), move_to_8);
  state.insert("moveto_9", nom::KeyboardAction(SDLK_9), move_to_9);

  // Register event callbacks for our input actions
  auto unlock_card( [=](const nom::Event& evt) {
    this->unlock_selected_card();
  });

  auto lock_card( [=](const nom::Event& evt) {
    this->lock_selected_card();
  });

  auto select_card( [=](const nom::Event& evt) {
    this->on_mouse_button_down(evt);
  });

  auto move_cursor_up( [=](const nom::Event& evt) {
    this->move_cursor_up();
  });

  auto move_cursor_down( [=](const nom::Event& evt) {
    this->move_cursor_down();
  });

  auto move_cursor_left( [=](const nom::Event& evt) {
    this->move_cursor_left();
  });

  auto move_cursor_right( [=](const nom::Event& evt) {
    this->move_cursor_right();
  });

  auto pause_game( [=](const nom::Event& evt) {
    this->game->set_state( Game::State::Pause );
  });

  state.insert( "unlock_card", nom::KeyboardAction(SDLK_x),
                unlock_card );
  state.insert( "lock_card", nom::KeyboardAction(SDLK_SPACE),
                lock_card );

  state.insert( "move_cursor_up", nom::KeyboardAction(SDLK_UP),
                move_cursor_up );

  state.insert( "move_cursor_down",
                nom::KeyboardAction(SDLK_DOWN), move_cursor_down );

  state.insert( "move_cursor_left", nom::KeyboardAction(SDLK_LEFT),
                move_cursor_left );
  state.insert( "move_cursor_right", nom::KeyboardAction(SDLK_RIGHT),
                move_cursor_right );

  state.insert("pause_game", nom::KeyboardAction(SDLK_p), pause_game);

  // TODO: Declare a debug_state nom::InputMapper var so we can insert said
  // state only when the applicable debug flags are toggled on -- for sake of
  // cleanliness:
  //
  // debug_state.insert( "action", action, action_callback );
  // // ...and so on
  //
  // #if defined( TTCARDS_DEBUG_PLAY_STATE )
  //  this->game->input_mapper.insert( "DebugPlayState", debug_state, true );
  // #endif
  //
  if( this->game->debug_game_ == true ) {

    auto control_turn( [=](const nom::Event& evt) {
      // FIXME: Why are these inversed???
      this->skip_turn_ = true;
      this->end_turn();
    });

    auto skip_turn( [=](const nom::Event& evt) {
        // FIXME: Why are these inversed???
        this->skip_turn_ = false;
        this->end_turn();
    });

    auto delete_card( [=](const nom::Event& evt) {
      auto player_turn = this->player_turn();
      Point2i cursor_pos;
      cursor_pos.x = this->player_cursor_coords_[player_turn].x;
      cursor_pos.y = this->player_cursor_coords_[player_turn].y;

      Card selected_card = this->game->hand[player_turn].getSelectedCard();

      this->game->hand[player_turn].erase(selected_card);
      this->game->cursor_->set_position(cursor_pos);
    });

    state.insert( "control_turn", nom::KeyboardAction(SDLK_e, KMOD_LGUI),
                  control_turn );
    state.insert("skip_turn", nom::KeyboardAction(SDLK_e), skip_turn);
    state.insert("delete_card", nom::KeyboardAction(SDLK_d), delete_card);

    auto increase_north_rank( [=](const nom::Event& evt) {
      auto player_turn = this->player_turn();
      tt::modify_card_rank( this->game->card_res_.get(),
                            &this->game->hand[player_turn], true, RANK_NORTH );
    });

    auto decrease_north_rank( [=](const nom::Event& evt) {
      auto player_turn = this->player_turn();
      tt::modify_card_rank( this->game->card_res_.get(),
                            &this->game->hand[player_turn], false, RANK_NORTH );
    });

    auto increase_south_rank( [=](const nom::Event& evt) {
      auto player_turn = this->player_turn();
      tt::modify_card_rank( this->game->card_res_.get(),
                            &this->game->hand[player_turn], true, RANK_SOUTH );
    });

    auto decrease_south_rank( [=](const nom::Event& evt) {
      auto player_turn = this->player_turn();
      tt::modify_card_rank( this->game->card_res_.get(),
                            &this->game->hand[player_turn], false, RANK_SOUTH );
    });

    auto increase_west_rank( [=](const nom::Event& evt) {
      auto player_turn = this->player_turn();
      tt::modify_card_rank( this->game->card_res_.get(),
                            &this->game->hand[player_turn], true, RANK_WEST );
    });

    auto decrease_west_rank( [=](const nom::Event& evt) {
      auto player_turn = this->player_turn();
      tt::modify_card_rank( this->game->card_res_.get(),
                            &this->game->hand[player_turn], false, RANK_WEST );
    });

    auto increase_east_rank( [=](const nom::Event& evt) {
      auto player_turn = this->player_turn();
      tt::modify_card_rank( this->game->card_res_.get(),
                            &this->game->hand[player_turn], true, RANK_EAST );
    });

    auto decrease_east_rank( [=](const nom::Event& evt) {
      auto player_turn = this->player_turn();
      tt::modify_card_rank( this->game->card_res_.get(),
                            &this->game->hand[player_turn], false, RANK_EAST );
    });

    state.insert( "increase_north_rank", nom::KeyboardAction(SDLK_UP,
                  KMOD_LSHIFT), increase_north_rank );
    state.insert( "decrease_north_rank", nom::KeyboardAction(SDLK_UP,
                  KMOD_LCTRL), decrease_north_rank );

    state.insert( "increase_south_rank", nom::KeyboardAction(SDLK_DOWN,
                  KMOD_LSHIFT), increase_south_rank );
    state.insert( "decrease_south_rank", nom::KeyboardAction(SDLK_DOWN,
                  KMOD_LCTRL), decrease_south_rank );

    state.insert( "increase_west_rank", nom::KeyboardAction(SDLK_LEFT,
                  KMOD_LSHIFT), increase_west_rank );
    state.insert( "decrease_west_rank", nom::KeyboardAction(SDLK_LEFT,
                  KMOD_LCTRL), decrease_west_rank );

    state.insert( "increase_east_rank", nom::KeyboardAction(SDLK_RIGHT,
                  KMOD_LSHIFT), increase_east_rank );
    state.insert( "decrease_east_rank", nom::KeyboardAction(SDLK_RIGHT,
                  KMOD_LCTRL), decrease_east_rank );

    // TODO: Redeclare in game state initialization
    auto cfg = this->game->config_.get();
    auto& paths = this->game->paths_;

    auto debug_load_combo_rule( [=](const nom::Event& evt) mutable {
      const std::string SAVE_GAME_PATH =
        paths["DEBUG_DATA_DIR"] + "combo_rule.json";
      this->load_game(SAVE_GAME_PATH);
    });

    auto debug_load_same_rule( [=](const nom::Event& evt) mutable {
      const std::string SAVE_GAME_PATH =
        paths["DEBUG_DATA_DIR"] + "same_rule.json";
      this->load_game(SAVE_GAME_PATH);
    });

    auto debug_load_player_win( [=](const nom::Event& evt) mutable {
      const std::string SAVE_GAME_PATH =
        paths["DEBUG_DATA_DIR"] + "player_win.json";
      this->load_game(SAVE_GAME_PATH);
    });

    auto debug_load_player_lose( [=](const nom::Event& evt) mutable {
      const std::string SAVE_GAME_PATH =
        paths["DEBUG_DATA_DIR"] + "player_lose.json";
      this->load_game(SAVE_GAME_PATH);
    });

    auto debug_quick_load_game( [=](const nom::Event& evt) mutable {
      this->load_game(paths["SAVE_GAME_PATH"]);
    });

    auto debug_quick_save_game( [=](const nom::Event& evt) mutable {
      this->save_game(paths["SAVE_GAME_PATH"]);
    });

    // ...Game rules tests...
    state.insert( "debug_load_combo_rule", nom::KeyboardAction(SDLK_1,
                  platform_key_mod), debug_load_combo_rule );
    state.insert( "debug_load_same_rule", nom::KeyboardAction(SDLK_2,
                  platform_key_mod), debug_load_same_rule );

    state.insert( "debug_load_player_win", nom::KeyboardAction(SDLK_9,
                  platform_key_mod), debug_load_player_win );
    state.insert( "debug_load_player_lose", nom::KeyboardAction(SDLK_0,
                  platform_key_mod), debug_load_player_lose );

    // ...Game state save && load...
    state.insert( "debug_quick_save_game", nom::KeyboardAction(SDLK_s,
                  platform_key_mod), debug_quick_save_game );
    state.insert( "debug_quick_load_game", nom::KeyboardAction(SDLK_l,
                  platform_key_mod), debug_quick_load_game );
  } // end if DEBUG_GAME

  // ...Mouse button && wheel mappings...

  state.insert( "select_card", nom::MouseButtonAction(nom::LEFT_MOUSE_BUTTON,
                nom::InputState::RELEASED), select_card );
  state.insert( "select_card", nom::MouseButtonAction(nom::RIGHT_MOUSE_BUTTON,
                nom::InputState::RELEASED), select_card );

  state.insert( "move_cursor_up",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_UP),
                move_cursor_up );
  state.insert( "move_cursor_down",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_DOWN),
                move_cursor_down );

  // ...Joystick input mappings...
  auto& joystick_id = this->game->joystick_id_;

  state.insert( "unlock_card",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_B), unlock_card );

  state.insert( "lock_card",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_A), lock_card );

  state.insert( "move_cursor_up",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_UP), move_cursor_up );

  state.insert( "move_cursor_down",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_DOWN), move_cursor_down );

  state.insert( "move_cursor_left",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_LEFT), move_cursor_left );

  state.insert( "move_cursor_right",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_RIGHT), move_cursor_right );

  state.insert( "pause_game",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_START), pause_game );

  this->game->input_mapper.erase("PlayState");
  this->game->input_mapper.insert("PlayState", state, true );
  this->game->input_mapper.activate_only("PlayState");
  this->game->input_mapper.activate("Game");

  this->game->triad_->set_frame(0);
  this->game->actions_.run_action(this->game->triad_action_);

  // ...Initialization of animation sprites...

  this->flash_action_sprite_ = std::make_shared<Sprite>();
  if( this->flash_action_sprite_ != nullptr ) {
    this->flash_action_sprite_->init_with_color(Color4i::White, CARD_DIMS);
    this->flash_action_sprite_->set_alpha(Color4i::ALPHA_TRANSPARENT);
  }
}

// Private scope

bool PlayState::on_event(const nom::Event& ev)
{
  return false;
}

void PlayState::on_mouse_button_down(const nom::Event& ev)
{
  if( this->game->actions_.action_running("move_card_up") == true ) {
    return;
  }

  auto player_turn = this->player_turn(); // Ignore player2 mouse input

  // Player cursor positioning
  Point2i player_pos = this->players_[player_turn]->position();

  // mouse input coordinates
  Point2i mouse_input( ev.mouse.x, ev.mouse.y );

  // Rectangle bounds of player's card
  IntRect card_bounds;

  // Disable mouse input if we are not controlling the other player
  if( this->skip_turn_ == false ) {
    if( this->player_turn() != 0 ) {
      return;
    }
  }

  // Player hand selection checks; we must calculate the valid bounds of any
  // given card in the player's hand -- the current player position combined
  // with the known card size yields rectangular coordinates we can compare.
  for ( nom::int32 idx = 0; idx < this->game->hand[player_turn].size(); idx++ )
  {
    card_bounds = IntRect ( player_pos.x,
                            player_pos.y,
                            CARD_WIDTH,
                            ( CARD_HEIGHT / 2  ) * ( idx + 1 )
                          );

    // If the mouse coordinates match within the card bounds, we have a match!
    if( card_bounds.contains( mouse_input ) )
    {
      // 1. Update player's selected card
      // 2. Update cursor position
      // 3. Play sound event
      this->game->hand[player_turn].set_position(idx);

      this->game->cursor_->set_position( Point2i(this->player_cursor_coords_[ player_turn ].x, this->player_cursor_coords_[ player_turn ].y + ( CARD_HEIGHT / 2 ) * idx) );

      this->game->cursor_move->Play();

      // We must break the loop here upon the end of a matching coords check
      // in order to prevent a nasty "last card stays permanently selected"
      // bug from cropping back up!
      break;
    }
  }

  // Board grid coords check; player is attempting to place a card on the board
  // when the player hand coords check above comes back false
  IntRect mouse_map =
    this->game->board_->getGlobalBounds( ev.mouse.x, ev.mouse.y );

  // Attempts to move card onto board; validity checking is performed within
  // the following method call
  if( mouse_map != nom::IntRect::null ) {
    this->move_to( mouse_map.position() );
  }
}

PlayerIndex PlayState::player_turn() const
{
  return this->player_turn_;
}

void PlayState::set_player_turn(PlayerIndex player_index)
{
  this->player_turn_ = player_index;
}

void PlayState::end_turn()
{
  this->unlock_selected_card();

  this->game->hand[PlayerIndex::PLAYER_1].clearSelectedCard();
  this->game->hand[PlayerIndex::PLAYER_2].clearSelectedCard();

  if( this->player_turn() == PlayerIndex::PLAYER_1 ) {
    this->set_player_turn(PlayerIndex::PLAYER_2);

    this->initialize_cpu_player_turn();
  } else if( this->player_turn() == PlayerIndex::PLAYER_2 ) {
    this->set_player_turn(PlayerIndex::PLAYER_1);
  }

  this->reset_cursor();
}

void PlayState::on_update_info_dialogs()
{
  auto player_turn = this->player_turn();
  Card selected_card;
  nom::Point2i cursor_pos;
  nom::IntRect board_pos;
  std::stringstream os;

  cursor_pos = this->game->cursor_->position();
  board_pos = this->game->board_->getGlobalBounds(cursor_pos.x, cursor_pos.y);

  if( this->cursor_locked() == true ) {

    // ...Board select state...
    selected_card = this->game->board_->get(board_pos.x, board_pos.y);
  } else {
    // ...player hand select state...

    if( this->game->debug_game_ == true ) {
      // Watch both player's card data stream in debug builds
      selected_card = this->game->hand[player_turn].getSelectedCard();
    }
  }

  if( this->game->debug_game_ == true ) {

    // Additional card info
    int32 card_id = selected_card.id;
    int32 player_id = selected_card.player_id;
    int32 player_owner = selected_card.player_owner;

    os << card_id << "/" << player_id << " " << "[" << player_owner << "]";
  } else {
    selected_card = this->game->hand[PlayerIndex::PLAYER_1].getSelectedCard();
  }

  if( selected_card == Card::null ) {
    this->game->debug_box_.hide();
    this->game->info_box_.hide();
  } else {

    if( this->game->debug_game_ == true ) {
      this->game->debug_box_.show();
    }

    this->game->info_box_.show();

    std::string card_name = selected_card.name;

    // Northern info window (debug info)
    this->game->debug_box_.set_message_text( os.str() );

    // Southern info window (always shown)
    this->game->info_box_.set_message_text(card_name);
  }
}

bool PlayState::cursor_locked()
{
  return( this->cursor_locked_ == true );
}

void PlayState::lock_cursor(bool state)
{
  this->cursor_locked_ = state;
}

void PlayState::reset_cursor()
{
  Point2i cursor_pos(Point2i::zero);
  auto player_turn = this->player_turn();

  this->game->hand[player_turn].front();

  this->cursor_state_ = CursorState::PLAYER;

  // Only set the position of the game interface cursor for the opponent when
  // we are controlling him (debug builds)
  if( this->skip_turn_ == true ) {
    cursor_pos.x = this->player_cursor_coords_[player_turn].x;
    cursor_pos.y = this->player_cursor_coords_[player_turn].y;
  } else {
    cursor_pos.x = this->player_cursor_coords_[0].x;
    cursor_pos.y = this->player_cursor_coords_[0].y;
  }

  this->game->cursor_->set_position(cursor_pos);
}

void PlayState::unlock_selected_card()
{
  this->cursor_state_ = CursorState::PLAYER;

  this->reset_cursor();

  this->lock_cursor(false);

  this->game->cursor_cancel->Play();
}

void PlayState::lock_selected_card()
{
  nom::IntRect coords; // temp container var to hold cursor pos mapping coords

  this->cursor_state_ = CursorState::BOARD;

  if( this->cursor_locked() == false ) {

    if( this->player_turn() == 0 ) {
      Point2i cursor_pos;
      cursor_pos.x = CURSOR_ORIGIN_X-16;  // FIXME
      cursor_pos.y = CURSOR_ORIGIN_Y;
      this->game->cursor_->set_position(cursor_pos);
    } else if( this->player_turn() == 1 ) {
      Point2i cursor_pos;
      cursor_pos.x = CURSOR_ORIGIN_X+16;  // FIXME
      cursor_pos.y = CURSOR_ORIGIN_Y;
      this->game->cursor_->set_position(cursor_pos);
    }

    this->lock_cursor(true);
  } else {
    int cursor_x = this->game->cursor_->position().x;
    int cursor_y = this->game->cursor_->position().y;

    coords = this->game->board_->getGlobalBounds(cursor_x, cursor_y);
    if ( coords != nom::IntRect::null ) {
      this->move_to( coords.position() );
    }

    this->unlock_selected_card();
  }
}

void PlayState::move_to(const nom::Point2i& rel_board_pos)
{
  auto player_turn = this->player_turn();

  Card selected_card = this->game->hand[player_turn].getSelectedCard();
  if( selected_card.id == BAD_CARD_ID ) {
    NOM_LOG_ERR( TTCARDS, "Sanity check failed: the selected card is invalid!" );
    return; // Do not end turn
  }

  // Audible indicator that the move is **not** allowed
  if( this->game->board_->status(rel_board_pos) != BAD_CARD_ID ) {
    // Move is not allowed; card exists
    this->game->cursor_wrong->Play();
    return; // Do not end turn
  }

  if( this->game->actions_.action_running("move_card_up") == true ) {
    return;
  }

  if( this->game->board_->status(rel_board_pos) == BAD_CARD_ID ) {

    // ...Move is allowed...

    // Animation for the card leaving the player's hand and landing onto the
    // board
    this->move_card_up_action(rel_board_pos, [=](const Card& card) {

      Card selected_card = card;
      this->game->board_->update(rel_board_pos, selected_card);
      this->game->hand[player_turn].erase(selected_card);

      this->flip_cards(rel_board_pos, [=]() {
        this->end_turn();
      });
    }); // end move_card_up_action callback
  } // end if game board position is open
}

void
PlayState::flip_cards(  const nom::Point2i& rel_board_pos,
                        const std::function<void()>& on_completion_func )
{
  auto player_turn = this->player_turn();
  auto& rules = this->game->rules_;
  Point2i card_pos;

  // Check for flippable cards
  board_tiles_result grid =
    this->game->board_->check_board(rel_board_pos);
  for( auto itr = grid.begin(); itr != grid.end(); ++itr ) {

    int gpos0 = itr->tile.position().x;
    int gpos1 = itr->tile.position().y;
    Point2i gpos(gpos0, gpos1);
    uint32 applied_rule = itr->applied_ruleset;

    if( applied_rule == CardRuleset::SAME_RULESET ) {
      this->text_action_sprite_ = this->game->same_text_sprite_;
    } else {
      this->text_action_sprite_ = nullptr;
    }

    auto flip_text_action = create_text_action(this->text_action_sprite_);
    if( flip_text_action != nullptr ) {
      if( applied_rule == CardRuleset::SAME_RULESET ) {
        flip_text_action->set_name("same_text_action");
      }
    }

    if( this->text_action_sprite_ != nullptr ) {
      // Reset position for action to translate from
      nom::set_alignment( this->text_action_sprite_.get(), Point2i::zero,
                          GAME_RESOLUTION, nom::Anchor::MiddleRight );
    }

    this->game->actions_.run_action(flip_text_action, [=]() mutable {

      Card pcard =
        this->game->board_->get(gpos.x, gpos.y);
      if( pcard.card_renderer != nullptr ) {
        card_pos = pcard.card_renderer->position();
      }

      auto action =
        this->game->create_flip_card_action(this->flash_action_sprite_, card_pos);

      this->game->actions_.run_action(action);

      auto player_id = tt::flip_player_id(player_turn);
      this->game->board_->flip_card(gpos, player_id);

      this->update_score();
      this->game->card_flip->Play();

      if( tt::is_card_rule_set(&rules, CardRule::COMBO_RULE) == true ) {

        // Do a second round of flippable cards check for the COMBO rule-set
        board_tiles_result tgrid =
          this->game->board_->check_board(gpos);

        for( auto itr = tgrid.begin(); itr != tgrid.end(); ++itr ) {
          int tgpos0 = itr->tile.position().x;
          int tgpos1 = itr->tile.position().y;
          Point2i tgpos(tgpos0, tgpos1);
          // uint32 applied_rule = itr->applied_ruleset;

          this->text_action_sprite_ = this->game->combo_text_sprite_;

          auto combo_action = create_text_action(this->text_action_sprite_);
          if( combo_action != nullptr ) {
            combo_action->set_name("combo_text_action");
          }

          if( this->text_action_sprite_ != nullptr ) {
            // Reset position for action to translate from
            nom::set_alignment( this->text_action_sprite_.get(),
                                Point2i::zero, GAME_RESOLUTION,
                                nom::Anchor::MiddleRight );
          }

          this->game->actions_.run_action(combo_action, [=]() mutable {

            Card pcard =
              this->game->board_->get(tgpos.x, tgpos.y);
            if( pcard.card_renderer != nullptr ) {
              card_pos = pcard.card_renderer->position();
            }

            auto action =
              this->game->create_flip_card_action(this->flash_action_sprite_, card_pos);

            this->game->actions_.run_action(action);

            auto player_id = tt::flip_player_id(player_turn);
            this->game->board_->flip_card(tgpos, player_id);

            this->update_score();
            this->game->card_flip->Play();
          });
        } // end inner for loop (additional flips)
      } // end if combo rule is in effect
    }); // end outer callback
  } // end outer for loop (flippable cards)

  NOM_ASSERT(on_completion_func != nullptr);
  on_completion_func.operator()();
}

// TODO: In order to resolve the cursor / card selection synchronization issue,
// we need to ensure that we do **not** update the selection anytime this
// action is enqueued.
void
PlayState::move_card_up_action( const nom::Point2i& rel_board_pos,
                                const move_card_up_action_callback&
                                on_completion_func )
{
  auto player_turn = this->player_turn();

  this->game->actions_.cancel_action("move_card_up");

  Card pcard =
    this->game->hand[player_turn].getSelectedCard();

  auto card_renderer = pcard.card_renderer;

  this->move_card_up_sprite_ =
    card_renderer->rendered_card();

#if 1
  // FIXME:
  if( this->move_card_up_sprite_ == nullptr ||
      this->move_card_up_sprite_->valid() == false )
  {
    return;
  }
#else
  NOM_ASSERT(this->move_card_up_sprite_ != nullptr);
  NOM_ASSERT(this->move_card_up_sprite_->valid() == true);
#endif

  Point2i player_pos = this->players_[player_turn]->position();

  // We begin the animation by moving the selected card from the player's hand
  // upward, so we need to apply the offset that is rendered in IPlayer::draw

  // TODO: Perhaps we should just apply the rendering offset to the player's
  // position getter???
  player_pos.y +=
    (CARD_HEIGHT / 2 ) * this->game->hand[player_turn].position();
  if( player_turn == 0 ) {
    player_pos.x -= 16;
  } else if( player_turn == PlayerIndex::PLAYER_2 ) {
    player_pos.x += 16;
  }

  this->move_card_up_sprite_->set_position(player_pos);

  auto move_card_up =
    std::make_shared<MoveByAction>( this->move_card_up_sprite_,
                                    Point2i(0, -GAME_RESOLUTION.h), 1.0f);
  NOM_ASSERT(move_card_up != nullptr);
  move_card_up->set_timing_curve(nom::Expo::ease_out);

  // Global (screen) coordinates
  Point2i board_pos;
  board_pos.x =
    this->game->board_->tile(rel_board_pos.x, rel_board_pos.y).bounds().size().w;
  board_pos.y =
    this->game->board_->tile(rel_board_pos.x, rel_board_pos.y).bounds().size().h;

  // We must reverse the board position coordinates for 2, y and 0, y
  if( player_turn == PlayerIndex::PLAYER_1 ) {
    if( rel_board_pos.x == 2 ) {
      board_pos.x = BOARD_ORIGIN_X;
    } else if( rel_board_pos.x == 0 ) {
      board_pos.x = BOARD_ORIGIN_X + (CARD_WIDTH * 2);
    }
  }

  // The off-screen animation delta for moving along the X-axis
  Point2i delta(board_pos.x, 0);

  if( player_turn == PlayerIndex::PLAYER_1 ) {
    // Subtract away the player's card rendering offset; this undoes the
    // player_pos calculation done above to get us the actual board position
    delta.x -= 48;
    // We are going right to left here, from the player's origin point
    delta.x = -delta.x;
    delta.y = 0;
  }

  // The opponent requires no special delta calculation because we do not need
  // to compute the board position from right to left as we do with the player

  auto move_card_along =
    std::make_shared<MoveByAction>( this->move_card_up_sprite_,
                                    delta, 0.1f );
  NOM_ASSERT(move_card_along != nullptr);

  auto move_card_down =
    std::make_shared<MoveByAction>( this->move_card_up_sprite_,
                                    Point2i(0, board_pos.y), 0.3f);
  NOM_ASSERT(move_card_down != nullptr);
  move_card_down->set_timing_curve(nom::Expo::ease_out);

  auto move_card_sequence =
    nom::create_action<SequenceAction>( { move_card_up, move_card_along,
                                          move_card_down } );
  NOM_ASSERT(move_card_sequence != nullptr);
  move_card_sequence->set_name("move_card_up");

  this->game->card_place->Play();

  auto remove_card_up_action =
    nom::create_action<RemoveAction>(move_card_up);
  NOM_ASSERT(remove_card_up_action != nullptr);

  auto remove_card_along_action =
    nom::create_action<RemoveAction>(move_card_along);
  NOM_ASSERT(remove_card_along_action != nullptr);

  auto remove_card_down_action =
    nom::create_action<RemoveAction>(move_card_down);
  NOM_ASSERT(remove_card_down_action != nullptr);

  this->game->actions_.run_action(move_card_sequence, [=]() {

    this->game->actions_.run_action(remove_card_up_action);
    this->game->actions_.run_action(remove_card_along_action);
    this->game->actions_.run_action(remove_card_down_action);

    NOM_ASSERT(on_completion_func != nullptr);
    on_completion_func.operator()(pcard);
  });
}

// Helper method for obtaining card hand index position based off given origin
// coords definitions, creating us an ID map, initialized early on within the
// encapsulating class
//
// Works with both player1 and player2 coords mapping
//
// cursor_coords_map
//   [ index, y coordinate value ]
//
nom::uint32 PlayState::cursor_position()
{
  unsigned int pos = 0;
  nom::int32 idx = 0;

  for ( idx = 0; idx < MAX_PLAYER_HAND; idx++ )
  {
    if( this->game->cursor_->position().y <= this->cursor_bounds_[idx].y )
      return this->cursor_bounds_[idx].x;
    else // catch all safety switch
      // assume we are at the last position in the index when all else fails
      pos = MAX_PLAYER_HAND;
  }

  return pos;
}

void PlayState::set_cursor_position(nom::uint32 cursor_pos)
{
  // auto pturn = this->game->player_turn();
  Point2i render_pos;

  if( cursor_pos >= 0 && cursor_pos < MAX_PLAYER_HAND ) {
    this->game->hand[PlayerIndex::PLAYER_1].set_position(cursor_pos);

    render_pos.x = this->player_cursor_coords_[PlayerIndex::PLAYER_1].x;
    render_pos.y = this->player_cursor_coords_[PlayerIndex::PLAYER_1].y +
      (CARD_HEIGHT / 2) * cursor_pos;
    this->game->cursor_->set_position(render_pos);
  }
}

void PlayState::move_cursor_left()
{
  Point2i move_to_offset(Point2i::zero);

  // locked cursor to board select mode
  if( this->cursor_state_ == CursorState::BOARD ) {

    if( this->game->cursor_->position().x >
        BOARD_ORIGIN_X + (CARD_WIDTH * 1) )
    {
      move_to_offset.x = -(CARD_WIDTH);
      move_to_offset.y = 0;
      this->game->cursor_->translate(move_to_offset);
    }
  }

  this->game->cursor_move->Play();
}

void PlayState::move_cursor_right()
{
  Point2i move_to_offset(Point2i::zero);

  // locked cursor to board select mode
  if( this->cursor_state_ == CursorState::BOARD ) {

    if( this->game->cursor_->position().x <
        BOARD_ORIGIN_X + (CARD_WIDTH * 2) )
    {
      move_to_offset.x = CARD_WIDTH;
      move_to_offset.y = 0;
      this->game->cursor_->translate(move_to_offset);
    }
  }

  this->game->cursor_move->Play();
}

void PlayState::move_cursor_up()
{
  unsigned int pos = 0;
  unsigned int player_turn = this->player_turn();
  Point2i move_to_offset(Point2i::zero);

  if( this->game->actions_.action_running("move_card_up") ) {
    return;
  }

  if ( this->cursor_state_ == CursorState::PLAYER )
  {
    if( this->game->cursor_->position().y > PLAYER1_CURSOR_ORIGIN_Y ) {
      move_to_offset.x = 0;
      move_to_offset.y = -(CARD_HEIGHT / 2);
      this->game->cursor_->translate(move_to_offset);

      pos = this->cursor_position();
      this->game->hand[player_turn].previous();
    }
  }
  else if ( this->cursor_state_ == CursorState::BOARD ) // locked cursor to board select mode
  {
    if( this->game->cursor_->position().y >
        BOARD_ORIGIN_Y + ( CARD_HEIGHT * 1 ) )
    {
      move_to_offset.x = 0;
      move_to_offset.y = -(CARD_HEIGHT);
      this->game->cursor_->translate(move_to_offset);
    }
  }
  this->game->cursor_move->Play();
}

void PlayState::move_cursor_down()
{
  uint32 pos = 0;
  PlayerIndex player_turn = this->player_turn();
  Point2i move_to_offset(Point2i::zero);

  if( this->game->actions_.action_running("move_card_up") ) {
    return;
  }

  if ( this->cursor_state_ == CursorState::PLAYER )
  {
    if( this->game->cursor_->position().y <
        (CARD_HEIGHT / 2) * ( this->game->hand[player_turn].size() ) )
    {
      move_to_offset.x = 0;
      move_to_offset.y = (CARD_HEIGHT / 2);
      this->game->cursor_->translate(move_to_offset);

      pos = this->cursor_position();
      this->game->hand[player_turn].next();
    }
  }
  else if ( this->cursor_state_ == CursorState::BOARD ) // locked cursor to board select mode
  {
    if( this->game->cursor_->position().y <
        BOARD_ORIGIN_Y + ( CARD_HEIGHT * 2 ) )
    {
      move_to_offset.x = 0;
      move_to_offset.y = CARD_HEIGHT;
      this->game->cursor_->translate(move_to_offset);
    }
  }
  this->game->cursor_move->Play();
}

void PlayState::update_cursor()
{
  auto player_turn = this->player_turn();
  bool blinking_cursor_state =
    this->game->actions_.action_running("blinking_cursor_action");

  if( this->cursor_state_ == CursorState::BOARD ) {

    if( blinking_cursor_state == false ) {
      this->game->cursor_->set_frame(INTERFACE_CURSOR_HIDDEN);
      this->game->actions_.run_action(this->game->blinking_cursor_action_);
    }
  } else {  // this->cursor_state_ != CursorState::BOARD

    this->game->actions_.cancel_action("blinking_cursor_action");

    if( player_turn == PlayerIndex::PLAYER_1 ) {

      this->game->cursor_->set_sprite_sheet(this->game->right_cursor_frames_);
      this->game->cursor_->set_frame(INTERFACE_CURSOR_SHOWN);
    } else if(  this->skip_turn_ == true &&
                player_turn == PlayerIndex::PLAYER_2 )
    {
      this->game->cursor_->set_sprite_sheet(this->game->left_cursor_frames_);
      this->game->cursor_->set_frame(INTERFACE_CURSOR_SHOWN);
    }
  }
}

void PlayState::update_score()
{
  nom::uint32 board_count = 0;
  nom::uint32 hand_count = 0;
  uint32 score = 0;
  Point2i score_pos;
  std::string score_str;

  for(  nom::uint32 player_index = 0;
        player_index < PlayerIndex::TOTAL_PLAYERS;
        ++player_index )
  {
    auto player_id = tt::player_id( NOM_SCAST(PlayerIndex, player_index) );

    // Number of cards player owns on the board
    board_count = this->game->board_->getPlayerCount(player_id);

    // Number of cards player has remaining
    hand_count = this->game->hand[player_index].size();

    score = (board_count + hand_count);
    this->players_[player_index]->set_score(score);

    score_str = this->players_[player_index]->score_string();
    score_pos = this->player_scoreboard_[player_index];

    this->game->scoreboard_text[player_index].set_text(score_str);
    this->game->scoreboard_text[player_index].set_position(score_pos);
  }
}

void PlayState::on_update(nom::real32 delta_time)
{
  uint32 player_turn = this->player_turn();

  this->update_cursor();

  this->on_update_info_dialogs();
  this->game->gui_window_.update();

  this->players_[PlayerIndex::PLAYER_1]->update(delta_time);

  // CPU player's hand animation
  if( delta_time >= this->last_delta_ + this->cpu_hand_delay_seconds_ ) {
    last_delta_ = delta_time;

    // Only show the animation when we are not controlling him (debug builds)
    if( this->skip_turn_ == false ) {

      // Fixes a out of bounds issue that occurs occasionally upon state
      // phasing
      if( this->game->hand[PlayerIndex::PLAYER_2].size() > 0 ) {
        nom::uint32 rand_pick =
          nom::uniform_int_rand<uint32>(0, this->game->hand[PlayerIndex::PLAYER_2].size() - 1);
        this->game->hand[PlayerIndex::PLAYER_2].set_position(rand_pick);
      }
    }
  }

  if( player_turn == PlayerIndex::PLAYER_1 ) {

    this->game->triad_->set_position(PLAYER1_TRIAD_ORIGIN);
  } else if( player_turn == PlayerIndex::PLAYER_2 ) {

    this->game->triad_->set_position(PLAYER2_TRIAD_ORIGIN);

    // Skipping a turn like this is only available in debug versions
    if( this->skip_turn_ == false ) {

      if( this->game->actions_.action_running("cpu_move_delay") == false &&
          this->game->actions_.action_running("move_card_up") == false )
      {
        this->players_[PlayerIndex::PLAYER_2]->update(delta_time);
      }
    } // end if skip_turn_ == FALSE
  } else {
    NOM_ASSERT("PlayState::update(): player_turn > PlayerIndex::PLAYER_2");
  }

  this->game->window.update();

  this->check_gameover_conditions();
}

void PlayState::on_draw(nom::RenderWindow& target)
{
  this->game->background.draw ( target );

  this->game->board_->draw(target);

  this->players_[PlayerIndex::PLAYER_1]->draw(target);
  this->players_[PlayerIndex::PLAYER_2]->draw(target);

  this->game->triad_->draw(target);

  if( this->game->cursor_ != nullptr && this->game->cursor_->valid() ) {
    this->game->cursor_->draw(target);
  }

  // Draw each player's scoreboard
  this->game->scoreboard_text[PlayerIndex::PLAYER_1].draw(target);
  this->game->scoreboard_text[PlayerIndex::PLAYER_2].draw(target);

  TT_RENDER_ACTION(this->move_card_up_sprite_, "move_card_up");
  TT_RENDER_ACTION(this->flash_action_sprite_, "flip_card_action");
  TT_RENDER_ACTION(this->text_action_sprite_, "same_text_action");
  TT_RENDER_ACTION(this->text_action_sprite_, "combo_text_action");
  TT_RENDER_ACTION(this->gameover_text_action_sprite_, "gameover_action");

  this->game->gui_window_.draw();
}

bool PlayState::save_game(const std::string& filename)
{
  // auto cfg = this->game->config_.get();
  auto& paths = this->game->paths_;
  auto p1_db = this->game->cards_db_[PlayerIndex::PLAYER_1].get();
  auto p2_db = this->game->cards_db_[PlayerIndex::PLAYER_2].get();
  auto& p1_hand = this->game->hand[PlayerIndex::PLAYER_1];
  auto& p2_hand = this->game->hand[PlayerIndex::PLAYER_2];
  auto board = this->game->board_.get();

  if( this->game->save_deck(p1_db, paths["PLAYER_DECK_PATH"]) == false ) {
    this->game->cursor_wrong->Play();
    return false;
  }

  if( this->game->save_deck(p2_db, paths["OPPONENT_DECK_PATH"]) == false ) {
    this->game->cursor_wrong->Play();
    return false;
  }

  this->game->cursor_pos_ = this->cursor_position();

  if( this->game->save_player_hand( board, &p1_hand, &p2_hand,
                                    true, filename ) == false )
  {
    this->game->cursor_wrong->Play();
    return false;
  }

  // Success!
  this->game->save_game_sfx->Play();
  return true;
}

bool PlayState::load_game(const std::string& filename)
{
  // auto cfg = this->game->config_.get();
  auto& paths = this->game->paths_;
  auto p1_db = this->game->cards_db_[PlayerIndex::PLAYER_1].get();
  auto p2_db = this->game->cards_db_[PlayerIndex::PLAYER_2].get();
  auto& p1_hand = this->game->hand[PlayerIndex::PLAYER_1];
  auto& p2_hand = this->game->hand[PlayerIndex::PLAYER_2];
  auto board = this->game->board_.get();

  if( this->game->load_deck(p1_db, paths["PLAYER_DECK_PATH"]) == false ) {
    this->game->cursor_wrong->Play();
    return false;
  }

  if( this->game->load_deck(p2_db, paths["OPPONENT_DECK_PATH"]) == false ) {
    this->game->cursor_wrong->Play();
    return false;
  }

  if( this->game->load_player_hand( board, &p1_hand, &p2_hand,
                                    true, filename ) == false )
  {
    this->game->cursor_wrong->Play();
    return false;
  }

  // FIXME (?):
  // if( this->player_turn() == PlayerIndex::PLAYER_2 ) {
  //   this->initialize_cpu_player_turn();
  // }

  auto cursor_pos = this->game->cursor_pos_;
  this->set_cursor_position(cursor_pos);

  // Success!
  this->update_score();

  this->game->load_game_sfx->Play();
  return true;
}

void PlayState::check_gameover_conditions()
{
  auto board_count = this->game->board_->getCount();
  NOM_ASSERT(board_count <= BOARD_COUNT_MAX);

  auto player1_score = this->players_[PlayerIndex::PLAYER_1]->score();
  auto player1_num_cards = this->game->hand[PlayerIndex::PLAYER_1].size();

  auto player2_score = this->players_[PlayerIndex::PLAYER_2]->score();
  auto player2_num_cards = this->game->hand[PlayerIndex::PLAYER_2].size();

  auto& rules = this->game->rules_;
  bool sudden_death_rule_applied =
    tt::is_card_rule_set(&rules, CardRuleset::SUDDEN_DEATH_RULESET);

  if( this->game->actions_.action_running("move_card_up") == true ) {
    return;
  }

  if( this->game->actions_.action_running("flip_card_action") == true ) {
    return;
  }

  if( this->game->actions_.action_running("same_text_action") == true ) {
    return;
  }

  if( this->game->actions_.action_running("combo_text_action") == true ) {
    return;
  }

  if( board_count == BOARD_COUNT_MAX || player1_num_cards == 0 ||
      player2_num_cards == 0 )
  {
    if( this->gameover_state_ != GameOverType::NotOver ) {

      if( this->game->actions_.action_running("fade_window_out_action") == true ) {
        return;
      }

      this->game->window.fill(Color4i::Black);
      this->game->window.update();

      this->game->actions_.cancel_actions();

      if( this->gameover_state_ == GameOverType::Tie &&
          sudden_death_rule_applied == false )
      {
        this->game->set_state(Game::State::GameOver, &this->gameover_state_);
      } else {
        // TODO: Implement CardRules::SuddenDeath
        this->game->set_state(Game::State::GameOver, &this->gameover_state_);
      }
    }

    if( this->game->actions_.action_running("gameover_action") == true ) {
      return;
    }

    const real32 GAME_OVER_FADE_OUT_DURATION =
      this->game->config_->get_real32("GAME_OVER_FADE_OUT_DURATION", 1.5f);

    if( player1_score > player2_score ) {

      auto gameover_text_action =
        this->create_gameover_text_action(GameOverType::Won, "gameover_action");

      if( this->game->winning_track->getStatus() != Playing ) {
        this->game->theme_track_->Stop();
        this->game->winning_track->Play();
      }

      this->game->actions_.run_action(gameover_text_action, [=]() {
        this->gameover_state_ = GameOverType::Won;

        this->game->fade_window(  GAME_OVER_FADE_OUT_DURATION, Color4i::Black,
                                  nom::Color4i::ALPHA_TRANSPARENT,
                                  SCREEN_RESOLUTION );
      });
    } else if( player1_score < player2_score ) {

      auto gameover_text_action =
        this->create_gameover_text_action(GameOverType::Lost, "gameover_action");

      this->game->actions_.run_action(gameover_text_action, [=]() {
        this->gameover_state_ = GameOverType::Lost;

        this->game->fade_window(  GAME_OVER_FADE_OUT_DURATION, Color4i::Black,
                                  nom::Color4i::ALPHA_TRANSPARENT,
                                  SCREEN_RESOLUTION );
      });
    } else {

      auto gameover_text_action =
        this->create_gameover_text_action(GameOverType::Tie, "gameover_action");

      this->game->actions_.run_action(gameover_text_action, [=]() {
        this->gameover_state_ = GameOverType::Tie;
        this->game->fade_window(  GAME_OVER_FADE_OUT_DURATION, Color4i::Black,
                                  nom::Color4i::ALPHA_TRANSPARENT,
                                  SCREEN_RESOLUTION );
      });
    }
  } // end game over conditions check
}

std::shared_ptr<nom::IActionObject>
PlayState::create_gameover_text_action( GameOverType type,
                                        const std::string& action_name )
{
  const real32 GAMEOVER_TEXT_FADE_DURATION =
    this->game->config_->get_real32("GAMEOVER_TEXT_FADE_DURATION", 1.0f);

  auto transition_delay_action =
    std::make_shared<WaitForDurationAction>(GAMEOVER_TEXT_FADE_DURATION);
  NOM_ASSERT(transition_delay_action != nullptr);

  if( type == GameOverType::Won ) {
    this->gameover_text_action_sprite_ = this->game->won_text_sprite_;
  } else if( type == GameOverType::Lost ) {
    this->gameover_text_action_sprite_ = this->game->lost_text_sprite_;
  } else if( type == GameOverType::Tie ) {
    this->gameover_text_action_sprite_ = this->game->tied_text_sprite_;
  } else {
    this->gameover_text_action_sprite_ = nullptr;
  }

  // Reset the action's internal state
  if( this->gameover_text_action_sprite_ != nullptr ) {
    this->gameover_text_action_sprite_->set_alpha(Color4i::ALPHA_TRANSPARENT);
  }

  auto gameover_text_fade_in_action =
    std::make_shared<FadeInAction>( this->gameover_text_action_sprite_,
                                    GAMEOVER_TEXT_FADE_DURATION );
  NOM_ASSERT(gameover_text_fade_in_action != nullptr);

  nom::action_list sequence_list = {  gameover_text_fade_in_action,
                                      transition_delay_action };

  auto gameover_text_action =
    nom::create_action<SequenceAction>(sequence_list);
  NOM_ASSERT(gameover_text_action != nullptr);
  gameover_text_action->set_name(action_name);

  return gameover_text_action;
}

std::shared_ptr<nom::IActionObject>
PlayState::create_text_action(const std::shared_ptr<nom::Sprite>& sp)
{
  const auto TEXT_ACTION_TIMING_CURVE_STR =
    this->game->config_->get_string("TEXT_ACTION_TIMING_CURVE");
  const auto TEXT_ACTION_TIMING_CURVE =
    nom::make_timing_curve_from_string(TEXT_ACTION_TIMING_CURVE_STR);
  Point2i delta(Point2i::zero);

  if( sp != nullptr ) {
    delta.x = -( (GAME_RESOLUTION.w - sp->size().w) / 2);
  }

  auto text_action0 =
    nom::create_action<MoveByAction>(sp, delta, 0.5f);

  auto text_action1 =
    nom::create_action<WaitForDurationAction>(0.5f);

  auto text_action2 =
    nom::create_action<MoveByAction>(sp, delta, 0.5f);

  action_list seq_list = {text_action0, text_action1, text_action2};

  auto flip_text_action =
    nom::create_action<SequenceAction>(seq_list);
  if( flip_text_action != nullptr ) {
    flip_text_action->set_timing_curve(TEXT_ACTION_TIMING_CURVE);
  }

  return flip_text_action;
}

void PlayState::initialize_cpu_player_turn()
{
  real32 cpu_move_delay_seconds = 0.0f;

  if( this->game->debug_game_ == true ) {
    cpu_move_delay_seconds = 2.0f;
  } else {
    cpu_move_delay_seconds =
      this->game->config_->get_real32("CPU_MOVE_DELAY_SECONDS", 4.0f);
  }

  auto cpu_move_delay_timer =
    std::make_shared<WaitForDurationAction>(cpu_move_delay_seconds);
  NOM_ASSERT(cpu_move_delay_timer != nullptr);
  cpu_move_delay_timer->set_name("cpu_move_delay");

  if( cpu_move_delay_timer != nullptr ) {
    this->game->actions_.run_action(cpu_move_delay_timer);
  }
}

} // namespace tt
