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
#ifndef TTCARDS_HELPERS_HPP
#define TTCARDS_HELPERS_HPP

#include <nomlib/config.hpp>
#include <nomlib/math.hpp>
#include <nomlib/graphics.hpp>

#include "config.hpp"
#include "types.hpp"

namespace tt {

// Forward declarations
struct Card;
class CardResourceLoader;
class Board;
class CardHand;

// Get the player's unique instance identifier.
PlayerID player_id(PlayerIndex player_index);

// Flip the card's player owning ID to the opponent
PlayerID flip_player_id(PlayerIndex player_index);

bool
render_card_background( PlayerID player_id, const nom::Point2i& pos,
                        nom::Gradient* card_background,
                        nom::RenderTarget& target,
                        const nom::Texture* render_target = nullptr );

bool
render_card_face( CardID face_id, const nom::Point2i& pos,
                  nom::SpriteBatch* card_face,
                  nom::RenderTarget& target,
                  const nom::Texture* render_target = nullptr );

bool
render_custom_card_face(  nom::Sprite* card_face, const nom::Point2i& pos,
                          nom::RenderTarget& target,
                          const nom::Texture* render_target );

bool render_card_element( nom::uint32 element_id, const nom::Point2i& pos,
                          nom::SpriteBatch* card_element,
                          nom::RenderTarget& target,
                          const nom::Texture* render_target = nullptr );

bool
render_card_text( nom::uint32 rank, const nom::Point2i& pos,
                  nom::Text* card_text, nom::RenderTarget& target,
                  const nom::Texture* render_target = nullptr );

std::shared_ptr<nom::Sprite>
generate_text_sprite(nom::Text& font, const std::string& text);

/// \brief Mutate a player's hand (card ranks).
///
/// \fixme This method is not properly handling the position of a player's
/// hand when more than one card of the same ID is present.
///
/// \fixme This method breaks when we use ::set_position or ::selectCard; the
/// modified card's position is incorrect and crashes under Windows OS.
///
/// \todo Implement generic usage of this function (i.e.: board) -- perhaps
/// by breaking things into multiple functions, with the base version taking a
/// typical Card object ...
void
modify_card_rank( CardResourceLoader* card_res, CardHand* player_hand, bool modifier,
                  nom::uint32 direction );

void set_face_down(CardHand* player_hand, bool face_down);

/// \brief Set the ownership and rendering color of a card.
void set_card_id(Card& card, PlayerID pid);

/// \brief Set the rendering coordinates of the player's cards in their hand.
///
/// \returns The number of successfully modified cards.
nom::uint32
update_hand_rendering(CardHand* phand, const nom::Point2i& origin);

/// \brief Render the player's hand.
void render_player_hand(const nom::RenderTarget& target, const CardHand* phand);

// NOTE: This only modifies the rank attribute(s) -- no rendering is done to
// the number glyphs.
void increase_card_rank(CardRank rank, Card& card);

// NOTE: This only modifies the rank attribute(s) -- no rendering is done to
// the number glyphs.
void decrease_card_rank(CardRank rank, Card& card);

void set_frame_interval(nom::uint32 interval);

std::string version_string();

} // namespace tt

#endif // include guard defined
