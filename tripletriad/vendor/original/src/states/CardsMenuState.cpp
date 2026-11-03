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
#include "CardsMenuState.hpp"

// Private headers
#include "helpers.hpp"
#include "CardRenderer.hpp"

// Forward declarations
#include "Game.hpp"

using namespace nom;

namespace tt {

CardsMenuState::CardsMenuState(nom::SDLApp* object) :
  nom::IState( Game::State::CardsMenu ),
  game( NOM_SCAST(Game*, object) )
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );
}

CardsMenuState::~CardsMenuState()
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );
}

void CardsMenuState::on_init(nom::void_ptr data)
{
  uint16 platform_key_mod = KMOD_LCTRL;

  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  this->cursor_state_ = 0;

  auto& p1_hand = this->game->hand[PlayerIndex::PLAYER_1];
  auto& p2_hand = this->game->hand[PlayerIndex::PLAYER_2];

  p1_hand.clear();
  p2_hand.clear();

  // ...Initialize game rules...

  auto& rules = this->game->rules_;
  auto cfg = this->game->config_.get();
  this->game->init_game_rules(cfg, rules);

  this->game->hand[PlayerIndex::PLAYER_1].init( this->game->card_res_.get(),
                                                PlayerIndex::PLAYER_1 );
  this->game->hand[PlayerIndex::PLAYER_2].init( this->game->card_res_.get(),
                                                PlayerIndex::PLAYER_2 );

  auto p1_db = this->game->cards_db_[PlayerIndex::PLAYER_1].get();
  auto p2_db = this->game->cards_db_[PlayerIndex::PLAYER_2].get();
  NOM_ASSERT(p1_db != nullptr);
  NOM_ASSERT(p2_db != nullptr);

  for( auto itr = p2_db->begin(); itr != p2_db->end(); ++itr ) {

    if( this->game->debug_game_ == false ) {
      itr->face_down = true;
    } else {
      itr->face_down = false;
    }
  }

  while( p2_hand.size() < MAX_PLAYER_HAND ) {
    p2_hand.add_random_card(1, 10, p2_db);
  }

  // ...Initialize opponent's rendering position -- to the left of player 1...
  nom::size_type hand_idx = 0;
  Point2i p2_pos(Point2i::zero);
  for( auto card = p2_hand.begin(); card != p2_hand.end(); ++card ) {

    p2_pos.x = PLAYER2_ORIGIN_X;
    p2_pos.y = PLAYER2_ORIGIN_Y + (CARD_HEIGHT / 2) * hand_idx;
    ++hand_idx;

    auto card_renderer =
      card->card_renderer;
    if( card_renderer != nullptr && card_renderer->valid() == true ) {
      card_renderer->set_position(p2_pos);
    }
  }

  if( this->game->cards_page_model_ == nullptr ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Failed to initialize the cards data source UI." );
    exit(NOM_EXIT_FAILURE);
    // return false;
  }

  if( this->game->cards_menu_.set_context(&this->game->gui_window_) == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could set GUI desktop." );
    exit(NOM_EXIT_FAILURE);
    // return false;
  }

  const auto GUI_CARDS_MENU =
    this->game->res_cfg_->get_string("GUI_CARDS_MENU");
  if( this->game->cards_menu_.load_document_file(GUI_CARDS_MENU) == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:",
                  GUI_CARDS_MENU );
    exit(NOM_EXIT_FAILURE);
    // return false;
  }

  // Create the card pages with the active player's card deck
  Cards deck;
  for( auto itr = p1_db->begin(); itr != p1_db->end(); ++itr ) {
    if( (itr)->num > 0 ) {
      deck.push_back(*itr);
    }
  }

  // ...Initialize card data for UI presentation...

  this->game->cards_page_model_->erase_cards();
  this->update_page_count_title(0);

  this->game->cards_page_model_->insert_cards(0, deck);

  // Commit the card data for rendering as early as we can, so the player has
  // minimal latency
  this->game->gui_window_.update();

  NOM_LOG_DEBUG( TTCARDS_LOG_CATEGORY_CARDS_MENU_STATE,
                  "---Cards page model---",
                  this->game->cards_page_model_->dump() );

  // Event listener for mouse button clicks

  // TODO: This is currently a leaked resource -- no body is freeing it, and it
  // is our responsibility! I'm not sure whose responsibility it should be,
  // though -- should it be nom::UIEventListener, nom::UIWidget or
  // application-level..?
  this->game->cards_menu_.register_event_listener(
    this->game->cards_menu_.document(), "mouseup",
    new nom::UIEventListener( [&] ( Rocket::Core::Event& ev ) {
      this->on_mouse_button_up(ev);
    })
  );

  // Build offset coordinate map for the game cursor; this is necessary for
  // syncing key, mouse wheel and joystick input.
  Rocket::Core::ElementList tags;
  Rocket::Core::Element* top =
    this->game->cards_menu_.document()->GetElementById("content");

  NOM_ASSERT(top != nullptr);
  if( top != nullptr ) {
    Rocket::Core::ElementUtilities::GetElementsByTagName(tags, top, "datagridrow");

    // NOM_DUMP( tags.size() );
    Rocket::Core::Vector2f position;
    Rocket::Core::Vector2f size;
    IntRect row;
    for(auto itr = tags.begin(); itr != tags.end(); ++itr)
    {
      // NOM_DUMP( (*itr)->GetTagName().CString() );

      position = (*itr)->GetAbsoluteOffset(Rocket::Core::Box::PADDING);
      size = (*itr)->GetBox().GetSize(Rocket::Core::Box::PADDING);

      row.x = position.x - this->game->cursor_->size().w;
      row.y = position.y + this->game->cursor_->size().h / 2;
      row.w = size.x;

      // This value correlates with the line-height RCSS property value in
      // gui/dataview.rml and must **not** be fractional, or else we accumulate
      // rounding err with each cursor offset row that results in mis-placement
      // of the cursor. This can ultimately break its intended Y-axis bounds.
      row.h = size.y;

      this->cursor_coords_map_.push_back(row);

      NOM_DUMP_VAR(TTCARDS_LOG_CATEGORY_TEST, "cursor_coords_map_:", row);

      // Check the value of row.h to ensure non-fractional
      NOM_DUMP_VAR(TTCARDS_LOG_CATEGORY_TEST, "datagridrow:", position.x, ",", position.y, ",", size.x, ",", size.y );
    }
  }

  // Starting origin for game cursor
  this->game->cursor_->set_position( this->cursor_coords_map_[0].position() );
  this->game->cursor_->set_sprite_sheet(this->game->right_cursor_frames_);
  this->game->cursor_->set_frame(INTERFACE_CURSOR_SHOWN);

  nom::InputActionMapper state;

  auto cursor_prev( [=](const nom::Event& evt) {
    this->cursor_prev();
  });

  auto cursor_next( [=](const nom::Event& evt) {
    this->cursor_next();
  });

  auto prev_page( [=](const nom::Event& evt) {
    this->prev_page();
  });

  auto next_page( [=](const nom::Event& evt) {
    this->next_page();
  });

  auto delete_card( [=](const nom::Event& evt) {

    // Player's rendered card
    auto page_pos = this->cursor_position();
    auto card_pos = this->game->cards_page_model_->map_card_pos(page_pos);
    Card card_sp =
      this->game->cards_page_model_->find_by_pos(card_pos);

    this->remove_player_card(card_sp);
  });

  auto select_card( [=, &p1_hand](const nom::Event& evt) {
    // Player's rendered card
    auto page_pos = this->cursor_position();
    auto card_pos = this->game->cards_page_model_->map_card_pos(page_pos);
    Card card_sp =
      this->game->cards_page_model_->find_by_pos(card_pos);

    this->add_player_card(card_sp);
  });

  auto start_game( [=](const nom::Event& evt) {
    this->game->set_state(Game::State::ConfirmationDialog);
  });

  // TODO: Rename to save_player_game0 ..?
  auto save_game0( [=](const nom::Event& evt) {

    if( this->save_player_hand("build0.json") == false ) {
      this->game->cursor_wrong->Play();
    }

    // Success!
    this->game->save_game_sfx->Play();
  });

  // TODO: Rename to save_player_game1 ..?
  auto save_game1( [=](const nom::Event& evt) {

    if( this->save_player_hand("build1.json") == false ) {
      this->game->cursor_wrong->Play();
    }

    // Success!
    this->game->save_game_sfx->Play();
  });

  // TODO: Rename to load_player_game0 ..?
  auto load_game0( [=](const nom::Event& evt) {

    if( this->load_player_hand("build0.json") == false ) {
      this->game->cursor_wrong->Play();
    }

    // Success!
    this->game->save_game_sfx->Play();
  });

  // TODO: Rename to load_player_game1 ..?
  auto load_game1( [=](const nom::Event& evt) {

    if( this->load_player_hand("build1.json") == false ) {
      this->game->cursor_wrong->Play();
    }

    // Success!
    this->game->save_game_sfx->Play();
  });

  auto clear_player_hand( [=](const nom::Event& evt) mutable {

    // Render an invalid card texture afterwards
    auto card_sp = Card::null;
    this->set_display_card(card_sp);

    this->erase_player_cards(&p1_hand);
    this->game->card_place->Play();
  });

  // ...Keyboard input mappings...

  state.insert("cursor_prev", nom::KeyboardAction(SDLK_UP), cursor_prev);
  state.insert("cursor_next", nom::KeyboardAction(SDLK_DOWN), cursor_next);
  state.insert("prev_page", nom::KeyboardAction(SDLK_LEFT), prev_page);
  state.insert("next_page", nom::KeyboardAction(SDLK_RIGHT), next_page);
  state.insert("delete_card", nom::KeyboardAction(SDLK_d), delete_card);
  state.insert("select_card", nom::KeyboardAction(SDLK_SPACE), select_card);
  state.insert("start_game", nom::KeyboardAction(SDLK_RETURN), start_game);

  state.insert("save_game0", nom::KeyboardAction(SDLK_1, KMOD_LALT), save_game0);
  state.insert("save_game1", nom::KeyboardAction(SDLK_2, KMOD_LALT), save_game1);

  state.insert( "load_game0", nom::KeyboardAction(SDLK_1, platform_key_mod),
                load_game0 );
  state.insert( "load_game1", nom::KeyboardAction(SDLK_2, platform_key_mod),
                load_game1);
  state.insert( "clear_player_hand", nom::KeyboardAction(SDLK_0,
                platform_key_mod), clear_player_hand);

  // ...Mouse wheel input mappings...

  state.insert( "cursor_prev",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_UP), cursor_prev );
  state.insert( "cursor_next",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_DOWN), cursor_next );
  state.insert( "prev_page",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_LEFT), prev_page );
  state.insert( "next_page",
                nom::MouseWheelAction(nom::MOUSE_WHEEL_RIGHT), next_page );

  // ...Joystick input mappings...
  auto& joystick_id = this->game->joystick_id_;

  state.insert( "cursor_prev",
                nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_UP), cursor_prev );

  state.insert( "cursor_next", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_DOWN), cursor_next );

  state.insert( "prev_page", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_LEFT), prev_page );

  state.insert( "next_page", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_DPAD_RIGHT), next_page );

  state.insert( "delete_card", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_B), delete_card );

  state.insert( "select_card", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_A), select_card );

  state.insert( "start_game", nom::GameControllerButtonAction(joystick_id,
                nom::GameController::BUTTON_START), start_game );


  if( this->game->debug_game_ == true ) {

    // NOTE: ...Testing of CardsPageDataSource functionality...

    auto debug_p1_append_card( [=](const nom::Event& evt) {

      nom::size_type num_deck = (p1_db->size() - 1);
      uint32 loop_ticks = 0;

      // Pick a card at random
      while( loop_ticks < 100 ) {

        int random_card_id =
          nom::uniform_int_rand<int>(0, num_deck);

        Card card = p1_db->find(random_card_id);
        if( card != Card::null ) {
          this->append_player_card(card);
          break;
        }

        // NOTE: Infinite loop guard
        ++loop_ticks;
      }
    });

    auto debug_p1_append_cards( [=](const nom::Event& evt) {

      int max_per_page =
        this->game->cards_page_model_->cards_per_page();
      nom::size_type num_cards = 0;
      nom::size_type num_deck = (p1_db->size() - 1);
      Cards cards;

      // Fill one page worth with randomly picked cards
      while( num_cards < max_per_page ) {

        int random_card_id =
          nom::uniform_int_rand<int>(0, num_deck);

        Card card = p1_db->find(random_card_id);
        if( card != Card::null ) {
          cards.push_back(card);
          ++num_cards;
        }
      }

      this->append_player_cards(cards);
    });

    auto debug_p1_erase_card( [=](const nom::Event& evt) {

      int page_pos = -1;
      int card_pos = 0;
      int erased_pos = 0;

      page_pos = this->cursor_position();
      card_pos = this->game->cards_page_model_->map_card_pos(page_pos);

      if( page_pos != -1 ) {
        erased_pos = this->game->cards_page_model_->erase_card(card_pos);
      }
    });

    state.insert( "debug_p1_append_card", nom::KeyboardAction(SDLK_t),
                  debug_p1_append_card );
    state.insert( "debug_p1_append_cards",
                  nom::KeyboardAction(SDLK_t, platform_key_mod),
                  debug_p1_append_cards );

    state.insert( "debug_p1_erase_card",
                  nom::KeyboardAction(SDLK_d, platform_key_mod),
                  debug_p1_erase_card );
  } // end if debug game is enabled

  this->game->input_mapper.erase( "CardsMenuState" );
  this->game->input_mapper.insert( "CardsMenuState", state, true );
  this->game->input_mapper.activate_only( "CardsMenuState" );
  this->game->input_mapper.activate( "Game" );

  this->game->cards_menu_.show();

  // Yeah buddy!
  this->game->theme_track_->Play();

  // Set the initial display card -- for selection -- to always be the first
  // card of the player's deck, and always render the card as if it was
  // already in the player's deck
  Card card_sp = p1_db->front();
  tt::set_card_id(card_sp, p1_hand.player_id() );

  this->set_display_card(card_sp);
}

