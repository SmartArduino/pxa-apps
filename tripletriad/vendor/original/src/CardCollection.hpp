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
#ifndef TTCARDS_CARD_COLLECTION_HPP
#define TTCARDS_CARD_COLLECTION_HPP

#include <string>

#include "config.hpp"
#include "Card.hpp"

namespace tt {

class CardCollection
{
  public:
    CardCollection();
    ~CardCollection();

    void clear();
    nom::size_type size() const;

    /// \returns The first card in the container on success, or Card::null on
    /// failure, such as when the container is empty.
    const Card& front() const;

    /// Save the current board grid data to a file as a series of RFC 4627
    /// compliant JSON objects.
    bool save(const std::string& filename);

    /// Load saved board grid data from a file encoded as RFC 4627 compliant
    /// JSON objects.
    bool load(const std::string& filename);

    /// \returns The resulting card by its name on success, or Card::null on
    /// failure to find a card by the given name.
    const Card& find(const std::string& card_name) const;

    /// \returns The resulting card by its ID on success, or Card::null on
    /// failure to find a card by the given ID.
    const Card& find(CardID card_id) const;

    /// \brief Append a card at the end of the deck.
    ///
    /// \note When adding a card that exists in the deck, the number attribute
    /// is incremented by one -- if less than 99. When the card does **not**
    /// exist, it will be appended with a number attribute of one (1).
    void append_card(const Card& card);

    /// \brief Append one or more cards at the end of the deck.
    void append_cards(const Cards& cards);

    /// \brief Erase a card from the deck.
    ///
    /// \note When removing a card from the deck, the number attribute is
    /// decremented by one (1) and will be removed from the deck once the
    /// number attribute is equal to zero (0).
    void erase_card(const Card& card);

    ConstCardsIterator begin() const;
    ConstCardsIterator end() const;

    CardsIterator begin();
    CardsIterator end();

  private:
    Cards cards_;
};

nom::Value
serialize_deck(const CardCollection* deck);

tt::Cards
deserialize_deck(const nom::Value& objects);

} // namespace tt

#endif // GAMEAPP_CARD_COLLECTION_HEADERS defined
