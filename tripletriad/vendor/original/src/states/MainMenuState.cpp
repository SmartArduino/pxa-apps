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
#include "MainMenuState.hpp"

// Private headers
#include "helpers.hpp"

// Forward declarations
#include "Game.hpp"

using namespace nom;

namespace tt {

MainMenuState::MainMenuState(nom::SDLApp* object) :
  nom::IState(Game::State::MainMenu),
  game( NOM_SCAST(Game*, object) )
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE_STATES);
}

MainMenuState::~MainMenuState()
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE_STATES);
}

void MainMenuState::on_init(nom::void_ptr data)
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE_STATES);

  nom::size_type menu_element = 0;

  if( this->game->player_deck_exists() == true ) {
    this->continue_game_ = true;
  }

  menu_element = this->append_menu_entry("Continue");

  if( this->continue_game_ == false ) {
    this->menu_entries_[menu_element].set_color(Color4i::LightGray);
  } else {
    this->menu_entries_[menu_element].set_color(Color4i::White);
  }

  this->append_menu_entry("New Game");
  this->append_menu_entry("View Cards");
  this->append_menu_entry("Options");
  this->append_menu_entry("Credits");
  this->append_menu_entry("Quit");

  this->update_menu_entries();

  // ...Initialize game cursor...

  IntRect cursor_pos(IntRect::zero);
  for(  auto cursor_index = 0;
        cursor_index != MenuEntry::TOTAL_MENU_ENTRIES;
        ++cursor_index )
  {
    Size2i menu_text_dims =
      this->menu_entries_[cursor_index].size();

    Point2i align_pos =
      nom::alignment_rect(  this->game->cursor_->size(), Point2i::zero,
                            GAME_RESOLUTION, nom::Anchor::MiddleCenter );

    cursor_pos.w = CURSOR_WIDTH;
    cursor_pos.h = CURSOR_HEIGHT;

    cursor_pos.x = align_pos.x - menu_text_dims.w;

    // Align roughly to the middle of each text entry
    cursor_pos.y =
      (align_pos.y + (menu_text_dims.h) * cursor_index);

    this->cursor_pos_bounds_.push_back(cursor_pos);
  }

  this->game->cursor_->set_position( this->cursor_pos_bounds_[0].position() );
  this->game->cursor_->set_sprite_sheet(this->game->right_cursor_frames_);
  this->game->cursor_->set_frame(INTERFACE_CURSOR_SHOWN);

  nom::InputActionMapper state;

  auto cursor_prev( [=](const nom::Event& evt) {
    this->cursor_prev();
  });

  auto cursor_next( [=](const nom::Event& evt) {
    this->cursor_next();
  });

  auto confirm_selection( [=](const nom::Event& evt) {
    this->on_confirm_selection(evt);
  });

  auto confirm_mouse_cursor_move( [=](const nom::Event& evt) {
    this->on_mouse_button_up(evt);
  });

  // ...Keyboard input mappings...

  state.insert("cursor_prev", nom::KeyboardAction(SDLK_UP), cursor_prev);
  state.insert("cursor_next", nom::KeyboardAction(SDLK_DOWN), cursor_next);
  state.insert( "confirm_selection",
                nom::KeyboardAction(SDLK_RETURN), confirm_selection );

  // ...Mouse wheel input mappings...

  state.insert( "cursor_prev",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_UP), cursor_prev );
  state.insert( "cursor_next",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_DOWN), cursor_next );

  state.insert( "confirm_mouse_cursor_move",
                nom::MouseButtonAction( nom::LEFT_MOUSE_BUTTON,
                nom::InputState::RELEASED ), confirm_mouse_cursor_move );

  // ...Joystick input mappings...
  auto& joystick_id = this->game->joystick_id_;

  state.insert( "cursor_prev",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_UP), cursor_prev );
  state.insert( "cursor_next", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_DOWN), cursor_next );
  state.insert( "confirm_selection",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_START), confirm_selection );

  this->game->input_mapper.erase("MainMenuState");
  this->game->input_mapper.insert("MainMenuState", state, true);
  this->game->input_mapper.activate_only("MainMenuState");
  this->game->input_mapper.activate("Game");
}

void MainMenuState::on_exit(nom::void_ptr data)
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE_STATES);

  this->game->actions_.cancel_actions();
}