void CardsMenuState::on_exit(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  this->game->cards_menu_.close();
  Rocket::Core::Factory::ClearStyleSheetCache();
  Rocket::Core::Factory::ClearTemplateCache();

  this->game->actions_.cancel_actions();
}

void CardsMenuState::on_pause(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  // Hide the cursor so that it doesn't show up during undesirable states such
  // as during the ConfirmationDialogState or Pause states.
  this->game->cursor_->set_frame(INTERFACE_CURSOR_HIDDEN);
}

void CardsMenuState::on_resume(nom::void_ptr data)
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE_STATES );

  // Restore the rendering of the game cursor
  this->game->cursor_->set_frame(INTERFACE_CURSOR_SHOWN);

  // Ensure that the correct input contexts are reset
  this->game->input_mapper.activate_only("CardsMenuState");
  this->game->input_mapper.activate("Game");

  // Response from the previous game state
  nom::int32_ptr response = static_cast<nom::int32_ptr>(data);

  // User is happy with their cards -- let's play! Response from
  // ConfirmationDialogState was 'yes'
  if( response != nullptr && *response == 1 ) {

    NOM_DELETE_PTR(response);

    auto p1_db = this->game->cards_db_[PlayerIndex::PLAYER_1].get();
    auto& p1_hand = this->game->hand[PlayerIndex::PLAYER_1];
    // auto& p2_hand = this->game->hand[PlayerIndex::PLAYER_2];

    if( this->game->debug_game_ == true ) {

      while( p1_hand.size() < MAX_PLAYER_HAND ) {
        p1_hand.add_random_card(1, 10, p1_db);
      }
    } // end if debug game is enabled

    // Validate conditions necessary to advance onwards
    if( p1_hand.size() < MAX_PLAYER_HAND ) {
      this->game->cursor_wrong->Play();
    } else {
      this->game->set_state(Game::State::Play);
    }
  } // end if response was YES
}

