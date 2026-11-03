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
#ifndef GAMEAPP_CARDS_MENU_HEADERS
#define GAMEAPP_CARDS_MENU_HEADERS

#include <iostream>
#include <string>
#include <memory>

#include <nomlib/graphics.hpp>
#include <nomlib/gui.hpp>
#include <nomlib/system.hpp>

#include "config.hpp"
#include "CardsPageDataSource.hpp"
#include "Card.hpp"

namespace tt {

// Forward declarations
class Game;
class CardHand;

class CardsMenuState: public nom::IState
{
  public:
    CardsMenuState(nom::SDLApp* object);

    virtual ~CardsMenuState();

    /// \todo Change return type to bool
    void on_init(nom::void_ptr data);
    void on_exit( nom::void_ptr data );

    void on_pause(nom::void_ptr data);
    void on_resume(nom::void_ptr data);

    void on_update(nom::real32 delta_time);
    void on_draw(nom::RenderWindow& target);

    /// \brief Render the player's card sprite.
    ///
    /// \param card An existing card with a valid renderer attached.
    ///
    /// \returns Boolean TRUE when the card's sprite renderer has been
    /// successfully created, and boolean FALSE when the card's renderer has
    /// **not** been created, such as when the card renderer is invalid.
    bool set_display_card(Card& card);

  private:
    /// \brief The default event handler for this state.
    bool on_event(const nom::Event& ev) override;

    /// \brief GUI event callback for mouse button actions.
    ///
    /// \note This is processed on libRocket's event loop.
    void on_mouse_button_up(Rocket::Core::Event& ev);

    // void on_left_mouse_button_up(const nom::Event& ev);
    // void on_right_mouse_button_up(const nom::Event& ev);

    void update_cursor();

    /// \brief Set the appropriate page arrows for the shown page.
    void update_page_indicators();

    /// \brief Get the game cursor indexed position.
    ///
    /// \returns A row index number between zero (0) and the set number of
    /// cards per page, i.e.: ~0..10.
    ///
    /// \see tt::CardsPageDataSource
    int cursor_position();

    /// \brief Set the rendering position of the game cursor.
    ///
    /// \param pos The position index of the game cursor, relative to the shown
    /// page.
    void set_cursor_position(int pos);

    /// \brief Go to the previous page.
    void prev_page();

    /// \brief Go to the next page.
    void next_page();

    /// \brief Move the game cursor to the previous card entry.
    void cursor_prev();

    /// \brief Move the game cursor to the next card entry.
    void cursor_next();

    /// \brief Add a card to the player's hand.
    void add_player_card(const Card& card);

    /// \brief Remove a card from the player's hand.
    void remove_player_card(const Card& card);

    /// \brief Set the shown cards page.
    void update_page_count_title(nom::size_type page);

    /// \brief Add a card to the player's deck.
    void append_player_card(const Card& card);

    /// \brief Add one or more cards to the player's deck.
    void append_player_cards(const Cards& cards);

    /// \brief Remove all of the cards in the player's deck.
    ///
    /// \param phand A pointer to the player's hand instance.
    ///
    /// \remarks The player's hand is also cleared.
    void erase_player_cards(CardHand* phand);

    // Serialize the player's current build and store to disk
    bool save_player_hand(const std::string& filename);

    // Deserialize the player's current build from disk store
    bool load_player_hand(const std::string& filename);

    /// \brief Update the player's card sprite position.
    ///
    /// \returns Boolean TRUE when the sprite's position has been successfully
    /// updated, and boolean FALSE when the sprite's position has **not** been
    /// updated, such as when the sprite is NULL.
    bool update_display_card();

    Game* game = nullptr;

    std::shared_ptr<nom::Sprite> p1_selected_card_sprite_ = nullptr;

    /// \brief Bounds map for game cursor movement.
    std::vector<nom::IntRect> cursor_coords_map_;

    /// \brief Cursor state
    ///
    /// \remarks Not implemented
    nom::uint32 cursor_state_ = 0;
};

// Convenience declarations for changing state
typedef std::unique_ptr<CardsMenuState> CardsMenuStatePtr;

} // namespace tt

#endif // GAMEAPP_CARDS_MENU_HEADERS defined
