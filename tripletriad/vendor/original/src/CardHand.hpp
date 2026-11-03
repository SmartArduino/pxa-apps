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
#ifndef GAMEAPP_CARD_HAND_HEADERS
#define GAMEAPP_CARD_HAND_HEADERS

#include <vector>
#include <random>
#include <algorithm>
#include <memory>

#include <nomlib/config.hpp>

#include "Card.hpp"
#include "config.hpp"

namespace tt {

// Forward declarations
class CardCollection;
class CardResourceLoader;

class CardHand
{
  public:
    CardHand();
    ~CardHand();

    /// \brief Initialize the player's hand.
    ///
    /// \param pid One of the enumeration values of tt::PlayerIndex
    ///
    /// \remarks The player's index controls the ownership and rendering color
    /// of their cards.
    ///
    /// \see ::update
    bool init(CardResourceLoader* res, PlayerIndex pid);

    /// \brief Re-render the player's hand.
    ///
    /// \note This method call is necessary when render-able card attributes
    /// are modified outside of this class interface.
    bool update();

    /// \brief Append a card to the player's hand.
    ///
    /// \remarks The ownership and rendering color attributes of the card will
    /// be set to the player determined from this object's instance
    /// initialization.
    ///
    /// \see ::init
    bool push_back(const Card& card);

    /// \brief ...
    ///
    /// \param
    ///
    /// \remarks ...
    bool push_back(const Cards& cards);

    bool erase(const Card& card);

    void clearSelectedCard();
    const Card& getSelectedCard();

    /// \deprecated Use the provided iterators ::previous, ::next,
    /// ::set_position, etc.
    void selectCard(const Card& card);

    bool exists(const Card& card) const;

    /// Make selected card the first element in stack
    ///
    /// The current card position is reset to zero.
    void front();

    /// Obtain the current position we are at in the cards vector
    ///
    /// Returns a number between zero and the maximum card element in the
    /// vector.
    nom::size_type position();

    /// Increment the position we are at in the cards vector by one.
    ///
    /// If we are at the end of the vector of cards, we reset the position to
    /// zero.
    ///
    /// We always update the selected card to this calculated position.
    void next();

    /// Decrement the position we are at in the cards vector by one.
    ///
    /// If we are at the beginning or the end of the vector of cards, we reset
    /// the position to zero.
    ///
    /// We always update the selected card to this calculated position.
    void previous();

    /// Sets the position we are at in the cards vector
    void set_position(nom::size_type pos);

    void clear();

    bool empty() const;
    nom::uint32 size() const;
    nom::int32 at(const Card& card);

    /// \brief Create a randomized player hand with a set criteria.
    void
    add_random_card(  nom::uint32 min_level, nom::uint32 max_level,
                      const CardCollection* db );

    /// Getter for obtaining the strongest card in the player's hand.
    ///
    /// Note that this does not take into account game rules that may be in
    /// effect!
    Card strongest();

    /// Getter for obtaining the weakest card in the player's hand.
    ///
    /// Note that this does not take into account game rules that may be in
    /// effect!
    Card weakest();

    CardsIterator begin();
    CardsIterator end();

    ConstCardsIterator begin() const;
    ConstCardsIterator end() const;

    PlayerID player_id() const;
    PlayerIndex player_index() const;

    /// \todo Declare in private scope
    Cards cards;

  private:
    /// Track the current position we are at in the cards vector
    nom::size_type position_ = 0;

    /// Player's active card
    Card selectedCard;

    /// \brief Our reference to the cards resource loader.
    ///
    /// \remarks We do not own this pointer, so we **must not** free it!
    CardResourceLoader* card_res_;

    /// \brief The player identifier.
    PlayerIndex player_index_ = PLAYER_INVALID;
};

/// Pretty print the the card attributes.
std::ostream& operator <<(std::ostream& os, const CardHand& rhs);

nom::Value
serialize_hand(const CardHand* phand);

tt::Cards
deserialize_hand(PlayerID player_id, const nom::Value& objects);

} // namespace tt

#endif // GAMEAPP_CARD_HAND_HEADERS defined