void CardsMenuState::on_update(nom::real32 delta_time)
{
  this->update_cursor();

  this->game->window.update();
  this->game->gui_window_.update();

  this->update_page_indicators();
}

void CardsMenuState::on_draw(nom::RenderWindow& target)
{
  this->game->background.draw(target);

  // Player 2 is to the left
  tt::render_player_hand(target, &this->game->hand[PlayerIndex::PLAYER_2]);

  // Player 1 is to the right
  tt::render_player_hand(target, &this->game->hand[PlayerIndex::PLAYER_1]);

  this->game->gui_window_.draw();

  TT_RENDER_SPRITE(this->game->cursor_);
  TT_RENDER_SPRITE(this->p1_selected_card_sprite_);
}

bool CardsMenuState::set_display_card(Card& card)
{
  bool result = false;

  // NOTE: Render the card as if it was already in the player's deck
  tt::set_card_id(card, this->game->hand[PlayerIndex::PLAYER_1].player_id() );

  auto p1_card_renderer =
    tt::create_card_renderer(this->game->card_res_.get(), card);

  NOM_ASSERT(p1_card_renderer != nullptr);
  NOM_ASSERT(p1_card_renderer->valid() == true);

  if( p1_card_renderer != nullptr && p1_card_renderer->valid() == true ) {

    this->p1_selected_card_sprite_ = p1_card_renderer->rendered_card();
    if( this->update_display_card() == true ) {
      // Success!
      result = true;
    } else {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Failed to update the player's display card" );
    }
  }

  // Err
  return result;
}

