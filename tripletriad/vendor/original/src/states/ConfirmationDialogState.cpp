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
#include "ConfirmationDialogState.hpp"

// Forward declarations
#include "Game.hpp"

using namespace nom;

namespace tt {

ConfirmationDialogState::ConfirmationDialogState(nom::SDLApp* object) :
  nom::IState( Game::State::ConfirmationDialog, nom::IState::Flags::BackRender, nom::IState::Type::Child ),
  game( NOM_SCAST(Game*, object) )
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );
}

ConfirmationDialogState::~ConfirmationDialogState()
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );
}

void ConfirmationDialogState::on_init( nom::void_ptr data )
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  if( this->game->question_box_.set_context(&this->game->gui_window_) == false )
  {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION, "Could set GUI desktop." );
    // return false;
  }

  const auto GUI_QBOX =
    this->game->res_cfg_->get_string("GUI_QBOX");
  if( this->game->question_box_.load_document_file(GUI_QBOX) == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource file:", GUI_QBOX );
    exit(NOM_EXIT_FAILURE);
    // return false;
  }

  this->game->question_box_.set_title_text("CHOICE");

  this->game->question_box_.show();

  this->cursor_.set_texture(this->game->cursor_tex_);
  this->cursor_.set_sprite_sheet(this->game->right_cursor_frames_);
  this->cursor_.set_frame(INTERFACE_CURSOR_SHOWN);

  // Build offset coordinate map for the game cursor; this is necessary for
  // syncing key, mouse wheel and joystick input.
  Point2i cursor_offset;
  Rocket::Core::ElementList tags;
  Rocket::Core::Element* top =
    this->game->question_box_.document()->GetElementById("content");

  NOM_ASSERT(top != nullptr);
  if( top != nullptr ) {
    Rocket::Core::ElementUtilities::GetElementsByTagName(tags, top, "answer");

    // NOM_DUMP( tags.size() );
    Rocket::Core::Vector2f position;
    Rocket::Core::Vector2f size;
    IntRect row;
    std::vector<IntRect> cursor_map;
    for(auto itr = tags.begin(); itr != tags.end(); ++itr)
    {
      // NOM_DUMP( (*itr)->GetId().CString() );

      position = (*itr)->GetAbsoluteOffset(Rocket::Core::Box::PADDING);
      size = (*itr)->GetBox().GetSize(Rocket::Core::Box::PADDING);

      row.x = position.x - this->cursor_.size().w;
      row.y = position.y + (size.y / 2);
      row.w = size.x;
      row.h = size.y;
      cursor_map.push_back(row);

      NOM_DUMP_VAR(TTCARDS_LOG_CATEGORY_TEST, "position_map:", row);
    }

    this->cursor_.set_position_map(cursor_map);

    // Starting origin for game cursor
    this->cursor_.set_position( cursor_map[0].position() );
  }

  // Starting game cursor offset is on the 'no' element
  this->cursor_.set_cursor_position(1);

  nom::InputActionMapper state;

  // FIXME:
  // nom::InputActionMapper key_bindings, gamepad_bindings;

  auto cursor_prev( [=](const nom::Event& evt) {
    if( this->cursor_.prev() ) {
      this->game->cursor_move->Play();
    }
  });

  auto cursor_next( [=](const nom::Event& evt) {
    if( this->cursor_.next() ) {
      this->game->cursor_move->Play();
    }
  });

  auto select( [=](const nom::Event& evt) {
    this->send_response();
  });

  auto cancel( [=](const nom::Event& evt) {
    this->game->state()->pop_state(nullptr);
  });

  // Equivalent to 'cursor_prev' and 'cursor_next' actions
  auto mouse_click( [=](const nom::Event& evt) {
    this->on_mouse_button_up(evt);
  });

  // Equivalent to 'select' action
  auto mouse_select( [=](const nom::Event& evt) {
    this->on_mouse_button_dblclick(evt);
  });

  // ...Keyboard mappings...

  state.insert("cursor_prev", nom::KeyboardAction(SDLK_UP), cursor_prev);
  state.insert("cursor_next", nom::KeyboardAction(SDLK_DOWN), cursor_next);
  state.insert("select", nom::KeyboardAction(SDLK_SPACE), select);
  state.insert("select", nom::KeyboardAction(SDLK_RETURN), select);
  state.insert("cancel", nom::KeyboardAction(SDLK_ESCAPE), cancel);

  // ...Mouse button && wheel mappings...
  state.insert( "click", nom::MouseButtonAction(nom::LEFT_MOUSE_BUTTON,
                nom::InputState::RELEASED), mouse_click );
  state.insert( "select", nom::MouseButtonAction(nom::LEFT_MOUSE_BUTTON, 2,
                nom::InputState::RELEASED), mouse_select );

  state.insert( "cursor_prev",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_UP), cursor_prev );
  state.insert( "cursor_next",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_DOWN), cursor_next );

  // ...Joystick input mappings...
  auto& joystick_id = this->game->joystick_id_;

  state.insert( "cursor_prev",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_UP), cursor_prev );

  state.insert( "cursor_next", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_DOWN), cursor_next );

  state.insert( "select", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_A), select );

  state.insert( "cancel", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_B), cancel );

  this->game->input_mapper.erase( "ConfirmationDialogState" );
  this->game->input_mapper.insert( "ConfirmationDialogState", state, true );

  // FIXME:
  // this->game->input_mapper.insert( "ConfirmationDialogState", key_bindings, true );
  // this->game->input_mapper.insert( "ConfirmationDialogState", gamepad_bindings, true );

  this->game->input_mapper.activate_only("ConfirmationDialogState");
  this->game->input_mapper.activate("Game");
}