void MainMenuState::on_pause(nom::void_ptr data)
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE_STATES);

  // Hide the cursor so that it doesn't show up during undesirable states such
  // as during the ConfirmationDialogState or Pause states.
  this->game->cursor_->set_frame(INTERFACE_CURSOR_HIDDEN);
}

void MainMenuState::on_resume(nom::void_ptr data)
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE_STATES);

  // Restore the rendering of the game cursor
  this->game->cursor_->set_frame(INTERFACE_CURSOR_SHOWN);

  // Ensure that the correct input contexts are reset
  this->game->input_mapper.activate_only("MainMenuState");
  this->game->input_mapper.activate("Game");
}

void MainMenuState::on_update(nom::real32 delta_time)
{
  this->update_cursor();

  this->game->window.update();
}

void MainMenuState::on_draw(nom::RenderWindow& target)
{
#if 1
  this->game->window.fill(nom::Color4i::Black);
#else
  this->game->gameover_background.draw(target);
#endif

  for(  auto itr = this->menu_entries_.begin();
        itr != this->menu_entries_.end();
        ++itr )
  {
    (itr)->second.draw(target);
  }

  TT_RENDER_SPRITE(this->game->cursor_);
}

// Private scope

bool MainMenuState::on_event(const nom::Event& evt)
{
  return false;
}

void MainMenuState::on_mouse_button_up(const nom::Event& evt)
{
  Point2i mouse_input(evt.mouse.x, evt.mouse.y);
  IntRect menu_text_bounds(IntRect::zero);

  for(  auto itr = this->menu_entries_.begin();
        itr != this->menu_entries_.end();
        ++itr )
  {
    menu_text_bounds.x = (itr)->second.position().x;
    menu_text_bounds.y = (itr)->second.position().y;
    menu_text_bounds.w = (itr)->second.size().w;
    menu_text_bounds.h = (itr)->second.size().h;

    if( menu_text_bounds.contains(mouse_input) ) {

      this->set_cursor_position( (itr)->first );
      this->game->cursor_move->Play();
      this->on_confirm_selection(evt);
      break;
    }
  }
}

void MainMenuState::on_confirm_selection(const nom::Event& evt)
{
  File fp;
  auto& paths = this->game->paths_;

  const real32 FADE_DURATION = 1.0f;
  int cursor_pos = this->cursor_position();

  auto p1_db = this->game->cards_db_[PlayerIndex::PLAYER_1].get();
  auto p2_db = this->game->cards_db_[PlayerIndex::PLAYER_2].get();

  switch(cursor_pos)
  {
    default:
    {
      NOM_ASSERT(cursor_pos != -1);
    } break;

    case MENU_ENTRY_CONTINUE:
    {
      if( this->continue_game_ == false ) {
        this->game->cursor_wrong->Play();
        break;
      }

      if( this->game->load_deck(p1_db, paths["PLAYER_DECK_PATH"]) == false ) {
        // Err; could not open or parse file?
        NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                      "Could not load the player's deck:",
                      paths["PLAYER_DECK_PATH"] );
        this->game->cursor_wrong->Play();
        break;
      }

      // TODO: Once we have game levels implemented, we can probably handle
      // a missing opponent deck by using the player's existing game state data
      // for the level?
      if( this->game->load_deck(p2_db, paths["OPPONENT_DECK_PATH"]) == false ) {
        // Err; could not open or parse file?
        NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                      "Could not load the opponent's deck:",
                      paths["OPPONENT_DECK_PATH"] );
        this->game->cursor_wrong->Play();
        break;
      }

      auto fade_transistion( [=]() {
        this->game->load_game_sfx->Play();
        this->game->set_state(Game::State::CardsMenu);
      });

      this->game->fade_window(  FADE_DURATION, Color4i::White,
                                Color4i::ALPHA_OPAQUE, SCREEN_RESOLUTION,
                                fade_transistion );
    } break;

    case MENU_ENTRY_NEW_GAME:
    {
      NOM_ASSERT( fp.exists(paths["CARDS_DB_PATH"]) == true );

      if( this->game->load_new_deck(p1_db, paths["CARDS_DB_PATH"]) == false ) {
        // Err; could not open or parse file?
        NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                      "Could not load the player's deck:",
                      paths["CARDS_DB_PATH"] );
        this->game->cursor_wrong->Play();
        break;
      }

      if( this->game->load_new_deck(p2_db, paths["CARDS_DB_PATH"]) == false ) {
        // Err; could not open or parse file?
        NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                      "Could not load the opponent's deck:",
                      paths["CARDS_DB_PATH"] );
        this->game->cursor_wrong->Play();
        break;
      }

      auto fade_transistion( [=]() {
        this->game->load_game_sfx->Play();
        this->game->set_state(Game::State::CardsMenu);
      });

      this->game->fade_window(  FADE_DURATION, Color4i::Black,
                                Color4i::ALPHA_TRANSPARENT,
                                SCREEN_RESOLUTION, fade_transistion );
    } break;

    case MENU_ENTRY_VIEW_CARDS:
    {
      // TODO: Implement!
      this->game->cursor_wrong->Play();
    } break;

    case MENU_ENTRY_OPTIONS:
    {
      // TODO: Implement!
      this->game->cursor_wrong->Play();
    } break;

    case MENU_ENTRY_CREDITS:
    {
      // TODO: Implement!
      this->game->cursor_wrong->Play();
    } break;

    case MENU_ENTRY_QUIT:
    {
      auto fade_transistion( [=]() {
        this->game->on_game_quit(evt);
      });

      this->game->fade_window(  FADE_DURATION, Color4i::Black,
                                nom::Color4i::ALPHA_TRANSPARENT,
                                SCREEN_RESOLUTION, fade_transistion );
    } break;
  }
}