// Private scope

bool CardsMenuState::on_event(const nom::Event& ev)
{
  return false;
}

void CardsMenuState::on_mouse_button_up(Rocket::Core::Event& ev)
{
  using namespace Rocket::Core;

  Element* target = ev.GetTargetElement();

  NOM_ASSERT( target != nullptr );
  if( target == nullptr ) {
    return;
  }

  if( ev == "mouseup" ) {

    Input::KeyIdentifier button =
      (Input::KeyIdentifier) ev.GetParameter<int>("button", 3);

    NOM_ASSERT( this->game->cards_page_model_ != nullptr );
    if( this->game->cards_page_model_ == nullptr ) {
      return;
    }

    std::string card_name = target->GetInnerRML().CString();
    Card card =
      this->game->cards_page_model_->find_by_name(card_name);

    if( target->GetTagName() == "card" && button == 0 ) {
      // Left click
      this->add_player_card(card);
    } else if( target->GetTagName() == "card" && button == 1 ) {
      // Right click
      this->remove_player_card(card);
    }

    NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_TEST, target->GetInnerRML().CString() );
  } // end if mouseup event
}

// void CardsMenuState::on_left_mouse_button_up(const nom::Event& ev)
// {
//   using namespace Rocket::Core;
//   Vector2f mouse_coords(ev.mouse.x, ev.mouse.y);

