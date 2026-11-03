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
#include "PauseState.hpp"

#include "helpers.hpp"

// Forward declarations
#include "Game.hpp"

using namespace nom;

namespace tt {

PauseState::PauseState(nom::SDLApp* object) :
  nom::IState( Game::State::Pause, nom::IState::Flags::BackRender, nom::IState::Type::Child),
  game( NOM_SCAST(Game*, object) )
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );
}

PauseState::~PauseState()
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );
}

void PauseState::on_init(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  if( this->game->pause_window_.set_context(&this->game->gui_window_) == false )
  {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION, "Could set GUI desktop." );
    // return false;
  }

  const auto GUI_PAUSE =
    this->game->res_cfg_->get_string("GUI_PAUSE");
  if( this->game->pause_window_.load_document_file(GUI_PAUSE) == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:", GUI_PAUSE );
    exit(NOM_EXIT_FAILURE);
    // return false;
  }

  // ...Render a darkened overlay for this state as a nice visual cue...
  this->overlay_sprite_ = nom::make_unique<Sprite>();
  if( this->overlay_sprite_ != nullptr ) {
    this->overlay_sprite_->init_with_color(Color4i::Black, SCREEN_RESOLUTION);
    this->overlay_sprite_->set_position(Point2i::zero);
    this->overlay_sprite_->set_color_blend_mode(BlendMode::BLEND_MODE_BLEND);
    this->overlay_sprite_->set_alpha(128); // 50% opacity
  }

  this->blink_update.start();

  nom::InputActionMapper state;

  auto pause_game( [=](const nom::Event& evt) {
    this->game->state()->pop_state(nullptr);
  });

  // ...Keyboard mappings ...
  state.insert("pause_game", nom::KeyboardAction(SDLK_p), pause_game);

  // ...Joystick mappings ...
  auto& joystick_id = this->game->joystick_id_;
  state.insert( "pause_game", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_START), pause_game );

  this->game->input_mapper.erase( "PauseState" );
  this->game->input_mapper.insert( "PauseState", state, true );
  this->game->input_mapper.activate_only( "PauseState" );
  this->game->input_mapper.activate( "Game" );

  // Disable all input contexts and leave access to only the global input
  // mappings

  // this->game->input_mapper.disable();
  // this->game->input_mapper.activate_only( "Game" );

  // Stop updating animations and do not show the game cursor
  this->game->actions_.pause();
  this->game->cursor_->set_frame(INTERFACE_CURSOR_HIDDEN);

  this->game->pause_window_.set_title_text("PAUSE");
  this->game->pause_window_.set_message_text( tt::version_string() );
  this->game->pause_window_.show();
}

void PauseState::on_exit(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  this->game->pause_window_.close();

  Rocket::Core::Factory::ClearStyleSheetCache();
  Rocket::Core::Factory::ClearTemplateCache();

  // Resume updating animations and show the game cursor
  this->game->cursor_->set_frame(INTERFACE_CURSOR_SHOWN);
  this->game->actions_.resume();
}

void PauseState::on_pause(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );
}

void PauseState::on_resume(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );
}

bool PauseState::on_event(const nom::Event& ev)
{
  return false;
}

void PauseState::on_update(nom::real32 delta_time)
{
  this->game->pause_window_.set_title_text("PAUSE");

  if( this->blink_update.ticks() > 800 ) {
    this->blink_update.stop();
    this->game->pause_window_.set_title_text("");
    this->blink_text = true;
  }

  this->game->gui_window_.update();

  this->game->window.update();
}

void PauseState::on_draw(nom::RenderWindow& target)
{
  TT_RENDER_SPRITE(this->overlay_sprite_);

  this->game->gui_window_.draw();

  if( this->blink_text == true ) {
    this->game->pause_window_.set_title_text("");
    this->blink_update.start();
    this->blink_text = false;
  }
}

} // namespace tt