void ConfirmationDialogState::on_exit( nom::void_ptr data )
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  this->game->question_box_.close();
  Rocket::Core::Factory::ClearStyleSheetCache();
  Rocket::Core::Factory::ClearTemplateCache();
}

void ConfirmationDialogState::on_resume( nom::void_ptr data )
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  this->game->input_mapper.activate_only( "ConfirmationDialogState" );
  this->game->input_mapper.activate( "Game" );
}

void ConfirmationDialogState::on_mouse_button_up(const nom::Event& ev)
{
  using namespace Rocket::Core;

  Element* yes_response =
    this->game->question_box_.document()->GetElementById("yes");
  Element* no_response =
    this->game->question_box_.document()->GetElementById("no");

  NOM_ASSERT( yes_response != nullptr );
  NOM_ASSERT( no_response != nullptr );

  if( yes_response == nullptr || no_response == nullptr ) return;

  Vector2f mouse_coords(ev.mouse.x, ev.mouse.y);

  if( yes_response->IsPointWithinElement(mouse_coords) ) {
    if( this->cursor_.prev() ) {
      this->game->cursor_move->Play();
    }
  }
  else if( no_response->IsPointWithinElement(mouse_coords) ) {
    if( this->cursor_.next() ) {
      this->game->cursor_move->Play();
    }
  }
}

void ConfirmationDialogState::on_mouse_button_dblclick(const nom::Event& ev)
{
  using namespace Rocket::Core;

  Element* yes_response =
    this->game->question_box_.document()->GetElementById("yes");
  Element* no_response =
    this->game->question_box_.document()->GetElementById("no");

  NOM_ASSERT( yes_response != nullptr );
  NOM_ASSERT( no_response != nullptr );

  if( yes_response == nullptr || no_response == nullptr ) return;

  Vector2f mouse_coords(ev.mouse.x, ev.mouse.y);

  if( yes_response->IsPointWithinElement(mouse_coords) ) {
    this->send_response();
    return;
  }

  if( no_response->IsPointWithinElement(mouse_coords) ) {
    this->send_response();
    return;
  }
}

void ConfirmationDialogState::on_update( float delta_time )
{
  this->game->gui_window_.update();

  this->game->window.update();
}

void ConfirmationDialogState::on_draw( nom::RenderWindow& target )
{
  this->game->gui_window_.draw();

  this->cursor_.draw(target);
}

void ConfirmationDialogState::send_response()
{
  // We will use the positioning of the cursor to map user's response;
  //
  // When the choice is valid, the response is the cursor position plus one,
  // otherwise the response is invalid and will be zero (nullptr).
  //
  // The state that receives the response will be the state that was in
  // existence directly before ConfirmationDialogState, and must be captured in
  // ::on_resume.
  int choice = this->cursor_.cursor_position();
  int32_ptr response = nullptr;

  if( choice >= 0 && choice <= this->cursor_.last() ) {
    response = new nom::int32(choice+1);
  }
  else {  // Invalid response
    this->game->cursor_wrong->Play();
  }

  this->game->state()->pop_state(response);
}

} // namespace tt