//   Element* target =
//     this->game->cards_menu_.document()->GetElementById("datagridrow");
//   NOM_ASSERT( target != nullptr );
//   if( target == nullptr ) return;

//   NOM_ASSERT( this->game->cards_page_model_ != nullptr );
//   if( this->game->cards_page_model_ == nullptr ) return;

//   Card card =
//     this->game->cards_page_model_->find_by_name( target->GetInnerRML().CString() );

//   if( target->IsPointWithinElement(mouse_coords) ) {
//       this->add_player_card(card);
//   } // end if target is within mouse hit test

//   NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_TEST, target->GetInnerRML().CString() );
// }

// void CardsMenuState::on_right_mouse_button_up(const nom::Event& ev)
// {
//   using namespace Rocket::Core;
//   Vector2f mouse_coords(ev.mouse.x, ev.mouse.y);

//   Element* target =
//     this->game->cards_menu_.document()->GetElementById("datagridrow");
//   NOM_ASSERT( target != nullptr );
//   if( target == nullptr ) return;

//   NOM_ASSERT( this->game->cards_page_model_ != nullptr );
//   if( this->game->cards_page_model_ == nullptr ) return;

//   Card card =
//     this->game->cards_page_model_->find_by_name( target->GetInnerRML().CString() );

//   if( target->IsPointWithinElement(mouse_coords) ) {
//       this->remove_player_card(card);
//   } // end if target is within mouse hit test

//   NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_TEST, target->GetInnerRML().CString() );
// }

void CardsMenuState::update_cursor()
{
  if( this->cursor_state_ == 0 &&
      this->game->cursor_->position() != Point2i::null )
  {
    this->game->cursor_->set_frame(INTERFACE_CURSOR_SHOWN);
  } else {
    this->game->cursor_->set_frame(INTERFACE_CURSOR_HIDDEN);
  }
}

void CardsMenuState::update_page_indicators()
{
  using namespace Rocket::Core;

  int page = 0;         // The current card page
  int num_pages = 0;  // Total card pages
  Element* page_left = nullptr;
  Element* page_right = nullptr;

  NOM_ASSERT( this->game->cards_page_model_ != nullptr );
  if( this->game->cards_page_model_ == nullptr ) {
    return;
  }

  page = this->game->cards_page_model_->page();
  num_pages = this->game->cards_page_model_->total_pages();

  // Update left page indicator
  page_left =
    this->game->cards_menu_.document()->GetElementById("page-left");
  NOM_ASSERT( page_left != nullptr );

  if( page_left ) {
    page_left->SetClassNames("");

    if( page > 0 ) {
      page_left->SetClassNames("page-left");
    }
  } // end if page_left

  // Update right page indicator
  page_right =
    this->game->cards_menu_.document()->GetElementById("page-right");
  NOM_ASSERT( page_right != nullptr );

  if( page_right ) {
    page_right->SetClassNames("");

    if( page >= 0 && (page < num_pages - 1) ) {
      page_right->SetClassNames("page-right");
    }
  } // end if page_right
}

