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
#ifndef TTCARDS_MAIN_MENU_HPP
#define TTCARDS_MAIN_MENU_HPP

#include <iostream>
#include <string>
#include <memory>

#include <nomlib/graphics.hpp>
// #include <nomlib/gui.hpp>
#include <nomlib/system.hpp>

#include "config.hpp"
#include "Card.hpp"

namespace tt {

// Forward declarations
class Game;

class MainMenuState: public nom::IState
{
  public:
    MainMenuState(nom::SDLApp* object);

    virtual ~MainMenuState();

    /// \todo Change return type to bool
    void on_init(nom::void_ptr data);
    void on_exit(nom::void_ptr data);

    void on_pause(nom::void_ptr data);
    void on_resume(nom::void_ptr data);

    void on_update(nom::real32 delta_time);
    void on_draw(nom::RenderWindow& target);

  private:
    /// \brief The default event handler for this state.
    bool on_event(const nom::Event& evt) override;

    void on_mouse_button_up(const nom::Event& evt);

    void on_confirm_selection(const nom::Event& evt);

    /// \brief Add a textual menu item.
    ///
    /// \returns The element index of the menu entry.
    nom::size_type append_menu_entry(const std::string& entry_text);

    /// \brief Reset rendering positions for stored text entries.
    bool update_menu_entries();

    void update_cursor();

    /// \brief Get the game cursor position.
    ///
    /// \returns A number between the first menu entry index and last menu
    /// entry index on success, and negative one (-1) on failure.
    int cursor_position();

    /// \brief Set the rendering position of the game cursor.
    ///
    /// \param pos A number between the first menu entry index and last menu
    /// entry index.
    void set_cursor_position(int pos);

    /// \brief Move the game cursor to the previous menu text entry.
    void cursor_prev();

    /// \brief Move the game cursor to the next menu text entry.
    void cursor_next();

    Game* game = nullptr;

    /// \brief The renderable menu entries.
    std::map<nom::size_type, nom::Text> menu_entries_;

    /// \brief The game cursor's minimum and maximum rendering bounds.
    std::vector<nom::IntRect> cursor_pos_bounds_;

    /// \brief The menu element index.
    ///
    /// \see MenuEntry enumeration
    int cursor_pos_ = 0;

    /// \brief The menu text entries.
    ///
    /// \see ::on_confirm_selection
    enum MenuEntry
    {
      /// Load a game
      MENU_ENTRY_CONTINUE = 0,

      /// Start a new game
      MENU_ENTRY_NEW_GAME,

      MENU_ENTRY_VIEW_CARDS,
      MENU_ENTRY_OPTIONS,
      MENU_ENTRY_CREDITS,
      MENU_ENTRY_QUIT,

      /// Total number of menu entries
      TOTAL_MENU_ENTRIES,
    };

    bool continue_game_ = false;
};

// Convenience declarations for changing state
typedef std::unique_ptr<MainMenuState> MainMenuStatePtr;

} // namespace tt

#endif // GAMEAPP_CARDS_MENU_HEADERS defined