nom::size_type
MainMenuState::append_menu_entry(const std::string& entry_text)
{
  nom::size_type num_entries = this->menu_entries_.size();

  this->menu_entries_[num_entries] = Text();
  this->menu_entries_[num_entries].set_font(&this->game->menu_font_);
  this->menu_entries_[num_entries].set_text_size(22);
  this->menu_entries_[num_entries].set_text(entry_text);

  return num_entries;
}

bool MainMenuState::update_menu_entries()
{
  // nom::size_type num_entries = this->menu_entries_.size();

  Point2i align_text_pos(Point2i::zero);
  Point2i text_offset(Point2i::zero);
  int idx = 0;
  for(  auto itr = this->menu_entries_.begin();
        itr != this->menu_entries_.end();
        ++itr )
  {
    if( idx > 0 ) {
      text_offset.y = text_offset.y + itr->second.size().h;
    } else {
      // text_offset.y = 0;
    }

    align_text_pos =
      nom::alignment_rect(  itr->second.size(), text_offset,
                            GAME_RESOLUTION, nom::Anchor::MiddleCenter );
    itr->second.set_position(align_text_pos);
    ++idx;
  }

  return true;
}

void MainMenuState::update_cursor()
{
  this->game->cursor_->set_frame(INTERFACE_CURSOR_SHOWN);
}

int MainMenuState::cursor_position()
{
  int pos = this->cursor_pos_;
  int result = -1;

  if( pos < MenuEntry::TOTAL_MENU_ENTRIES && pos >= 0 ) {
    result = pos;
  }

  return result;
}

void MainMenuState::set_cursor_position(int pos)
{
  if( pos < MenuEntry::TOTAL_MENU_ENTRIES && pos >= 0 ) {
    auto cursor_pos =
      this->cursor_pos_bounds_[pos].position();

    this->cursor_pos_ = pos;

    this->game->cursor_->set_position(cursor_pos);
  }
}

void MainMenuState::cursor_prev()
{
  int pos = this->cursor_pos_; // The card's position index
  Point2i move_to_offset(Point2i::zero);

  // Move up if the game cursor is before the first menu entry
  if( this->cursor_pos_ > 0 ) {

    --this->cursor_pos_;
    move_to_offset.x = 0;
    move_to_offset.y = -(this->cursor_pos_bounds_.at(pos).h);
    this->game->cursor_->translate(move_to_offset);

    this->game->cursor_move->Play();
  }
}

void MainMenuState::cursor_next()
{
  int pos = this->cursor_pos_; // The card's position index
  Point2i move_to_offset(Point2i::zero);

  // Move down if the game cursor is not at the last menu entry
  if( this->cursor_pos_ < (MenuEntry::TOTAL_MENU_ENTRIES - 1) ) {
    ++this->cursor_pos_;
    move_to_offset.x = 0;
    move_to_offset.y = this->cursor_pos_bounds_.at(pos).h;
    this->game->cursor_->translate(move_to_offset);

    this->game->cursor_move->Play();
  }
}

} // namespace tt