int CardsMenuState::cursor_position()
{
  NOM_ASSERT( this->game->cards_page_model_ != nullptr );

  int pos = 0;

  if( this->game->cards_page_model_ != nullptr ) {

    for(  auto idx = 0;
          idx < this->game->cards_page_model_->cards_per_page();
          ++idx )
    {
      if( this->game->cursor_->position().y <= this->cursor_coords_map_.at(idx).y ) {
        return idx;
      } else {

        // assume we are at the last position in the index when all else fails
        pos = this->game->cards_page_model_->cards_per_page();
      }
    }
  }

  return pos;
}

void CardsMenuState::set_cursor_position(int pos)
{
  nom::size_type num_rows = this->cursor_coords_map_.size();

  Point2i render_pos = Point2i::null;
  if( pos <= num_rows ) {
    render_pos = this->cursor_coords_map_.at(pos).position();
    this->game->cursor_->set_position(render_pos);
  }
}

void CardsMenuState::prev_page()
{
  using namespace Rocket::Core;

  Card card_sp(Card::null);
  int page = 0;
  int page_pos = -1;
  int card_pos = 0;
  std::string page_str;

  if( this->cursor_state_ == 0 ) {

    NOM_ASSERT( this->game->cards_page_model_ != nullptr );
    if( this->game->cards_page_model_ == nullptr ) {
      return;
    }

    page = this->game->cards_page_model_->page();

    // Update cards model, play audio & set column text for previous page
    if( page > 0 ) {

      --page;
      this->game->cards_page_model_->set_page(page);
      this->update_page_count_title(page);

      // Player's rendered card
      page_pos = this->cursor_position();
      card_pos = this->game->cards_page_model_->map_card_pos(page_pos);
      card_sp =
        this->game->cards_page_model_->find_by_pos(card_pos);

      if( this->set_display_card(card_sp) == true ) {
        this->game->cursor_move->Play();
      } else {
        this->game->cursor_wrong->Play();
      }
    }
  } // end if cursor state == 0
}

void CardsMenuState::next_page()
{
  using namespace Rocket::Core;

  Card card_sp(Card::null);
  int page = 0;
  int num_pages = 0;
  int page_pos = -1;
  int card_pos = 0;
  std::string page_str;

  if( this->cursor_state_ == 0 ) {

    NOM_ASSERT( this->game->cards_page_model_ != nullptr );
    if( this->game->cards_page_model_ == nullptr ) {
      return;
    }

    page = this->game->cards_page_model_->page();
    num_pages = this->game->cards_page_model_->total_pages();

    // Update cards model, play audio & set column text for next page
    if( page < num_pages - 1 ) {

      ++page;
      this->game->cards_page_model_->set_page(page);
      this->update_page_count_title(page);

      // Player's rendered card
      page_pos = this->cursor_position();
      card_pos = this->game->cards_page_model_->map_card_pos(page_pos);
      card_sp =
        this->game->cards_page_model_->find_by_pos(card_pos);

      if( this->set_display_card(card_sp) == true ) {
        this->game->cursor_move->Play();
      } else {
        this->game->cursor_wrong->Play();
      }
    }

  } // end if cursor state == 0
}

void CardsMenuState::cursor_prev()
{
  Card card_sp(Card::null);
  int page_pos = -1;
  int card_pos = 0;
  Point2i move_to_offset(Point2i::zero);

  if( this->cursor_state_ == 0 ) {

    // Move up if the game cursor is before the first card entry of the page
    if( this->game->cursor_->position().y > this->cursor_coords_map_.at(0).y ) {

      move_to_offset.x = 0;
      move_to_offset.y = -(this->cursor_coords_map_.at(card_pos).h);
      this->game->cursor_->translate(move_to_offset);

      NOM_DUMP_VAR( TTCARDS_LOG_CATEGORY_TEST, "cursor.y:",
                    this->game->cursor_->position().y );

      // Player's rendered card
      page_pos = this->cursor_position();
      card_pos = this->game->cards_page_model_->map_card_pos(page_pos);
      card_sp =
        this->game->cards_page_model_->find_by_pos(card_pos);

      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_CARDS_MENU_STATE, "page_pos:",
                      page_pos );
      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_CARDS_MENU_STATE, "card_pos:",
                      card_pos );
      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_CARDS_MENU_STATE, "card_sp.id:",
                      card_sp.id );

      if( this->set_display_card(card_sp) == true ) {
        this->game->cursor_move->Play();
      } else {
        this->game->cursor_wrong->Play();
      }
    }
  } // end if cursor state == 0
}

void CardsMenuState::cursor_next()
{
  Card card_sp(Card::null);
  int page_pos = -1;
  int max_per_page = 0;
  int card_pos = 0;
  Point2i move_to_offset(Point2i::zero);

  NOM_ASSERT( this->game->cards_page_model_ != nullptr );
  if( this->game->cards_page_model_ != nullptr ) {
    // Next to last card
    max_per_page = this->game->cards_page_model_->cards_per_page() - 1;
    NOM_DUMP_VAR(TTCARDS_LOG_CATEGORY_TEST, "cursor.at(max_per_page).y:", this->cursor_coords_map_.at(max_per_page).y );
  }

  if( this->cursor_state_ == 0 ) {

    // Move down if the game cursor is not at the last card entry of the page
    if( this->game->cursor_->position().y >= this->cursor_coords_map_.at(0).y &&
        this->game->cursor_->position().y < this->cursor_coords_map_.at(max_per_page).y )
    {
      move_to_offset.x = 0;
      move_to_offset.y = this->cursor_coords_map_.at(card_pos).h;
      this->game->cursor_->translate(move_to_offset);

      NOM_DUMP_VAR(TTCARDS_LOG_CATEGORY_TEST, "cursor.y:", this->game->cursor_->position().y );

      // Player's rendered card
      page_pos = this->cursor_position();
      card_pos = this->game->cards_page_model_->map_card_pos(page_pos);
      card_sp =
        this->game->cards_page_model_->find_by_pos(card_pos);

      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_CARDS_MENU_STATE, "page_pos:",
                      page_pos );
      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_CARDS_MENU_STATE, "card_pos:",
                      card_pos );
      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_CARDS_MENU_STATE, "card_sp.id:",
                      card_sp.id );

      if( this->set_display_card(card_sp) == true ) {
        this->game->cursor_move->Play();
      } else {
        this->game->cursor_wrong->Play();
      }
    }
  } // end if cursor state == 0
}

void CardsMenuState::add_player_card(const Card& card)
{
  using namespace Rocket::Core;

  int page_pos = -1;
  int card_pos = 0;

  auto& p1_hand = this->game->hand[PlayerIndex::PLAYER_1];
  Point2i p1_card_pos(Point2i::zero);

  NOM_ASSERT( this->game->cards_page_model_ != nullptr );
  if( this->game->cards_page_model_ == nullptr ) {
    return;
  }

  Card c = card;
  page_pos = this->cursor_position();
  card_pos = this->game->cards_page_model_->map_card_pos(page_pos);

  // Card logic for adding a card to the player's hand
  //
  // 1. Decrease available card count by one
  // 2. Sync the cards model to reflect modified card count (-1)
  // 3. Update player hand w/ modified card.
  // 5. Queue audio clip
  if( c.num > 0 ) {

    c.num = (c.num - 1);

    if( p1_hand.push_back(c) == true ) {

      this->game->cards_page_model_->insert_card(card_pos, c);

      tt::update_hand_rendering(&p1_hand, PLAYER1_ORIGIN);

      // Success; card has been added to the player's hand
      this->game->card_place->Play();
    } else {
      this->game->cursor_wrong->Play();
    }
  }
}

void CardsMenuState::remove_player_card(const Card& card)
{
  using namespace Rocket::Core;

  int page_pos = -1;
  int card_pos = 0;

  auto& p1_hand = this->game->hand[PlayerIndex::PLAYER_1];
  Point2i p1_card_pos(Point2i::zero);

  NOM_ASSERT( this->game->cards_page_model_ != nullptr );
  if( this->game->cards_page_model_ == nullptr ) {
    return;
  }

  Card c = card;
  page_pos = this->cursor_position();
  card_pos = this->game->cards_page_model_->map_card_pos(page_pos);

  // Use the opponent's card deck to compare total number of cards (to return)
  //
  // TODO: This is stub code and needs to be replaced with an actual deck
  // query that is independent of the opponent's total number of cards count
  Card ref_card =
    this->game->cards_db_[PlayerIndex::PLAYER_1]->find(c.id);

  // Card logic for removing a card from the player's hand
  //
  // 1. Increase available card count by one
  // 2. Sync the cards model to reflect modified card count (+1)
  // 3. Remove the card from the player's hand.
  // 5. Queue audio clip
  if( c.num < ref_card.num ) {

    c.num = (c.num + 1);

    if( p1_hand.erase(c) == true ) {

      this->game->cards_page_model_->insert_card(card_pos, c);

      tt::update_hand_rendering(&p1_hand, PLAYER1_ORIGIN);

      this->game->cursor_cancel->Play();
    } else {
      this->game->cursor_wrong->Play();
    }
  }
}

void CardsMenuState::update_page_count_title(nom::size_type page)
{
  const std::string page_str = std::to_string(page + 1);
  const std::string page_title = "CARDS P. " + page_str;

  this->game->cards_page_model_->set_page(page);
  this->game->cards_menu_.set_column_title(tt::CARDS_COLUMN_TITLE, page_title);
  this->update_page_indicators();
}

void CardsMenuState::append_player_card(const Card& card)
{
  int curr_page = this->game->cards_page_model_->page();

  this->game->cards_page_model_->append_card(card);

  if( this->game->cards_page_model_->total_pages() > curr_page ) {
    // Reset UI
    this->next_page();
    this->set_cursor_position(0);
  }
}

void CardsMenuState::append_player_cards(const Cards& cards)
{
  int curr_page = this->game->cards_page_model_->page();

  this->game->cards_page_model_->append_cards(cards);

  if( this->game->cards_page_model_->total_pages() > curr_page ) {
    // Reset UI
    this->next_page();
    this->set_cursor_position(0);
  }
}

void CardsMenuState::erase_player_cards(CardHand* phand)
{
  NOM_ASSERT(phand != nullptr);
  if( phand != nullptr ) {
    phand->clear();
  } else {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Failed to erase the player's hand:",
                  "memory location was NULL." );
  }

  this->game->cards_page_model_->erase_cards();

  // Reset UI
  this->update_page_count_title(0);
  this->set_cursor_position(0);

  // Render invalid card texture
  auto card_sp = Card::null;
  this->set_display_card(card_sp);
}

bool CardsMenuState::save_player_hand(const std::string& filename)
{
  auto& paths = this->game->paths_;
  auto& p1_hand = this->game->hand[PlayerIndex::PLAYER_1];

  const std::string SAVE_GAME_PATH =
    paths["SAVE_DECK_DIR"] + filename;

  if( this->game->save_player_hand( nullptr, &p1_hand, nullptr,
                                    false, SAVE_GAME_PATH ) == false )
  {
    this->game->cursor_wrong->Play();
    return false;
  }

  // Success!
  this->game->save_game_sfx->Play();
  return true;
}

// TODO: Consider reloading the player's deck when it is empty..?
bool CardsMenuState::load_player_hand(const std::string& filename)
{
  auto& paths = this->game->paths_;
  auto& p1_hand = this->game->hand[PlayerIndex::PLAYER_1];

  const std::string SAVE_GAME_PATH =
    paths["SAVE_DECK_DIR"] + filename;

  if( this->game->load_player_hand( nullptr, &p1_hand, nullptr,
                                    false, SAVE_GAME_PATH ) == false )
  {
    this->game->cursor_wrong->Play();
    return false;
  }

  if( tt::update_hand_rendering(&p1_hand, PLAYER1_ORIGIN) > 0 ) {

    // Reset UI
    this->update_page_count_title(0);
    this->set_cursor_position(0);

    if( this->update_display_card() == true ) {

      // Success!
      this->game->save_game_sfx->Play();

      return true;
    }
  } else {
    // Err; the player's hand was not updated
    this->game->cursor_wrong->Play();
    return false;
  }
}

bool CardsMenuState::update_display_card()
{
  bool result = false;
  auto page_pos = -1;
  auto card_pos = 0;

  Card card_sp(Card::null);
  Point2i render_pos(Point2i::zero);

  page_pos = this->cursor_position();
  card_pos = this->game->cards_page_model_->map_card_pos(page_pos);
  NOM_ASSERT(card_pos >= 0);

  // Player's rendered card
  card_sp =
    this->game->cards_page_model_->find_by_pos(card_pos);

  render_pos.x =
    BOARD_ORIGIN_X + ( CARD_DIMS.w * 2 );

  render_pos.y =
    BOARD_ORIGIN_Y + ( (CARD_DIMS.h / 2) + CARD_DIMS.h * 1) - 8;

  if( this->p1_selected_card_sprite_ != nullptr ) {
    this->p1_selected_card_sprite_->set_position(render_pos);
    result = true;
  }

  return result;
}

} // namespace tt
